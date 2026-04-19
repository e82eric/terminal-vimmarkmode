#pragma once

#include "AiPromptControl.g.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>

namespace winrt::Microsoft::Terminal::Control::implementation
{
    // Provider-agnostic "small/fast" vs "large/smart" slot. Mapped to the actual
    // model name in AiPromptControl::_getModelString() based on _currentProvider.
    enum class AiModel
    {
        Haiku, // small / fast
        Sonnet // large / smart
    };

    struct AiPromptControl : AiPromptControlT<AiPromptControl>
    {
    public:
        AiPromptControl();
        void SetProvider(Control::AiPromptProvider provider);
        void SetMode(Control::AiPromptMode mode);

        til::property_changed_event PropertyChanged;
        static Windows::UI::Xaml::DependencyProperty BorderColorProperty();
        static Windows::UI::Xaml::DependencyProperty HeaderTextColorProperty();
        static Windows::UI::Xaml::DependencyProperty BackgroundColorProperty();
        static Windows::UI::Xaml::DependencyProperty InnerBorderThicknessProperty();
        static Windows::UI::Xaml::DependencyProperty TextColorProperty();

        Windows::UI::Xaml::Media::Brush BorderColor();
        void BorderColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush HeaderTextColor();
        void HeaderTextColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush BackgroundColor();
        void BackgroundColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Thickness InnerBorderThickness();
        void InnerBorderThickness(Windows::UI::Xaml::Thickness const& value);

        Windows::UI::Xaml::Media::Brush TextColor();
        void TextColor(Windows::UI::Xaml::Media::Brush const& value);

        void Show();
        void ShowWithContext(const winrt::hstring& terminalContext, const winrt::hstring& cursorLine);
        bool ContainsFocus();

        void _TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _ResultTextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _CopyButtonClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        
        TYPED_EVENT(Closed, Control::AiPromptControl, Windows::UI::Xaml::RoutedEventArgs);
        TYPED_EVENT(OnReturn, Control::AiPromptControl, hstring);

        private:
        void _close();
        void _sendToClaude(const winrt::hstring& prompt);
        winrt::Windows::Foundation::IAsyncAction _sendToClaudeAsync(const winrt::hstring& prompt, uint32_t requestId);
        void _showSpinner();
        void _hideSpinner();
        void _displayResult(const winrt::hstring& result);
        void _cycleMode();
        void _updateModeDisplay();
        void _cycleModel();
        void _cycleProvider();
        std::wstring _getModelString() const;
        void _updateModelIndicator();
        void _sendResultToTerminal();
        std::unordered_set<winrt::Windows::Foundation::IInspectable> _focusableElements;
        winrt::hstring _terminalContext;
        std::wstring _extractedCommand;
        uint32_t _requestCounter = 0;
        uint32_t _currentRequestId = 0;
        size_t _originalCursorLineLength = 0;
        Control::AiPromptMode _currentMode = Control::AiPromptMode::Command;
        Control::AiPromptProvider _currentProvider = Control::AiPromptProvider::Claude;
        AiModel _currentModel = AiModel::Haiku;

        struct _KeyBinding
        {
            std::wstring_view label;
            std::wstring_view description;
        };

        std::vector<_KeyBinding> _keyBindings;
        bool _helpVisible{ false };
        void _initKeyBindings();
        void _toggleHelp();

        static Windows::UI::Xaml::DependencyProperty _borderColorProperty;
        static Windows::UI::Xaml::DependencyProperty _headerTextColorProperty;
        static Windows::UI::Xaml::DependencyProperty _BackgroundColorProperty;
        static Windows::UI::Xaml::DependencyProperty _InnerBorderThicknessProperty;
        static Windows::UI::Xaml::DependencyProperty _TextColorProperty;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(AiPromptControl);
}
