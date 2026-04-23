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
            _applySearchBoxForeground();
        }
    }

    // The default TextBox template swaps Foreground to TextControlForegroundFocused /
    // TextControlForegroundPointerOver via visual states, which otherwise override the
    // Foreground binding we set in XAML. Mirror TextColor into those resource slots so the
    // user-typed text always matches the list item text color.
    void SnippetSearchControl::_applySearchBoxForeground()
    {
        const auto brush = TextColor();
        if (!brush)
        {
            return;
        }

        const auto tb = SearchBox();
        if (!tb)
        {
            return;
        }
        auto resources = tb.Resources();
        resources.Insert(winrt::box_value(L"TextControlForeground"), brush);
        resources.Insert(winrt::box_value(L"TextControlForegroundPointerOver"), brush);
        resources.Insert(winrt::box_value(L"TextControlForegroundFocused"), brush);
        resources.Insert(winrt::box_value(L"TextControlForegroundDisabled"), brush);
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
        _focusableElements.insert(SearchBox());
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
        if (_autoCompleteMode && _searchPattern.size() < 1)
        {
            _close();
            return;
        }

        //if (_autoCompleteMode && _searchPattern.back() == L' ')
        //{
        //    _close();
        //    return;
        //}

        const size_t nonSpace = std::count_if(_searchPattern.begin(), _searchPattern.end(), [](wchar_t ch){ return ch != L' '; });
        int minScore = _autoCompleteMode ? static_cast<int>(nonSpace) * 15 : 0;

        if (_searchPattern.empty())
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
        auto patternStr = _autoCompleteMode ? L"" + _searchPattern : _searchPattern;
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
        if (_suppressSearchBoxChange)
        {
            return;
        }
        _searchPattern = SearchBox().Text();
        _performFuzzySearch();
    }

    void SnippetSearchControl::_close()
    {
        ListBox().Items().Clear();
        {
            _suppressSearchBoxChange = true;
            auto reset = wil::scope_exit([this] { _suppressSearchBoxChange = false; });
            SearchBox().Text(L"");
        }
        _searchPattern = L"";
        if (_termControl)
        {
            _termControl.SetSnippetSearchSwapChainOffset(0.0f);
            _termControl.Focus(Windows::UI::Xaml::FocusState::Programmatic);
        }
        _ClosedHandlers(*this, RoutedEventArgs{});
    }

    void SnippetSearchControl::_TextBoxKeyDown(const Windows::Foundation::IInspectable& /*sender*/, const Input::KeyRoutedEventArgs& e)
    {
        const auto vkey = gsl::narrow_cast<WORD>(e.OriginalKey());
        if (vkey == VK_ESCAPE)
        {
            _close();
            e.Handled(true);
            return;
        }

        switch (vkey)
        {
        case VK_RETURN:
        case VK_TAB:
        case VK_UP:
        case VK_DOWN:
        {
            const auto window = CoreWindow::GetForCurrentThread();
            const auto shiftState = window.GetKeyState(Windows::System::VirtualKey::Shift);
            DWORD mods = 0;
            WI_SetFlagIf(mods, SHIFT_PRESSED, WI_IsFlagSet(shiftState, CoreVirtualKeyStates::Down));
            if (HandleKeyPress(vkey, 0, Core::ControlKeyStates{ mods }, true))
            {
                e.Handled(true);
            }
            break;
        }
        default:
            break;
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
            bool autoCompleteMode,
            float characterHeight,
            float swapChainOffset)
    {
        _autoCompleteMode = autoCompleteMode;
        _cursorX = cursorX - static_cast<int32_t>(currentWord.size());
        _currentWord = currentWord;
        _searchPattern = currentWord;
        _termControl = termControl;
        _anchor = anchor;
        _space = space;
        _prefixWidth = prefixWidth;
        _characterHeight = characterHeight;
        _swapChainOffset = swapChainOffset;

        // In explicit mode, the SearchBox drives the query and takes focus. In
        // auto-complete mode, the terminal keeps focus and SetCurrentWord drives the
        // query, so hide the SearchBox to avoid focus ambiguity.
        if (autoCompleteMode)
        {
            SearchBoxBorder().Visibility(Visibility::Collapsed);
        }
        else
        {
            SearchBoxBorder().Visibility(Visibility::Visible);
            _suppressSearchBoxChange = true;
            auto reset = wil::scope_exit([this] { _suppressSearchBoxChange = false; });
            SearchBox().Text(currentWord);
            SearchBox().SelectionStart(currentWord.size());
        }

        _performFuzzySearch();
        Visibility(Visibility::Visible);
        _recalculateTopMargin();

        if (!autoCompleteMode)
        {
            SearchBox().Focus(Windows::UI::Xaml::FocusState::Programmatic);
        }
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
        _searchPattern = _currentWord;
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
        _recalculateHorizontalPlacement();

        this->VerticalAlignment(winrt::Windows::UI::Xaml::VerticalAlignment::Top);
        auto currentMargin = Margin();
        currentMargin.Top = (_anchor.Y + _characterHeight + 5);
        currentMargin.Bottom = 0;
        Margin(currentMargin);
    }

    void SnippetSearchControl::_recalculateHorizontalPlacement()
    {
        const auto availableWidth = std::max(0.0f, gsl::narrow_cast<float>(_space.Width));

        Width(availableWidth);

        auto m = Margin();
        m.Left = 0.0;
        Margin(m);
    }
}
