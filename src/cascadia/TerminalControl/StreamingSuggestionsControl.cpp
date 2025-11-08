// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "FuzzySearchTextSegment.h"
#include "StreamingSuggestionsControl.h"
#include "StreamingSuggestionsControl.g.cpp"
#include <LibraryResources.h>
#include "../fzfcpp/fzf.h"

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
            _close();
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

    void StreamingSuggestionsControl::_close()
    {
        _termControl.PreviewInput(L"");
        ListBox().Items().Clear();
        Visibility(Windows::UI::Xaml::Visibility::Collapsed);
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
                //self->_updateNoItemsVisibility();  
            }
        });
    }

    bool StreamingSuggestionsControl::HandleKeyPress(WORD vkey, WORD /*scanCode*/, Core::ControlKeyStates /*modifiers*/, bool keyDown)
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
                    ListBox().SelectedIndex(currentIndex - 1);
                    ListBox().ScrollIntoView(ListBox().SelectedItem());
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
                    ListBox().SelectedIndex(currentIndex + 1);
                    ListBox().ScrollIntoView(ListBox().SelectedItem());
                }
            }
            return true;
        }
        case VK_ESCAPE:
        {
            _close();
            return true;
        }
        case VK_TAB:
        case VK_RETURN:
        {
            auto selectedItem = ListBox().SelectedItem();
            if (selectedItem)
            {
                auto castedItem = selectedItem.try_as<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>();
                if (castedItem)
                {
                    auto backspaces = std::wstring(_currentWord.size(), L'\x7f');

                    auto segs = castedItem.Segments();
                    std::wstring combined;

                    // Pre-size to avoid repeated reallocations
                    size_t total = 0;
                    for (auto const& s : segs)
                    {
                        total += s.TextSegment().size();
                    }
                    combined.reserve(total);

                    for (auto const& s : segs)
                    {
                        auto const& hs = s.TextSegment();
                        combined.append(hs.c_str(), hs.size());
                    }

                    auto text = winrt::hstring{ fmt::format(FMT_COMPILE(L"{}{}"), backspaces, combined) };

                    _termControl.SendInput(text);
                }
            }

            // TODO: Handle selection - insert the selected item
            _close();
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

    winrt::Windows::Foundation::IAsyncAction StreamingSuggestionsControl::_performFuzzySearch(std::wstring searchTerm, uint64_t version)
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
            auto searchResults = winrt::single_threaded_observable_vector<Control::FuzzySearchTextLine>();

            for (const auto& batch : batchesSnapshot)
            {
                for (auto item : batch.Items())
                {
                    auto runs = winrt::single_threaded_observable_vector<Control::FuzzySearchTextSegment>();
                    auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(item.Text, false);
                    runs.Append(textSegment);
                    auto line = winrt::make<implementation::FuzzySearchTextLine>(runs, item.Ordinal, 0);

                    searchResults.Append(line);

                    if (searchResults.Size() >= 1000)
                    {
                        break;
                    }
                }
            }

            co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);
            if (version == _searchVersion)
            {
                ListBox().Items().Clear();
                for (auto a : searchResults)
                {
                    ListBox().Items().Append(a);
                }

                InvalidateMeasure();

                Visibility(Visibility::Visible);
                _recalculateTopMargin();
            }


            if (ListBox().SelectedIndex() == -1)
            {
                ListBox().SelectedIndex(0);
            }

            NoItemsPlaceholder().Visibility(ListBox().Items().Size() == 0 ? Visibility::Visible : Visibility::Collapsed);

            co_return;
        }
        
        auto pattern = fzfcpp::matcher::ParsePatternWithTypes(searchTerm);
        
        struct ScoredItem
        {
            Microsoft::Terminal::Control::SuggestionSearchItem item;
            int32_t score;
            std::optional<std::vector<fzfcpp::matcher::TextRun>> runs;
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
            auto segments = winrt::single_threaded_observable_vector<Control::FuzzySearchTextSegment>();
            if (scoredItem.runs)
            {
                size_t cursor = 0;
                for (auto run : scoredItem.runs.value())
                {
                    if (cursor < run.Start)
                    {
                        const hstring nonMatch{ til::safe_slice_abs(scoredItem.item.Text, cursor, static_cast<size_t>(run.Start)) };
                        auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(nonMatch, false);
                        segments.Append(textSegment);
                    }
                    const hstring matchSeg{ til::safe_slice_abs(scoredItem.item.Text, static_cast<size_t>(run.Start), static_cast<size_t>(run.End + 1)) };
                    auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(matchSeg, true);
                    segments.Append(textSegment);
                    cursor = run.End + 1;
                }

                if (cursor < scoredItem.item.Text.size())
                {
                    const hstring matchSeg{ til::safe_slice_abs(scoredItem.item.Text, cursor, scoredItem.item.Text.size()) };
                    auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(matchSeg, false);
                    segments.Append(textSegment);
                }
            }

            auto line = winrt::make<implementation::FuzzySearchTextLine>(segments, scoredItem.ordinal, 0);
            ListBox().Items().Append(line);
        }

        InvalidateMeasure();

        _allItemsSearched = true;
        if (!scoredItems.empty() && ListBox().SelectedIndex() == -1)
        {
            ListBox().SelectedIndex(0);
        }

        Visibility(Visibility::Visible);
        _recalculateTopMargin();

        NoItemsPlaceholder().Visibility(ListBox().Items().Size() == 0 ? Visibility::Visible : Visibility::Collapsed);
        co_return;
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
