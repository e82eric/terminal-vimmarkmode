// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "StreamingSuggestions.h"
#include "StreamingSuggestions.g.cpp"
#include "FuzzyHighlightText.g.cpp"
#include "fzf/fzf.h"
#include <algorithm>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    FuzzyHighlightText::FuzzyHighlightText(
        winrt::hstring nameText,
        winrt::hstring descriptionText,
        Microsoft::Terminal::Core::Point start,
        Microsoft::Terminal::Core::Point end) :
        _NameText(nameText), _DescriptionText(descriptionText), _Start(start), _End(end)
    {
    }
    FuzzyHighlightText::FuzzyHighlightText(
        winrt::Windows::Foundation::Collections::IVector<winrt::TerminalApp::HighlightedRun> nameHighlights,
        winrt::Windows::Foundation::Collections::IVector<winrt::TerminalApp::HighlightedRun> descriptionHighlights,
        winrt::hstring nameText,
        winrt::hstring descriptionText,
        Microsoft::Terminal::Core::Point start,
        Microsoft::Terminal::Core::Point end) :
        _NameHighlights(nameHighlights), _DescriptionHighlights(descriptionHighlights), _NameText(nameText), _DescriptionText(descriptionText), _Start(start), _End(end)
    {
    }

    StreamingSuggestions::StreamingSuggestions()
    {
        InitializeComponent();
        _filteredActions = winrt::single_threaded_observable_vector<TerminalApp::FuzzyHighlightText>();
        
        _sizeChangedRevoker = TestListView().SizeChanged(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            if (Visibility() == Visibility::Visible)
            {
                //this->_recalculateTopMargin();
            }
        });

        TestListView().SelectionChanged({ this, &StreamingSuggestions::_selectedCommandChanged });
    }

    void StreamingSuggestions::SelectNextItem(const bool moveDown)
    {
        auto selected = TestListView().SelectedIndex();
        const auto numItems = ::base::saturated_cast<int>(TestListView().Items().Size());

        if (numItems != 0 && (selected != -1 || moveDown))
        {
            const auto newIndex = ((numItems + selected + (moveDown ? 1 : -1)) % numItems);
            TestListView().SelectedIndex(newIndex);
            TestListView().ScrollIntoView(TestListView().SelectedItem());
        }
    }
    
    void StreamingSuggestions::Open(
        Microsoft::Terminal::Control::TermControl const& termControl,
        winrt::hstring needle,
        Windows::Foundation::Point anchor,
        Windows::Foundation::Size space,
        float characterHeight,
        winrt::hstring currentWord)
    {
        _characterHeight = characterHeight;
        _termControl = termControl; // Store the TermControl
        _currentWord = currentWord;
        _currentSearchTerm = currentWord;
        _filteredActions.Clear();
        
        _anchor = anchor;
        _space = space;
        _searchVersion = 0;
        
        Visibility(Visibility::Visible);
        TestListView().ItemsSource(_filteredActions);
        FocusSearchBox();
        SearchBox().Text(_currentWord);
        SearchBox().Select(currentWord.size(), 0);
        
        _recalculateTopMargin();

        {
            std::lock_guard<std::mutex> lock(_batchesMutex);
            _batches.clear();
        }

        auto op = termControl.SuggestionScrollBackSearchAsync(
            needle,
            Microsoft::Terminal::Control::SuggestionBatchHandler{
                [weakThis = get_weak()](Microsoft::Terminal::Control::SuggestionBatch const& batch) {
                    if (auto self = weakThis.get())
                    {
                        {
                            std::lock_guard<std::mutex> lock(self->_batchesMutex);
                            self->_batches.push_back(batch);
                        }
                        self->_triggerSearch();
                    }
                } });
    }

    void StreamingSuggestions::SearchBox_TextChanged(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::TextChangedEventArgs const&)
    {
        {
            const std::wstring searchTerm{ SearchBox().Text() };
            std::lock_guard lock(_searchTermMutex);
            _currentSearchTerm = searchTerm;
        }

        _triggerSearch();
    }
    
    void StreamingSuggestions::FocusSearchBox()
    {
        SearchBox().Focus(Windows::UI::Xaml::FocusState::Keyboard);
    }

    void StreamingSuggestions::UserControl_KeyUp(const IInspectable& /*sender*/, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& /*e*/)
    {
    }
    
    void StreamingSuggestions::UserControl_KeyDown(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        if (e.Key() == Windows::System::VirtualKey::Escape)
        {
            _filteredActions.Clear();
            _termControl.PreviewInput(L"");
            SearchBox().Text(L"");
            Visibility(Windows::UI::Xaml::Visibility::Collapsed);
            e.Handled(true);
        }
        else if (e.Key() == Windows::System::VirtualKey::Up)
        {
            SelectNextItem(false);
            e.Handled(true);
        }
        else if (e.Key() == Windows::System::VirtualKey::Down)
        {
            SelectNextItem(true);
            e.Handled(true);
        }
        else if (e.Key() == Windows::System::VirtualKey::Enter ||
                 e.Key() == Windows::System::VirtualKey::Tab)
        {
            const auto shiftState = Windows::UI::Core::CoreWindow::GetForCurrentThread().GetKeyState(Windows::System::VirtualKey::Shift);
            const bool shiftDown = (shiftState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;
            _termControl.PreviewInput(L"");
            _dispatchSelectedCommand(shiftDown);
            e.Handled(true);
        }
    }
    
    void StreamingSuggestions::_triggerSearch()
    {
        std::wstring term;
        {
            std::lock_guard lock(_searchTermMutex);
            term = _currentSearchTerm;
        }
        const std::uint64_t myVersion = ++_searchVersion;

        _performFuzzySearch(term, myVersion);
    }

    bool contains_ci(std::wstring_view haystack, std::wstring_view needle)
    {
        if (needle.empty())
        {
            return true;
        }

        auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), [](wchar_t h, wchar_t n) {
            return std::tolower(h) == std::tolower(n);
        });
        return it != haystack.end();
    }

    winrt::Windows::Foundation::IAsyncAction StreamingSuggestions::_performFuzzySearch(std::wstring searchTerm, uint64_t version)
    {
        co_await winrt::resume_background();
        using namespace std::chrono_literals;
        if (_searchVersion > 1)
        {
            co_await winrt::resume_after(50ms);
            if (version != _searchVersion)
            {
                co_return;
            }
        }

        std::vector<Microsoft::Terminal::Control::SuggestionBatch> batchesSnapshot;
        {
            std::lock_guard<std::mutex> lock(_batchesMutex);
            batchesSnapshot.assign(_batches.begin(), _batches.end());
        }
        
        if (searchTerm.empty())
        {
            std::vector<TerminalApp::FuzzyHighlightText> allItems;
            {
                for (const auto& batch : batchesSnapshot)
                {
                    if (version != _searchVersion)
                    {
                        co_return;
                    }
                    for (const auto& item : batch.Items())
                    {
                        auto highlight = winrt::make<FuzzyHighlightText>(item.Text, item.Row, item.StartPos, item.EndPos);
                        allItems.push_back(highlight);
                        if (allItems.size() >= 1000)
                        {
                            break;
                        }
                    }
                    if (allItems.size() >= 1000)
                    {
                        break;
                    }
                }
            }
            
            co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);
            if (version == _searchVersion)
            {
                _filteredActions.ReplaceAll(allItems);
                if (!allItems.empty() && TestListView().SelectedIndex() == -1)
                {
                    TestListView().SelectedIndex(0);
                }
            }
            
            co_return;
        }
        
        auto pattern = fzf::matcher::ParsePatternWithTypes(searchTerm);
        
        struct ScoredItem
        {
            Microsoft::Terminal::Control::SuggestionSearchItem item;
            int32_t score;
            std::optional<std::vector<fzf::matcher::TextRun>> runs;
            std::optional<std::vector<fzf::matcher::TextRun>> descriptionRuns;
            int32_t ordinal;
        };
        
        std::vector<ScoredItem> scoredItems;
        
        for (const auto& batch : batchesSnapshot)
        {
            if (version != _searchVersion)
            {
                co_return;
            }
            for (const auto& item : batch.Items())
            {
                auto score = 0;
                auto matchResult = fzf::matcher::MatchToken(item.Text, item.Row, pattern);

                std::optional<std::vector<fzf::matcher::TextRun>> tokenRuns = std::nullopt;
                std::optional<std::vector<fzf::matcher::TextRun>> contextRuns = std::nullopt;
                if (matchResult->ExpectsMatchOnToken)
                {
                    if (matchResult->TokenResult.has_value())
                    {
                        score = score += matchResult->TokenResult.value().Score;
                        tokenRuns = matchResult->TokenResult->Runs;
                    }
                    else
                    {
                        continue;
                    }
                }
                if (matchResult->ExpectsMatchOnContext)
                {
                    if (matchResult->ContextResult.has_value())
                    {
                        score = score += matchResult->ContextResult.value().Score;
                        contextRuns = matchResult->ContextResult->Runs;
                    }
                    else
                    {
                        continue;
                    }
                }

                scoredItems.push_back({ item, score, tokenRuns, contextRuns, item.Ordinal });
            }
        }

        auto MaxResults = 1000;
        if (scoredItems.size() > MaxResults)
        {
            std::ranges::partial_sort(scoredItems, scoredItems.begin() + MaxResults, [](const ScoredItem& a, const ScoredItem& b) {
                if (a.score == b.score)
                {
                    return a.ordinal < b.ordinal;
                }
                return a.score > b.score;
            });
            scoredItems.resize(MaxResults);
        }
        else
        {
            std::ranges::sort(scoredItems.begin(), scoredItems.end(), [](const ScoredItem& a, const ScoredItem& b) {
                if (a.score == b.score)
                {
                    return a.ordinal < b.ordinal;
                }
                return a.score > b.score;
            });
        }
        
        co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);

        //if (version == _searchVersion)
        //{
            _filteredActions.Clear();
            for (const auto& scoredItem : scoredItems)
            {
                std::vector<winrt::TerminalApp::HighlightedRun> segments;
                std::vector<winrt::TerminalApp::HighlightedRun> descriptionSegments;
                if (scoredItem.runs)
                {
                    segments.resize(scoredItem.runs.value().size());
                    std::transform(scoredItem.runs.value().begin(), scoredItem.runs.value().end(), segments.begin(), [](auto&& run) -> winrt::TerminalApp::HighlightedRun {
                        return { run.Start, run.End };
                    });
                }
                if (scoredItem.descriptionRuns)
                {
                    descriptionSegments.resize(scoredItem.descriptionRuns.value().size());
                    std::transform(scoredItem.descriptionRuns.value().begin(), scoredItem.descriptionRuns.value().end(), descriptionSegments.begin(), [](auto&& run) -> winrt::TerminalApp::HighlightedRun {
                        return { run.Start, run.End };
                    });
                }
                auto highlight = winrt::make<FuzzyHighlightText>(
                    winrt::single_threaded_vector(std::move(segments)),
                    winrt::single_threaded_vector(std::move(descriptionSegments)),
                    scoredItem.item.Text,
                    scoredItem.item.Row,
                    scoredItem.item.StartPos,
                    scoredItem.item.EndPos);
                _filteredActions.Append(highlight);
            }
            if (!scoredItems.empty() && TestListView().SelectedIndex() == -1)
            {
                TestListView().SelectedIndex(0);
            }
        //}
        
        co_return;
    }
    
    void StreamingSuggestions::_dispatchSelectedCommand(bool selectRow)
    {
        const auto selectedIndex = TestListView().SelectedIndex();
        if (selectedIndex >= 0 && selectedIndex < static_cast<int32_t>(_filteredActions.Size()))
        {
            const auto selectedItem = _filteredActions.GetAt(selectedIndex);
            const auto commandText = selectedItem.NameText();
            
            if (!commandText.empty())
            {
                if (selectRow)
                {
                    _termControl.SelectRow(selectedItem.Start().Y, selectedItem.Start().X);
                }
                else
                {
                    auto cmd = Microsoft::Terminal::Settings::Model::Command::ScrollBackSuggestionToCommand(commandText, _currentWord, L"");
                    DispatchCommandRequested.raise(*this, cmd);
                }
                
                Visibility(Windows::UI::Xaml::Visibility::Collapsed);
                _filteredActions.Clear();
                SearchBox().Text(L"");
            }
        }
    }

    void StreamingSuggestions::_selectedCommandChanged(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        if (Visibility() == Visibility::Visible)
        {
            const auto selectedIndex = TestListView().SelectedIndex();
            uint32_t newSelectedIndex = 0;

            if (selectedIndex >= 0 && selectedIndex < static_cast<int32_t>(_filteredActions.Size()))
            {
                const auto selectedItem = _filteredActions.GetAt(selectedIndex);
                
                // Convert all filtered actions to SuggestionSearchItem vector and sort by coordinates
                auto highlightVector = winrt::single_threaded_vector<Microsoft::Terminal::Control::SuggestionSearchItem>();
                if (SearchBox().Text().size() < 2)
                {
                        auto suggestionItem = Microsoft::Terminal::Control::SuggestionSearchItem{};
                        suggestionItem.Text = selectedItem.NameText();
                        suggestionItem.Row = selectedItem.DescriptionText();
                        suggestionItem.StartPos = selectedItem.Start();
                        suggestionItem.EndPos = selectedItem.End();
                        highlightVector.Append({ suggestionItem });
                }
                else
                {
                    std::vector<std::pair<Microsoft::Terminal::Control::SuggestionSearchItem, uint32_t>> itemsWithIndex;
                    for (uint32_t i = 0; i < _filteredActions.Size(); ++i)
                    {
                        const auto item = _filteredActions.GetAt(i);
                        auto suggestionItem = Microsoft::Terminal::Control::SuggestionSearchItem{};
                        suggestionItem.Text = item.NameText();
                        suggestionItem.Row = item.DescriptionText();
                        suggestionItem.StartPos = item.Start();
                        suggestionItem.EndPos = item.End();
                        itemsWithIndex.push_back({ suggestionItem, i });
                    }
                    // Sort by Y coordinate first, then by X coordinate
                    std::sort(itemsWithIndex.begin(), itemsWithIndex.end(), [](const auto& a, const auto& b) {
                        if (a.first.StartPos.Y != b.first.StartPos.Y)
                        {
                            return a.first.StartPos.Y < b.first.StartPos.Y;
                        }
                        return a.first.StartPos.X < b.first.StartPos.X;
                    });

                    for (uint32_t i = 0; i < itemsWithIndex.size(); ++i)
                    {
                        highlightVector.Append(itemsWithIndex[i].first);
                        if (itemsWithIndex[i].second == static_cast<uint32_t>(selectedIndex))
                        {
                            newSelectedIndex = i;
                        }
                    }
                }
                

                auto scrollOffset = _willCoverSelectedHighlight();
                
                _termControl.SetSuggestionHighlights(highlightVector, newSelectedIndex, scrollOffset);

                auto backspaces = std::wstring(_currentWord.size(), L'\x7f');
                auto previewText = winrt::hstring{ fmt::format(FMT_COMPILE(L"{}{}"), backspaces, selectedItem.NameText()) };
                _termControl.PreviewInput(previewText);
            }
        }
    }

    void StreamingSuggestions::_recalculateTopMargin()
    {
        auto currentMargin = Margin();
        
        // Calculate vertical position - prefer below the text, fallback to top of screen
        const auto controlHeight = 250;
        const auto spaceBelow = _space.Height - _anchor.Y;
        
        double finalY;
        if (spaceBelow >= controlHeight)
        {
            finalY = _anchor.Y + _characterHeight;
        }
        else
        {
            finalY = 100;
        }
        
        currentMargin.Top = finalY;
        Margin(currentMargin);
    }

    int32_t StreamingSuggestions::_willCoverSelectedHighlight()
    {
        auto viewportTop = _termControl.GetViewportTop();
        const auto selectedIndex = TestListView().SelectedIndex();
        if (selectedIndex < 0 || selectedIndex >= static_cast<int32_t>(_filteredActions.Size()))
        {
            return false;
        }

        if (!_termControl)
        {
            return false;
        }

        const auto selectedItem = _filteredActions.GetAt(selectedIndex);
        const auto highlightStart = selectedItem.Start();
        const auto highlightEnd = selectedItem.End();
        
        const auto fontSize = _characterHeight;
        
        const auto highlightTopY = (highlightStart.Y - viewportTop) * fontSize;
        const auto highlightBottomY = ((highlightEnd.Y - viewportTop) + 1) * fontSize; // +1 for full line height
        
        const auto margin = Margin();
        const auto controlTop = margin.Top;
        const auto controlBottom = controlTop + ActualHeight();
        
        const auto verticalOverlap = (highlightTopY < controlBottom) && (highlightBottomY > controlTop);
        if (!verticalOverlap)
        {
            return 0;
        }
        int32_t lines = static_cast<int32_t>((controlBottom - highlightTopY) / fontSize + 1);
        return lines;
    }
}
