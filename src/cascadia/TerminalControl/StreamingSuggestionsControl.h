

#pragma once

#include "StreamingSuggestionsControl.g.h"
#include "SuggestionSearchRow.g.h"
#include "../fzfcpp/fzf.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct SuggestionSearchRow : SuggestionSearchRowT<SuggestionSearchRow>
    {
        SuggestionSearchRow() = default;
        SuggestionSearchRow(Control::FuzzySearchTextLine const& line,
                            Control::FuzzySearchTextLine const& secondaryLine,
                            Control::SuggestionSearchItem const& item,
                            Windows::UI::Xaml::Media::Brush const& textColor,
                            Windows::UI::Xaml::Media::Brush const& highlightedTextColor) :
            _Line(line), _SecondaryLine(secondaryLine), _Item(item), _TextColor(textColor), _HighlightedTextColor(highlightedTextColor) {}

        Control::FuzzySearchTextLine Line() const { return _Line; }
        Control::FuzzySearchTextLine SecondaryLine() const { return _SecondaryLine; }
        bool HasSecondaryLine() const { return _SecondaryLine != nullptr; }
        Windows::UI::Xaml::Visibility SecondaryVisibility() const { return HasSecondaryLine() ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed; }
        Control::SuggestionSearchItem Item() const { return _Item; }
        Windows::UI::Xaml::Media::Brush TextColor() const { return _TextColor; }
        Windows::UI::Xaml::Media::Brush HighlightedTextColor() const { return _HighlightedTextColor; }

    private:
        Control::FuzzySearchTextLine _Line{ nullptr };
        Control::FuzzySearchTextLine _SecondaryLine{ nullptr };
        Control::SuggestionSearchItem _Item{};
        Windows::UI::Xaml::Media::Brush _TextColor{ nullptr };
        Windows::UI::Xaml::Media::Brush _HighlightedTextColor{ nullptr };
    };

    struct SuggestionRowSource
    {
        Microsoft::Terminal::Control::SuggestionSearchItem item;
        std::optional<std::vector<fzfcpp::matcher::TextRun>> runs;
        winrt::hstring displayText;
        std::optional<std::vector<fzfcpp::matcher::TextRun>> secondaryRuns;
        winrt::hstring secondaryText;
    };

    struct LazySuggestionRowVector;

    struct LazySuggestionRowIterator : winrt::implements<LazySuggestionRowIterator,
                                                         winrt::Windows::Foundation::Collections::IIterator<winrt::Windows::Foundation::IInspectable>>
    {
        LazySuggestionRowIterator(winrt::com_ptr<LazySuggestionRowVector> owner) :
            _owner(std::move(owner)) {}

        winrt::Windows::Foundation::IInspectable Current() const;
        bool HasCurrent() const noexcept;
        bool MoveNext() noexcept;
        uint32_t GetMany(winrt::array_view<winrt::Windows::Foundation::IInspectable> items);

    private:
        winrt::com_ptr<LazySuggestionRowVector> _owner;
        uint32_t _index{ 0 };
    };

    struct LazySuggestionRowVectorView;

    struct LazySuggestionRowVector : winrt::implements<LazySuggestionRowVector,
                                                       winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Foundation::IInspectable>,
                                                       winrt::Windows::Foundation::Collections::IIterable<winrt::Windows::Foundation::IInspectable>>
    {
        LazySuggestionRowVector(std::vector<SuggestionRowSource> sources,
                                Windows::UI::Xaml::Media::Brush textColor,
                                Windows::UI::Xaml::Media::Brush highlightedTextColor) :
            _sources(std::move(sources)),
            _textColor(textColor),
            _highlightedTextColor(highlightedTextColor)
        {
            _cache.resize(_sources.size());
        }

        uint32_t Size() const noexcept { return static_cast<uint32_t>(_sources.size()); }
        winrt::Windows::Foundation::IInspectable GetAt(uint32_t index);
        winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable> GetView();
        bool IndexOf(winrt::Windows::Foundation::IInspectable const& value, uint32_t& index) const noexcept;
        uint32_t GetMany(uint32_t startIndex, winrt::array_view<winrt::Windows::Foundation::IInspectable> items);
        winrt::Windows::Foundation::Collections::IIterator<winrt::Windows::Foundation::IInspectable> First();

        void SetAt(uint32_t, winrt::Windows::Foundation::IInspectable const&) { throw winrt::hresult_not_implemented(); }
        void InsertAt(uint32_t, winrt::Windows::Foundation::IInspectable const&) { throw winrt::hresult_not_implemented(); }
        void RemoveAt(uint32_t) { throw winrt::hresult_not_implemented(); }
        void Append(winrt::Windows::Foundation::IInspectable const&) { throw winrt::hresult_not_implemented(); }
        void RemoveAtEnd() { throw winrt::hresult_not_implemented(); }
        void Clear() { throw winrt::hresult_not_implemented(); }
        void ReplaceAll(winrt::array_view<winrt::Windows::Foundation::IInspectable const>) { throw winrt::hresult_not_implemented(); }

    private:
        std::vector<SuggestionRowSource> _sources;
        std::vector<winrt::Windows::Foundation::IInspectable> _cache;
        Windows::UI::Xaml::Media::Brush _textColor{ nullptr };
        Windows::UI::Xaml::Media::Brush _highlightedTextColor{ nullptr };
    };

    struct LazySuggestionRowVectorView : winrt::implements<LazySuggestionRowVectorView,
                                                           winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable>,
                                                           winrt::Windows::Foundation::Collections::IIterable<winrt::Windows::Foundation::IInspectable>>
    {
        LazySuggestionRowVectorView(winrt::com_ptr<LazySuggestionRowVector> owner) :
            _owner(std::move(owner)) {}

        uint32_t Size() const noexcept { return _owner->Size(); }
        winrt::Windows::Foundation::IInspectable GetAt(uint32_t index) { return _owner->GetAt(index); }
        bool IndexOf(winrt::Windows::Foundation::IInspectable const& value, uint32_t& index) const noexcept { return _owner->IndexOf(value, index); }
        uint32_t GetMany(uint32_t startIndex, winrt::array_view<winrt::Windows::Foundation::IInspectable> items) { return _owner->GetMany(startIndex, items); }
        winrt::Windows::Foundation::Collections::IIterator<winrt::Windows::Foundation::IInspectable> First() { return _owner->First(); }

    private:
        winrt::com_ptr<LazySuggestionRowVector> _owner;
    };

    struct CommandSearchHelper;

    struct StreamingSuggestionsControl : StreamingSuggestionsControlT<StreamingSuggestionsControl>
    {
        winrt::event_token PropertyChanged(const winrt::Windows::UI::Xaml::Data::PropertyChangedEventHandler& handler);
        void PropertyChanged(const winrt::event_token& token) noexcept;

    public:
        StreamingSuggestionsControl();
        ~StreamingSuggestionsControl();

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

        void Open(
            Microsoft::Terminal::Control::TermControl const& termControl,
            winrt::hstring const& needle,
            Windows::Foundation::Point anchor,
            Windows::Foundation::Size space,
            winrt::hstring const& currentWord,
            float prefixWidth,
            float characterHeight,
            float swapChainOffset);
        void OpenTasks(
            Microsoft::Terminal::Control::TermControl const& termControl,
            Windows::Foundation::Collections::IVector<Microsoft::Terminal::Control::SnippetSearchItem> snippets,
            Windows::Foundation::Point anchor,
            Windows::Foundation::Size space,
            winrt::hstring const& filterText,
            winrt::hstring const& currentWord,
            winrt::hstring const& currentCommandline,
            float prefixWidth,
            float characterHeight,
            float swapChainOffset,
            int32_t replaceTarget);
        void OpenCommand(
            Microsoft::Terminal::Control::TermControl const& termControl,
            winrt::hstring executable,
            Windows::Foundation::Collections::IVector<winrt::hstring> args,
            winrt::hstring commandTemplate,
            winrt::hstring workingDirectory,
            int32_t suggestionRow,
            Windows::Foundation::Point anchor,
            Windows::Foundation::Size space,
            winrt::hstring const& filterText,
            winrt::hstring const& currentWord,
            winrt::hstring const& currentCommandline,
            float prefixWidth,
            float characterHeight,
            float swapChainOffset,
            bool sortResults,
            bool useCommandline,
            bool prefillFilter,
            int32_t replaceTarget);
        bool ContainsFocus();
        std::optional<SuggestionSearchItem> _TryGetSelectedSuggestion();

        bool HandleKeyPress(WORD vkey, WORD scanCode, Core::ControlKeyStates modifiers, bool keyDown);

        void _SearchBoxTextChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
        void _SplitSearchBoxTextChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::RoutedEventArgs const&);
        void _SearchBoxKeyDown(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const&);

    private:
        winrt::Windows::UI::Xaml::DispatcherTimer _copyNotificationTimer{ nullptr };
        enum StreamingSuggestionsMode
        {
            Normal,
            WordSplit
        };
        enum class StreamingSuggestionsDataSource
        {
            Scrollback,
            Command,
            Tasks
        };
        enum class ReplaceTarget : int32_t
        {
            Default = 0,
            Word,
            Line
        };

        bool _sortResults = false;
        bool _useCommandline = false;
        ReplaceTarget _replaceTarget = ReplaceTarget::Default;
        StreamingSuggestionsMode _mode = StreamingSuggestionsMode::Normal;
        StreamingSuggestionsDataSource _dataSource = StreamingSuggestionsDataSource::Scrollback;
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
        float _swapChainOffset = 0.0f;
        hstring _currentWord;
        hstring _currentCommandline;
        hstring _currentSearchTerm;
        hstring _commandTemplate;
        std::shared_ptr<CommandSearchHelper> _commandSearchHelper;
        float _prefixWidth;
        Windows::Foundation::Point _anchor;
        Windows::Foundation::Size _space;
        int _searchVersion;
        bool _allItemsLoaded;
        bool _allItemsSearched;
        bool _controlShown;
        std::vector<Microsoft::Terminal::Control::SuggestionBatch> _batches;
        std::vector<Microsoft::Terminal::Control::SnippetSearchItem> _taskItems;
        std::vector<Microsoft::Terminal::Control::SuggestionSearchItem> _splitItems;
        std::mutex _batchesMutex;
        std::chrono::steady_clock::time_point _lastBatchTriggerTime{};
        bool _searchInFlight{ false };
        bool _batchArrivedDuringSearch{ false };
        bool _isStreaming{ false };
        int _lastCompletedSearchVersion{ 0 };
        uint64_t _sessionVersion{ 0 };
        std::mutex _searchTermMutex;
        winrt::Windows::UI::Xaml::Controls::ListView::SizeChanged_revoker _sizeChangedRevoker;
        bool _suppressSearchBoxChange{ false };
        struct _OpenState
        {
            StreamingSuggestionsDataSource dataSource;
            winrt::hstring commandTemplate;
            Windows::Foundation::Point anchor;
            Windows::Foundation::Size space;
            winrt::hstring filterText;
            winrt::hstring currentWord;
            winrt::hstring currentCommandline;
            float prefixWidth;
            float characterHeight;
            float swapChainOffset;
            bool sortResults{ false };
            bool useCommandline{ false };
            bool prefillFilter{ true };
            ReplaceTarget replaceTarget{ ReplaceTarget::Default };
        };
        winrt::Windows::UI::Xaml::Controls::ListView _activeListBox();
        uint64_t _beginOpen(TermControl const& termControl, const _OpenState& state);
        Microsoft::Terminal::Control::SuggestionBatchHandler _makeBatchHandler(uint64_t sessionVersion);
        winrt::Windows::Foundation::IAsyncAction _finishStreamingLoad(uint64_t sessionVersion);
        void _showSplitOverlay(bool show);
        void _triggerSearch();
        void _selectItem(int32_t index);
        void _swapItemsPreservingSelection(std::vector<SuggestionRowSource>&& sources);
        winrt::Windows::Foundation::IAsyncAction _performFuzzySearch(std::wstring searchTerm, uint64_t version, uint64_t sessionVersion);
        void _populateSplitList(std::wstring searchTerm);

        void _recalculateTopMargin();
        void _recalculateHorizontalPlacement();
        void _applySearchBoxForeground();
        void _ensureCellWidth();
        void _setDirection();
        void _OnCopyNotificationTimerTick(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::Foundation::IInspectable const&);
        void _showCopyNotification(const hstring& text);
        void _updateLoadingIndicator();
        size_t _replacementLength() const;

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

        bool _searchBoxMode{ false };

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
