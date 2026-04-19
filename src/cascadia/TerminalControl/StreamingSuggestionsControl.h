

#pragma once

#include "StreamingSuggestionsControl.g.h"
#include "../fzfcpp/fzf.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct StreamingSuggestionsControl : StreamingSuggestionsControlT<StreamingSuggestionsControl>
    {
        winrt::event_token PropertyChanged(const winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler& handler);
        void PropertyChanged(const winrt::event_token& token) noexcept;

    public:
        StreamingSuggestionsControl();

        static Windows::UI::Xaml::DependencyProperty BorderColorProperty();
        static Windows::UI::Xaml::DependencyProperty HeaderTextColorProperty();
        static Windows::UI::Xaml::DependencyProperty BackgroundColorProperty();
        static Windows::UI::Xaml::DependencyProperty SelectedItemColorProperty();
        static Windows::UI::Xaml::DependencyProperty InnerBorderThicknessProperty();
        static Windows::UI::Xaml::DependencyProperty TextColorProperty();
        static Windows::UI::Xaml::DependencyProperty HighlightedTextColorProperty();

        Windows::UI::Xaml::Media::Brush BorderColor();
        void BorderColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush HeaderTextColor();
        void HeaderTextColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush BackgroundColor();
        void BackgroundColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Color SelectedItemColor();
        void SelectedItemColor(Windows::UI::Color const& value);

        Windows::UI::Xaml::Thickness InnerBorderThickness();
        void InnerBorderThickness(Windows::UI::Xaml::Thickness const& value);

        Windows::UI::Xaml::Media::Brush TextColor();
        void TextColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush HighlightedTextColor();
        void HighlightedTextColor(Windows::UI::Xaml::Media::Brush const& value);

        void SetCurrentWord(const winrt::hstring& value, int32_t cursorX);
        void Open(
            Microsoft::Terminal::Control::TermControl const& termControl,
            winrt::hstring needle,
            Windows::Foundation::Point anchor,
            Windows::Foundation::Size space,
            winrt::hstring currentWord,
            float prefixWidth,
            int32_t cursorX,
            float characterHeight);
        std::optional<SuggestionSearchItem> _TryGetSelectedSuggestion();
        void ToggleAutoComplete();

        bool HandleKeyPress(WORD vkey, WORD scanCode, Core::ControlKeyStates modifiers, bool keyDown);

        bool UseFuzzySearch() const { return _useFuzzySearch; }
        void UseFuzzySearch(bool value) { _useFuzzySearch = value; }

    private:
        winrt::Windows::UI::Xaml::DispatcherTimer _copyNotificationTimer{ nullptr };
        enum StreamingSuggestionsMode
        {
            Normal,
            WordSplit
        };

        bool _autoCompleteEnabled = false;
        bool _autoCompleteMode = false;
        bool _useFuzzySearch = false;
        StreamingSuggestionsMode _mode = StreamingSuggestionsMode::Normal;
        int32_t _cursorX;
        void _selectFirstItem();
        void _close(bool scrollToCursor);
        winrt::handle _lastSwapChainHandle{ nullptr };
        std::unordered_set<winrt::Windows::Foundation::IInspectable> _focusableElements;
        til::size _fontSize;
        winrt::Windows::System::DispatcherQueue _dispatcher{ nullptr };
        bool _initialized = false;
        float _panelWidth{ 0 };
        float _panelHeight{ 0 };
        float _compositionScale{ 0 };
        winrt::event<winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler> _propertyChangedEvent;
        float _characterHeight;
        Microsoft::Terminal::Control::TermControl _termControl{ nullptr };
        bool _scrollToSpan = false;
        hstring _currentWord;
        hstring _currentSearchTerm;
        float _prefixWidth;
        Windows::Foundation::Point _anchor;
        Windows::Foundation::Size _space;
        int _searchVersion;
        bool _allItemsLoaded;
        bool _allItemsSearched;
        bool _controlShown;
        std::vector<Microsoft::Terminal::Control::SuggestionBatch> _batches;
        std::mutex _batchesMutex;
        std::mutex _searchTermMutex;
        winrt::Windows::UI::Xaml::Controls::ListView::SizeChanged_revoker _sizeChangedRevoker;
        void _triggerSearch();
        void _selectItem(int32_t index);
        winrt::Windows::Foundation::IAsyncAction _performFuzzySearch(std::wstring searchTerm, uint64_t version);
        Windows::UI::Xaml::Controls::ListViewItem _makeListViewItem(Control::FuzzySearchTextLine const& line,
                                                                    winrt::Windows::Foundation::IInspectable const&
                                                                    dataContext);

        Control::FuzzySearchTextLine _BuildLine(hstring const& text,
                                                int32_t row,
                                                int32_t col,
                                                std::optional<std::vector<fzfcpp::matcher::TextRun>> const& runs);

        void _enterWordSplitMode();
        void _recalculateTopMargin();
        void _recalculateHorizontalPlacement();
        void _ensureCellWidth();
        void _setDirection(bool openUpward);
        void _OnCopyNotificationTimerTick(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::Foundation::IInspectable const&);
        void _showCopyNotification(const hstring& text);

        struct _KeyBinding
        {
            DWORD requiredMods;
            WORD vkey;
            std::wstring_view label;
            std::wstring_view description;
            std::function<bool(bool /*keyDown*/)> action;
        };

        std::vector<_KeyBinding> _keyBindings;
        bool _helpVisible{ false };
        void _initKeyBindings();
        void _toggleHelp();
        bool _applySelectedOrClose(bool keyDown);

        static Windows::UI::Xaml::DependencyProperty _borderColorProperty;
        static Windows::UI::Xaml::DependencyProperty _headerTextColorProperty;
        static Windows::UI::Xaml::DependencyProperty _BackgroundColorProperty;
        static Windows::UI::Xaml::DependencyProperty _SelectedItemColorProperty;
        static Windows::UI::Xaml::DependencyProperty _InnerBorderThicknessProperty;
        static Windows::UI::Xaml::DependencyProperty _TextColorProperty;
        static Windows::UI::Xaml::DependencyProperty _HighlightedTextColorProperty;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(StreamingSuggestionsControl);
}
