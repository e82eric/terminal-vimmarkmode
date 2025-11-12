// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "FuzzySearchTextSegment.h"
#include "StreamingSuggestionsControl.h"
#include "StreamingSuggestionsControl.g.cpp"
#include <LibraryResources.h>

#include "IInputEvent.hpp"

using namespace winrt::Windows::UI::Xaml::Media;

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace winrt::Microsoft::Terminal::Control::implementation
{
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

        _sizeChangedRevoker = ListBox().SizeChanged(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            if (Visibility() == Visibility::Visible)
            {
                this->_recalculateTopMargin();
            }
        });
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
        }
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

    void StreamingSuggestionsControl::SetCurrentWord(const winrt::hstring& value, int32_t cursorX)
    {
        if (cursorX < _cursorX)
        {
            _close(true);
            return;
        }

        auto betweenCursors = til::safe_slice_abs(value, _cursorX, cursorX);

        _currentWord = betweenCursors;
        {
            std::lock_guard lock(_searchTermMutex);
            _currentSearchTerm = betweenCursors;
        }
        _triggerSearch();
    }

    DependencyProperty StreamingSuggestionsControl::HighlightedTextColorProperty()
    {
        return _HighlightedTextColorProperty;
    }

    void StreamingSuggestionsControl::_selectFirstItem()
    {
        if (ListBox().Items().Size() > 0)
        {
            ListBox().SelectedIndex(0);
        }
    }

    void StreamingSuggestionsControl::_close(bool scrollToCursor)
    {
        ListBox().Items().Clear();
        Visibility(Windows::UI::Xaml::Visibility::Collapsed);
        _termControl.ClearHighlights(scrollToCursor);
    }

    void StreamingSuggestionsControl::Open(
        TermControl const& termControl,
        winrt::hstring needle,
        Windows::Foundation::Point anchor,
        Windows::Foundation::Size space,
        winrt::hstring currentWord,
        float prefixWidth,
        int32_t cursorX)
    {
        _scrollToSpan = false;
        _mode = StreamingSuggestionsMode::Normal;
        _cursorX = cursorX - static_cast<int32_t>(currentWord.size());
        _termControl = termControl;
        _currentWord = currentWord;
        _currentSearchTerm = currentWord;
        _prefixWidth = prefixWidth;

        const auto proposedX = gsl::narrow_cast<int>(anchor.X - prefixWidth);
        const auto maxX = gsl::narrow_cast<int>(space.Width - ActualWidth());
        const auto clampedX = std::clamp(proposedX, 0, maxX);
        Margin(Windows::UI::Xaml::ThicknessHelper::FromLengths(clampedX, 0, 0, 0));

        _anchor = anchor;
        _space = space;
        _searchVersion = 0;
        _allItemsLoaded = false;
        _allItemsSearched = false;
        _controlShown = false;

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
    }

    std::optional<SuggestionSearchItem> StreamingSuggestionsControl::_TryGetSelectedSuggestion()
    {
        auto selected = ListBox().SelectedItem();
        if (!selected)
        {
            return std::nullopt;
        }

        Controls::ListViewItem lvi = selected.try_as<Controls::ListViewItem>();
        Windows::Foundation::IInspectable data = lvi ? lvi.DataContext() : selected;

        if (auto suggestion = data.try_as<SuggestionSearchItem>())
        {
            return suggestion;
        }

        return std::nullopt;
    }

    bool StreamingSuggestionsControl::HandleKeyPress(WORD vkey, WORD /*scanCode*/, Core::ControlKeyStates modifiers, bool keyDown)
    {
        const auto itemCount = ListBox().Items().Size();
        if (itemCount == 0)
        {
            return false;
        }

        switch (vkey)
        {
        case VK_UP:
        {
            if (keyDown)
            {
                const auto currentIndex = ListBox().SelectedIndex();
                if (currentIndex > 0)
                {
                    _selectItem(currentIndex - 1);
                }
            }
            return true;
        }
        case VK_DOWN:
        {
            if (keyDown)
            {
                const auto currentIndex = ListBox().SelectedIndex();
                if (currentIndex < static_cast<int32_t>(itemCount) - 1)
                {
                    _selectItem(currentIndex + 1);
                }
            }
            return true;
        }
        case VK_ESCAPE:
        {
            if (keyDown)
            {
                if (_mode == StreamingSuggestionsMode::WordSplit)
                {
                    _mode = StreamingSuggestionsMode::Normal;
                    _triggerSearch();
                }
                else
                {
                    _close(true);
                }
            }
            return true;
        }
        case VK_TAB:
        {
            if (keyDown)
            {
                const auto ctrlPressed = WI_IsFlagSet(modifiers.Value, LEFT_CTRL_PRESSED);
                if (ctrlPressed)
                {
                    _scrollToSpan = !_scrollToSpan;
                    auto selectedIndex = ListBox().SelectedIndex();
                    _selectItem(selectedIndex);
                }
                else
                {
                    _enterWordSplitMode();
                }
            }
            return true;
        }
        case VK_RETURN:
        {
            hstring combined = L"";
            auto backspaces = std::wstring(_currentWord.size(), L'\x7f');
            switch (_mode)
            {
            case StreamingSuggestionsMode::Normal:
            {
                if (auto castedDc = _TryGetSelectedSuggestion())
                {
                    const auto shiftPressed = WI_IsFlagSet(modifiers.Value, SHIFT_PRESSED);
                    if (shiftPressed)
                    {
                        _termControl.SendInput(backspaces);
                        _termControl.SelectRow(castedDc->StartPos.Y, castedDc->StartPos.X);
                        _close(false);
                        return true;
                    }

                    combined = castedDc->Text;
                }
                break;
            }
            case StreamingSuggestionsMode::WordSplit:
            {
                if (auto selected = ListBox().SelectedItem())
                {
                    Controls::ListViewItem lvi = selected.try_as<Controls::ListViewItem>();
                    Windows::Foundation::IInspectable data = lvi ? lvi.DataContext() : selected;

                    if (auto word = data.try_as<hstring>())
                    {
                        combined = word.value();
                    }
                }
                break;
            }
            }

            auto text = winrt::hstring{ fmt::format(FMT_COMPILE(L"{}{}"), backspaces, combined) };
            _termControl.SendInput(text);
            _close(true);

            return true;
        }
        default:
            return false;
        }
    }

    void StreamingSuggestionsControl::_triggerSearch()
    {
        std::wstring term;
        {
            std::lock_guard lock(_searchTermMutex);
            term = _currentSearchTerm;
        }

        const std::uint64_t myVersion = ++_searchVersion;

        _performFuzzySearch(term, myVersion);
    }

    void StreamingSuggestionsControl::_selectItem(int32_t index)
    {
        const auto size = gsl::narrow_cast<int32_t>(ListBox().Items().Size());
        if (index < 0 || index >= size)
        {
            return;
        }

        ListBox().SelectedIndex(index);
        ListBox().ScrollIntoView(ListBox().SelectedItem());

        if (_mode != StreamingSuggestionsMode::Normal)
        {
            return;
        }

        if (auto selectedItem = _TryGetSelectedSuggestion())
        {
            _termControl.HighlightPointSpan(selectedItem.value().StartPos, selectedItem.value().EndPos, _scrollToSpan);
        }
    }

    Control::FuzzySearchTextLine StreamingSuggestionsControl::_BuildLine(hstring const& text,
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

    winrt::Windows::Foundation::IAsyncAction StreamingSuggestionsControl::_performFuzzySearch(std::wstring searchTerm, uint64_t version)
    {
        struct ScoredItem
        {
            Microsoft::Terminal::Control::SuggestionSearchItem item;
            int32_t score;
            std::optional<std::vector<fzfcpp::matcher::TextRun>> runs;
            int32_t ordinal;
        };

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
            co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);
            ListBox().Items().Clear();
            for (const auto& batch : batchesSnapshot)
            {
                for (auto item : batch.Items())
                {
                    auto line = _BuildLine(item.Text, item.StartPos.Y, item.StartPos.X, {});
                    auto lbi = _makeListViewItem(line, box_value(item));
                    ListBox().Items().Append(lbi);

                    if (ListBox().Items().Size() >= 1000)
                    {
                        break;
                    }
                }
            }

            Visibility(Visibility::Visible);
            _recalculateTopMargin();

            if (ListBox().SelectedIndex() == -1)
            {
                _selectItem(0);
            }

            NoItemsPlaceholder().Visibility(ListBox().Items().Size() == 0 ? Visibility::Visible : Visibility::Collapsed);
            InvalidateMeasure();

            co_return;
        }

        auto pattern = fzfcpp::matcher::ParsePatternWithTypes(searchTerm);

        std::vector<ScoredItem> scoredItems;

        for (const auto& batch : batchesSnapshot)
        {
            if (version != _searchVersion)
            {
                co_return;
            }
            for (const auto& item : batch.Items())
            {
                auto text = item.Text;
                auto matchResult = fzfcpp::matcher::Match(text, pattern);
                if (matchResult)
                {
                    scoredItems.push_back({ item, matchResult->Score, matchResult->Runs, item.Ordinal });
                }
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

        ListBox().Items().Clear();
        for (const auto& scoredItem : scoredItems)
        {
            auto line = _BuildLine(scoredItem.item.Text, scoredItem.item.StartPos.Y, scoredItem.item.StartPos.X, scoredItem.runs);
            auto lbi = _makeListViewItem(line, box_value(scoredItem.item));
            ListBox().Items().Append(lbi);
        }

        _allItemsSearched = true;
        if (!scoredItems.empty() && ListBox().SelectedIndex() == -1)
        {
            _selectItem(0);
        }

        Visibility(Visibility::Visible);

        InvalidateMeasure();
        _recalculateTopMargin();

        NoItemsPlaceholder().Visibility(ListBox().Items().Size() == 0 ? Visibility::Visible : Visibility::Collapsed);
        co_return;
    }

    static const std::wstring kWordDelimiters = L"";

    static bool IsDelimiter(wchar_t ch, std::wstring_view delims)
    {
        return iswspace(ch) || delims.find(ch) != std::wstring_view::npos;
    }

    std::vector<winrt::hstring> SplitWordsLongerThan5(winrt::hstring const& line)
    {
        std::wstring_view v{ line.c_str(), line.size() };
        std::vector<winrt::hstring> out;

        size_t i = 0, n = v.size();
        while (i < n)
        {
            while (i < n && IsDelimiter(v[i], kWordDelimiters))
            {
                ++i;
            }
            const size_t start = i;

            while (i < n && !IsDelimiter(v[i], kWordDelimiters))
            {
                ++i;
            }
            const size_t len = i - start;

            if (len >= 6)
            {
                out.emplace_back(v.substr(start, len));
            }
        }
        return out;
    }

    Controls::ListViewItem StreamingSuggestionsControl::_makeListViewItem(Control::FuzzySearchTextLine const& line, winrt::Windows::Foundation::IInspectable const& dataContext)
    {
        const auto input = Control::FuzzySearchTextControl{};
        input.TextColor(TextColor());
        input.HighlightedTextColor(HighlightedTextColor());
        input.HorizontalAlignment(Windows::UI::Xaml::HorizontalAlignment::Left);
        input.Margin(Windows::UI::Xaml::ThicknessHelper::FromLengths(0, 0, 0, 0));
        input.Text(line);

        auto lbi = winrt::Windows::UI::Xaml::Controls::ListViewItem{};
        lbi.Content(input);
        if (dataContext)
        {
            lbi.DataContext(dataContext);
        }
        return lbi;
    }

    void StreamingSuggestionsControl::_enterWordSplitMode()
    {
        _mode = StreamingSuggestionsMode::WordSplit;

        if (auto suggestionSearchItem = _TryGetSelectedSuggestion())
        {
            auto combined = suggestionSearchItem->Text;
            auto words = SplitWordsLongerThan5(hstring{ combined });

            ListBox().Items().Clear();
            for (auto word : words)
            {
                auto runs = winrt::single_threaded_observable_vector<Control::FuzzySearchTextSegment>();
                auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(word, false);
                runs.Append(textSegment);
                auto line = winrt::make<implementation::FuzzySearchTextLine>(runs, 0, 0);
                auto lbi = _makeListViewItem(line, box_value(word));
                ListBox().Items().Append(lbi);
            }

            _selectItem(0);
        }
    }

    void StreamingSuggestionsControl::_recalculateTopMargin()
    {
        const auto controlHeight = 250.0;
        const auto spaceBelow = _space.Height - _anchor.Y;

        auto openUpward = true;
        if (spaceBelow >= controlHeight)
        {
            openUpward = false;
        }
        _setDirection(openUpward);
    }

    void StreamingSuggestionsControl::_setDirection(bool openUpward)
    {
        RootGrid().Measure({
            static_cast<float>(ActualWidth()),
            static_cast<float>(ActualHeight()),
        });

        auto currentMargin = Margin();

        // Use explicit control dimensions (set in XAML)
        const auto controlWidth = ActualWidth();
        const auto controlHeight = ActualHeight();

        const auto proposedX = gsl::narrow_cast<int>(_anchor.X - _prefixWidth - 5.0f);
        const auto maxX = gsl::narrow_cast<int>(_space.Width - controlWidth);
        const auto clampedX = std::clamp(proposedX, 0, maxX);
        currentMargin.Left = clampedX;

        if (openUpward)
        {
            const auto marginTop = (_anchor.Y - controlHeight);
            currentMargin.Top = marginTop;
        }
        else
        {
            currentMargin.Top = (_anchor.Y + 20); // Position below the cursor line
        }
        Margin(currentMargin);
    }
}
