// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "AiPromptControl.h"
#include "AiPromptControl.g.cpp"

using namespace winrt::Windows::UI::Xaml::Media;

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::Web::Http;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Storage::Streams;

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

    AiPromptControl::AiPromptControl()
    {
        InitializeComponent();
        _focusableElements.insert(FuzzySearchTextBox());
        _focusableElements.insert(ResultTextBox());
        _httpClient = HttpClient{};
        _httpClient.DefaultRequestHeaders().UserAgent().TryParseAdd(L"Windows-Terminal/1.0");
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
                // Shift+Enter: Send result to terminal
                auto resultText = ResultTextBox().Text();
                if (!resultText.empty() && resultText != L"AI response will appear here...")
                {
                    std::wstring backspaces(_originalCursorLineLength, L'\b');

                    auto finalText = backspaces + resultText.c_str();

                    _OnReturnHandlers(*this, hstring{ finalText });
                    _close();
                }
                else
                {
                    _close();
                }
            }
            else
            {
                // Enter: Trigger AI query
                auto text = FuzzySearchTextBox().Text();
                if (!text.empty())
                {
                    _sendToOpenAI(text);
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
            //const auto shiftState = window.GetKeyState(Windows::System::VirtualKey::Shift);
            //const bool shiftDown = (shiftState & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;
            
            if (ctrlDown)
            {
                // Ctrl+Shift+1: Cycle between models
                _cycleModel();
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
            _sendToOpenAI(cursorLine);
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

    void AiPromptControl::_sendToOpenAI(const winrt::hstring& prompt)
    {
        _currentRequestId = ++_requestCounter;
        _sendToOpenAIAsync(prompt, _currentRequestId);
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

    winrt::Windows::Foundation::IAsyncAction AiPromptControl::_sendToOpenAIAsync(const winrt::hstring& prompt, uint32_t requestId)
    {
        _showSpinner();
        try
        {
            if (requestId != _currentRequestId)
            {
                _hideSpinner();
                co_return;
            }

            const auto apiKey = _getOpenAIApiKey();
            if (apiKey.empty())
            {
                if (requestId != _currentRequestId)
                {
                    _hideSpinner();
                    co_return;
                }
                _hideSpinner();
                _displayResult(L"Error: OpenAI API key not found. Please store your API key in Windows Credential Manager with resource name 'WindowsTerminal_OpenAI_API_Key' and username 'OpenAI', or set the OPENAI_API_KEY environment variable.");
                co_return;
            }

            JsonObject requestBody;
            requestBody.SetNamedValue(L"model", JsonValue::CreateStringValue(hstring{ _getModelString() }));

            std::wstring input;
            
            if (_currentMode == AiMode::CommandSuggestions)
            {
                input = L"[SYSTEM]\n"
                        L"Return EXACTLY ONE command on a single line.\n"
                        L"- You are inside of windows terminal, use the context to understand the current shell \n"
                        L"- Always return a command that can be executed in a terminal"
                        L"- No explanations, comments, or prose.\n"
                        L"- Do not include multiple commands joined by &&, ;, |, or newline.\n"
                        L"- Do not wrap in code fences.\n"
                        L"[USER]\n";
            }
            else // Chat mode
            {
                input = L"[SYSTEM]\n"
                        L"You are a helpful assistant within Windows Terminal.\n"
                        L"- Provide helpful, conversational responses\n"
                        L"- You can explain commands, concepts, and provide guidance\n"
                        L"- Use the terminal context to understand the user's environment\n"
                        L"- Don't Format code with syntax highlighting\n"
                        L"- Be concise but informative\n"
                        L"[USER]\n";
            }

            if (!_terminalContext.empty())
            {
                input += L"[CONTEXT]\n";
                input += _terminalContext.c_str();
                input += L"\n";
            }
            input += L"[USER]\n";
            input += prompt.c_str();

            requestBody.SetNamedValue(L"input", JsonValue::CreateStringValue(hstring{ input }));

            auto uri = winrt::Windows::Foundation::Uri{ L"https://api.openai.com/v1/responses" };
            HttpRequestMessage request{ HttpMethod::Post(), uri };
            request.Headers().Insert(L"Authorization", std::wstring(L"Bearer ") + apiKey);
            request.Headers().Accept().TryParseAdd(L"application/json");

            auto jsonString = requestBody.Stringify();
            auto content = HttpStringContent(jsonString, UnicodeEncoding::Utf8, L"application/json");
            request.Content(content);

            if (requestId != _currentRequestId)
            {
                _hideSpinner();
                co_return;
            }

            auto response = co_await _httpClient.SendRequestAsync(request);
            auto responseContent = co_await response.Content().ReadAsStringAsync();

            if (response.IsSuccessStatusCode())
            {
                auto responseJson = JsonObject::Parse(responseContent);
                hstring out;

                if (responseJson.HasKey(L"output"))
                {
                    const auto output = responseJson.GetNamedArray(L"output");
                    for (uint32_t oi = 0; oi < output.Size(); ++oi)
                    {
                        const auto item = output.GetObjectAt(oi);
                        if (!item.HasKey(L"content"))
                        {
                            continue;
                        }

                        const auto contentArr = item.GetNamedArray(L"content");
                        for (uint32_t ci = 0; ci < contentArr.Size(); ++ci)
                        {
                            const auto part = contentArr.GetObjectAt(ci);
                            if (!part.HasKey(L"type"))
                                continue;

                            const auto ptype = part.GetNamedString(L"type");
                            if (ptype == L"output_text")
                            {
                                if (part.HasKey(L"text") && part.GetNamedValue(L"text").ValueType() == JsonValueType::String)
                                {
                                    out = out + part.GetNamedString(L"text");
                                }
                                else if (part.HasKey(L"text") && part.GetNamedValue(L"text").ValueType() == JsonValueType::Object)
                                {
                                    const auto textObj = part.GetNamedObject(L"text");
                                    if (textObj.HasKey(L"value"))
                                    {
                                        out = out + textObj.GetNamedString(L"value");
                                    }
                                }
                            }
                        }
                    }
                }

                if (out.empty() && responseJson.HasKey(L"output_text"))
                {
                    out = responseJson.GetNamedString(L"output_text");
                }

                if (requestId != _currentRequestId)
                {
                    _hideSpinner();
                    co_return;
                }

                _hideSpinner();
                _displayResult(out.empty() ? L"No response from AI" : out);
            }
            else
            {
                if (requestId != _currentRequestId)
                {
                    _hideSpinner();
                    co_return;
                }
                _hideSpinner();
                auto errorMsg = std::wstring(L"API Error: ") + std::to_wstring(static_cast<int>(response.StatusCode())) + L" - " + responseContent.c_str();
                _displayResult(hstring{ errorMsg });
            }
        }
        catch (...)
        {
            if (requestId != _currentRequestId)
            {
                _hideSpinner();
                co_return;
            }
            _hideSpinner();
            _displayResult(L"Error: Failed to connect to OpenAI API");
        }
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
            _currentModel = AiModel::GPT5; // Chat mode uses GPT-5
        }
        else
        {
            _currentMode = AiMode::CommandSuggestions;
            _currentModel = AiModel::GPT4_1; // Command mode uses GPT-4.1
        }
        _updateModeDisplay();
        _updateModelIndicator();
    }

    void AiPromptControl::_updateModeDisplay()
    {
        if (_currentMode == AiMode::CommandSuggestions)
        {
            VimSearchHeaderTextBlock().Text(L"AI Prompt - Command Mode");
            CopyButton().Visibility(Visibility::Collapsed);
        }
        else
        {
            VimSearchHeaderTextBlock().Text(L"AI Prompt - Chat Mode");
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

    std::wstring AiPromptControl::_getOpenAIApiKey()
    {
        constexpr wchar_t kTarget[] = L"WindowsTerminal_OpenAI_API_Key";
        PCREDENTIALW cred = nullptr;
        std::wstring apiKey;

        if (CredReadW(kTarget, CRED_TYPE_GENERIC, 0, &cred))
        {
            if (cred && cred->Type == CRED_TYPE_GENERIC && cred->CredentialBlob && cred->CredentialBlobSize)
            {
                // Prefer UTF-16LE (how you should have written it with CredWriteW)
                if ((cred->CredentialBlobSize % sizeof(wchar_t)) == 0)
                {
                    const wchar_t* w = reinterpret_cast<const wchar_t*>(cred->CredentialBlob);
                    size_t count = cred->CredentialBlobSize / sizeof(wchar_t);
                    if (count && w[count - 1] == L'\0')
                    {
                        --count;
                    } // trim single trailing NUL
                    apiKey.assign(w, w + count);
                }
                else
                {
                    // Fallback: treat as UTF-8 if someone wrote bytes that aren't UTF-16
                    const char* bytes = reinterpret_cast<const char*>(cred->CredentialBlob);
                    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, static_cast<int>(cred->CredentialBlobSize), nullptr, 0);
                    if (wlen > 0)
                    {
                        apiKey.resize(wlen);
                        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, static_cast<int>(cred->CredentialBlobSize), apiKey.data(), wlen);
                    }
                }
            }
            CredFree(cred);
            if (!apiKey.empty())
                return apiKey;
        }

        // Optional (less secure): environment variable fallback
        // Consider removing or gating behind a setting.
        wchar_t* env = nullptr;
        size_t len = 0;
        if (_wdupenv_s(&env, &len, L"OPENAI_API_KEY") == 0 && env)
        {
            std::wstring v(env);
            free(env);
            if (!v.empty())
                return v;
        }
        return L"";
    }

    void AiPromptControl::_cycleModel()
    {
        if (_currentModel == AiModel::GPT5)
        {
            _currentModel = AiModel::GPT4_1;
        }
        else
        {
            _currentModel = AiModel::GPT5;
        }
        _updateModelIndicator();
    }

    std::wstring AiPromptControl::_getModelString() const
    {
        switch (_currentModel)
        {
            case AiModel::GPT4_1:
                return L"gpt-4.1";
            case AiModel::GPT5:
            default:
                return L"gpt-5";
        }
    }

    void AiPromptControl::_updateModelIndicator()
    {
        switch (_currentModel)
        {
            case AiModel::GPT4_1:
                ModelIndicator().Text(L"GPT-4.1");
                break;
            case AiModel::GPT5:
            default:
                ModelIndicator().Text(L"GPT-5");
                break;
        }
    }
}
