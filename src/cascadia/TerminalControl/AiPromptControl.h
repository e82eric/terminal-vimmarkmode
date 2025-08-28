#pragma once

#include "AiPromptControl.g.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include <ControlSettings.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <wincred.h>

namespace winrt::Microsoft::Terminal::Control::implementation
{
    enum class AiMode
    {
        CommandSuggestions,
        Chat
    };

    struct AiPromptControl : AiPromptControlT<AiPromptControl>
    {
    public:
        AiPromptControl();

        til::property_changed_event PropertyChanged;
        static Windows::UI::Xaml::DependencyProperty BorderColorProperty();
        static Windows::UI::Xaml::DependencyProperty HeaderTextColorProperty();
        static Windows::UI::Xaml::DependencyProperty BackgroundColorProperty();
        static Windows::UI::Xaml::DependencyProperty InnerBorderThicknessProperty();

        Windows::UI::Xaml::Media::Brush BorderColor();
        void BorderColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush HeaderTextColor();
        void HeaderTextColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush BackgroundColor();
        void BackgroundColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Thickness InnerBorderThickness();
        void InnerBorderThickness(Windows::UI::Xaml::Thickness const& value);

        void Show();
        void ShowWithContext(const winrt::hstring& terminalContext, const winrt::hstring& cursorLine);
        bool ContainsFocus();

        void _TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _CopyButtonClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        
        TYPED_EVENT(Closed, Control::AiPromptControl, Windows::UI::Xaml::RoutedEventArgs);
        TYPED_EVENT(OnReturn, Control::AiPromptControl, hstring);

        private:
        void _close();
        void _sendToOpenAI(const winrt::hstring& prompt);
        winrt::Windows::Foundation::IAsyncAction _sendToOpenAIAsync(const winrt::hstring& prompt, uint32_t requestId);
        void _showSpinner();
        void _hideSpinner();
        void _displayResult(const winrt::hstring& result);
        void _cycleMode();
        void _updateModeDisplay();
        std::wstring _getOpenAIApiKey();
        std::unordered_set<winrt::Windows::Foundation::IInspectable> _focusableElements;
        winrt::Windows::Web::Http::HttpClient _httpClient;
        winrt::hstring _terminalContext;
        uint32_t _requestCounter = 0;
        uint32_t _currentRequestId = 0;
        size_t _originalCursorLineLength = 0;
        AiMode _currentMode = AiMode::CommandSuggestions;

        static Windows::UI::Xaml::DependencyProperty _borderColorProperty;
        static Windows::UI::Xaml::DependencyProperty _headerTextColorProperty;
        static Windows::UI::Xaml::DependencyProperty _BackgroundColorProperty;
        static Windows::UI::Xaml::DependencyProperty _InnerBorderThicknessProperty;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(AiPromptControl);
}