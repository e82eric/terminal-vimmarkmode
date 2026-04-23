// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "FuzzySearchTextSegment.h"
#include "StreamingSuggestionsControl.h"
#include "StreamingSuggestionsControl.g.cpp"
#include "SuggestionSearchRow.g.cpp"

#ifdef NDEBUG
#include <execution>
#endif

using namespace winrt::Windows::UI::Xaml::Media;

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace std::chrono_literals;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    static constexpr auto StreamingSuggestionItemHeight = 40.0;

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

        _copyNotificationTimer = winrt::Windows::UI::Xaml::DispatcherTimer();
        _copyNotificationTimer.Interval(1s);
        _copyNotificationTimer.Tick({ this, &StreamingSuggestionsControl::_OnCopyNotificationTimerTick });

        _sizeChangedRevoker = ListBox().SizeChanged(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            if (Visibility() == Visibility::Visible)
            {
                this->_recalculateTopMargin();
            }
        });

        _initKeyBindings();
        _updateModeIndicator();
    }

    Controls::ListView StreamingSuggestionsControl::_activeListBox()
    {
        return _mode == StreamingSuggestionsMode::WordSplit ? SplitListBox() : ListBox();
    }

    void StreamingSuggestionsControl::_showSplitOverlay(bool show)
    {
        SplitOverlay().Visibility(show ? Visibility::Visible : Visibility::Collapsed);
        if (!show)
        {
            SplitSearchBox().Text(L"");
            SplitListBox().ItemsSource(nullptr);
            _splitItems.clear();
            SplitNoItemsPlaceholder().Visibility(Visibility::Collapsed);
        }
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
            _applySearchBoxForeground();
        }
    }

    // The default TextBox template swaps Foreground to TextControlForegroundFocused /
    // TextControlForegroundPointerOver via visual states, which otherwise override the
    // Foreground binding we set in XAML. Mirror TextColor into those resource slots so the
    // user-typed text always matches the list item text color.
    void StreamingSuggestionsControl::_applySearchBoxForeground()
    {
        const auto brush = TextColor();
        if (!brush)
        {
            return;
        }

        const auto apply = [&](const Windows::UI::Xaml::Controls::TextBox& tb) {
            if (!tb)
            {
                return;
            }
            auto resources = tb.Resources();
            resources.Insert(winrt::box_value(L"TextControlForeground"), brush);
            resources.Insert(winrt::box_value(L"TextControlForegroundPointerOver"), brush);
            resources.Insert(winrt::box_value(L"TextControlForegroundFocused"), brush);
            resources.Insert(winrt::box_value(L"TextControlForegroundDisabled"), brush);
        };

        apply(SearchBox());
        apply(SplitSearchBox());
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

    DependencyProperty StreamingSuggestionsControl::HighlightedTextColorProperty()
    {
        return _HighlightedTextColorProperty;
    }

    void StreamingSuggestionsControl::_selectFirstItem()
    {
        auto listBox = _activeListBox();
        if (listBox.Items().Size() > 0)
        {
            listBox.SelectedIndex(0);
        }
    }

    void StreamingSuggestionsControl::_close(bool scrollToCursor)
    {
        _searchBoxMode = false;
        _showSplitOverlay(false);
        SearchBox().Text(L"");
        ListBox().ItemsSource(nullptr);
        Visibility(Windows::UI::Xaml::Visibility::Collapsed);
        _termControl.SetStreamingSuggestionsSwapChainOffset(0.0f);
        _termControl.ClearHighlights(scrollToCursor);
        _termControl.Focus(Windows::UI::Xaml::FocusState::Programmatic);
    }

    void StreamingSuggestionsControl::Open(
        TermControl const& termControl,
        const winrt::hstring& needle,
        Windows::Foundation::Point anchor,
        Windows::Foundation::Size space,
        const winrt::hstring& currentWord,
        float prefixWidth,
        float characterHeight,
        float swapChainOffset)
    {
        _mode = StreamingSuggestionsMode::Normal;
        _showSplitOverlay(false);
        _characterHeight = characterHeight;
        _termControl = termControl;
        _currentWord = currentWord;
        _currentSearchTerm = currentWord;
        _prefixWidth = prefixWidth;
        _swapChainOffset = swapChainOffset;

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
        ListBox().ItemsSource(nullptr);
        ListBox().SelectedIndex(-1);
        NoItemsPlaceholder().Visibility(Visibility::Collapsed);

        _searchBoxMode = true;
        SearchBox().Text(currentWord);
        Visibility(Windows::UI::Xaml::Visibility::Visible);
        SearchBox().Focus(Windows::UI::Xaml::FocusState::Programmatic);
        // Move caret to end so the user can continue typing from where they left off
        SearchBox().SelectionStart(currentWord.size());

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
        auto selected = _activeListBox().SelectedItem();
        if (!selected)
        {
            return std::nullopt;
        }

        if (auto row = selected.try_as<Control::SuggestionSearchRow>())
        {
            return row.Item();
        }

        return std::nullopt;
    }

    static void _copyToClipboard(const UINT format, const void* src, const size_t bytes)
    {
        wil::unique_hglobal handle{ THROW_LAST_ERROR_IF_NULL(GlobalAlloc(GMEM_MOVEABLE, bytes)) };

        const auto locked = GlobalLock(handle.get());
        memcpy(locked, src, bytes);
        GlobalUnlock(handle.get());

        THROW_LAST_ERROR_IF_NULL(SetClipboardData(format, handle.get()));
        handle.release();
    }

    static wil::unique_close_clipboard_call _openClipboard(HWND hwnd)
    {
        bool success = false;

        // OpenClipboard may fail to acquire the internal lock --> retry.
        for (DWORD sleep = 10;; sleep *= 2)
        {
            if (OpenClipboard(hwnd))
            {
                success = true;
                break;
            }
            // 10 iterations
            if (sleep > 10000)
            {
                break;
            }
            Sleep(sleep);
        }

        return wil::unique_close_clipboard_call{ success };
    }

    static void copyToClipboard(wil::zwstring_view text)
    {
        const auto clipboard = _openClipboard(nullptr);
        if (!clipboard)
        {
            LOG_LAST_ERROR();
            return;
        }

        EmptyClipboard();

        if (!text.empty())
        {
            // As per: https://learn.microsoft.com/en-us/windows/win32/dataxchg/standard-clipboard-formats
            //   CF_UNICODETEXT: [...] A null character signals the end of the data.
            // --> We add +1 to the length. This works because .c_str() is null-terminated.
            _copyToClipboard(CF_UNICODETEXT, text.c_str(), (text.size() + 1) * sizeof(wchar_t));

        }
    }

    void StreamingSuggestionsControl::_OnCopyNotificationTimerTick(
        winrt::Windows::Foundation::IInspectable const&,
        winrt::Windows::Foundation::IInspectable const&)
    {
        _copyNotificationTimer.Stop();
        copyNotificationContainer().Visibility(winrt::Windows::UI::Xaml::Visibility::Collapsed);
    }

    void StreamingSuggestionsControl::_showCopyNotification(const hstring& text)
    {
        copyNotificationContainer().Visibility(Visibility::Visible);
        copyNotificationText().Text(L"Copied: " + text);

        _copyNotificationTimer.Stop();
        _copyNotificationTimer.Start();
    }

    bool StreamingSuggestionsControl::HandleKeyPress(WORD vkey, WORD /*scanCode*/, Core::ControlKeyStates modifiers, bool keyDown)
    {
        const auto itemCount = _activeListBox().Items().Size();
        const auto mods = modifiers.Value;

        // Allow the help toggle and mode toggle even with no results — UX aids,
        // not list-level actions.
        const bool isHelpKey = (vkey == VK_OEM_2) &&
                               WI_AreAllFlagsSet(mods, LEFT_CTRL_PRESSED | SHIFT_PRESSED);
        const bool isModeToggleKey = vkey == VK_TAB && mods == 0;
        if (itemCount == 0 && !isHelpKey && !isModeToggleKey && !_helpVisible)
        {
            return false;
        }

        for (const auto& b : _keyBindings)
        {
            if (b.vkey == vkey && (mods & b.requiredMods) == b.requiredMods)
            {
                return b.action(keyDown);
            }
        }
        return false;
    }

    bool StreamingSuggestionsControl::_applySelectedOrClose(bool keyDown)
    {
        if (!keyDown)
        {
            return true;
        }

        hstring combined = L"";
        auto backspaces = std::wstring(_currentWord.size(), L'\x7f');
        if (auto castedDc = _TryGetSelectedSuggestion())
        {
            combined = castedDc->Text;
        }

        std::wstring_view trimmed{ combined };
        while (!trimmed.empty() && trimmed.back() == L' ')
        {
            trimmed.remove_suffix(1);
        }

        auto text = winrt::hstring{ fmt::format(FMT_COMPILE(L"{}{}"), backspaces, trimmed) };
        _termControl.SendInput(text);
        _close(true);
        return true;
    }

    void StreamingSuggestionsControl::_toggleHelp()
    {
        _helpVisible = !_helpVisible;
        if (!_helpVisible)
        {
            helpOverlay().Visibility(Visibility::Collapsed);
            return;
        }

        helpEntriesPanel().Children().Clear();
        for (const auto& b : _keyBindings)
        {
            Controls::StackPanel row;
            row.Orientation(Controls::Orientation::Horizontal);

            Controls::TextBlock keyText;
            keyText.Text(hstring{ b.label });
            keyText.Width(140);
            keyText.Foreground(TextColor());
            keyText.FontFamily(Media::FontFamily{ L"Consolas" });

            Controls::TextBlock descText;
            descText.Text(hstring{ b.description });
            descText.Foreground(TextColor());

            row.Children().Append(keyText);
            row.Children().Append(descText);
            helpEntriesPanel().Children().Append(row);
        }
        helpOverlay().Visibility(Visibility::Visible);
    }

    void StreamingSuggestionsControl::_initKeyBindings()
    {
        using KB = _KeyBinding;

        // Entries are checked in order; first matching vkey + required-mod mask
        // wins. Put more-specific modifier combinations before less-specific.
        _keyBindings = {
            KB{
                LEFT_CTRL_PRESSED | SHIFT_PRESSED,
                VK_OEM_2,
                L"Ctrl+Shift+?",
                L"Show/hide this help",
                [this](bool keyDown) {
                    if (keyDown)
                    {
                        _toggleHelp();
                    }
                    return true;
                },
            },
            KB{
                LEFT_CTRL_PRESSED,
                'C',
                L"Ctrl+C",
                L"Copy selected item",
                [this](bool /*keyDown*/) {
                    if (auto castedDc = _TryGetSelectedSuggestion())
                    {
                        copyToClipboard(castedDc->Text.c_str());
                        _showCopyNotification(castedDc->Text);
                        return true;
                    }
                    return false;
                },
            },
            KB{
                LEFT_CTRL_PRESSED,
                'L',
                L"Ctrl+L",
                L"Expand to scrollback line matches",
                [this](bool keyDown) {
                    if (!keyDown)
                    {
                        return false;
                    }
                    {
                        std::lock_guard<std::mutex> lock(_batchesMutex);
                        _batches.clear();
                    }
                    if (auto castedDc = _TryGetSelectedSuggestion())
                    {
                        std::wstring needle = L"^.*";
                        needle += SearchBox().Text();
                        needle += L".*$";
                        auto op = _termControl.SuggestionScrollBackSearchAsync(
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
                        return true;
                    }
                    return false;
                },
            },
            KB{
                LEFT_CTRL_PRESSED,
                'B',
                L"Ctrl+B",
                L"Word-split mode",
                [this](bool keyDown) {
                    if (!keyDown)
                    {
                        return false;
                    }
                    auto castedDc = _TryGetSelectedSuggestion();
                    if (castedDc)
                    {
                        _mode = StreamingSuggestionsMode::WordSplit;
                        const auto lineNumber = castedDc->StartPos.Y;
                        auto results = _termControl.LineSearchAsync(lineNumber);
                        _allItemsLoaded = true;
                        _allItemsSearched = true;
                        _showSplitOverlay(true);

                        _splitItems.clear();
                        _splitItems.reserve(results.Size());
                        for (const auto& item : results)
                        {
                            _splitItems.emplace_back(item);
                        }
                        SplitSearchBox().Text(L"");
                        SplitSearchBox().Focus(Windows::UI::Xaml::FocusState::Programmatic);
                        SplitSearchBox().SelectionStart(0);
                        _populateSplitList(L"");

                        Visibility(Visibility::Visible);
                        _recalculateTopMargin();

                        InvalidateMeasure();
                        return true;
                    }
                    return false;
                },
            },
            KB{
                SHIFT_PRESSED,
                VK_RETURN,
                L"Shift+Enter",
                L"Select the row in the terminal",
                [this](bool keyDown) {
                    if (!keyDown)
                    {
                        return true;
                    }
                    if (auto castedDc = _TryGetSelectedSuggestion())
                    {
                        auto backspaces = std::wstring(_currentWord.size(), L'\x7f');
                        _termControl.SendInput(backspaces);
                        _termControl.SelectRow(castedDc->StartPos.Y, castedDc->StartPos.X);
                        _close(false);
                        return true;
                    }
                    return _applySelectedOrClose(keyDown);
                },
            },
            KB{
                0,
                VK_RETURN,
                L"Enter",
                L"Insert selected item",
                [this](bool keyDown) { return _applySelectedOrClose(keyDown); },
            },
            KB{
                0,
                VK_TAB,
                L"Tab",
                L"Toggle between fuzzy and contains search",
                [this](bool keyDown) {
                    if (!keyDown)
                    {
                        return true;
                    }
                    if (_mode == StreamingSuggestionsMode::WordSplit)
                    {
                        return false;
                    }
                    _useFuzzySearch = !_useFuzzySearch;
                    _updateModeIndicator();
                    _triggerSearch();
                    return true;
                },
            },
            KB{
                0,
                VK_PRIOR,
                L"PgUp",
                L"Scroll up one page",
                [this](bool keyDown) {
                    if (keyDown)
                    {
                        if (_helpVisible)
                        {
                            const auto sv = helpScrollViewer();
                            sv.ScrollToVerticalOffset(sv.VerticalOffset() - sv.ViewportHeight());
                        }
                        else
                        {
                            auto listBox = _activeListBox();
                            const auto pageSize = std::max(1, static_cast<int32_t>(listBox.ActualHeight() / StreamingSuggestionItemHeight));
                            const auto currentIndex = listBox.SelectedIndex();
                            _selectItem(std::max(0, currentIndex - pageSize));
                        }
                    }
                    return true;
                },
            },
            KB{
                0,
                VK_NEXT,
                L"PgDn",
                L"Scroll down one page",
                [this](bool keyDown) {
                    if (keyDown)
                    {
                        if (_helpVisible)
                        {
                            const auto sv = helpScrollViewer();
                            sv.ScrollToVerticalOffset(sv.VerticalOffset() + sv.ViewportHeight());
                        }
                        else
                        {
                            auto listBox = _activeListBox();
                            const auto pageSize = std::max(1, static_cast<int32_t>(listBox.ActualHeight() / StreamingSuggestionItemHeight));
                            const auto size = static_cast<int32_t>(listBox.Items().Size());
                            const auto currentIndex = listBox.SelectedIndex();
                            _selectItem(std::min(size - 1, currentIndex + pageSize));
                        }
                    }
                    return true;
                },
            },
            KB{
                0,
                VK_UP,
                L"Up",
                L"Previous item",
                [this](bool keyDown) {
                    if (keyDown)
                    {
                        const auto currentIndex = _activeListBox().SelectedIndex();
                        if (currentIndex > 0)
                        {
                            _selectItem(currentIndex - 1);
                        }
                    }
                    return true;
                },
            },
            KB{
                0,
                VK_DOWN,
                L"Down",
                L"Next item",
                [this](bool keyDown) {
                    if (keyDown)
                    {
                        auto listBox = _activeListBox();
                        const auto size = static_cast<int32_t>(listBox.Items().Size());
                        const auto currentIndex = listBox.SelectedIndex();
                        if (currentIndex < size - 1)
                        {
                            _selectItem(currentIndex + 1);
                        }
                    }
                    return true;
                },
            },
            KB{
                0,
                VK_ESCAPE,
                L"Esc",
                L"Close (or exit word-split mode)",
                [this](bool keyDown) {
                    if (!keyDown)
                    {
                        return true;
                    }
                    if (_helpVisible)
                    {
                        _toggleHelp();
                        return true;
                    }
                    if (_mode == StreamingSuggestionsMode::WordSplit)
                    {
                        _mode = StreamingSuggestionsMode::Normal;
                        _showSplitOverlay(false);
                        _triggerSearch();
                    }
                    else
                    {
                        _close(true);
                    }
                    return true;
                },
            },
        };
    }

    void StreamingSuggestionsControl::_triggerSearch()
    {
        std::wstring term;
        {
            std::lock_guard lock(_searchTermMutex);
            term = _currentSearchTerm;
        }

        const std::uint64_t myVersion = ++_searchVersion;

        if (_useFuzzySearch || term.empty())
        {
            _performFuzzySearch(term, myVersion);
        }
        else
        {
            _performContainsSearch(term, myVersion);
        }
    }

    void StreamingSuggestionsControl::_selectItem(int32_t index)
    {
        auto listBox = _activeListBox();
        const auto size = gsl::narrow_cast<int32_t>(listBox.Items().Size());
        if (index < 0 || index >= size)
        {
            return;
        }

        listBox.SelectedIndex(index);
        listBox.ScrollIntoView(listBox.SelectedItem());

        if (auto selectedItem = _TryGetSelectedSuggestion())
        {
            const auto clippedTopPixels = _swapChainOffset;
            const auto selectionScrolledToSpan = !_termControl.HighlightPointSpan(selectedItem.value().StartPos, selectedItem.value().EndPos, clippedTopPixels);

            if (selectionScrolledToSpan)
            {
                _termControl.SetStreamingSuggestionsSwapChainOffset(0.0f);
            }
            else
            {
                _termControl.SetStreamingSuggestionsSwapChainOffset(_swapChainOffset);
            }
        }
    }

    static Control::FuzzySearchTextLine BuildLine(hstring const& text,
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

    winrt::Windows::Foundation::IInspectable LazySuggestionRowVector::GetAt(uint32_t index)
    {
        if (index >= _sources.size())
        {
            throw winrt::hresult_out_of_bounds();
        }
        auto& cached = _cache[index];
        if (!cached)
        {
            const auto& src = _sources[index];
            auto line = BuildLine(src.item.Text, src.item.StartPos.Y, src.item.StartPos.X, src.runs);
            cached = winrt::make<SuggestionSearchRow>(line, src.item, _textColor, _highlightedTextColor);
        }
        return cached;
    }

    winrt::Windows::Foundation::Collections::IVectorView<winrt::Windows::Foundation::IInspectable> LazySuggestionRowVector::GetView()
    {
        return winrt::make<LazySuggestionRowVectorView>(get_strong());
    }

    bool LazySuggestionRowVector::IndexOf(winrt::Windows::Foundation::IInspectable const& value, uint32_t& index) const noexcept
    {
        for (uint32_t i = 0; i < _cache.size(); ++i)
        {
            if (_cache[i] && _cache[i] == value)
            {
                index = i;
                return true;
            }
        }
        index = 0;
        return false;
    }

    uint32_t LazySuggestionRowVector::GetMany(uint32_t startIndex, winrt::array_view<winrt::Windows::Foundation::IInspectable> items)
    {
        const auto total = static_cast<uint32_t>(_sources.size());
        if (startIndex >= total)
        {
            return 0;
        }
        const auto available = total - startIndex;
        const auto copied = std::min<uint32_t>(available, static_cast<uint32_t>(items.size()));
        for (uint32_t i = 0; i < copied; ++i)
        {
            items[i] = GetAt(startIndex + i);
        }
        return copied;
    }

    winrt::Windows::Foundation::Collections::IIterator<winrt::Windows::Foundation::IInspectable> LazySuggestionRowVector::First()
    {
        return winrt::make<LazySuggestionRowIterator>(get_strong());
    }

    winrt::Windows::Foundation::IInspectable LazySuggestionRowIterator::Current() const
    {
        if (_index >= _owner->Size())
        {
            throw winrt::hresult_out_of_bounds();
        }
        return _owner->GetAt(_index);
    }

    bool LazySuggestionRowIterator::HasCurrent() const noexcept
    {
        return _index < _owner->Size();
    }

    bool LazySuggestionRowIterator::MoveNext() noexcept
    {
        if (_index < _owner->Size())
        {
            ++_index;
        }
        return _index < _owner->Size();
    }

    uint32_t LazySuggestionRowIterator::GetMany(winrt::array_view<winrt::Windows::Foundation::IInspectable> items)
    {
        const auto copied = _owner->GetMany(_index, items);
        _index += copied;
        return copied;
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

        std::vector<Microsoft::Terminal::Control::SuggestionSearchItem> itemsSnapshot;
        std::vector<Microsoft::Terminal::Control::SuggestionBatch> batchesSnapshot;
        {
            std::lock_guard<std::mutex> lock(_batchesMutex);
            batchesSnapshot.assign(_batches.begin(), _batches.end());
        }
        for (const auto& batch : batchesSnapshot)
        {
            for (const auto& item : batch.Items())
            {
                itemsSnapshot.emplace_back(item);
            }
        }

        if (searchTerm.empty())
        {
            co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);
            if (version != _searchVersion)
            {
                co_return;
            }

            std::vector<SuggestionRowSource> sources;
            for (const auto& item : itemsSnapshot)
            {
                sources.push_back({ item, std::nullopt });
                if (sources.size() >= 5)
                {
                    break;
                }
            }
            const auto shown = static_cast<uint32_t>(sources.size());
            ListBox().ItemsSource(winrt::make<LazySuggestionRowVector>(std::move(sources), TextColor(), HighlightedTextColor()));
            ListBox().SelectedIndex(-1);

            Visibility(Visibility::Visible);
            _recalculateTopMargin();

            if (ListBox().SelectedIndex() == -1)
            {
                _selectItem(0);
            }

            NoItemsPlaceholder().Visibility(shown == 0 ? Visibility::Visible : Visibility::Collapsed);
            if (shown == 0)
            {
                _termControl.ClearHighlights(false);
            }

            InvalidateMeasure();

            co_return;
        }

        using clock = std::chrono::steady_clock;
        const auto t0 = clock::now();

        auto pattern = fzfcpp::matcher::ParsePatternWithTypes(searchTerm);

        const auto tParse = clock::now();

        std::vector<ScoredItem> scoredItems;

#ifdef NDEBUG
        std::vector<std::optional<ScoredItem>> slots(itemsSnapshot.size());
        std::transform(std::execution::par, itemsSnapshot.begin(), itemsSnapshot.end(), slots.begin(),
            [&pattern](const auto& item) -> std::optional<ScoredItem> {
                auto matchResult = fzfcpp::matcher::Match(item.Text, pattern);
                if (!matchResult)
                {
                    return std::nullopt;
                }
                return ScoredItem{ item, matchResult->Score, matchResult->Runs, item.Ordinal };
            });

        if (version != _searchVersion)
        {
            co_return;
        }

        scoredItems.reserve(slots.size());
        for (auto& slot : slots)
        {
            if (slot)
            {
                scoredItems.push_back(std::move(*slot));
            }
        }
#else
        for (const auto& item : itemsSnapshot)
        {
            if (version != _searchVersion)
            {
                co_return;
            }
            auto text = item.Text;
            auto matchResult = fzfcpp::matcher::Match(text, pattern);
            if (matchResult)
            {
                scoredItems.push_back({ item, matchResult->Score, matchResult->Runs, item.Ordinal });
            }
        }
#endif

        const auto tMatch = clock::now();
        const auto matchedCount = scoredItems.size();

        auto MaxResults = 100000;
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

        const auto tSort = clock::now();

        co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);

        const auto tForeground = clock::now();

        std::vector<SuggestionRowSource> sources;
        sources.reserve(scoredItems.size());
        for (auto& scoredItem : scoredItems)
        {
            sources.push_back({ scoredItem.item, std::move(scoredItem.runs) });
        }
        ListBox().ItemsSource(winrt::make<LazySuggestionRowVector>(std::move(sources), TextColor(), HighlightedTextColor()));
        ListBox().SelectedIndex(-1);

        const auto tAppend = clock::now();

        const auto us = [](auto a, auto b) {
            return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
        };
        OutputDebugStringW(fmt::format(
            FMT_COMPILE(L"[StreamingSuggestions] fuzzy total={}us parse={}us match={}us sort={}us fg_switch={}us append={}us items={} matched={} shown={}\n"),
            us(t0, tAppend),
            us(t0, tParse),
            us(tParse, tMatch),
            us(tMatch, tSort),
            us(tSort, tForeground),
            us(tForeground, tAppend),
            itemsSnapshot.size(),
            matchedCount,
            scoredItems.size()).c_str());

        _allItemsSearched = true;
        if (!scoredItems.empty())
        {
            _selectItem(0);
        }

        Visibility(Visibility::Visible);

        InvalidateMeasure();
        _recalculateTopMargin();

        NoItemsPlaceholder().Visibility(scoredItems.empty() ? Visibility::Visible : Visibility::Collapsed);
        if (scoredItems.empty())
        {
            _termControl.ClearHighlights(false);
        }
        co_return;
    }

    void StreamingSuggestionsControl::_SearchBoxTextChanged(
        Windows::Foundation::IInspectable const& /*sender*/,
        Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (!_searchBoxMode)
            return;

        auto text = SearchBox().Text();
        {
            std::lock_guard lock(_searchTermMutex);
            _currentSearchTerm = text;
        }
        if (_mode == StreamingSuggestionsMode::WordSplit)
        {
            return;
        }
        _triggerSearch();
    }

    void StreamingSuggestionsControl::_SplitSearchBoxTextChanged(
        Windows::Foundation::IInspectable const& /*sender*/,
        Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (_mode != StreamingSuggestionsMode::WordSplit)
        {
            return;
        }

        _populateSplitList(SplitSearchBox().Text().c_str());
    }

    void StreamingSuggestionsControl::_SearchBoxKeyDown(
        Windows::Foundation::IInspectable const& /*sender*/,
        Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        const auto key = gsl::narrow_cast<WORD>(e.OriginalKey());
        auto listBox = _activeListBox();
        const auto itemCount = listBox.Items().Size();
        auto mods = DWORD{ 0 };

        const auto window = CoreWindow::GetForCurrentThread();
        const auto ctrlState = window.GetKeyState(Windows::System::VirtualKey::Control);
        const auto shiftState = window.GetKeyState(Windows::System::VirtualKey::Shift);
        WI_SetFlagIf(mods, LEFT_CTRL_PRESSED, WI_IsFlagSet(ctrlState, CoreVirtualKeyStates::Down));
        WI_SetFlagIf(mods, SHIFT_PRESSED, WI_IsFlagSet(shiftState, CoreVirtualKeyStates::Down));

        const bool isHelpKey = key == VK_OEM_2 &&
                               WI_AreAllFlagsSet(mods, LEFT_CTRL_PRESSED | SHIFT_PRESSED);
        const bool isModeToggleKey = key == VK_TAB && mods == 0;
        if (itemCount == 0 && key != VK_ESCAPE && !isHelpKey && !isModeToggleKey && !_helpVisible)
        {
            return;
        }

        for (const auto& binding : _keyBindings)
        {
            if (binding.vkey == key && (mods & binding.requiredMods) == binding.requiredMods)
            {
                e.Handled(binding.action(true));
                return;
            }
        }
    }

    winrt::Windows::Foundation::IAsyncAction StreamingSuggestionsControl::_performContainsSearch(std::wstring searchTerm, uint64_t version)
    {
        struct MatchedItem
        {
            Microsoft::Terminal::Control::SuggestionSearchItem item;
            std::optional<std::vector<fzfcpp::matcher::TextRun>> runs;
        };

        co_await winrt::resume_background();

        std::vector<Microsoft::Terminal::Control::SuggestionBatch> batchesSnapshot;
        {
            std::lock_guard<std::mutex> lock(_batchesMutex);
            batchesSnapshot.assign(_batches.begin(), _batches.end());
        }

        using clock = std::chrono::steady_clock;
        const auto t0 = clock::now();

        auto pattern = fzfcpp::matcher::ParsePatternContainsOnly(searchTerm);

        const auto tParse = clock::now();

        constexpr auto MaxResults = 5;
        std::vector<MatchedItem> matchedItems;
        size_t itemsScanned = 0;

        for (const auto& batch : batchesSnapshot)
        {
            if (version != _searchVersion)
            {
                co_return;
            }
            for (const auto& item : batch.Items())
            {
                ++itemsScanned;
                if (auto matchResult = fzfcpp::matcher::Match(item.Text, pattern))
                {
                    matchedItems.push_back({ item, matchResult->Runs });
                    if (matchedItems.size() >= MaxResults)
                        goto done;
                }
            }
        }
done:
        // Items are already in ordinal-ascending order (scrollback scanned top→bottom),
        // so no sort is needed.
        const auto tMatch = clock::now();

        co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);

        const auto tForeground = clock::now();

        std::vector<SuggestionRowSource> sources;
        sources.reserve(matchedItems.size());
        for (auto& matched : matchedItems)
        {
            sources.push_back({ matched.item, std::move(matched.runs) });
        }
        ListBox().ItemsSource(winrt::make<LazySuggestionRowVector>(std::move(sources), TextColor(), HighlightedTextColor()));
        ListBox().SelectedIndex(-1);

        const auto tAppend = clock::now();

        const auto us = [](auto a, auto b) {
            return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
        };
        OutputDebugStringW(fmt::format(
            FMT_COMPILE(L"[StreamingSuggestions] contains total={}us parse={}us match={}us fg_switch={}us append={}us scanned={} shown={}\n"),
            us(t0, tAppend),
            us(t0, tParse),
            us(tParse, tMatch),
            us(tMatch, tForeground),
            us(tForeground, tAppend),
            itemsScanned,
            matchedItems.size()).c_str());

        _allItemsSearched = true;
        if (!matchedItems.empty())
        {
            _selectItem(0);
        }

        Visibility(Visibility::Visible);

        InvalidateMeasure();
        _recalculateTopMargin();

        NoItemsPlaceholder().Visibility(matchedItems.empty() ? Visibility::Visible : Visibility::Collapsed);
        if (matchedItems.empty())
        {
            _termControl.ClearHighlights(false);
        }
        co_return;
    }

    void StreamingSuggestionsControl::_populateSplitList(std::wstring searchTerm)
    {
        struct ScoredItem
        {
            Microsoft::Terminal::Control::SuggestionSearchItem item;
            int32_t score;
            std::optional<std::vector<fzfcpp::matcher::TextRun>> runs;
            int32_t ordinal;
        };

        std::vector<SuggestionRowSource> sources;

        if (searchTerm.empty())
        {
            sources.reserve(_splitItems.size());
            for (const auto& item : _splitItems)
            {
                sources.push_back({ item, std::nullopt });
            }
        }
        else
        {
            auto pattern = fzfcpp::matcher::ParsePatternWithTypes(searchTerm);
            std::vector<ScoredItem> scoredItems;
            scoredItems.reserve(_splitItems.size());

            for (const auto& item : _splitItems)
            {
                auto text = item.Text;
                if (auto matchResult = fzfcpp::matcher::Match(text, pattern))
                {
                    scoredItems.push_back({ item, matchResult->Score, matchResult->Runs, item.Ordinal });
                }
            }

            std::ranges::sort(scoredItems, [](const ScoredItem& a, const ScoredItem& b) {
                if (a.score == b.score)
                {
                    return a.ordinal < b.ordinal;
                }
                return a.score > b.score;
            });

            sources.reserve(scoredItems.size());
            for (auto& scoredItem : scoredItems)
            {
                sources.push_back({ scoredItem.item, std::move(scoredItem.runs) });
            }
        }

        const auto shown = static_cast<uint32_t>(sources.size());
        SplitListBox().ItemsSource(winrt::make<LazySuggestionRowVector>(std::move(sources), TextColor(), HighlightedTextColor()));
        SplitListBox().SelectedIndex(-1);

        SplitNoItemsPlaceholder().Visibility(shown == 0 ? Visibility::Visible : Visibility::Collapsed);
        if (shown > 0)
        {
            _selectItem(0);
        }
        else
        {
            _termControl.ClearHighlights(false);
        }
    }

    void StreamingSuggestionsControl::_recalculateTopMargin()
    {
        _recalculateHorizontalPlacement();

        const auto autoLength = Windows::UI::Xaml::GridLengthHelper::FromValueAndType(1.0, Windows::UI::Xaml::GridUnitType::Auto);
        const auto starLength = Windows::UI::Xaml::GridLengthHelper::FromValueAndType(1.0, Windows::UI::Xaml::GridUnitType::Star);
        InnerRow0().Height(autoLength);
        InnerRow1().Height(starLength);
        SplitInnerRow0().Height(autoLength);
        SplitInnerRow1().Height(starLength);
        Controls::Grid::SetRow(SearchBoxBorder(), 0);
        Controls::Grid::SetRow(ListBox(), 1);
        Controls::Grid::SetRow(NoItemsPlaceholder(), 1);
        Controls::Grid::SetRow(SplitSearchBoxBorder(), 0);
        Controls::Grid::SetRow(SplitListBox(), 1);
        Controls::Grid::SetRow(SplitNoItemsPlaceholder(), 1);
        SearchBoxBorder().BorderThickness(Windows::UI::Xaml::ThicknessHelper::FromLengths(0, 0, 0, 1));
        SplitSearchBoxBorder().BorderThickness(Windows::UI::Xaml::ThicknessHelper::FromLengths(0, 0, 0, 1));

        auto currentMargin = Margin();
        currentMargin.Top = (_anchor.Y + _characterHeight + 5);
        Margin(currentMargin);
    }

    void StreamingSuggestionsControl::_recalculateHorizontalPlacement()
    {
        const auto availableWidth = std::max(0.0f, gsl::narrow_cast<float>(_space.Width));

        Width(availableWidth);

        auto m = Margin();
        m.Left = 0.0;
        Margin(m);
    }

    void StreamingSuggestionsControl::_updateModeIndicator()
    {
        // Guard against being called before XAML parts are wired up
        // (UseFuzzySearch setter may fire during initialization).
        if (!ModeIndicatorText())
        {
            return;
        }
        ModeIndicatorText().Text(_useFuzzySearch ? L"Fuzzy" : L"Contains");
    }
}
