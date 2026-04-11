// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "AiPromptControl.h"
#include "AiPromptControl.g.cpp"

using namespace winrt::Windows::UI::Xaml::Media;

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace
{
    // Sanitizes a prompt for use inside a `cmd.exe /s /c "claude ... -p "X""` invocation,
    // where X is wrapped in bare double quotes (no backslash escaping).
    // - CR is dropped; LF is replaced with literal " \n " because cmd.exe cannot carry real
    //   newlines across a single command line.
    // - Double quotes are replaced with single quotes. Embedding literal quotes inside a
    //   cmd-quoted section and having them survive the C runtime argv parser in the child
    //   is a lost cause, so we accept losing quote characters from the context.
    // - % and ! are passed through; prompts containing %VAR% may be subject to environment
    //   variable expansion before the child process sees them.
    std::wstring EscapeForCmdPromptArg(std::wstring_view s)
    {
        std::wstring out;
        out.reserve(s.size() + 16);
        for (wchar_t c : s)
        {
            switch (c)
            {
            case L'\r':
                break;
            case L'\n':
                out += L" \\n ";
                break;
            case L'"':
                out.push_back(L'\'');
                break;
            default:
                out.push_back(c);
                break;
            }
        }
        return out;
    }

    std::wstring Utf8ToWide(const std::string& in)
    {
        if (in.empty())
        {
            return {};
        }
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, in.data(), static_cast<int>(in.size()), nullptr, 0);
        if (wlen <= 0)
        {
            return {};
        }
        std::wstring w(static_cast<size_t>(wlen), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, in.data(), static_cast<int>(in.size()), w.data(), wlen);
        return w;
    }

    std::string WideToUtf8(std::wstring_view in)
    {
        if (in.empty())
        {
            return {};
        }
        const int len = WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in.size()), nullptr, 0, nullptr, nullptr);
        if (len <= 0)
        {
            return {};
        }
        std::string out(static_cast<size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, in.data(), static_cast<int>(in.size()), out.data(), len, nullptr, nullptr);
        return out;
    }

    // Launches an AI CLI via cmd.exe with a system prompt and a user prompt,
    // captures stdout (with stderr merged in), and returns the exit code.
    // Returns true if the process was launched successfully.
    //
    // The command line shape differs per provider:
    //   Claude: claude --model <m> --effort <e> --append-system-prompt "<sys>" -p "<usr>"
    //   Codex:  codex exec --model <m> -c model_reasoning_effort=<e> "<sys>\n\n<usr>"
    // Codex has no dedicated system-prompt flag, so the rules get prepended to
    // the exec argument with a blank-line separator.
    bool RunAiCli(winrt::Microsoft::Terminal::Control::implementation::AiProvider provider,
                  std::wstring_view model,
                  std::wstring_view effort,
                  std::wstring_view systemPrompt,
                  std::wstring_view userPrompt,
                  std::string& outStdout,
                  DWORD& exitCode)
    {
        SECURITY_ATTRIBUTES saAttr{};
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;

        // stdout pipe: child writes, parent reads.
        HANDLE hOutRead = nullptr;
        HANDLE hOutWrite = nullptr;
        if (!CreatePipe(&hOutRead, &hOutWrite, &saAttr, 0))
        {
            return false;
        }
        // Parent's read end must not be inherited by the child.
        SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);

        // stdin pipe: parent writes the prompt, child reads it. We pipe the prompt rather
        // than putting it on the command line because cmd.exe's command-line length limit
        // (~8191 wchars) is trivially exceeded once the terminal scrollback is included.
        HANDLE hInRead = nullptr;
        HANDLE hInWrite = nullptr;
        if (!CreatePipe(&hInRead, &hInWrite, &saAttr, 0))
        {
            CloseHandle(hOutRead);
            CloseHandle(hOutWrite);
            return false;
        }
        // Parent's write end must not be inherited by the child.
        SetHandleInformation(hInWrite, HANDLE_FLAG_INHERIT, 0);

        // Build the command line. /s makes cmd strip the first and last quote and use
        // the rest as-is, so we use bare "..." groups for the quoted args — cmd parses
        // them, the .cmd shim forwards them via %*, and node.exe's C runtime yields
        // single argv elements without the surrounding quotes.
        //
        // The large user prompt (terminal context + question) is NOT on the command line;
        // it goes through the stdin pipe below. Only small fixed flags remain here.
        using winrt::Microsoft::Terminal::Control::implementation::AiProvider;
        std::wstring cmdLine = L"cmd.exe /s /c \"";
        std::string stdinPayload;
        if (provider == AiProvider::Claude)
        {
            //   claude --model <m> --effort <e> --append-system-prompt "<sys>" -p
            // With no positional prompt arg and stdin redirected, `claude -p` reads the
            // user prompt from stdin. System prompt stays on the command line because it
            // needs the dedicated --append-system-prompt flag (and is small).
            cmdLine += L"claude --model ";
            cmdLine.append(model);
            cmdLine += L" --effort ";
            cmdLine.append(effort);
            cmdLine += L" --append-system-prompt \"";
            cmdLine += EscapeForCmdPromptArg(systemPrompt);
            cmdLine += L"\" -p";
            stdinPayload = WideToUtf8(userPrompt);
        }
        else // Codex
        {
            //   codex exec --skip-git-repo-check --model <m> [-c model_reasoning_effort=<e>]
            // Codex has no --append-system-prompt flag, so sys+user are concatenated and
            // fed through stdin together. --skip-git-repo-check bypasses codex's trusted-
            // directory gate (safe for us — we only read codex's stdout).
            cmdLine += L"codex exec --skip-git-repo-check --model ";
            cmdLine.append(model);
            if (!effort.empty())
            {
                cmdLine += L" -c model_reasoning_effort=";
                cmdLine.append(effort);
            }

            std::wstring combined;
            combined.reserve(systemPrompt.size() + userPrompt.size() + 4);
            combined.append(systemPrompt);
            combined += L"\n\n";
            combined.append(userPrompt);
            stdinPayload = WideToUtf8(combined);
        }
        cmdLine += L"\"";

        STARTUPINFOW si{};
        si.cb = sizeof(STARTUPINFOW);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = hInRead;
        si.hStdOutput = hOutWrite;
        si.hStdError = hOutWrite; // merge stderr into stdout to avoid pipe deadlocks

        PROCESS_INFORMATION pi{};

        // CreateProcessW requires a mutable command-line buffer.
        std::vector<wchar_t> buf(cmdLine.begin(), cmdLine.end());
        buf.push_back(L'\0');

        const BOOL launched = CreateProcessW(
            nullptr,
            buf.data(),
            nullptr,
            nullptr,
            TRUE, // inherit handles
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi);

        // Parent no longer needs the child's end of either pipe.
        CloseHandle(hOutWrite);
        CloseHandle(hInRead);

        if (!launched)
        {
            CloseHandle(hOutRead);
            CloseHandle(hInWrite);
            return false;
        }

        // Writer thread: stream the prompt into the child's stdin. We do this off the
        // main thread because the write can block if the payload exceeds the pipe
        // buffer before the child starts reading — and we need to be simultaneously
        // draining stdout on the current thread to avoid a reciprocal deadlock.
        std::thread writer([hInWrite, payload = std::move(stdinPayload)]() {
            const char* p = payload.data();
            size_t remaining = payload.size();
            while (remaining > 0)
            {
                DWORD written = 0;
                const DWORD chunk = static_cast<DWORD>(std::min<size_t>(remaining, 65536));
                if (!WriteFile(hInWrite, p, chunk, &written, nullptr) || written == 0)
                {
                    break;
                }
                p += written;
                remaining -= written;
            }
            // Closing the write end signals EOF to the child, which is how claude/codex
            // know the prompt is complete and they can start generating.
            CloseHandle(hInWrite);
        });

        // Drain stdout until EOF (child closes its write end on exit).
        outStdout.clear();
        char readBuf[4096];
        DWORD bytesRead = 0;
        while (ReadFile(hOutRead, readBuf, sizeof(readBuf), &bytesRead, nullptr) && bytesRead > 0)
        {
            outStdout.append(readBuf, bytesRead);
        }

        writer.join();

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(hOutRead);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
}

namespace winrt::Microsoft::Terminal::Control::implementation
{
    DependencyProperty AiPromptControl::_borderColorProperty =
        DependencyProperty::Register(
            L"BorderColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::AiPromptControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty AiPromptControl::_headerTextColorProperty =
        DependencyProperty::Register(
            L"HeaderTextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::AiPromptControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty AiPromptControl::_BackgroundColorProperty =
        DependencyProperty::Register(
            L"BackgroundColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::AiPromptControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty AiPromptControl::_InnerBorderThicknessProperty =
        DependencyProperty::Register(
            L"BorderThickness",
            xaml_typename<Thickness>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::AiPromptControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty AiPromptControl::_TextColorProperty =
        DependencyProperty::Register(
            L"TextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::AiPromptControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty AiPromptControl::BackgroundColorProperty()
    {
        return _BackgroundColorProperty;
    }

    Brush AiPromptControl::BackgroundColor()
    {
        return GetValue(_BackgroundColorProperty).as<Brush>();
    }

    void AiPromptControl::BackgroundColor(Brush const& value)
    {
        if (value != BackgroundColorProperty())
        {
            SetValue(_BackgroundColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"BackgroundColor" });
        }
    }

    DependencyProperty AiPromptControl::BorderColorProperty()
    {
        return _borderColorProperty;
    }

    Brush AiPromptControl::BorderColor()
    {
        return GetValue(_borderColorProperty).as<Brush>();
    }

    void AiPromptControl::BorderColor(Brush const& value)
    {
        if (value != BorderColor())
        {
            SetValue(_borderColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"BorderColor" });
        }
    }

    DependencyProperty AiPromptControl::HeaderTextColorProperty()
    {
        return _headerTextColorProperty;
    }

    Brush AiPromptControl::HeaderTextColor()
    {
        return GetValue(_headerTextColorProperty).as<Brush>();
    }

    void AiPromptControl::HeaderTextColor(Brush const& value)
    {
        if (value != HeaderTextColor())
        {
            SetValue(_headerTextColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"HeaderTextColor" });
        }
    }

    DependencyProperty AiPromptControl::InnerBorderThicknessProperty()
    {
        return _InnerBorderThicknessProperty;
    }

    Thickness AiPromptControl::InnerBorderThickness()
    {
        return GetValue(_InnerBorderThicknessProperty).as<Thickness>();
    }

    void AiPromptControl::InnerBorderThickness(Thickness const& value)
    {
        if (value != InnerBorderThickness())
        {
            SetValue(_InnerBorderThicknessProperty, box_value(value));
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"InnerBorderThickness" });
        }
    }

    DependencyProperty AiPromptControl::TextColorProperty()
    {
        return _TextColorProperty;
    }

    Brush AiPromptControl::TextColor()
    {
        return GetValue(_TextColorProperty).as<Brush>();
    }

    void AiPromptControl::TextColor(Brush const& value)
    {
        if (value != TextColor())
        {
            SetValue(_TextColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"TextColor" });

            // A WinUI TextBox ignores its Foreground brush in the focused/pointer-over/disabled
            // visual states and instead reads the TextControlForeground* theme resources. To make
            // the editable FuzzySearchTextBox match the read-only ResultTextBox in every state,
            // override those theme resource keys on the TextBox itself.
            if (auto tb = FuzzySearchTextBox())
            {
                const auto resources = tb.Resources();
                const Windows::Foundation::IInspectable brushAsInspectable{ value };
                for (const auto key : { L"TextControlForeground",
                                        L"TextControlForegroundPointerOver",
                                        L"TextControlForegroundFocused",
                                        L"TextControlForegroundDisabled" })
                {
                    const auto boxedKey = box_value(hstring{ key });
                    if (resources.HasKey(boxedKey))
                    {
                        resources.Remove(boxedKey);
                    }
                    resources.Insert(boxedKey, brushAsInspectable);
                }
            }
        }
    }

    AiPromptControl::AiPromptControl()
    {
        InitializeComponent();
        _focusableElements.insert(FuzzySearchTextBox());
        _focusableElements.insert(ResultTextBox());
    }

    void AiPromptControl::_close()
    {
        _ClosedHandlers(*this, RoutedEventArgs{});
    }

    void AiPromptControl::_TextBoxKeyDown(const Windows::Foundation::IInspectable& /*sender*/, const Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == Windows::System::VirtualKey::Escape)
        {
            _hideSpinner();
            _close();
            e.Handled(true);
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::Enter)
        {
            const auto shiftState = Windows::UI::Core::CoreWindow::GetForCurrentThread().GetKeyState(Windows::System::VirtualKey::Shift);
            const bool shiftDown = (shiftState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;
            
            if (shiftDown)
            {
                // Shift+Enter: Send result (or extracted command) to terminal
                _sendResultToTerminal();
            }
            else
            {
                // Enter: Trigger AI query
                auto text = FuzzySearchTextBox().Text();
                if (!text.empty())
                {
                    _sendToClaude(text);
                }
            }
            e.Handled(true);
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::Tab)
        {
            const auto shiftState = Windows::UI::Core::CoreWindow::GetForCurrentThread().GetKeyState(Windows::System::VirtualKey::Shift);
            const bool shiftDown = (shiftState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;
            
            if (shiftDown)
            {
                // Shift+Tab: Cycle through modes
                _cycleMode();
            }
            else
            {
                // Tab: Cycle focus between search box and result box
                auto focusedElement = Input::FocusManager::GetFocusedElement(this->XamlRoot());
                if (focusedElement == FuzzySearchTextBox())
                {
                    Input::FocusManager::TryFocusAsync(ResultTextBox(), FocusState::Keyboard);
                }
                else
                {
                    Input::FocusManager::TryFocusAsync(FuzzySearchTextBox(), FocusState::Keyboard);
                }
            }
            e.Handled(true);
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::M)
        {
            const auto window = Windows::UI::Core::CoreWindow::GetForCurrentThread();
            const auto ctrlState = window.GetKeyState(Windows::System::VirtualKey::Control);
            const bool ctrlDown = (ctrlState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;

            if (ctrlDown)
            {
                // Ctrl+M: Cycle between models
                _cycleModel();
                e.Handled(true);
            }
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::P)
        {
            const auto ctrlState = Windows::UI::Core::CoreWindow::GetForCurrentThread().GetKeyState(Windows::System::VirtualKey::Control);
            const bool ctrlDown = (ctrlState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;

            if (ctrlDown)
            {
                // Ctrl+P: Cycle between AI providers (Claude / Codex)
                _cycleProvider();
                e.Handled(true);
            }
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::C)
        {
            const auto ctrlState = Windows::UI::Core::CoreWindow::GetForCurrentThread().GetKeyState(Windows::System::VirtualKey::Control);
            const bool ctrlDown = (ctrlState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;
            
            if (ctrlDown)
            {
                // Ctrl+C: Copy response text
                try
                {
                    auto resultText = ResultTextBox().Text();
                    if (!resultText.empty() && resultText != L"AI response will appear here...")
                    {
                        auto dataPackage = winrt::Windows::ApplicationModel::DataTransfer::DataPackage();
                        dataPackage.SetText(resultText);
                        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(dataPackage);
                    }
                }
                catch (...)
                {
                    // Clipboard operation failed, but don't crash the app
                }
                e.Handled(true);
            }
        }
    }

    void AiPromptControl::_TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        auto currentText = FuzzySearchTextBox().Text();

        if (currentText.empty())
        {
            ResultTextBox().Text(L"AI response will appear here...");
        }
    }

    void AiPromptControl::Show()
    {
        _terminalContext = L"";
        _originalCursorLineLength = 0;
        _extractedCommand.clear();
        _currentMode = AiMode::CommandSuggestions;
        FuzzySearchTextBox().Text(L"");
        ResultTextBox().Text(L"AI response will appear here...");
        _updateModeDisplay();
        _updateModelIndicator();

        if (FuzzySearchTextBox())
        {
            Input::FocusManager::TryFocusAsync(FuzzySearchTextBox(), FocusState::Keyboard);
        }
    }

    void AiPromptControl::ShowWithContext(const winrt::hstring& terminalContext, const winrt::hstring& cursorLine)
    {
        _terminalContext = terminalContext;
        _currentMode = AiMode::CommandSuggestions;
        _originalCursorLineLength = cursorLine.size();
        _extractedCommand.clear();

        if (!cursorLine.empty())
        {
            FuzzySearchTextBox().Text(cursorLine);
        }
        else
        {
            FuzzySearchTextBox().Text(L"");
        }

        ResultTextBox().Text(L"AI response will appear here...");
        _updateModeDisplay();
        _updateModelIndicator();

        if (FuzzySearchTextBox())
        {
            Input::FocusManager::TryFocusAsync(FuzzySearchTextBox(), FocusState::Keyboard);
        }

        if (!cursorLine.empty())
        {
            _sendToClaude(cursorLine);
        }
    }

    bool AiPromptControl::ContainsFocus()
    {
        auto focusedElement = Input::FocusManager::GetFocusedElement(this->XamlRoot());
        if (_focusableElements.count(focusedElement) > 0)
        {
            return true;
        }

        return false;
    }

    void AiPromptControl::_sendToClaude(const winrt::hstring& prompt)
    {
        _currentRequestId = ++_requestCounter;
        _extractedCommand.clear();
        _sendToClaudeAsync(prompt, _currentRequestId);
    }

    void AiPromptControl::_sendResultToTerminal()
    {
        std::wstring textToSend;
        if (!_extractedCommand.empty())
        {
            textToSend = _extractedCommand;
        }
        else
        {
            auto resultText = ResultTextBox().Text();
            if (resultText.empty() || resultText == L"AI response will appear here...")
            {
                _close();
                return;
            }
            textToSend = resultText.c_str();
        }

        std::wstring backspaces(_originalCursorLineLength, L'\b');
        auto finalText = backspaces + textToSend;
        _OnReturnHandlers(*this, hstring{ finalText });
        _close();
    }

    void AiPromptControl::_showSpinner()
    {
        LoadingSpinner().IsActive(true);
        LoadingSpinner().Visibility(Visibility::Visible);
    }

    void AiPromptControl::_hideSpinner()
    {
        LoadingSpinner().IsActive(false);
        LoadingSpinner().Visibility(Visibility::Collapsed);
    }

    winrt::Windows::Foundation::IAsyncAction AiPromptControl::_sendToClaudeAsync(const winrt::hstring& prompt, uint32_t requestId)
    {
        _showSpinner();

        if (requestId != _currentRequestId)
        {
            _hideSpinner();
            co_return;
        }

        // The system prompt is passed to claude via --append-system-prompt so it carries
        // real system-prompt weight (not just literal text in the user message). Keep
        // the command-mode instructions forceful to override claude's default tendency
        // to wrap answers in code fences and add explanatory prose.
        std::wstring systemPrompt;
        if (_currentMode == AiMode::CommandSuggestions)
        {
            systemPrompt =
                L"You are running inside Windows Terminal as a command suggester. "
                L"Your response MUST follow this exact format:\n"
                L"1. A brief one- or two-sentence explanation of what the command does and why.\n"
                L"2. A blank line.\n"
                L"3. The exact command to run, wrapped in <cmd> and </cmd> tags on a single line, "
                L"with nothing else on that line.\n"
                L"\n"
                L"Example:\n"
                L"This rewrites the last three commits into a single one via an interactive rebase.\n"
                L"\n"
                L"<cmd>git rebase -i HEAD~3</cmd>\n"
                L"\n"
                L"Rules:\n"
                L"- Infer the current shell (PowerShell, cmd, bash, etc.) from the provided context.\n"
                L"- The <cmd>...</cmd> block must contain exactly ONE command, executable as-is.\n"
                L"- Do not chain commands with &&, ;, |, or newlines inside the <cmd> block.\n"
                L"- Do not wrap the command in code fences or backticks.\n"
                L"- Do not add text after the </cmd> tag.";
        }
        else // Chat mode
        {
            systemPrompt =
                L"You are a helpful assistant embedded in Windows Terminal. "
                L"Provide concise, conversational answers. You may explain commands and concepts. "
                L"Do not wrap output in code fences or use syntax highlighting - the terminal "
                L"cannot render markdown. Keep responses short and informative.";
        }

        // The user prompt carries the terminal context and the user's actual question.
        std::wstring userPrompt;
        if (!_terminalContext.empty())
        {
            userPrompt += L"[Terminal context]\n";
            userPrompt += _terminalContext.c_str();
            userPrompt += L"\n\n";
        }
        userPrompt += L"[Question]\n";
        userPrompt += prompt.c_str();

        const std::wstring model = _getModelString();
        const std::wstring effort = (_currentMode == AiMode::CommandSuggestions) ? L"low" : L"medium";

        auto strongThis{ get_strong() };

        // Run the CLI on a background thread so we don't block the UI dispatcher.
        co_await winrt::resume_background();

        const AiProvider provider = _currentProvider;

        std::string stdoutBytes;
        DWORD exitCode = 0;
        const bool launched = RunAiCli(provider, model, effort, systemPrompt, userPrompt, stdoutBytes, exitCode);

        co_await winrt::resume_foreground(strongThis->Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);

        if (requestId != _currentRequestId)
        {
            _hideSpinner();
            co_return;
        }

        _hideSpinner();

        if (!launched)
        {
            const wchar_t* exe = (provider == AiProvider::Claude) ? L"claude" : L"codex";
            std::wstring err = L"Error: failed to launch '";
            err += exe;
            err += L"' CLI. Make sure it is installed and on your PATH.";
            _displayResult(hstring{ err });
            co_return;
        }

        std::wstring result = Utf8ToWide(stdoutBytes);

        // Trim trailing whitespace/newlines.
        while (!result.empty() && (result.back() == L'\n' || result.back() == L'\r' || result.back() == L' ' || result.back() == L'\t'))
        {
            result.pop_back();
        }

        if (exitCode != 0)
        {
            _extractedCommand.clear();
            const wchar_t* exe = (provider == AiProvider::Claude) ? L"claude" : L"codex";
            std::wstring err = exe;
            err += L" exited with code ";
            err += std::to_wstring(exitCode);
            if (!result.empty())
            {
                err += L":\n";
                err += result;
            }
            _displayResult(hstring{ err });
            co_return;
        }

        // If claude wrapped a command in <cmd>...</cmd>, pull it out so Shift+Enter can
        // send just the command instead of the whole explanation. Strip the tags from
        // the display text so the user sees clean output.
        _extractedCommand.clear();
        {
            constexpr std::wstring_view openTag = L"<cmd>";
            constexpr std::wstring_view closeTag = L"</cmd>";
            const auto openPos = result.find(openTag);
            if (openPos != std::wstring::npos)
            {
                const auto cmdStart = openPos + openTag.size();
                const auto closePos = result.find(closeTag, cmdStart);
                if (closePos != std::wstring::npos)
                {
                    _extractedCommand = result.substr(cmdStart, closePos - cmdStart);
                    // Trim whitespace around the extracted command.
                    while (!_extractedCommand.empty() && iswspace(_extractedCommand.back()))
                    {
                        _extractedCommand.pop_back();
                    }
                    size_t lead = 0;
                    while (lead < _extractedCommand.size() && iswspace(_extractedCommand[lead]))
                    {
                        ++lead;
                    }
                    if (lead > 0)
                    {
                        _extractedCommand.erase(0, lead);
                    }

                    // Strip the tags from the display text (erase close first so the
                    // open-tag offset stays valid).
                    result.erase(closePos, closeTag.size());
                    result.erase(openPos, openTag.size());
                }
            }
        }

        _displayResult(result.empty() ? hstring{ L"No response from AI" } : hstring{ result });
    }

    void AiPromptControl::_displayResult(const winrt::hstring& result)
    {
        ResultTextBox().Text(result);

        // Scroll to top of result text
        ResultTextBox().SelectionStart(0);
        ResultTextBox().SelectionLength(0);

        // Show copy button for chat mode
        if (_currentMode == AiMode::Chat)
        {
            CopyButton().Visibility(Visibility::Visible);
        }
        else
        {
            CopyButton().Visibility(Visibility::Collapsed);
        }

        if (FuzzySearchTextBox())
        {
            Input::FocusManager::TryFocusAsync(FuzzySearchTextBox(), FocusState::Keyboard);
        }
    }

    void AiPromptControl::_cycleMode()
    {
        if (_currentMode == AiMode::CommandSuggestions)
        {
            _currentMode = AiMode::Chat;
            _currentModel = AiModel::Sonnet; // Chat mode defaults to Sonnet
        }
        else
        {
            _currentMode = AiMode::CommandSuggestions;
            _currentModel = AiModel::Haiku; // Command mode defaults to Haiku
        }
        _updateModeDisplay();
        _updateModelIndicator();
    }

    void AiPromptControl::_updateModeDisplay()
    {
        const std::wstring_view providerName = (_currentProvider == AiProvider::Claude) ? L"Claude" : L"Codex";
        if (_currentMode == AiMode::CommandSuggestions)
        {
            std::wstring text{ providerName };
            text += L" - Command Mode";
            VimSearchHeaderTextBlock().Text(hstring{ text });
            CopyButton().Visibility(Visibility::Collapsed);
        }
        else
        {
            std::wstring text{ providerName };
            text += L" - Chat Mode";
            VimSearchHeaderTextBlock().Text(hstring{ text });
            // Copy button visibility will be set when displaying results
        }
    }

    void AiPromptControl::_ResultTextBoxKeyDown(const Windows::Foundation::IInspectable& /*sender*/, const Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == Windows::System::VirtualKey::Escape)
        {
            _hideSpinner();
            _close();
            e.Handled(true);
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::Enter)
        {
            const auto shiftState = Windows::UI::Core::CoreWindow::GetForCurrentThread().GetKeyState(Windows::System::VirtualKey::Shift);
            const bool shiftDown = (shiftState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;

            if (shiftDown)
            {
                // Shift+Enter: Send result (or extracted command) to terminal
                _sendResultToTerminal();
                e.Handled(true);
            }
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::Tab)
        {
            const auto shiftState = Windows::UI::Core::CoreWindow::GetForCurrentThread().GetKeyState(Windows::System::VirtualKey::Shift);
            const bool shiftDown = (shiftState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;
            
            if (shiftDown)
            {
                // Shift+Tab: Cycle through modes
                _cycleMode();
            }
            else
            {
                // Tab: Cycle focus back to search box
                Input::FocusManager::TryFocusAsync(FuzzySearchTextBox(), FocusState::Keyboard);
            }
            e.Handled(true);
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::Number1)
        {
            const auto window = Windows::UI::Core::CoreWindow::GetForCurrentThread();
            const auto ctrlState = window.GetKeyState(Windows::System::VirtualKey::Control);
            const bool ctrlDown = (ctrlState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;
            const auto shiftState = window.GetKeyState(Windows::System::VirtualKey::Shift);
            const bool shiftDown = (shiftState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;

            if (ctrlDown && shiftDown)
            {
                // Ctrl+Shift+1: Cycle between models
                _cycleModel();
            }
            e.Handled(true);
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::P)
        {
            const auto ctrlState = Windows::UI::Core::CoreWindow::GetForCurrentThread().GetKeyState(Windows::System::VirtualKey::Control);
            const bool ctrlDown = (ctrlState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;

            if (ctrlDown)
            {
                // Ctrl+P: Cycle between AI providers (Claude / Codex)
                _cycleProvider();
                e.Handled(true);
            }
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::PageUp)
        {
            // Page Up: Scroll up in result text
            auto currentPosition = ResultTextBox().SelectionStart();
            if (currentPosition > 0)
            {
                // Move cursor up by approximately a page worth of lines
                auto text = ResultTextBox().Text();
                auto linesPerPage = 10; // Approximate lines per page
                auto newPosition = std::max(0, static_cast<int>(currentPosition) - (80 * linesPerPage)); // 80 chars per line estimate
                ResultTextBox().SelectionStart(newPosition);
                ResultTextBox().SelectionLength(0);
            }
            e.Handled(true);
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::PageDown)
        {
            // Page Down: Scroll down in result text
            auto currentPosition = ResultTextBox().SelectionStart();
            auto text = ResultTextBox().Text();
            if (currentPosition < static_cast<int>(text.size()))
            {
                // Move cursor down by approximately a page worth of lines
                auto linesPerPage = 10; // Approximate lines per page
                auto newPosition = std::min(static_cast<int>(text.size()), static_cast<int>(currentPosition) + (80 * linesPerPage)); // 80 chars per line estimate
                ResultTextBox().SelectionStart(newPosition);
                ResultTextBox().SelectionLength(0);
            }
            e.Handled(true);
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::C)
        {
            const auto ctrlState = Windows::UI::Core::CoreWindow::GetForCurrentThread().GetKeyState(Windows::System::VirtualKey::Control);
            const bool ctrlDown = (ctrlState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;
            
            if (ctrlDown)
            {
                // Ctrl+C: Copy response text
                try
                {
                    auto resultText = ResultTextBox().Text();
                    if (!resultText.empty() && resultText != L"AI response will appear here...")
                    {
                        auto dataPackage = winrt::Windows::ApplicationModel::DataTransfer::DataPackage();
                        dataPackage.SetText(resultText);
                        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(dataPackage);
                    }
                }
                catch (...)
                {
                    // Clipboard operation failed, but don't crash the app
                }
                e.Handled(true);
            }
        }
    }

    void AiPromptControl::_CopyButtonClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        try
        {
            auto resultText = ResultTextBox().Text();
            if (!resultText.empty() && resultText != L"AI response will appear here...")
            {
                auto dataPackage = winrt::Windows::ApplicationModel::DataTransfer::DataPackage();
                dataPackage.SetText(resultText);
                winrt::Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(dataPackage);
            }
        }
        catch (...)
        {
            // Clipboard operation failed, but don't crash the app
        }
    }

    void AiPromptControl::_cycleModel()
    {
        if (_currentModel == AiModel::Haiku)
        {
            _currentModel = AiModel::Sonnet;
        }
        else
        {
            _currentModel = AiModel::Haiku;
        }
        _updateModelIndicator();
    }

    void AiPromptControl::_cycleProvider()
    {
        if (_currentProvider == AiProvider::Claude)
        {
            _currentProvider = AiProvider::Codex;
        }
        else
        {
            _currentProvider = AiProvider::Claude;
        }
        _updateModelIndicator();
        _updateModeDisplay();
    }

    std::wstring AiPromptControl::_getModelString() const
    {
        // Map the provider-agnostic Fast/Smart slot to a real model name.
        if (_currentProvider == AiProvider::Codex)
        {
            switch (_currentModel)
            {
                case AiModel::Sonnet:
                    return L"gpt-5";
                case AiModel::Haiku:
                default:
                    return L"gpt-5-codex";
            }
        }
        // Claude
        switch (_currentModel)
        {
            case AiModel::Sonnet:
                return L"sonnet";
            case AiModel::Haiku:
            default:
                return L"haiku";
        }
    }

    void AiPromptControl::_updateModelIndicator()
    {
        std::wstring text = (_currentProvider == AiProvider::Claude) ? L"Claude" : L"Codex";
        text += L" | ";
        if (_currentProvider == AiProvider::Codex)
        {
            text += (_currentModel == AiModel::Sonnet) ? L"gpt-5" : L"gpt-5-codex";
        }
        else
        {
            text += (_currentModel == AiModel::Sonnet) ? L"Sonnet" : L"Haiku";
        }
        ModelIndicator().Text(hstring{ text });
    }
}
