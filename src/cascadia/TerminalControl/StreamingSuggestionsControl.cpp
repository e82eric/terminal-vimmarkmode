// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "FuzzySearchTextSegment.h"
#include "ControlCore.h"
#include "StreamingSuggestionsControl.h"
#include "StreamingSuggestionsControl.g.cpp"
#include "SuggestionSearchRow.g.cpp"
#include "../WinRTUtils/inc/WtExeUtils.h"

#include <future>
#include <thread>

#include <condition_variable>

using namespace winrt::Windows::UI::Xaml::Media;

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace std::chrono_literals;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    static constexpr auto StreamingSuggestionItemHeight = 40.0;

    static size_t _streamingSuggestionsWorkerCount(const size_t itemCount) noexcept
    {
        if (itemCount == 0)
        {
            return 1;
        }

        const auto hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
        const auto defaultWorkers = std::max<size_t>(1, static_cast<size_t>(hardwareThreads) / 2);
        return std::min(defaultWorkers, itemCount);
    }

    struct CommandSearchHelper : std::enable_shared_from_this<CommandSearchHelper>
    {
        void Stop()
        {
            ++_generation;
            _stop();
        }

        Windows::Foundation::IAsyncAction StartAsync(
            winrt::hstring executable,
            Windows::Foundation::Collections::IVector<winrt::hstring> args,
            winrt::hstring workingDirectory,
            int32_t suggestionRow,
            SuggestionBatchHandler const& onBatch)
        {
            auto lifetime = shared_from_this();
            auto batchCb = winrt::make_agile(onBatch);
            const auto generation = ++_generation;

            _stop();
            co_await resume_background();

            const auto cmdStart = std::chrono::steady_clock::now();
            std::optional<std::chrono::steady_clock::time_point> cmdFirstOutputAt;
            std::optional<std::chrono::steady_clock::time_point> cmdFirstBatchAt;
            int32_t cmdEmittedTotal = 0;
            int32_t cmdBatchCount = 0;

            if (executable.empty())
            {
                co_return;
            }

            SECURITY_ATTRIBUTES saAttr{};
            saAttr.nLength = sizeof(saAttr);
            saAttr.bInheritHandle = TRUE;

            HANDLE stdoutReadRaw = nullptr;
            HANDLE stdoutWriteRaw = nullptr;
            if (!CreatePipe(&stdoutReadRaw, &stdoutWriteRaw, &saAttr, 0))
            {
                co_return;
            }

            wil::unique_handle stdoutRead{ stdoutReadRaw };
            wil::unique_handle stdoutWrite{ stdoutWriteRaw };
            SetHandleInformation(stdoutRead.get(), HANDLE_FLAG_INHERIT, 0);

            wil::unique_handle stdinHandle{ CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &saAttr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };
            wil::unique_handle stderrHandle{ CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &saAttr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };
            if (!stdinHandle || !stderrHandle)
            {
                co_return;
            }

            std::wstring commandLine;
            QuoteAndEscapeCommandlineArg(std::wstring_view{ executable }, commandLine);
            if (args)
            {
                for (const auto& arg : args)
                {
                    commandLine.push_back(L' ');
                    QuoteAndEscapeCommandlineArg(std::wstring_view{ arg }, commandLine);
                }
            }

            STARTUPINFOW si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = stdinHandle.get();
            si.hStdOutput = stdoutWrite.get();
            si.hStdError = stderrHandle.get();

            PROCESS_INFORMATION pi{};
            std::vector<wchar_t> commandLineBuffer{ commandLine.begin(), commandLine.end() };
            commandLineBuffer.push_back(L'\0');

            OutputDebugStringW((L"CMD: " + commandLine + L"\n").c_str());
            const auto createStart = std::chrono::steady_clock::now();
            const BOOL launched = CreateProcessW(
                nullptr,
                commandLineBuffer.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                nullptr,
                workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
                &si,
                &pi);
            const auto createEnd = std::chrono::steady_clock::now();

            stdoutWrite.reset();
            if (!launched)
            {
                const auto createUs = std::chrono::duration_cast<std::chrono::microseconds>(createEnd - createStart).count();
                OutputDebugStringW((L"[StreamingSuggestions][CMD] create=" + std::to_wstring(createUs) + L"us launch_failed=1\n").c_str());
                co_return;
            }

            wil::unique_process_handle process{ pi.hProcess };
            wil::unique_handle thread{ pi.hThread };
            wil::unique_process_handle stopHandle;
            DuplicateHandle(GetCurrentProcess(), process.get(), GetCurrentProcess(), stopHandle.addressof(), PROCESS_TERMINATE | SYNCHRONIZE, FALSE, 0);

            {
                std::lock_guard<std::mutex> lock{ _mutex };
                if (_generation.load() != generation)
                {
                    TerminateProcess(process.get(), 1);
                    co_return;
                }

                if (stopHandle)
                {
                    _process = std::move(stopHandle);
                }
            }

            std::unordered_set<std::wstring> seen;
            std::vector<SuggestionSearchItem> items;
            items.reserve(32);
            int32_t ordinal = 0;

            const auto flushBatch = [&]() {
                if (items.empty() || _generation.load() != generation)
                {
                    return;
                }

                const auto count = items.size();
                auto batch = winrt::make<winrt::Microsoft::Terminal::Control::implementation::SuggestionBatch>(std::move(items));
                items.clear();
                items.reserve(32);

                cmdEmittedTotal += static_cast<int32_t>(count);
                ++cmdBatchCount;
                if (!cmdFirstBatchAt)
                {
                    const auto now = std::chrono::steady_clock::now();
                    cmdFirstBatchAt = now;
                    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - cmdStart).count();
                    OutputDebugStringW((L"CMD: first batch after " + std::to_wstring(ms) + L"ms (" + std::to_wstring(count) + L" items)\n").c_str());
                }

                if (auto cb = batchCb.get())
                {
                    cb(batch);
                }
            };

            const auto appendLine = [&](std::string_view utf8Line) {
                if (utf8Line.empty())
                {
                    return;
                }

                auto text = til::u8u16(utf8Line);
                if (!text.empty() && text.back() == L'\r')
                {
                    text.pop_back();
                }

                const auto nonWhitespaceCount =
                    std::count_if(text.begin(), text.end(), [](wchar_t ch) {
                        return !std::iswspace(ch);
                    });

                if (nonWhitespaceCount == 0 || !seen.insert(text).second)
                {
                    return;
                }

                items.emplace_back(SuggestionSearchItem{
                    hstring{ text },
                    ordinal,
                    Core::Point{ 0, suggestionRow },
                    Core::Point{ 0, suggestionRow }
                });

                if (items.size() >= 32)
                {
                    flushBatch();
                }
            };

            std::string pending;
            char readBuffer[4096];
            DWORD bytesRead = 0;
            while (_generation.load() == generation &&
                   ReadFile(stdoutRead.get(), readBuffer, sizeof(readBuffer), &bytesRead, nullptr) &&
                   bytesRead > 0)
            {
                if (!cmdFirstOutputAt)
                {
                    cmdFirstOutputAt = std::chrono::steady_clock::now();
                }
                pending.append(readBuffer, bytesRead);

                size_t newline = std::string::npos;
                while ((newline = pending.find('\n')) != std::string::npos)
                {
                    auto line = pending.substr(0, newline);
                    pending.erase(0, newline + 1);
                    appendLine(line);
                }
            }

            if (_generation.load() == generation && !pending.empty())
            {
                appendLine(pending);
            }
            flushBatch();

            WaitForSingleObject(process.get(), INFINITE);

            const auto cmdTotalMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - cmdStart).count();
            OutputDebugStringW((L"CMD: done in " + std::to_wstring(cmdTotalMs) + L"ms total=" + std::to_wstring(cmdEmittedTotal) + L" batches=" + std::to_wstring(cmdBatchCount) + L"\n").c_str());
            const auto createUs = std::chrono::duration_cast<std::chrono::microseconds>(createEnd - createStart).count();
            const auto firstOutputUs = cmdFirstOutputAt ? std::chrono::duration_cast<std::chrono::microseconds>(*cmdFirstOutputAt - cmdStart).count() : -1;
            const auto firstBatchUs = cmdFirstBatchAt ? std::chrono::duration_cast<std::chrono::microseconds>(*cmdFirstBatchAt - cmdStart).count() : -1;
            const auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - cmdStart).count();
            OutputDebugStringW((L"[StreamingSuggestions][CMD] create=" + std::to_wstring(createUs) +
                                L"us first_output=" + std::to_wstring(firstOutputUs) +
                                L"us first_batch=" + std::to_wstring(firstBatchUs) +
                                L"us total=" + std::to_wstring(totalUs) +
                                L"us items=" + std::to_wstring(cmdEmittedTotal) +
                                L" batches=" + std::to_wstring(cmdBatchCount) + L"\n").c_str());

            std::lock_guard<std::mutex> lock{ _mutex };
            if (_generation.load() == generation)
            {
                _process.reset();
            }
        }

    private:
        void _stop() noexcept
        {
            std::lock_guard<std::mutex> lock{ _mutex };
            if (_process)
            {
                TerminateProcess(_process.get(), 1);
                _process.reset();
            }
        }

        std::mutex _mutex;
        wil::unique_process_handle _process;
        std::atomic<uint64_t> _generation{ 0 };
    };

    struct FileWalkerScanState
    {
        std::mutex queueMutex;
        std::condition_variable queueCv;
        std::queue<std::pair<int, std::wstring>> queue;
        std::atomic<int> pendingDirectoryCount{ 0 };
        std::atomic<bool> stopped{ false };

        std::mutex itemsMutex;
        std::vector<SuggestionSearchItem> items;
        std::atomic<int32_t> ordinal{ 0 };
    };

    struct FileWalkerSearchHelper : std::enable_shared_from_this<FileWalkerSearchHelper>
    {
        void Stop() noexcept
        {
            ++_generation;
            std::shared_ptr<FileWalkerScanState> state;
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                state = _activeState;
            }
            if (state)
            {
                state->stopped.store(true);
                state->queueCv.notify_all();
            }
        }

        Windows::Foundation::IAsyncAction StartAsync(
            Windows::Foundation::Collections::IVector<winrt::hstring> roots,
            int32_t maxDepth,
            bool includeHidden,
            bool directoriesOnly,
            bool filesOnly,
            SuggestionBatchHandler const& onBatch)
        {
            auto lifetime = shared_from_this();
            auto batchCb = winrt::make_agile(onBatch);
            const auto generation = ++_generation;

            co_await resume_background();

            const auto fwStart = std::chrono::steady_clock::now();

            if (!roots || roots.Size() == 0 || (directoriesOnly && filesOnly))
            {
                co_return;
            }

            const int effectiveMaxDepth = maxDepth <= 0 ? std::numeric_limits<int>::max() : maxDepth;

            auto state = std::make_shared<FileWalkerScanState>();
            state->items.reserve(32);

            auto firstBatchAt = std::make_shared<std::atomic<int64_t>>(-1);
            auto emittedTotal = std::make_shared<std::atomic<int32_t>>(0);
            auto batchCount = std::make_shared<std::atomic<int32_t>>(0);
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                _activeState = state;
            }

            const auto flushBatch = [state, batchCb, generation, this, fwStart, firstBatchAt, emittedTotal, batchCount]() {
                std::vector<SuggestionSearchItem> snapshot;
                {
                    std::lock_guard<std::mutex> lock(state->itemsMutex);
                    if (state->items.empty())
                    {
                        return;
                    }
                    snapshot = std::move(state->items);
                    state->items.clear();
                    state->items.reserve(32);
                }
                if (_generation.load() != generation || state->stopped.load())
                {
                    return;
                }
                const auto count = snapshot.size();
                emittedTotal->fetch_add(static_cast<int32_t>(count));
                batchCount->fetch_add(1);
                int64_t expected = -1;
                const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - fwStart).count();
                if (firstBatchAt->compare_exchange_strong(expected, nowMs))
                {
                    OutputDebugStringW((L"FW: first batch after " + std::to_wstring(nowMs) + L"ms (" + std::to_wstring(count) + L" items)\n").c_str());
                }
                auto batch = winrt::make<winrt::Microsoft::Terminal::Control::implementation::SuggestionBatch>(std::move(snapshot));
                if (auto cb = batchCb.get())
                {
                    cb(batch);
                }
            };

            const auto enqueueItem = [state, &flushBatch](std::wstring&& path) {
                bool shouldFlush = false;
                {
                    std::lock_guard<std::mutex> lock(state->itemsMutex);
                    state->items.emplace_back(SuggestionSearchItem{
                        hstring{ path },
                        state->ordinal.fetch_add(1, std::memory_order_relaxed),
                        Core::Point{ 0, 0 },
                        Core::Point{ 0, 0 } });
                    shouldFlush = state->items.size() >= 32;
                }
                if (shouldFlush)
                {
                    flushBatch();
                }
            };

            for (const auto& r : roots)
            {
                std::wstring root{ r };
                while (root.size() > 1 && (root.back() == L'\\' || root.back() == L'/') && !(root.size() == 3 && root[1] == L':'))
                {
                    root.pop_back();
                }
                if (root.empty())
                {
                    continue;
                }
                //OutputDebugStringW((L"FW: seed root=" + root + L"\n").c_str());
                if (!filesOnly)
                {
                    enqueueItem(std::wstring{ root });
                }
                {
                    std::lock_guard<std::mutex> lock(state->queueMutex);
                    state->pendingDirectoryCount.fetch_add(1);
                    state->queue.push({ 0, std::move(root) });
                }
            }
            state->queueCv.notify_all();
            //OutputDebugStringW((L"FW: seeded, pending=" + std::to_wstring(state->pendingDirectoryCount.load()) + L"\n").c_str());

            if (state->pendingDirectoryCount.load() == 0)
            {
                flushBatch();
                {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    if (_activeState == state)
                    {
                        _activeState.reset();
                    }
                }
                co_return;
            }

            const auto numWorkers = std::max(1u, std::thread::hardware_concurrency());
            //OutputDebugStringW((L"FW: spawning " + std::to_wstring(numWorkers) + L" workers\n").c_str());

            const auto worker = [state, generation, this, effectiveMaxDepth, includeHidden, directoriesOnly, filesOnly, &flushBatch]() {
                constexpr size_t kLocalItemFlush = 32;

                std::vector<SuggestionSearchItem> localItems;
                localItems.reserve(kLocalItemFlush);
                std::vector<std::wstring> localChildDirs;
                localChildDirs.reserve(32);
                int32_t nextOrdinal = 0;
                int32_t ordinalsRemaining = 0;

                const auto reserveOrdinal = [&]() -> int32_t {
                    constexpr int32_t kOrdinalChunk = 1024;
                    if (ordinalsRemaining == 0)
                    {
                        nextOrdinal = state->ordinal.fetch_add(kOrdinalChunk, std::memory_order_relaxed);
                        ordinalsRemaining = kOrdinalChunk;
                    }
                    --ordinalsRemaining;
                    return nextOrdinal++;
                };

                const auto drainLocalItems = [&]() {
                    if (localItems.empty())
                    {
                        return;
                    }
                    bool shouldFlush = false;
                    {
                        std::lock_guard<std::mutex> lock(state->itemsMutex);
                        state->items.insert(state->items.end(),
                                            std::make_move_iterator(localItems.begin()),
                                            std::make_move_iterator(localItems.end()));
                        shouldFlush = state->items.size() >= kLocalItemFlush;
                    }
                    localItems.clear();
                    if (shouldFlush)
                    {
                        flushBatch();
                    }
                };

                const auto pushLocalChildren = [&](int childDepth) {
                    if (localChildDirs.empty())
                    {
                        return;
                    }
                    const auto count = static_cast<int>(localChildDirs.size());
                    {
                        std::lock_guard<std::mutex> lock(state->queueMutex);
                        state->pendingDirectoryCount.fetch_add(count, std::memory_order_relaxed);
                        for (auto& cd : localChildDirs)
                        {
                            state->queue.push({ childDepth, std::move(cd) });
                        }
                    }
                    localChildDirs.clear();
                    state->queueCv.notify_all();
                };

                auto onWorkerExit = wil::scope_exit([&] {
                    drainLocalItems();
                });

                while (true)
                {
                    if (state->stopped.load() || _generation.load() != generation)
                    {
                        return;
                    }

                    std::pair<int, std::wstring> entry;
                    bool gotEntry = false;
                    {
                        std::unique_lock<std::mutex> lock(state->queueMutex);
                        state->queueCv.wait(lock, [&] {
                            return !state->queue.empty()
                                || state->pendingDirectoryCount.load() == 0
                                || state->stopped.load()
                                || _generation.load() != generation;
                        });

                        if (state->stopped.load() || _generation.load() != generation)
                        {
                            return;
                        }

                        if (state->queue.empty())
                        {
                            return;
                        }
                        entry = std::move(state->queue.front());
                        state->queue.pop();
                        gotEntry = true;
                    }

                    if (!gotEntry)
                    {
                        return;
                    }

                    const int depth = entry.first;
                    const std::wstring dirPath = std::move(entry.second);

                    auto decrementOnExit = wil::scope_exit([&] {
                        if (state->pendingDirectoryCount.fetch_sub(1) == 1)
                        {
                            state->queueCv.notify_all();
                        }
                    });

                    if (state->stopped.load() || _generation.load() != generation)
                    {
                        continue;
                    }

                    std::wstring searchPath;
                    searchPath.reserve(dirPath.size() + 3);
                    searchPath = dirPath;
                    if (!searchPath.empty() && searchPath.back() != L'\\' && searchPath.back() != L'/')
                    {
                        searchPath.push_back(L'\\');
                    }
                    searchPath.push_back(L'*');

                    WIN32_FIND_DATAW findData{};
                    wil::unique_hfind handle{ FindFirstFileExW(searchPath.c_str(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH) };
                    if (handle)
                    {
                        do
                        {
                            if (state->stopped.load() || _generation.load() != generation)
                            {
                                handle.reset();
                                break;
                            }

                            const std::wstring_view name{ findData.cFileName };
                            if (name == L"." || name == L"..")
                            {
                                continue;
                            }

                            const bool isHidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
                            if (!includeHidden && isHidden)
                            {
                                continue;
                            }

                            const bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

                            std::wstring fullPath;
                            fullPath.reserve(dirPath.size() + 1 + name.size());
                            fullPath = dirPath;
                            if (!fullPath.empty() && fullPath.back() != L'\\' && fullPath.back() != L'/')
                            {
                                fullPath.push_back(L'\\');
                            }
                            fullPath.append(name);

                            if (isDir)
                            {
                                const bool willRecurse = depth + 1 < effectiveMaxDepth;
                                if (!filesOnly)
                                {
                                    localItems.emplace_back(SuggestionSearchItem{
                                        hstring{ fullPath },
                                        reserveOrdinal(),
                                        Core::Point{ 0, 0 },
                                        Core::Point{ 0, 0 } });
                                    if (localItems.size() >= kLocalItemFlush)
                                    {
                                        drainLocalItems();
                                    }
                                }
                                if (willRecurse)
                                {
                                    localChildDirs.emplace_back(std::move(fullPath));
                                }
                            }
                            else
                            {
                                if (!directoriesOnly)
                                {
                                    localItems.emplace_back(SuggestionSearchItem{
                                        hstring{ fullPath },
                                        reserveOrdinal(),
                                        Core::Point{ 0, 0 },
                                        Core::Point{ 0, 0 } });
                                    if (localItems.size() >= kLocalItemFlush)
                                    {
                                        drainLocalItems();
                                    }
                                }
                            }
                        } while (FindNextFileW(handle.get(), &findData));
                    }

                    pushLocalChildren(depth + 1);
                }
            };

            std::vector<std::thread> workers;
            workers.reserve(numWorkers);
            for (uint32_t i = 0; i < numWorkers; ++i)
            {
                workers.emplace_back(worker);
            }
            for (auto& w : workers)
            {
                if (w.joinable())
                {
                    w.join();
                }
            }

            flushBatch();

            const auto fwTotalMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - fwStart).count();
            OutputDebugStringW((L"FW: done in " + std::to_wstring(fwTotalMs) + L"ms total=" + std::to_wstring(emittedTotal->load()) + L" batches=" + std::to_wstring(batchCount->load()) + L"\n").c_str());
            const auto fwFirstBatchMs = firstBatchAt->load();
            const auto fwFirstBatchUs = fwFirstBatchMs >= 0 ? fwFirstBatchMs * 1000 : -1;
            const auto fwTotalUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - fwStart).count();
            OutputDebugStringW((L"[StreamingSuggestions][FW] first_batch=" + std::to_wstring(fwFirstBatchUs) +
                                L"us total=" + std::to_wstring(fwTotalUs) +
                                L"us items=" + std::to_wstring(emittedTotal->load()) +
                                L" batches=" + std::to_wstring(batchCount->load()) + L"\n").c_str());

            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                if (_activeState == state)
                {
                    _activeState.reset();
                }
            }
        }

    private:
        std::atomic<uint64_t> _generation{ 0 };
        std::mutex _stateMutex;
        std::shared_ptr<FileWalkerScanState> _activeState;
    };

    winrt::event_token StreamingSuggestionsControl::PropertyChanged(const winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler& handler)
    {
        return _propertyChangedEvent.add(handler);
    }

    void StreamingSuggestionsControl::PropertyChanged(const winrt::event_token& token) noexcept
    {
        _propertyChangedEvent.remove(token);
    }

    StreamingSuggestionsControl::StreamingSuggestionsControl()
    {
        InitializeComponent();
        _commandSearchHelper = std::make_shared<CommandSearchHelper>();
        _fileWalkerHelper = std::make_shared<FileWalkerSearchHelper>();
        _focusableElements.insert(SearchBox());
        _focusableElements.insert(SplitSearchBox());

        _copyNotificationTimer = winrt::Windows::UI::Xaml::DispatcherTimer();
        _copyNotificationTimer.Interval(1s);
        _copyNotificationTimer.Tick({ this, &StreamingSuggestionsControl::_OnCopyNotificationTimerTick });

        _sizeChangedRevoker = ListBox().SizeChanged(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            if (Visibility() == Visibility::Visible)
            {
                this->_recalculateTopMargin();
            }
        });

        _initKeyBindings();
    }

    StreamingSuggestionsControl::~StreamingSuggestionsControl()
    {
        if (_commandSearchHelper)
        {
            _commandSearchHelper->Stop();
            _commandSearchHelper.reset();
        }
        if (_fileWalkerHelper)
        {
            _fileWalkerHelper->Stop();
            _fileWalkerHelper.reset();
        }
    }

    Controls::ListView StreamingSuggestionsControl::_activeListBox()
    {
        return _mode == StreamingSuggestionsMode::WordSplit ? SplitListBox() : ListBox();
    }

    uint64_t StreamingSuggestionsControl::_beginOpen(TermControl const& termControl, const _OpenState& state)
    {
        _mode = StreamingSuggestionsMode::Normal;
        _dataSource = state.dataSource;
        _showSplitOverlay(false);
        _characterHeight = state.characterHeight;
        _termControl = termControl;
        _currentWord = state.currentWord;
        _currentCommandline = state.currentCommandline;
        _currentSearchTerm = state.prefillFilter ? state.filterText : winrt::hstring{};
        _commandTemplate = state.commandTemplate;
        _sortResults = state.sortResults;
        _useCommandline = state.useCommandline;
        _replaceTarget = state.replaceTarget;
        _prefixWidth = state.prefixWidth;
        _swapChainOffset = state.swapChainOffset;

        const auto proposedX = gsl::narrow_cast<int>(state.anchor.X - state.prefixWidth);
        const auto maxX = gsl::narrow_cast<int>(state.space.Width - ActualWidth());
        const auto clampedX = std::clamp(proposedX, 0, maxX);
        Margin(Windows::UI::Xaml::ThicknessHelper::FromLengths(clampedX, 0, 0, 0));

        _anchor = state.anchor;
        _space = state.space;
        _searchVersion = 0;
        const auto sessionVersion = ++_sessionVersion;
        _allItemsLoaded = false;
        _allItemsSearched = false;
        _controlShown = false;

        _recalculateTopMargin();

        {
            std::lock_guard<std::mutex> lock(_batchesMutex);
            _batches.clear();
            _searchInFlight = false;
            _rerunNeededDuringSearch = false;
        }
        ListBox().ItemsSource(nullptr);
        ListBox().SelectedIndex(-1);
        NoItemsPlaceholder().Visibility(Visibility::Collapsed);

        _searchBoxMode = true;
        _suppressSearchBoxChange = true;
        auto reset = wil::scope_exit([this] { _suppressSearchBoxChange = false; });
        SearchBox().Text(_currentSearchTerm);
        Visibility(Windows::UI::Xaml::Visibility::Visible);
        SearchBox().Focus(Windows::UI::Xaml::FocusState::Programmatic);
        SearchBox().SelectionStart(_currentSearchTerm.size());

        _lastCompletedSearchVersion = _searchVersion;
        _isStreaming = true;
        _lastMatchedCount = 0;
        _lastTotalCount = 0;
        _updateLoadingIndicator();

        if (_commandSearchHelper)
        {
            _commandSearchHelper->Stop();
        }
        if (_fileWalkerHelper)
        {
            _fileWalkerHelper->Stop();
        }

        return sessionVersion;
    }

    Microsoft::Terminal::Control::SuggestionBatchHandler StreamingSuggestionsControl::_makeBatchHandler(uint64_t sessionVersion)
    {
        return Microsoft::Terminal::Control::SuggestionBatchHandler{
            [weakThis = get_weak(), sessionVersion](Microsoft::Terminal::Control::SuggestionBatch const& batch) {
                if (auto self = weakThis.get())
                {
                    if (self->_sessionVersion != sessionVersion || !self->_searchBoxMode)
                    {
                        return;
                    }
                    bool shouldTrigger = false;
                    {
                        std::lock_guard<std::mutex> lock(self->_batchesMutex);
                        self->_batches.push_back(batch);

                        if (self->_searchInFlight)
                        {
                            self->_rerunNeededDuringSearch = true;
                        }
                        else
                        {
                            shouldTrigger = true;
                        }
                    }
                    if (shouldTrigger)
                    {
                        self->_triggerSearch();
                    }
                }
            }
        };
    }

    winrt::Windows::Foundation::IAsyncAction StreamingSuggestionsControl::_finishStreamingLoad(uint64_t sessionVersion)
    {
        if (_sessionVersion != sessionVersion || !_searchBoxMode)
        {
            co_return;
        }

        _allItemsLoaded = true;
        co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);

        if (_sessionVersion != sessionVersion || !_searchBoxMode)
        {
            co_return;
        }

        _isStreaming = false;
        _triggerSearch();
        _updateLoadingIndicator();
    }

    void StreamingSuggestionsControl::_showSplitOverlay(bool show)
    {
        SplitOverlay().Visibility(show ? Visibility::Visible : Visibility::Collapsed);
        if (!show)
        {
            SplitSearchBox().Text(L"");
            SplitListBox().ItemsSource(nullptr);
            _splitItems.clear();
            SplitNoItemsPlaceholder().Visibility(Visibility::Collapsed);
        }
    }

    DependencyProperty StreamingSuggestionsControl::_borderColorProperty =
        DependencyProperty::Register(
            L"BorderColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::StreamingSuggestionsControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty StreamingSuggestionsControl::_headerTextColorProperty =
        DependencyProperty::Register(
            L"HeaderTextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::StreamingSuggestionsControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty StreamingSuggestionsControl::_BackgroundColorProperty =
        DependencyProperty::Register(
            L"BackgroundColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::StreamingSuggestionsControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty StreamingSuggestionsControl::_SelectedItemColorProperty =
        DependencyProperty::Register(
            L"SelectedItemColor",
            xaml_typename<Windows::UI::Color>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::StreamingSuggestionsControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty StreamingSuggestionsControl::_InnerBorderThicknessProperty =
        DependencyProperty::Register(
            L"BorderThickness",
            xaml_typename<Thickness>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::StreamingSuggestionsControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty StreamingSuggestionsControl::_TextColorProperty =
        DependencyProperty::Register(
            L"TextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::StreamingSuggestionsControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty StreamingSuggestionsControl::_HighlightedTextColorProperty =
        DependencyProperty::Register(
            L"HighlightedTextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::StreamingSuggestionsControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty StreamingSuggestionsControl::BackgroundColorProperty()
    {
        return _BackgroundColorProperty;
    }

    Brush StreamingSuggestionsControl::BackgroundColor()
    {
        return GetValue(_BackgroundColorProperty).as<Brush>();
    }

    void StreamingSuggestionsControl::BackgroundColor(Brush const& value)
    {
        if (value != BackgroundColor())
        {
            SetValue(_BackgroundColorProperty, value);
            _propertyChangedEvent(*this, PropertyChangedEventArgs{ L"BackgroundColor" });
        }
    }

    DependencyProperty StreamingSuggestionsControl::SelectedItemColorProperty()
    {
        return _SelectedItemColorProperty;
    }

    Windows::UI::Color StreamingSuggestionsControl::SelectedItemColor()
    {
        return GetValue(_SelectedItemColorProperty).as<Windows::UI::Color>();
    }

    void StreamingSuggestionsControl::SelectedItemColor(Windows::UI::Color const& value)
    {
        if (value != SelectedItemColor())
        {
            SetValue(_SelectedItemColorProperty, box_value(value));
            _propertyChangedEvent(*this, PropertyChangedEventArgs{ L"SelectedItemColor" });
        }
    }

    DependencyProperty StreamingSuggestionsControl::BorderColorProperty()
    {
        return _borderColorProperty;
    }

    Brush StreamingSuggestionsControl::BorderColor()
    {
        return GetValue(_borderColorProperty).as<Brush>();
    }

    void StreamingSuggestionsControl::BorderColor(Brush const& value)
    {
        if (value != BorderColor())
        {
            SetValue(_borderColorProperty, value);
            _propertyChangedEvent(*this, PropertyChangedEventArgs{ L"BorderColor" });
        }
    }

    DependencyProperty StreamingSuggestionsControl::HeaderTextColorProperty()
    {
        return _headerTextColorProperty;
    }

    Brush StreamingSuggestionsControl::HeaderTextColor()
    {
        return GetValue(_headerTextColorProperty).as<Brush>();
    }

    void StreamingSuggestionsControl::HeaderTextColor(Brush const& value)
    {
        if (value != HeaderTextColor())
        {
            SetValue(_headerTextColorProperty, value);
            _propertyChangedEvent(*this, PropertyChangedEventArgs{ L"HeaderTextColor" });
        }
    }

    DependencyProperty StreamingSuggestionsControl::InnerBorderThicknessProperty()
    {
        return _InnerBorderThicknessProperty;
    }

    Thickness StreamingSuggestionsControl::InnerBorderThickness()
    {
        return GetValue(_InnerBorderThicknessProperty).as<Thickness>();
    }

    void StreamingSuggestionsControl::InnerBorderThickness(Thickness const& value)
    {
        if (value != InnerBorderThickness())
        {
            SetValue(_InnerBorderThicknessProperty, box_value(value));
            _propertyChangedEvent(*this, PropertyChangedEventArgs{ L"InnerBorderThickness" });
        }
    }

    Brush StreamingSuggestionsControl::TextColor()
    {
        return GetValue(_TextColorProperty).as<Brush>();
    }

    void StreamingSuggestionsControl::TextColor(Brush const& value)
    {
        if (value != TextColor())
        {
            SetValue(_TextColorProperty, value);
            _propertyChangedEvent(*this, PropertyChangedEventArgs{ L"TextColor" });
            _applySearchBoxForeground();
        }
    }

    // The default TextBox template swaps Foreground to TextControlForegroundFocused /
    // TextControlForegroundPointerOver via visual states, which otherwise override the
    // Foreground binding we set in XAML. Mirror TextColor into those resource slots so the
    // user-typed text always matches the list item text color.
    void StreamingSuggestionsControl::_applySearchBoxForeground()
    {
        const auto brush = TextColor();
        if (!brush)
        {
            return;
        }

        const auto apply = [&](const Windows::UI::Xaml::Controls::TextBox& tb) {
            if (!tb)
            {
                return;
            }
            auto resources = tb.Resources();
            resources.Insert(winrt::box_value(L"TextControlForeground"), brush);
            resources.Insert(winrt::box_value(L"TextControlForegroundPointerOver"), brush);
            resources.Insert(winrt::box_value(L"TextControlForegroundFocused"), brush);
            resources.Insert(winrt::box_value(L"TextControlForegroundDisabled"), brush);
        };

        apply(SearchBox());
        apply(SplitSearchBox());
    }

    DependencyProperty StreamingSuggestionsControl::TextColorProperty()
    {
        return _TextColorProperty;
    }

    Brush StreamingSuggestionsControl::HighlightedTextColor()
    {
        return GetValue(_HighlightedTextColorProperty).as<Brush>();
    }

    void StreamingSuggestionsControl::HighlightedTextColor(Brush const& value)
    {
        if (value != HighlightedTextColor())
        {
            SetValue(_HighlightedTextColorProperty, value);
            _propertyChangedEvent(*this, PropertyChangedEventArgs{ L"HighlightedTextColor" });
        }
    }

    DependencyProperty StreamingSuggestionsControl::HighlightedTextColorProperty()
    {
        return _HighlightedTextColorProperty;
    }

    void StreamingSuggestionsControl::_selectFirstItem()
    {
        auto listBox = _activeListBox();
        if (listBox.Items().Size() > 0)
        {
            listBox.SelectedIndex(0);
        }
    }

    void StreamingSuggestionsControl::_close(bool scrollToCursor)
    {
        ++_sessionVersion;
        _searchBoxMode = false;
        _showSplitOverlay(false);
        ++_searchVersion;
        SearchBox().Text(L"");
        ListBox().ItemsSource(nullptr);
        Visibility(Windows::UI::Xaml::Visibility::Collapsed);
        _isStreaming = false;
        _lastCompletedSearchVersion = _searchVersion;
        {
            std::lock_guard<std::mutex> lock(_batchesMutex);
            _batches.clear();
            _searchInFlight = false;
            _rerunNeededDuringSearch = false;
        }
        _updateLoadingIndicator();
        if (_termControl)
        {
            if (_commandSearchHelper)
            {
                _commandSearchHelper->Stop();
            }
            if (_fileWalkerHelper)
            {
                _fileWalkerHelper->Stop();
            }
            _termControl.SetStreamingSuggestionsSwapChainOffset(0.0f);
            _termControl.ClearHighlights(scrollToCursor);
            _termControl.Focus(Windows::UI::Xaml::FocusState::Programmatic);
        }
    }

    void StreamingSuggestionsControl::Open(
        TermControl const& termControl,
        const winrt::hstring& needle,
        Windows::Foundation::Point anchor,
        Windows::Foundation::Size space,
        const winrt::hstring& currentWord,
        float prefixWidth,
        float characterHeight,
        float swapChainOffset)
    {
        const auto sessionVersion = _beginOpen(termControl, _OpenState{
            .dataSource = StreamingSuggestionsDataSource::Scrollback,
            .commandTemplate = {},
            .anchor = anchor,
            .space = space,
            .filterText = currentWord,
            .currentWord = currentWord,
            .currentCommandline = {},
            .prefixWidth = prefixWidth,
            .characterHeight = characterHeight,
            .swapChainOffset = swapChainOffset,
        });

        auto op = termControl.SuggestionScrollBackSearchAsync(
            needle,
            _makeBatchHandler(sessionVersion));

        op.Completed([weakThis = get_weak(), sessionVersion](auto const&, auto const&) -> winrt::fire_and_forget {
            if (auto self = weakThis.get())
            {
                co_await self->_finishStreamingLoad(sessionVersion);
            }
        });
    }

    void StreamingSuggestionsControl::OpenTasks(
        TermControl const& termControl,
        Windows::Foundation::Collections::IVector<Microsoft::Terminal::Control::SnippetSearchItem> snippets,
        Windows::Foundation::Point anchor,
        Windows::Foundation::Size space,
        const winrt::hstring& filterText,
        const winrt::hstring& currentWord,
        const winrt::hstring& currentCommandline,
        float prefixWidth,
        float characterHeight,
        float swapChainOffset,
        int32_t replaceTarget)
    {
        _beginOpen(termControl, _OpenState{
            .dataSource = StreamingSuggestionsDataSource::Tasks,
            .commandTemplate = {},
            .anchor = anchor,
            .space = space,
            .filterText = filterText,
            .currentWord = currentWord,
            .currentCommandline = currentCommandline,
            .prefixWidth = prefixWidth,
            .characterHeight = characterHeight,
            .swapChainOffset = swapChainOffset,
            .replaceTarget = static_cast<ReplaceTarget>(replaceTarget),
        });
        _taskItems.clear();
        if (snippets)
        {
            _taskItems.reserve(snippets.Size());
            for (const auto& snippet : snippets)
            {
                _taskItems.emplace_back(snippet);
            }
        }

        _allItemsLoaded = true;
        _isStreaming = false;
        _triggerSearch();
    }

    void StreamingSuggestionsControl::OpenCommand(
        TermControl const& termControl,
        winrt::hstring executable,
        Windows::Foundation::Collections::IVector<winrt::hstring> args,
        winrt::hstring commandTemplate,
        winrt::hstring workingDirectory,
        int32_t suggestionRow,
        Windows::Foundation::Point anchor,
        Windows::Foundation::Size space,
        const winrt::hstring& filterText,
        const winrt::hstring& currentWord,
        const winrt::hstring& currentCommandline,
        float prefixWidth,
        float characterHeight,
        float swapChainOffset,
        bool sortResults,
        bool useCommandline,
        bool prefillFilter,
        int32_t replaceTarget)
    {
        const auto sessionVersion = _beginOpen(termControl, _OpenState{
            .dataSource = StreamingSuggestionsDataSource::Command,
            .commandTemplate = commandTemplate,
            .anchor = anchor,
            .space = space,
            .filterText = filterText,
            .currentWord = currentWord,
            .currentCommandline = currentCommandline,
            .prefixWidth = prefixWidth,
            .characterHeight = characterHeight,
            .swapChainOffset = swapChainOffset,
            .sortResults = sortResults,
            .useCommandline = useCommandline,
            .prefillFilter = prefillFilter,
            .replaceTarget = static_cast<ReplaceTarget>(replaceTarget),
        });

        const auto commandArgs = args ? args : winrt::single_threaded_vector<winrt::hstring>();
        auto op = _commandSearchHelper->StartAsync(
            executable,
            commandArgs,
            workingDirectory,
            suggestionRow,
            _makeBatchHandler(sessionVersion));

        op.Completed([weakThis = get_weak(), sessionVersion](auto const&, auto const&) -> winrt::fire_and_forget {
            if (auto self = weakThis.get())
            {
                co_await self->_finishStreamingLoad(sessionVersion);
            }
        });
    }

    void StreamingSuggestionsControl::OpenFileWalker(
        TermControl const& termControl,
        Windows::Foundation::Collections::IVector<winrt::hstring> roots,
        int32_t maxDepth,
        bool includeHidden,
        bool directoriesOnly,
        bool filesOnly,
        winrt::hstring commandTemplate,
        Windows::Foundation::Point anchor,
        Windows::Foundation::Size space,
        const winrt::hstring& filterText,
        const winrt::hstring& currentWord,
        const winrt::hstring& currentCommandline,
        float prefixWidth,
        float characterHeight,
        float swapChainOffset,
        bool sortResults,
        bool useCommandline,
        bool prefillFilter,
        int32_t replaceTarget)
    {
        const auto sessionVersion = _beginOpen(termControl, _OpenState{
            .dataSource = StreamingSuggestionsDataSource::FileWalker,
            .commandTemplate = commandTemplate,
            .anchor = anchor,
            .space = space,
            .filterText = filterText,
            .currentWord = currentWord,
            .currentCommandline = currentCommandline,
            .prefixWidth = prefixWidth,
            .characterHeight = characterHeight,
            .swapChainOffset = swapChainOffset,
            .sortResults = sortResults,
            .useCommandline = useCommandline,
            .prefillFilter = prefillFilter,
            .replaceTarget = static_cast<ReplaceTarget>(replaceTarget),
        });

        const auto walkerRoots = roots ? roots : winrt::single_threaded_vector<winrt::hstring>();
        auto op = _fileWalkerHelper->StartAsync(
            walkerRoots,
            maxDepth,
            includeHidden,
            directoriesOnly,
            filesOnly,
            _makeBatchHandler(sessionVersion));

        op.Completed([weakThis = get_weak(), sessionVersion](auto const&, auto const&) -> winrt::fire_and_forget {
            if (auto self = weakThis.get())
            {
                co_await self->_finishStreamingLoad(sessionVersion);
            }
        });
    }

    bool StreamingSuggestionsControl::ContainsFocus()
    {
        const auto focusedElement = winrt::Windows::UI::Xaml::Input::FocusManager::GetFocusedElement(this->XamlRoot());
        return _focusableElements.count(focusedElement) > 0;
    }

    std::optional<SuggestionSearchItem> StreamingSuggestionsControl::_TryGetSelectedSuggestion()
    {
        auto selected = _activeListBox().SelectedItem();
        if (!selected)
        {
            return std::nullopt;
        }

        if (auto row = selected.try_as<Control::SuggestionSearchRow>())
        {
            return row.Item();
        }

        return std::nullopt;
    }

    static void _copyToClipboard(const UINT format, const void* src, const size_t bytes)
    {
        wil::unique_hglobal handle{ THROW_LAST_ERROR_IF_NULL(GlobalAlloc(GMEM_MOVEABLE, bytes)) };

        const auto locked = GlobalLock(handle.get());
        memcpy(locked, src, bytes);
        GlobalUnlock(handle.get());

        THROW_LAST_ERROR_IF_NULL(SetClipboardData(format, handle.get()));
        handle.release();
    }

    static wil::unique_close_clipboard_call _openClipboard(HWND hwnd)
    {
        bool success = false;

        // OpenClipboard may fail to acquire the internal lock --> retry.
        for (DWORD sleep = 10;; sleep *= 2)
        {
            if (OpenClipboard(hwnd))
            {
                success = true;
                break;
            }
            // 10 iterations
            if (sleep > 10000)
            {
                break;
            }
            Sleep(sleep);
        }

        return wil::unique_close_clipboard_call{ success };
    }

    static void copyToClipboard(wil::zwstring_view text)
    {
        const auto clipboard = _openClipboard(nullptr);
        if (!clipboard)
        {
            LOG_LAST_ERROR();
            return;
        }

        EmptyClipboard();

        if (!text.empty())
        {
            // As per: https://learn.microsoft.com/en-us/windows/win32/dataxchg/standard-clipboard-formats
            //   CF_UNICODETEXT: [...] A null character signals the end of the data.
            // --> We add +1 to the length. This works because .c_str() is null-terminated.
            _copyToClipboard(CF_UNICODETEXT, text.c_str(), (text.size() + 1) * sizeof(wchar_t));

        }
    }

    void StreamingSuggestionsControl::_OnCopyNotificationTimerTick(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Windows::Foundation::IInspectable const&)
    {
        _copyNotificationTimer.Stop();
        copyNotificationContainer().Visibility(winrt::Windows::UI::Xaml::Visibility::Collapsed);
    }

    void StreamingSuggestionsControl::_showCopyNotification(const hstring& text)
    {
        copyNotificationContainer().Visibility(Visibility::Visible);
        copyNotificationText().Text(L"Copied: " + text);

        _copyNotificationTimer.Stop();
        _copyNotificationTimer.Start();
    }

    void StreamingSuggestionsControl::_updateLoadingIndicator()
    {
        const bool searching = _searchVersion != _lastCompletedSearchVersion;
        const bool active = _isStreaming || searching;
        LoadingSpinner().IsActive(active);
        LoadingSpinner().Visibility(active ? Visibility::Visible : Visibility::Collapsed);

        if (_searchBoxMode && (_lastTotalCount > 0 || active))
        {
            CounterText().Text(fmt::format(FMT_COMPILE(L"{}/{}"), _lastMatchedCount, _lastTotalCount));
            CounterText().Visibility(Visibility::Visible);
        }
        else
        {
            CounterText().Visibility(Visibility::Collapsed);
        }
    }

    bool StreamingSuggestionsControl::HandleKeyPress(WORD vkey, WORD /*scanCode*/, Core::ControlKeyStates modifiers, bool keyDown)
    {
        const auto itemCount = _activeListBox().Items().Size();
        const auto mods = modifiers.Value;

        // Allow the help toggle even with no results. It's a UX aid, not a
        // list-level action.
        const bool isHelpKey = (vkey == VK_OEM_2) &&
                               WI_AreAllFlagsSet(mods, LEFT_CTRL_PRESSED | SHIFT_PRESSED);
        if (itemCount == 0 && !isHelpKey && !_helpVisible)
        {
            return false;
        }

        for (const auto& b : _keyBindings)
        {
            if (b.vkey == vkey && (mods & b.requiredMods) == b.requiredMods)
            {
                return b.action(keyDown);
            }
        }
        return false;
    }

    bool StreamingSuggestionsControl::_applySelectedOrClose(bool keyDown)
    {
        if (!keyDown)
        {
            return true;
        }

        if (_dataSource == StreamingSuggestionsDataSource::Tasks)
        {
            if (auto selected = _TryGetSelectedSuggestion())
            {
                const auto ordinal = selected->Ordinal;
                if (ordinal >= 0 && static_cast<size_t>(ordinal) < _taskItems.size())
                {
                    if (auto handler = _taskItems[ordinal].OnInvoked())
                    {
                        _close(true);
                        handler();
                        return true;
                    }
                }
            }
        }

        hstring combined = L"";
        if (auto castedDc = _TryGetSelectedSuggestion())
        {
            combined = castedDc->Text;
        }

        std::wstring_view trimmed{ combined };
        while (!trimmed.empty() && trimmed.back() == L' ')
        {
            trimmed.remove_suffix(1);
        }

        winrt::hstring text;
        const auto replacementLength = _replacementLength();
        const auto backspaces = std::wstring(replacementLength, L'\x7f');
        if (!_commandTemplate.empty())
        {
            std::wstring formatted{ _commandTemplate };
            constexpr std::wstring_view token{ L"%s" };
            if (const auto pos = formatted.find(token); pos != std::wstring::npos)
            {
                formatted.replace(pos, token.length(), trimmed);
            }
            if (backspaces.empty())
            {
                text = winrt::hstring{ formatted };
            }
            else
            {
                text = winrt::hstring{ fmt::format(FMT_COMPILE(L"{}{}"), backspaces, formatted) };
            }
        }
        else
        {
            if (backspaces.empty())
            {
                text = winrt::hstring{ trimmed };
            }
            else
            {
                text = winrt::hstring{ fmt::format(FMT_COMPILE(L"{}{}"), backspaces, trimmed) };
            }
        }
        _termControl.SendInput(text);
        _close(true);
        return true;
    }

    size_t StreamingSuggestionsControl::_replacementLength() const
    {
        switch (_replaceTarget)
        {
        case ReplaceTarget::Word:
            return _currentWord.size();
        case ReplaceTarget::Line:
            return _currentCommandline.size();
        case ReplaceTarget::Default:
        default:
            if (!_commandTemplate.empty())
            {
                return _useCommandline ? _currentCommandline.size() : 0;
            }

            if (_dataSource == StreamingSuggestionsDataSource::Tasks)
            {
                return 0;
            }

            return _currentWord.size();
        }
    }

    void StreamingSuggestionsControl::_toggleHelp()
    {
        _helpVisible = !_helpVisible;
        if (!_helpVisible)
        {
            helpOverlay().Visibility(Visibility::Collapsed);
            return;
        }

        helpEntriesPanel().Children().Clear();
        for (const auto& b : _keyBindings)
        {
            Controls::StackPanel row;
            row.Orientation(Controls::Orientation::Horizontal);

            Controls::TextBlock keyText;
            keyText.Text(hstring{ b.label });
            keyText.Width(140);
            keyText.Foreground(TextColor());
            keyText.FontFamily(Media::FontFamily{ L"Consolas" });

            Controls::TextBlock descText;
            descText.Text(hstring{ b.description });
            descText.Foreground(TextColor());

            row.Children().Append(keyText);
            row.Children().Append(descText);
            helpEntriesPanel().Children().Append(row);
        }
        helpOverlay().Visibility(Visibility::Visible);
    }

    void StreamingSuggestionsControl::_initKeyBindings()
    {
        using KB = _KeyBinding;

        // Entries are checked in order; first matching vkey + required-mod mask
        // wins. Put more-specific modifier combinations before less-specific.
        _keyBindings = {
            KB{
                LEFT_CTRL_PRESSED | SHIFT_PRESSED,
                VK_OEM_2,
                L"Ctrl+Shift+?",
                L"Show/hide this help",
                [this](bool keyDown) {
                    if (keyDown)
                    {
                        _toggleHelp();
                    }
                    return true;
                },
            },
            KB{
                LEFT_CTRL_PRESSED,
                'C',
                L"Ctrl+C",
                L"Copy selected item",
                [this](bool /*keyDown*/) {
                    if (auto castedDc = _TryGetSelectedSuggestion())
                    {
                        copyToClipboard(castedDc->Text.c_str());
                        _showCopyNotification(castedDc->Text);
                        return true;
                    }
                    return false;
                },
            },
            KB{
                LEFT_CTRL_PRESSED,
                'L',
                L"Ctrl+L",
                L"Expand to scrollback line matches",
                [this](bool keyDown) {
                    if (_dataSource != StreamingSuggestionsDataSource::Scrollback)
                    {
                        return false;
                    }
                    if (!keyDown)
                    {
                        return false;
                    }
                    {
                        std::lock_guard<std::mutex> lock(_batchesMutex);
                        _batches.clear();
                    }
                    if (auto castedDc = _TryGetSelectedSuggestion())
                    {
                        std::wstring needle = L"^.*";
                        needle += SearchBox().Text();
                        needle += L".*$";
                        auto op = _termControl.SuggestionScrollBackSearchAsync(
                            needle,
                            Microsoft::Terminal::Control::SuggestionBatchHandler{
                                [weakThis = get_weak()](Microsoft::Terminal::Control::SuggestionBatch const& batch) {
                                    if (auto self = weakThis.get())
                                    {
                                        std::lock_guard<std::mutex> lock(self->_batchesMutex);
                                        self->_batches.push_back(batch);
                                        self->_triggerSearch();
                                    }
                                } });

                        op.Completed([weakThis = get_weak()](auto const&, auto const&) -> winrt::fire_and_forget {
                            if (auto self = weakThis.get())
                            {
                                self->_allItemsLoaded = true;
                                co_await winrt::resume_foreground(self->Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);
                            }
                        });
                        return true;
                    }
                    return false;
                },
            },
            KB{
                LEFT_CTRL_PRESSED,
                'B',
                L"Ctrl+B",
                L"Word-split mode",
                [this](bool keyDown) {
                    if (_dataSource != StreamingSuggestionsDataSource::Scrollback)
                    {
                        return false;
                    }
                    if (!keyDown)
                    {
                        return false;
                    }
                    auto castedDc = _TryGetSelectedSuggestion();
                    if (castedDc)
                    {
                        _mode = StreamingSuggestionsMode::WordSplit;
                        const auto lineNumber = castedDc->StartPos.Y;
                        auto results = _termControl.LineSearchAsync(lineNumber);
                        _allItemsLoaded = true;
                        _allItemsSearched = true;
                        _showSplitOverlay(true);

                        _splitItems.clear();
                        _splitItems.reserve(results.Size());
                        for (const auto& item : results)
                        {
                            _splitItems.emplace_back(item);
                        }
                        SplitSearchBox().Text(L"");
                        SplitSearchBox().Focus(Windows::UI::Xaml::FocusState::Programmatic);
                        SplitSearchBox().SelectionStart(0);
                        _populateSplitList(L"");

                        Visibility(Visibility::Visible);
                        _recalculateTopMargin();

                        InvalidateMeasure();
                        return true;
                    }
                    return false;
                },
            },
            KB{
                SHIFT_PRESSED,
                VK_RETURN,
                L"Shift+Enter",
                L"Select the row in the terminal",
                [this](bool keyDown) {
                    if (!keyDown)
                    {
                        return true;
                    }

                    if (_dataSource != StreamingSuggestionsDataSource::Scrollback)
                    {
                        return _applySelectedOrClose(keyDown);
                    }
                    if (auto castedDc = _TryGetSelectedSuggestion())
                    {
                        auto backspaces = std::wstring(_currentWord.size(), L'\x7f');
                        _termControl.SendInput(backspaces);
                        _termControl.SelectRow(castedDc->StartPos.Y, castedDc->StartPos.X);
                        _close(false);
                        return true;
                    }
                    return _applySelectedOrClose(keyDown);
                },
            },
            KB{
                0,
                VK_RETURN,
                L"Enter",
                L"Insert selected item",
                [this](bool keyDown) { return _applySelectedOrClose(keyDown); },
            },
            KB{
                0,
                VK_PRIOR,
                L"PgUp",
                L"Scroll up one page",
                [this](bool keyDown) {
                    if (keyDown)
                    {
                        if (_helpVisible)
                        {
                            const auto sv = helpScrollViewer();
                            sv.ScrollToVerticalOffset(sv.VerticalOffset() - sv.ViewportHeight());
                        }
                        else
                        {
                            auto listBox = _activeListBox();
                            const auto pageSize = std::max(1, static_cast<int32_t>(listBox.ActualHeight() / StreamingSuggestionItemHeight));
                            const auto currentIndex = listBox.SelectedIndex();
                            _selectItem(std::max(0, currentIndex - pageSize));
                        }
                    }
                    return true;
                },
            },
            KB{
                0,
                VK_NEXT,
                L"PgDn",
                L"Scroll down one page",
                [this](bool keyDown) {
                    if (keyDown)
                    {
                        if (_helpVisible)
                        {
                            const auto sv = helpScrollViewer();
                            sv.ScrollToVerticalOffset(sv.VerticalOffset() + sv.ViewportHeight());
                        }
                        else
                        {
                            auto listBox = _activeListBox();
                            const auto pageSize = std::max(1, static_cast<int32_t>(listBox.ActualHeight() / StreamingSuggestionItemHeight));
                            const auto size = static_cast<int32_t>(listBox.Items().Size());
                            const auto currentIndex = listBox.SelectedIndex();
                            _selectItem(std::min(size - 1, currentIndex + pageSize));
                        }
                    }
                    return true;
                },
            },
            KB{
                0,
                VK_UP,
                L"Up",
                L"Previous item",
                [this](bool keyDown) {
                    if (keyDown)
                    {
                        const auto currentIndex = _activeListBox().SelectedIndex();
                        if (currentIndex > 0)
                        {
                            _selectItem(currentIndex - 1);
                        }
                    }
                    return true;
                },
            },
            KB{
                0,
                VK_DOWN,
                L"Down",
                L"Next item",
                [this](bool keyDown) {
                    if (keyDown)
                    {
                        auto listBox = _activeListBox();
                        const auto size = static_cast<int32_t>(listBox.Items().Size());
                        const auto currentIndex = listBox.SelectedIndex();
                        if (currentIndex < size - 1)
                        {
                            _selectItem(currentIndex + 1);
                        }
                    }
                    return true;
                },
            },
            KB{
                0,
                VK_ESCAPE,
                L"Esc",
                L"Close (or exit word-split mode)",
                [this](bool keyDown) {
                    if (!keyDown)
                    {
                        return true;
                    }
                    if (_helpVisible)
                    {
                        _toggleHelp();
                        return true;
                    }
                    if (_mode == StreamingSuggestionsMode::WordSplit)
                    {
                        _mode = StreamingSuggestionsMode::Normal;
                        _showSplitOverlay(false);
                        _triggerSearch();
                    }
                    else
                    {
                        _close(true);
                    }
                    return true;
                },
            },
        };
    }

    void StreamingSuggestionsControl::_triggerSearch()
    {
        if (!_searchBoxMode)
        {
            return;
        }

        std::wstring term;
        {
            std::lock_guard lock(_searchTermMutex);
            term = _currentSearchTerm;
        }

        const std::uint64_t myVersion = ++_searchVersion;
        const auto sessionVersion = _sessionVersion;

        {
            std::lock_guard<std::mutex> lock(_batchesMutex);
            _searchInFlight = true;
            _rerunNeededDuringSearch = false;
        }

        if (Dispatcher().HasThreadAccess())
        {
            _updateLoadingIndicator();
        }
        else
        {
            Dispatcher().RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, [weakThis = get_weak()]() {
                if (auto self = weakThis.get())
                {
                    self->_updateLoadingIndicator();
                }
            });
        }

        _performFuzzySearch(term, myVersion, sessionVersion);
    }

    void StreamingSuggestionsControl::_swapItemsPreservingSelection(std::vector<SuggestionRowSource>&& sources,
                                                                    std::shared_ptr<fzfcpp::matcher::Pattern> pattern)
    {
        const auto previousIndex = ListBox().SelectedIndex();
        const auto newSize = static_cast<int32_t>(sources.size());

        ListBox().ItemsSource(winrt::make<LazySuggestionRowVector>(std::move(sources), TextColor(), HighlightedTextColor(), std::move(pattern)));
        if (newSize == 0)
        {
            ListBox().SelectedIndex(-1);
            return;
        }

        if (previousIndex >= 0 && previousIndex < newSize)
        {
            _selectItem(previousIndex);
        }
        else
        {
            _selectItem(0);
        }
    }

    void StreamingSuggestionsControl::_selectItem(int32_t index)
    {
        auto listBox = _activeListBox();
        const auto size = gsl::narrow_cast<int32_t>(listBox.Items().Size());
        if (index < 0 || index >= size)
        {
            return;
        }

        listBox.SelectedIndex(index);
        listBox.ScrollIntoView(listBox.SelectedItem());

        if (_dataSource == StreamingSuggestionsDataSource::Tasks ||
            _dataSource == StreamingSuggestionsDataSource::FileWalker)
        {
            return;
        }

        if (auto selectedItem = _TryGetSelectedSuggestion())
        {
            const auto clippedTopPixels = _swapChainOffset;
            const auto selectionScrolledToSpan = !_termControl.HighlightPointSpan(selectedItem.value().StartPos, selectedItem.value().EndPos, clippedTopPixels);

            if (selectionScrolledToSpan)
            {
                _termControl.SetStreamingSuggestionsSwapChainOffset(0.0f);
            }
            else
            {
                _termControl.SetStreamingSuggestionsSwapChainOffset(_swapChainOffset);
            }
        }
    }

    static Control::FuzzySearchTextLine BuildLine(hstring const& text,
                                                  int32_t row,
                                                  int32_t col,
                                                  std::optional<std::vector<fzfcpp::matcher::TextRun>> const& runs)
    {
        auto segments = winrt::single_threaded_observable_vector<Control::FuzzySearchTextSegment>();
        if (runs && !runs->empty())
        {
            size_t cursor = 0;
            for (const auto& r : *runs)
            {
                if (cursor < r.Start)
                {
                    const hstring nonMatch{ til::safe_slice_abs(text, cursor, static_cast<size_t>(r.Start)) };
                    segments.Append(winrt::make<implementation::FuzzySearchTextSegment>(nonMatch, false));
                }
                const hstring matchSeg{ til::safe_slice_abs(text, static_cast<size_t>(r.Start), static_cast<size_t>(r.End + 1)) };
                segments.Append(winrt::make<implementation::FuzzySearchTextSegment>(matchSeg, true));
                cursor = r.End + 1;
            }
            if (cursor < text.size())
            {
                const hstring tail{ til::safe_slice_abs(text, cursor, text.size()) };
                segments.Append(winrt::make<implementation::FuzzySearchTextSegment>(tail, false));
            }
        }
        else
        {
            segments.Append(winrt::make<implementation::FuzzySearchTextSegment>(text, false));
        }
        return winrt::make<implementation::FuzzySearchTextLine>(segments, row, col);
    }

    winrt::Windows::Foundation::IInspectable LazySuggestionRowVector::GetAt(uint32_t index)
    {
        if (index >= _sources.size())
        {
            throw winrt::hresult_out_of_bounds();
        }
        auto& cached = _cache[index];
        if (!cached)
        {
            const auto& src = _sources[index];
            const auto& displayText = src.displayText.empty() ? src.item.Text : src.displayText;

            std::optional<std::vector<fzfcpp::matcher::TextRun>> primaryRuns = src.runs;
            std::optional<std::vector<fzfcpp::matcher::TextRun>> secondaryRuns = src.secondaryRuns;

            if (_pattern)
            {
                const auto t0 = std::chrono::steady_clock::now();
                if (auto m = fzfcpp::matcher::Match(displayText, *_pattern))
                {
                    primaryRuns = std::move(m->Runs);
                }
                if (!src.secondaryText.empty())
                {
                    if (auto m = fzfcpp::matcher::Match(src.secondaryText, *_pattern))
                    {
                        secondaryRuns = std::move(m->Runs);
                    }
                }
                const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
                _lazyMatchUs.fetch_add(elapsed, std::memory_order_relaxed);
                _lazyMatchCount.fetch_add(1, std::memory_order_relaxed);
            }

            auto line = BuildLine(displayText, src.item.StartPos.Y, src.item.StartPos.X, primaryRuns);
            const auto secondaryLine = src.secondaryText.empty() ? nullptr : BuildLine(src.secondaryText, src.item.StartPos.Y, src.item.StartPos.X, secondaryRuns);
            cached = winrt::make<SuggestionSearchRow>(line, secondaryLine, src.item, _textColor, _highlightedTextColor);
        }
        return cached;
    }

    LazySuggestionRowVector::~LazySuggestionRowVector()
    {
        const auto count = _lazyMatchCount.load(std::memory_order_relaxed);
        if (count > 0)
        {
            const auto totalUs = _lazyMatchUs.load(std::memory_order_relaxed);
            OutputDebugStringW(fmt::format(
                FMT_COMPILE(L"[StreamingSuggestions] lazy match_us={} match_count={} avg_us={}\n"),
                totalUs,
                count,
                totalUs / count).c_str());
        }
    }

    winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable> LazySuggestionRowVector::GetView()
    {
        return winrt::make<LazySuggestionRowVectorView>(get_strong());
    }

    bool LazySuggestionRowVector::IndexOf(winrt::Windows::Foundation::IInspectable const& value, uint32_t& index) const noexcept
    {
        for (uint32_t i = 0; i < _cache.size(); ++i)
        {
            if (_cache[i] && _cache[i] == value)
            {
                index = i;
                return true;
            }
        }
        index = 0;
        return false;
    }

    uint32_t LazySuggestionRowVector::GetMany(uint32_t startIndex, winrt::array_view<winrt::Windows::Foundation::IInspectable> items)
    {
        const auto total = static_cast<uint32_t>(_sources.size());
        if (startIndex >= total)
        {
            return 0;
        }
        const auto available = total - startIndex;
        const auto copied = std::min<uint32_t>(available, static_cast<uint32_t>(items.size()));
        for (uint32_t i = 0; i < copied; ++i)
        {
            items[i] = GetAt(startIndex + i);
        }
        return copied;
    }

    winrt::Windows::Foundation::Collections::IIterator<winrt::Windows::Foundation::IInspectable> LazySuggestionRowVector::First()
    {
        return winrt::make<LazySuggestionRowIterator>(get_strong());
    }

    winrt::Windows::Foundation::IInspectable LazySuggestionRowIterator::Current() const
    {
        if (_index >= _owner->Size())
        {
            throw winrt::hresult_out_of_bounds();
        }
        return _owner->GetAt(_index);
    }

    bool LazySuggestionRowIterator::HasCurrent() const noexcept
    {
        return _index < _owner->Size();
    }

    bool LazySuggestionRowIterator::MoveNext() noexcept
    {
        if (_index < _owner->Size())
        {
            ++_index;
        }
        return _index < _owner->Size();
    }

    uint32_t LazySuggestionRowIterator::GetMany(winrt::array_view<winrt::Windows::Foundation::IInspectable> items)
    {
        const auto copied = _owner->GetMany(_index, items);
        _index += copied;
        return copied;
    }

    winrt::Windows::Foundation::IAsyncAction StreamingSuggestionsControl::_performFuzzySearch(std::wstring searchTerm, uint64_t version, uint64_t sessionVersion)
    {
        struct ScoredItem
        {
            Microsoft::Terminal::Control::SuggestionSearchItem item;
            int32_t score;
            int32_t ordinal;
            winrt::hstring displayText;
            winrt::hstring secondaryText;
        };

        co_await winrt::resume_background();

        const auto taskSource = _dataSource == StreamingSuggestionsDataSource::Tasks;
        std::vector<Microsoft::Terminal::Control::SuggestionSearchItem> itemsSnapshot;
        std::vector<Microsoft::Terminal::Control::SnippetSearchItem> taskItemsSnapshot;
        if (taskSource)
        {
            taskItemsSnapshot = _taskItems;
        }
        else
        {
            std::vector<Microsoft::Terminal::Control::SuggestionBatch> batchesSnapshot;
            {
                std::lock_guard<std::mutex> lock(_batchesMutex);
                batchesSnapshot.assign(_batches.begin(), _batches.end());
            }
            for (const auto& batch : batchesSnapshot)
            {
                for (const auto& item : batch.Items())
                {
                    itemsSnapshot.emplace_back(item);
                }
            }
        }

        const auto buildTaskSource = [](const Microsoft::Terminal::Control::SnippetSearchItem& snippet, int32_t ordinal, std::optional<std::vector<fzfcpp::matcher::TextRun>> runs, std::optional<std::vector<fzfcpp::matcher::TextRun>> secondaryRuns) {
            const auto description = snippet.Description();
            const auto escapedInput = snippet.EscapedInput();
            const auto input = snippet.Input();
            const auto primaryText = description.empty() ? (escapedInput.empty() ? input : escapedInput) : description;
            const auto secondaryText = description.empty() ? winrt::hstring{} : escapedInput;

            return SuggestionRowSource{
                Microsoft::Terminal::Control::SuggestionSearchItem{
                    input,
                    ordinal,
                    Core::Point{ 0, 0 },
                    Core::Point{ 0, 0 }
                },
                std::move(runs),
                primaryText,
                std::move(secondaryRuns),
                secondaryText
            };
        };

        const auto totalItems = taskSource ? taskItemsSnapshot.size() : itemsSnapshot.size();

        if (searchTerm.empty())
        {
            co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);
            if (!_searchBoxMode)
            {
                co_return;
            }

            std::vector<SuggestionRowSource> sources;
            if (taskSource)
            {
                sources.reserve(taskItemsSnapshot.size());
                int32_t ordinal = 0;
                for (const auto& snippet : taskItemsSnapshot)
                {
                    sources.push_back(buildTaskSource(snippet, ordinal++, std::nullopt, std::nullopt));
                    if (sources.size() >= 1000)
                    {
                        break;
                    }
                }
            }
            else
            {
                sources.reserve(itemsSnapshot.size());
                for (const auto& item : itemsSnapshot)
                {
                    sources.push_back({ item, std::nullopt, item.Text, std::nullopt, {} });
                    if (sources.size() >= 1000)
                    {
                        break;
                    }
                }
            }

            const auto shown = static_cast<uint32_t>(sources.size());
            _swapItemsPreservingSelection(std::move(sources));

            Visibility(Visibility::Visible);
            _recalculateTopMargin();

            NoItemsPlaceholder().Visibility(shown == 0 ? Visibility::Visible : Visibility::Collapsed);
            if (shown == 0)
            {
                _termControl.ClearHighlights(false);
            }

            InvalidateMeasure();

            _lastCompletedSearchVersion = static_cast<int>(version);
            _lastMatchedCount = totalItems;
            _lastTotalCount = totalItems;
            _updateLoadingIndicator();

            bool retrigger = false;
            {
                std::lock_guard<std::mutex> lock(_batchesMutex);
                if (version == _searchVersion)
                {
                    _searchInFlight = false;
                    if (_rerunNeededDuringSearch && _searchBoxMode)
                    {
                        _rerunNeededDuringSearch = false;
                        retrigger = true;
                    }
                }
            }
            if (retrigger)
            {
                _triggerSearch();
            }
            co_return;
        }

        using clock = std::chrono::steady_clock;
        const auto t0 = clock::now();

        auto pattern = std::make_shared<fzfcpp::matcher::Pattern>(fzfcpp::matcher::ParsePatternWithTypes(searchTerm));

        const auto tParse = clock::now();

        std::vector<ScoredItem> scoredItems;
        size_t matchWorkerCount = 1;

        if (taskSource)
        {
            int32_t ordinal = 0;
            for (const auto& snippet : taskItemsSnapshot)
            {
                if (!_searchBoxMode)
                {
                    co_return;
                }

                const auto description = snippet.Description();
                const auto escapedInput = snippet.EscapedInput();
                const auto input = snippet.Input();
                const auto primaryText = description.empty() ? (escapedInput.empty() ? input : escapedInput) : description;
                const auto secondaryText = description.empty() ? winrt::hstring{} : escapedInput;

                auto primaryScore = fzfcpp::matcher::Score(primaryText, *pattern);
                auto secondaryScore = secondaryText.empty() ? std::optional<int32_t>{} : fzfcpp::matcher::Score(secondaryText, *pattern);
                if (!primaryScore && !secondaryScore)
                {
                    ++ordinal;
                    continue;
                }

                const auto score = std::max(primaryScore.value_or(0), secondaryScore.value_or(0));
                scoredItems.push_back({
                    Microsoft::Terminal::Control::SuggestionSearchItem{
                        input,
                        ordinal,
                        Core::Point{ 0, 0 },
                        Core::Point{ 0, 0 }
                    },
                    score,
                    ordinal,
                    primaryText,
                    secondaryText
                });
                ++ordinal;
            }
        }
        else
        {
            matchWorkerCount = _streamingSuggestionsWorkerCount(itemsSnapshot.size());
            if (matchWorkerCount == 1)
            {
                for (const auto& item : itemsSnapshot)
                {
                    if (!_searchBoxMode)
                    {
                        co_return;
                    }

                    auto score = fzfcpp::matcher::Score(item.Text, *pattern);
                    if (score)
                    {
                        scoredItems.push_back({ item, *score, item.Ordinal, {}, {} });
                    }
                }
            }
            else
            {
                const auto itemsPerWorker = (itemsSnapshot.size() + matchWorkerCount - 1) / matchWorkerCount;
                std::vector<std::vector<ScoredItem>> workerResults(matchWorkerCount);
                std::vector<std::future<void>> workers;
                workers.reserve(matchWorkerCount);

                for (size_t workerIndex = 0; workerIndex < matchWorkerCount; ++workerIndex)
                {
                    const auto start = workerIndex * itemsPerWorker;
                    const auto end = std::min(start + itemsPerWorker, itemsSnapshot.size());
                    auto* localResults = &workerResults[workerIndex];

                    workers.emplace_back(std::async(std::launch::async, [&, localResults, start, end]() {
                        localResults->reserve(end - start);
                        for (size_t itemIndex = start; itemIndex < end; ++itemIndex)
                        {
                            const auto& item = itemsSnapshot[itemIndex];
                            auto score = fzfcpp::matcher::Score(item.Text, *pattern);
                            if (score)
                            {
                                localResults->push_back({ item, *score, item.Ordinal, {}, {} });
                            }
                        }
                    }));
                }

                for (auto& worker : workers)
                {
                    worker.get();
                }

                if (!_searchBoxMode)
                {
                    co_return;
                }

                size_t totalMatches = 0;
                for (const auto& localResults : workerResults)
                {
                    totalMatches += localResults.size();
                }

                scoredItems.reserve(totalMatches);
                for (auto& localResults : workerResults)
                {
                    scoredItems.insert(scoredItems.end(),
                                       std::make_move_iterator(localResults.begin()),
                                       std::make_move_iterator(localResults.end()));
                }
            }
        }

        const auto tMatch = clock::now();
        const auto matchedCount = scoredItems.size();

        const auto preferShorterTextOnTie = (_dataSource == StreamingSuggestionsDataSource::Command || _dataSource == StreamingSuggestionsDataSource::FileWalker) && _sortResults;
        constexpr size_t MaxResults = 1000;
        const auto cmp = [preferShorterTextOnTie](const ScoredItem& a, const ScoredItem& b) {
            if (a.score == b.score)
            {
                if (preferShorterTextOnTie)
                {
                    if (a.item.Text.size() != b.item.Text.size())
                    {
                        return a.item.Text.size() < b.item.Text.size();
                    }
                    if (const auto c = _wcsicmp(a.item.Text.c_str(), b.item.Text.c_str()); c != 0)
                    {
                        return c < 0;
                    }
                }
                return a.ordinal < b.ordinal;
            }
            return a.score > b.score;
        };
        if (scoredItems.size() > MaxResults)
        {
            std::ranges::partial_sort(scoredItems, scoredItems.begin() + MaxResults, cmp);
            scoredItems.resize(MaxResults);
        }
        else
        {
            std::ranges::sort(scoredItems, cmp);
        }

        const auto tSort = clock::now();

        co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);
        if (!_searchBoxMode)
        {
            co_return;
        }

        const auto tForeground = clock::now();

        std::vector<SuggestionRowSource> sources;
        sources.reserve(scoredItems.size());
        for (auto& scoredItem : scoredItems)
        {
            sources.push_back({
                scoredItem.item,
                std::nullopt,
                scoredItem.displayText,
                std::nullopt,
                scoredItem.secondaryText
            });
        }
        _swapItemsPreservingSelection(std::move(sources), pattern);

        const auto tAppend = clock::now();

        const auto us = [](auto a, auto b) {
            return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
        };
        OutputDebugStringW(fmt::format(
            FMT_COMPILE(L"[StreamingSuggestions] term={} fuzzy total={}us parse={}us match={}us sort={}us fg_switch={}us append={}us workers={} items={} matched={} shown={}\n"),
            searchTerm,
            us(t0, tAppend),
            us(t0, tParse),
            us(tParse, tMatch),
            us(tMatch, tSort),
            us(tSort, tForeground),
            us(tForeground, tAppend),
            matchWorkerCount,
            totalItems,
            matchedCount,
            scoredItems.size()).c_str());

        _allItemsSearched = true;

        Visibility(Visibility::Visible);

        InvalidateMeasure();
        _recalculateTopMargin();

        NoItemsPlaceholder().Visibility(scoredItems.empty() ? Visibility::Visible : Visibility::Collapsed);
        if (scoredItems.empty())
        {
            _termControl.ClearHighlights(false);
        }

        _lastCompletedSearchVersion = static_cast<int>(version);
        _lastMatchedCount = matchedCount;
        _lastTotalCount = totalItems;
        _updateLoadingIndicator();

        bool retrigger = false;
        {
            std::lock_guard<std::mutex> lock(_batchesMutex);
            if (version == _searchVersion)
            {
                _searchInFlight = false;
                if (_rerunNeededDuringSearch && _searchBoxMode)
                {
                    _rerunNeededDuringSearch = false;
                    retrigger = true;
                }
            }
        }
        if (retrigger)
        {
            _triggerSearch();
        }
        co_return;
    }

    void StreamingSuggestionsControl::_SearchBoxTextChanged(
        Windows::Foundation::IInspectable const& /*sender*/,
        Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (!_searchBoxMode || _suppressSearchBoxChange)
        {
            return;
        }

        auto text = SearchBox().Text();
        {
            std::lock_guard lock(_searchTermMutex);
            _currentSearchTerm = text;
        }
        if (_mode == StreamingSuggestionsMode::WordSplit)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(_batchesMutex);
            if (_searchInFlight)
            {
                _rerunNeededDuringSearch = true;
                return;
            }
        }
        _triggerSearch();
    }

    void StreamingSuggestionsControl::_SplitSearchBoxTextChanged(
        Windows::Foundation::IInspectable const& /*sender*/,
        Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (_mode != StreamingSuggestionsMode::WordSplit)
        {
            return;
        }

        _populateSplitList(SplitSearchBox().Text().c_str());
    }

    void StreamingSuggestionsControl::_SearchBoxKeyDown(
        Windows::Foundation::IInspectable const& /*sender*/,
        Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        const auto key = gsl::narrow_cast<WORD>(e.OriginalKey());
        auto listBox = _activeListBox();
        const auto itemCount = listBox.Items().Size();
        auto mods = DWORD{ 0 };

        const auto window = CoreWindow::GetForCurrentThread();
        const auto ctrlState = window.GetKeyState(Windows::System::VirtualKey::Control);
        const auto shiftState = window.GetKeyState(Windows::System::VirtualKey::Shift);
        WI_SetFlagIf(mods, LEFT_CTRL_PRESSED, WI_IsFlagSet(ctrlState, CoreVirtualKeyStates::Down));
        WI_SetFlagIf(mods, SHIFT_PRESSED, WI_IsFlagSet(shiftState, CoreVirtualKeyStates::Down));

        const bool isHelpKey = key == VK_OEM_2 &&
                               WI_AreAllFlagsSet(mods, LEFT_CTRL_PRESSED | SHIFT_PRESSED);
        if (itemCount == 0 && key != VK_ESCAPE && !isHelpKey && !_helpVisible)
        {
            return;
        }

        for (const auto& binding : _keyBindings)
        {
            if (binding.vkey == key && (mods & binding.requiredMods) == binding.requiredMods)
            {
                e.Handled(binding.action(true));
                return;
            }
        }
    }

    void StreamingSuggestionsControl::_populateSplitList(std::wstring searchTerm)
    {
        struct ScoredItem
        {
            Microsoft::Terminal::Control::SuggestionSearchItem item;
            int32_t score;
            std::optional<std::vector<fzfcpp::matcher::TextRun>> runs;
            int32_t ordinal;
        };

        std::vector<SuggestionRowSource> sources;

        if (searchTerm.empty())
        {
            sources.reserve(_splitItems.size());
            for (const auto& item : _splitItems)
            {
                sources.push_back({ item, std::nullopt });
            }
        }
        else
        {
            auto pattern = fzfcpp::matcher::ParsePatternWithTypes(searchTerm);
            std::vector<ScoredItem> scoredItems;
            scoredItems.reserve(_splitItems.size());

            for (const auto& item : _splitItems)
            {
                auto text = item.Text;
                if (auto matchResult = fzfcpp::matcher::Match(text, pattern))
                {
                    scoredItems.push_back({ item, matchResult->Score, matchResult->Runs, item.Ordinal });
                }
            }

            std::ranges::sort(scoredItems, [](const ScoredItem& a, const ScoredItem& b) {
                if (a.score == b.score)
                {
                    return a.ordinal < b.ordinal;
                }
                return a.score > b.score;
            });

            sources.reserve(scoredItems.size());
            for (auto& scoredItem : scoredItems)
            {
                sources.push_back({ scoredItem.item, std::move(scoredItem.runs) });
            }
        }

        const auto shown = static_cast<uint32_t>(sources.size());
        SplitListBox().ItemsSource(winrt::make<LazySuggestionRowVector>(std::move(sources), TextColor(), HighlightedTextColor()));
        SplitListBox().SelectedIndex(-1);

        SplitNoItemsPlaceholder().Visibility(shown == 0 ? Visibility::Visible : Visibility::Collapsed);
        if (shown > 0)
        {
            _selectItem(0);
        }
        else
        {
            _termControl.ClearHighlights(false);
        }
    }

    void StreamingSuggestionsControl::_recalculateTopMargin()
    {
        constexpr auto promptGap = 2.0f;
        _recalculateHorizontalPlacement();

        const auto autoLength = Windows::UI::Xaml::GridLengthHelper::FromValueAndType(1.0, Windows::UI::Xaml::GridUnitType::Auto);
        const auto starLength = Windows::UI::Xaml::GridLengthHelper::FromValueAndType(1.0, Windows::UI::Xaml::GridUnitType::Star);
        InnerRow0().Height(autoLength);
        InnerRow1().Height(starLength);
        SplitInnerRow0().Height(autoLength);
        SplitInnerRow1().Height(starLength);
        Controls::Grid::SetRow(SearchBoxBorder(), 0);
        Controls::Grid::SetRow(ListBox(), 1);
        Controls::Grid::SetRow(NoItemsPlaceholder(), 1);
        Controls::Grid::SetRow(SplitSearchBoxBorder(), 0);
        Controls::Grid::SetRow(SplitListBox(), 1);
        Controls::Grid::SetRow(SplitNoItemsPlaceholder(), 1);
        SearchBoxBorder().BorderThickness(Windows::UI::Xaml::ThicknessHelper::FromLengths(0, 0, 0, 1));
        SplitSearchBoxBorder().BorderThickness(Windows::UI::Xaml::ThicknessHelper::FromLengths(0, 0, 0, 1));

        auto currentMargin = Margin();
        currentMargin.Top = (_anchor.Y + _characterHeight + promptGap);
        currentMargin.Bottom = 0;
        Margin(currentMargin);
    }

    void StreamingSuggestionsControl::_recalculateHorizontalPlacement()
    {
        const auto availableWidth = std::max(0.0f, gsl::narrow_cast<float>(_space.Width));

        auto m = Margin();
        m.Left = 0;
        m.Right = 0;
        m.Bottom = 0;
        Margin(m);

        Width(availableWidth);
    }

}
