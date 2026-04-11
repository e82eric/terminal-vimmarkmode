// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "SnippetSearchControl.h"
#include "SnippetSearchControl.g.cpp"
#include "FuzzySearchTextSegment.h"
#include "../fzfcpp/fzf.h"

using namespace winrt::Windows::UI::Xaml::Media;

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    DependencyProperty SnippetSearchControl::_borderColorProperty =
        DependencyProperty::Register(
            L"BorderColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::SnippetSearchControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty SnippetSearchControl::_headerTextColorProperty =
        DependencyProperty::Register(
            L"HeaderTextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::SnippetSearchControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty SnippetSearchControl::_BackgroundColorProperty =
        DependencyProperty::Register(
            L"BackgroundColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::SnippetSearchControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty SnippetSearchControl::_SelectedItemColorProperty =
        DependencyProperty::Register(
            L"SelectedItemColor",
            xaml_typename<Windows::UI::Color>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::SnippetSearchControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty SnippetSearchControl::_InnerBorderThicknessProperty =
        DependencyProperty::Register(
            L"BorderThickness",
            xaml_typename<Thickness>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::SnippetSearchControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty SnippetSearchControl::_TextColorProperty =
        DependencyProperty::Register(
            L"TextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::SnippetSearchControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty SnippetSearchControl::_HighlightedTextColorProperty =
        DependencyProperty::Register(
            L"HighlightedTextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::SnippetSearchControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty SnippetSearchControl::_ResultFontSizeProperty =
        DependencyProperty::Register(
            L"ResultFontSize",
            xaml_typename<double>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::SnippetSearchControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty SnippetSearchControl::BackgroundColorProperty()
    {
        return _BackgroundColorProperty;
    }

    Brush SnippetSearchControl::BackgroundColor()
    {
        return GetValue(_BackgroundColorProperty).as<Brush>();
    }

    void SnippetSearchControl::BackgroundColor(Brush const& value)
    {
        if (value != BackgroundColorProperty())
        {
            SetValue(_BackgroundColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"BackgroundColor" });
        }
    }

    Brush SnippetSearchControl::TextColor()
    {
        return GetValue(_TextColorProperty).as<Brush>();
    }

    void SnippetSearchControl::TextColor(Brush const& value)
    {
        if (value != TextColor())
        {
            SetValue(_TextColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"TextColor" });
        }
    }

    DependencyProperty SnippetSearchControl::TextColorProperty()
    {
        return _TextColorProperty;
    }

    Brush SnippetSearchControl::HighlightedTextColor()
    {
        return GetValue(_HighlightedTextColorProperty).as<Brush>();
    }

    void SnippetSearchControl::HighlightedTextColor(Brush const& value)
    {
        if (value != HighlightedTextColorProperty())
        {
            SetValue(_HighlightedTextColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"HighlightedTextColor" });
        }
    }

    DependencyProperty SnippetSearchControl::HighlightedTextColorProperty()
    {
        return _HighlightedTextColorProperty;
    }

    DependencyProperty SnippetSearchControl::SelectedItemColorProperty()
    {
        return _SelectedItemColorProperty;
    }

    Windows::UI::Color SnippetSearchControl::SelectedItemColor()
    {
        return GetValue(_SelectedItemColorProperty).as<Windows::UI::Color>();
    }

    void SnippetSearchControl::SelectedItemColor(Windows::UI::Color const& value)
    {
        if (value != SelectedItemColor())
        {
            SetValue(_SelectedItemColorProperty, box_value(value));
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"SelectedItemColor" });
        }
    }

    DependencyProperty SnippetSearchControl::BorderColorProperty()
    {
        return _borderColorProperty;
    }

    Brush SnippetSearchControl::BorderColor()
    {
        return GetValue(_borderColorProperty).as<Brush>();
    }

    void SnippetSearchControl::BorderColor(Brush const& value)
    {
        if (value != BorderColor())
        {
            SetValue(_borderColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"BorderColor" });
        }
    }

    DependencyProperty SnippetSearchControl::HeaderTextColorProperty()
    {
        return _headerTextColorProperty;
    }

    Brush SnippetSearchControl::HeaderTextColor()
    {
        return GetValue(_headerTextColorProperty).as<Brush>();
    }

    void SnippetSearchControl::HeaderTextColor(Brush const& value)
    {
        if (value != HeaderTextColor())
        {
            SetValue(_headerTextColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"HeaderTextColor" });
        }
    }

    DependencyProperty SnippetSearchControl::InnerBorderThicknessProperty()
    {
        return _InnerBorderThicknessProperty;
    }

    Thickness SnippetSearchControl::InnerBorderThickness()
    {
        return GetValue(_InnerBorderThicknessProperty).as<Thickness>();
    }

    void SnippetSearchControl::InnerBorderThickness(Thickness const& value)
    {
        if (value != InnerBorderThickness())
        {
            SetValue(_InnerBorderThicknessProperty, box_value(value));
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"InnerBorderThickness" });
        }
    }

    DependencyProperty SnippetSearchControl::ResultFontSizeProperty()
    {
        return _ResultFontSizeProperty;
    }

    double SnippetSearchControl::ResultFontSize()
    {
        return GetValue(_ResultFontSizeProperty).as<double>();
    }

    void SnippetSearchControl::ResultFontSize(double const& value)
    {
        if (value != ResultFontSize())
        {
            SetValue(_ResultFontSizeProperty, box_value(value));
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"ResultFontSize" });
        }
    }

    SnippetSearchControl::SnippetSearchControl()
    {
        InitializeComponent();
        _sizeChangedRevoker = ListBox().SizeChanged(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            if (Visibility() == Visibility::Visible)
            {
                this->_recalculateTopMargin();
            }
        });
    }

    void SnippetSearchControl::_selectFirstItem()
    {
        if (ListBox().Items().Size() > 0)
        {
            ListBox().SelectedIndex(0);
        }
    }

    void SnippetSearchControl::_populateForEmptySearch()
    {
        ListBox().Items().Clear();
        for (const auto& snippet : _items)
        {
            auto runs = winrt::single_threaded_observable_vector<Control::FuzzySearchTextSegment>();
            auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(snippet.EscapedInput, false);
            runs.Append(textSegment);

            auto descriptionRuns = winrt::single_threaded_observable_vector<Control::FuzzySearchTextSegment>();
            auto descriptionTextSegment = winrt::make<implementation::FuzzySearchTextSegment>(snippet.Description, false);
            descriptionRuns.Append(descriptionTextSegment);

            _appendItem( runs, descriptionRuns, snippet.Input);
        }
        ListBox().SelectedIndex(0);
    }

    void SnippetSearchControl::_appendItem(
        Windows::Foundation::Collections::IObservableVector<Control::FuzzySearchTextSegment> segments,
        Windows::Foundation::Collections::IObservableVector<Control::FuzzySearchTextSegment> descriptionSegments,
        const hstring& input)
    {
        const auto descriptionLine = winrt::make<FuzzySearchTextLine>(descriptionSegments, 0, 0);
        const auto inputLine = winrt::make<FuzzySearchTextLine>(segments, 0, 0);

        const auto container = Controls::StackPanel{};
        container.Orientation(Controls::Orientation::Vertical);
        container.HorizontalAlignment(Windows::UI::Xaml::HorizontalAlignment::Left);

        const auto descriptionControl = Control::FuzzySearchTextControl{};
        descriptionControl.TextColor(TextColor());
        descriptionControl.HighlightedTextColor(HighlightedTextColor());
        descriptionControl.FontWeight(Windows::UI::Text::FontWeights::Bold());
        descriptionControl.FontSize(FontSize() + 1);
        descriptionControl.HorizontalAlignment(Windows::UI::Xaml::HorizontalAlignment::Left);
        descriptionControl.Text(descriptionLine);
        container.Children().Append(descriptionControl);

        const auto inputControl = Control::FuzzySearchTextControl{};
        inputControl.TextColor(TextColor());
        inputControl.HighlightedTextColor(HighlightedTextColor());
        inputControl.HorizontalAlignment(Windows::UI::Xaml::HorizontalAlignment::Left);
        inputControl.Margin(Windows::UI::Xaml::ThicknessHelper::FromLengths(0, 0, 0, 0));
        inputControl.Text(inputLine);
        container.Children().Append(inputControl);
        container.Padding(ThicknessHelper::FromUniformLength(8));

        auto lbi = Controls::ListViewItem();
        lbi.DataContext(box_value(input));
        lbi.Content(container);
        ListBox().Items().Append(lbi);
    }

    void SnippetSearchControl::_performFuzzySearch()
    {
        if (_autoCompleteMode && _currentWord.size() < 1)
        {
            _close();
            return;
        }

        //if (_autoCompleteMode && _currentWord.back() == L' ')
        //{
        //    _close();
        //    return;
        //}

        const size_t nonSpace = std::count_if(_currentWord.begin(), _currentWord.end(), [](wchar_t ch){ return ch != L' '; });
        int minScore = _autoCompleteMode ? static_cast<int>(nonSpace) * 15 : 0;

        if (_currentWord.empty())
        {
            _populateForEmptySearch();
            return;
        }

        struct ScoredItem
        {
            SnippetSearchItem item;
            int32_t score;
            std::optional<std::vector<fzfcpp::matcher::TextRun>> runs;
            std::optional<std::vector<fzfcpp::matcher::TextRun>> descriptionRuns;
        };

        std::vector<ScoredItem> scoredItems;
        auto patternStr = _autoCompleteMode ? L"" + _currentWord : _currentWord;
        auto pattern = fzfcpp::matcher::ParsePatternWithTypes(patternStr);
        for (auto item : _items)
        {
            auto text = item.EscapedInput;
            auto description = item.Description;
            auto textMatchResult = fzfcpp::matcher::Match(text, pattern);
            auto descriptionMatchResult = fzfcpp::matcher::Match(description, pattern);

            if (textMatchResult || descriptionMatchResult)
            {
                auto descriptionRuns = std::vector<fzfcpp::matcher::TextRun>{};
                auto descriptionScore = 0;
                auto textRuns = std::vector<fzfcpp::matcher::TextRun>{};
                auto textScore = 0;

                if (descriptionMatchResult.has_value())
                {
                    descriptionRuns = descriptionMatchResult.value().Runs;
                    descriptionScore = descriptionMatchResult.value().Score;
                }
                if (textMatchResult.has_value())
                {
                    textRuns = textMatchResult.value().Runs;
                    textScore = textMatchResult.value().Score;
                }

                auto score = std::max(textScore, descriptionScore);
                if (score >= minScore)
                {
                    scoredItems.push_back(ScoredItem{ item, score, textRuns, descriptionRuns });
                }
            }
        }

        std::ranges::sort(scoredItems, std::greater<>{}, &ScoredItem::score);

        ListBox().Items().Clear();
        for (const auto& scoredItem : scoredItems)
        {
            auto segments = winrt::single_threaded_observable_vector<Control::FuzzySearchTextSegment>();
            auto descriptionSegments = winrt::single_threaded_observable_vector<Control::FuzzySearchTextSegment>();
            if (scoredItem.runs)
            {
                size_t cursor = 0;
                for (auto run : scoredItem.runs.value())
                {
                    if (cursor < run.Start)
                    {
                        const hstring nonMatch{ til::safe_slice_abs(scoredItem.item.EscapedInput, cursor, run.Start) };
                        auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(nonMatch, false);
                        segments.Append(textSegment);
                    }
                    const hstring matchSeg{ til::safe_slice_abs(scoredItem.item.EscapedInput, run.Start, run.End + 1) };
                    auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(matchSeg, true);
                    segments.Append(textSegment);
                    cursor = run.End + 1;
                }

                if (cursor < scoredItem.item.EscapedInput.size())
                {
                    const hstring matchSeg{ til::safe_slice_abs(scoredItem.item.EscapedInput, cursor, scoredItem.item.EscapedInput.size()) };
                    auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(matchSeg, false);
                    segments.Append(textSegment);
                }

                cursor = 0;
                for (auto run : scoredItem.descriptionRuns.value())
                {
                    if (cursor < run.Start)
                    {
                        const hstring nonMatch{ til::safe_slice_abs(scoredItem.item.Description, cursor, run.Start) };
                        auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(nonMatch, false);
                        descriptionSegments.Append(textSegment);
                    }
                    const hstring matchSeg{ til::safe_slice_abs(scoredItem.item.Description, run.Start, run.End + 1) };
                    auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(matchSeg, true);
                    descriptionSegments.Append(textSegment);
                    cursor = run.End + 1;
                }

                if (cursor < scoredItem.item.Description.size())
                {
                    const hstring matchSeg{ til::safe_slice_abs(scoredItem.item.Description, cursor, scoredItem.item.Description.size()) };
                    auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(matchSeg, false);
                    descriptionSegments.Append(textSegment);
                }

                auto input = scoredItem.item.Input;
                _appendItem(segments, descriptionSegments, input);
            }
        }

        if (!scoredItems.empty() && ListBox().SelectedIndex() == -1)
        {
            ListBox().SelectedIndex(0);
        }

        if (ListBox().Items().Size() == 0 && _autoCompleteMode)
        {
            _close();
            return;
        }

        NoItemsPlaceholder().Visibility(ListBox().Items().Size() == 0 ? Visibility::Visible : Visibility::Collapsed);
    }

    void SnippetSearchControl::_TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
    }

    void SnippetSearchControl::_close()
    {
        ListBox().Items().Clear();
        _ClosedHandlers(*this, RoutedEventArgs{});
    }

    void SnippetSearchControl::_TextBoxKeyDown(const Windows::Foundation::IInspectable& /*sender*/, const Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == Windows::System::VirtualKey::Escape)
        {
            _close();
            e.Handled(true);
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::Enter)
        {
            if (const auto selectedItem = ListBox().SelectedItem())
            {
                if (const auto listBoxItem = selectedItem.try_as<Controls::ListViewItem>())
                {
                    if (const auto fuzzyMatch = listBoxItem.DataContext().try_as<hstring>())
                    {
                        _close();
                        _OnReturnHandlers(*this, fuzzyMatch.value());
                        e.Handled(true);
                    }
                }
            }
        }
    }

    winrt::hstring ToLowerFold(winrt::hstring const& s)
    {
        UErrorCode ec = U_ZERO_ERROR;
        auto src = reinterpret_cast<const UChar*>(s.c_str());
        int32_t srcLen = static_cast<int32_t>(s.size());

        int32_t needed = u_strToLower(nullptr, 0, src, srcLen, nullptr, &ec);
        if (ec != U_BUFFER_OVERFLOW_ERROR)
        {
            return s;
        }
        ec = U_ZERO_ERROR;

        std::u16string out(needed, u'\0');
        u_strToLower(reinterpret_cast<UChar*>(out.data()), needed, src, srcLen, nullptr, &ec);
        if (U_FAILURE(ec))
        {
            return s;
        }

        return winrt::hstring{ reinterpret_cast<const wchar_t*>(out.c_str()), static_cast<uint32_t>(out.size()) };
    }

    void SnippetSearchControl::SetSnippets(Windows::Foundation::Collections::IVector<Control::SnippetSearchItem> items)
    {
        _items.assign(items.begin(), items.end());
        _lowerInputs.clear();
        for (auto item : items)
        {
            auto lower = ToLowerFold(item.Input);
            _lowerInputs.emplace_back(lower);
        }

        std::sort(_lowerInputs.begin(), _lowerInputs.end(), [](auto const& a, auto const& b) {
            return std::wstring_view{ a } < std::wstring_view{ b };
        });
    }

    bool SnippetSearchControl::HasPrefixMatch(const hstring prefix)
    {
        if (!_autoCompleteEnabled)
        {
            return false;
        }

        if (_commitFlag)
        {
            _commitFlag = false;
            return false;
        }

        if (prefix.size() < 2)
        {
            return false;
        }

        auto lowerPrefix = ToLowerFold(prefix);

        auto it = std::lower_bound(_lowerInputs.begin(), _lowerInputs.end(), lowerPrefix, [](hstring const& el, std::wstring_view key) {
            return std::wstring_view{ el.c_str(), el.size() } < key;
        });

        if (it == _lowerInputs.end())
        {
            return false;
        }

        return std::wstring_view{it->c_str(), it->size() }.starts_with(lowerPrefix);
    }

    void SnippetSearchControl::Show(
            Windows::Foundation::Collections::IVector<Control::SnippetSearchItem> /*snippets*/,
            Microsoft::Terminal::Control::TermControl const& termControl,
            Windows::Foundation::Point anchor,
            Windows::Foundation::Size space,
            winrt::hstring currentWord,
            float prefixWidth,
            int32_t cursorX,
            bool autoCompleteMode)
    {
        _autoCompleteMode = autoCompleteMode;
        _cursorX = cursorX - static_cast<int32_t>(currentWord.size());
        _currentWord = currentWord;
        _termControl = termControl;
        _anchor = anchor;
        _space = space;
        _prefixWidth = prefixWidth;

        const auto proposedX = gsl::narrow_cast<int>(anchor.X - prefixWidth);
        const auto maxX = gsl::narrow_cast<int>(space.Width - ActualWidth());
        const auto clampedX = std::clamp(proposedX, 0, maxX);
        Margin(Windows::UI::Xaml::ThicknessHelper::FromLengths(clampedX, 0, 0, 0));

        _performFuzzySearch();
        Visibility(Visibility::Visible);
        _recalculateTopMargin();
    }

    bool SnippetSearchControl::HandleKeyPress(WORD vkey, WORD /*scanCode*/, Core::ControlKeyStates modifiers, bool keyDown)
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
                auto castedItem = selectedItem.try_as<Controls::ListViewItem>();
                if (castedItem)
                {
                    auto input = castedItem.DataContext().try_as<hstring>();
                    if (input)
                    {
                        auto backspaces = std::wstring(_currentWord.size(), L'\x7f');

                        auto text = winrt::hstring{ fmt::format(FMT_COMPILE(L"{}{}"), backspaces, std::wstring_view{ input->c_str() }) };

                        const auto shiftPressed = WI_IsFlagSet(modifiers.Value, SHIFT_PRESSED);
                        if (shiftPressed)
                        {
                            _termControl.SendInput(backspaces);
                        }
                        else
                        {
                            _termControl.SendInput(text);
                            _commitFlag = true;
                        }
                    }
                }
            }

            _close();
            return true;
        }
        default:
            return false;
        }
    }

    void SnippetSearchControl::SetCurrentWord(const winrt::hstring& value, int32_t cursorX)
    {
        if (cursorX < _cursorX)
        {
            _close();
            return;
        }

        auto betweenCursors = til::safe_slice_abs(value, _cursorX, cursorX);
        _currentWord = betweenCursors;
        _performFuzzySearch();
    }

    void SnippetSearchControl::ToggleAutoComplete()
    {
        _autoCompleteEnabled = !_autoCompleteEnabled;
    }

    bool SnippetSearchControl::ContainsFocus()
    {
        auto focusedElement = Input::FocusManager::GetFocusedElement(this->XamlRoot());
        if (_focusableElements.count(focusedElement) > 0)
        {
            return true;
        }

        return false;
    }

    void SnippetSearchControl::_recalculateTopMargin()
    {
        const auto preferredHeight = 500.0;
        const auto edgeInset = 12.0;
        const auto downwardOffset = 30.0; // matches the +20+10 used when opening downward
        const auto upwardOffset = 10.0; // matches the -10 used when opening upward

        const auto spaceAbove = std::max(0.0, _anchor.Y - upwardOffset - edgeInset);
        const auto spaceBelow = std::max(0.0, _space.Height - _anchor.Y - downwardOffset - edgeInset);

        bool openUpward;
        double maxAvailable;

        if (spaceBelow >= preferredHeight)
        {
            openUpward = false;
            maxAvailable = preferredHeight;
        }
        else if (spaceAbove >= preferredHeight)
        {
            openUpward = true;
            maxAvailable = preferredHeight;
        }
        else
        {
            // Neither side has enough room for the preferred height. Open on
            // whichever side has more space and shrink the control to fit.
            openUpward = spaceAbove > spaceBelow;
            maxAvailable = openUpward ? spaceAbove : spaceBelow;
        }

        // Cap both the outer UserControl and the inner RootGrid so the
        // rendered control never exceeds the available space.
        MaxHeight(maxAvailable);
        RootGrid().MaxHeight(maxAvailable);

        _setDirection(openUpward);
    }

    void SnippetSearchControl::_setDirection(bool openUpward)
    {
        const float edgeInset = 12.0f;
        const float availableWidth = std::max(0.0f, static_cast<float>(_space.Width) - 2 * edgeInset);

        MaxWidth(availableWidth);

        RootGrid().Measure({
            availableWidth,
            static_cast<float>(ActualHeight()),
        });

        auto currentMargin = Margin();

        const auto controlWidth = ActualWidth();
        // Use MaxHeight (set in _recalculateTopMargin) instead of ActualHeight
        // for positioning. ActualHeight is stale here because layout hasn't
        // happened yet, and RootGrid's only row is Height="*", which has no
        // intrinsic desired size, so Measure reports zero. MaxHeight is the
        // upper bound of what will actually render, so positioning against it
        // keeps the control inside the viewport in the worst case.
        const auto controlHeight = MaxHeight();

        const auto proposedX = static_cast<float>(_anchor.X - _prefixWidth - 5.0f);
        const auto maxX = std::max<float>(edgeInset, static_cast<float>(_space.Width) - static_cast<float>(controlWidth) - edgeInset);
        const auto clampedX = std::clamp(proposedX, edgeInset, maxX);
        currentMargin.Left = clampedX;

        double top;
        if (openUpward)
        {
            top = _anchor.Y - controlHeight - 10;
        }
        else
        {
            top = _anchor.Y + 20 + 10;
        }

        // Clamp so the control stays within the terminal viewport even when
        // there isn't enough room in the chosen direction.
        const auto maxTop = std::max<double>(edgeInset, _space.Height - controlHeight - edgeInset);
        top = std::clamp(top, static_cast<double>(edgeInset), maxTop);

        currentMargin.Top = top;
        Margin(currentMargin);
    }
}
