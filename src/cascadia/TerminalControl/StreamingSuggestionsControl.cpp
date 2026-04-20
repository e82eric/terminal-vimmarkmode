// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "FuzzySearchTextSegment.h"
#include "StreamingSuggestionsControl.h"
#include "StreamingSuggestionsControl.g.cpp"

using namespace winrt::Windows::UI::Xaml::Media;

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace std::chrono_literals;

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

        if (Visibility() == Visibility::Visible && _autoCompleteMode && betweenCursors.size() < 2)
        {
            _close(false);
            return;
        }

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
        int32_t cursorX,
        float characterHeight)
    {
        _autoCompleteMode = false;

        _scrollToSpan = false;
        _mode = StreamingSuggestionsMode::Normal;
        _cursorX = cursorX - static_cast<int32_t>(currentWord.size());
        _characterHeight = characterHeight;
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

    void StreamingSuggestionsControl::ToggleAutoComplete()
    {
        _autoCompleteEnabled = !_autoCompleteEnabled;
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
        const auto itemCount = ListBox().Items().Size();
        const auto mods = modifiers.Value;

        // Allow the help toggle even with no results — it's a UX aid, not a
        // list-level action.
        const bool isHelpKey = (vkey == VK_OEM_2) &&
                               WI_AreAllFlagsSet(mods, LEFT_CTRL_PRESSED | SHIFT_PRESSED);
        if (itemCount == 0 && !isHelpKey && !_helpVisible)
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
                        needle += castedDc->Text;
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
                    _mode = StreamingSuggestionsMode::WordSplit;
                    {
                        std::lock_guard<std::mutex> lock(_batchesMutex);
                        _batches.clear();
                    }
                    if (auto castedDc = _TryGetSelectedSuggestion())
                    {
                        std::wstring needle = L"[^\\s]{5,}";
                        auto op = _termControl.LineSearchAsync(
                            needle,
                            Microsoft::Terminal::Control::SuggestionBatchHandler{
                                [weakThis = get_weak()](Microsoft::Terminal::Control::SuggestionBatch const& batch) {
                                    if (auto self = weakThis.get())
                                    {
                                        std::lock_guard<std::mutex> lock(self->_batchesMutex);
                                        self->_batches.push_back(batch);
                                        self->_triggerSearch();
                                    }
                                } },
                            castedDc->StartPos.Y);

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
                VK_RETURN,
                L"Ctrl+Enter",
                L"Toggle scroll-to-span on selection",
                [this](bool keyDown) {
                    if (!keyDown)
                    {
                        return true;
                    }
                    if (_TryGetSelectedSuggestion())
                    {
                        _scrollToSpan = !_scrollToSpan;
                        _selectItem(ListBox().SelectedIndex());
                        return true;
                    }
                    return _applySelectedOrClose(keyDown);
                },
            },
            KB{
                LEFT_CTRL_PRESSED,
                VK_TAB,
                L"Ctrl+Tab",
                L"Toggle scroll-to-span on selection",
                [this](bool keyDown) {
                    if (!keyDown)
                    {
                        return true;
                    }
                    if (_TryGetSelectedSuggestion())
                    {
                        _scrollToSpan = !_scrollToSpan;
                        _selectItem(ListBox().SelectedIndex());
                        return true;
                    }
                    return _applySelectedOrClose(keyDown);
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
                SHIFT_PRESSED,
                VK_TAB,
                L"Shift+Tab",
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
                L"Insert selected item",
                [this](bool keyDown) { return _applySelectedOrClose(keyDown); },
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
                            const auto pageSize = std::max(1, static_cast<int32_t>(ListBox().ActualHeight() / 40.0));
                            const auto currentIndex = ListBox().SelectedIndex();
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
                            const auto pageSize = std::max(1, static_cast<int32_t>(ListBox().ActualHeight() / 40.0));
                            const auto size = static_cast<int32_t>(ListBox().Items().Size());
                            const auto currentIndex = ListBox().SelectedIndex();
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
                        const auto currentIndex = ListBox().SelectedIndex();
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
                        const auto size = static_cast<int32_t>(ListBox().Items().Size());
                        const auto currentIndex = ListBox().SelectedIndex();
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

        if (_useFuzzySearch || term.empty() || _mode == StreamingSuggestionsMode::WordSplit)
            _performFuzzySearch(term, myVersion);
        else
            _performContainsSearch(term, myVersion);
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

        //if (_mode != StreamingSuggestionsMode::Normal)
        //{
        //    return;
        //}

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

        std::vector<Microsoft::Terminal::Control::SuggestionBatch> batchesSnapshot;
        {
            std::lock_guard<std::mutex> lock(_batchesMutex);
            batchesSnapshot.assign(_batches.begin(), _batches.end());
        }

        if (searchTerm.empty() || _mode == StreamingSuggestionsMode::WordSplit)
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

                    if (ListBox().Items().Size() >= 10)
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

        auto pattern = fzfcpp::matcher::ParsePatternContainsOnly(searchTerm);
        constexpr auto MaxResults = 1000;
        std::vector<MatchedItem> matchedItems;

        for (const auto& batch : batchesSnapshot)
        {
            if (version != _searchVersion)
            {
                co_return;
            }
            for (const auto& item : batch.Items())
            {
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
        co_await winrt::resume_foreground(Dispatcher(), Windows::UI::Core::CoreDispatcherPriority::Normal);

        ListBox().Items().Clear();
        for (const auto& matched : matchedItems)
        {
            auto line = _BuildLine(matched.item.Text, matched.item.StartPos.Y, matched.item.StartPos.X, matched.runs);
            auto lbi = _makeListViewItem(line, box_value(matched.item));
            ListBox().Items().Append(lbi);
        }

        _allItemsSearched = true;
        if (!matchedItems.empty() && ListBox().SelectedIndex() == -1)
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

    void StreamingSuggestionsControl::_recalculateHorizontalPlacement()
    {
        const float availableWidth = gsl::narrow_cast<float>(_space.Width);

        RootGrid().Measure({ availableWidth, std::numeric_limits<float>::infinity() });

        const float desiredWidth = RootGrid().DesiredSize().Width;

        const float minWidth = 400.0f;
        const float width = std::clamp(desiredWidth, minWidth, availableWidth);

        Width(width);

        float left = gsl::narrow_cast<float>(_anchor.X - _prefixWidth - 5.0f);
        left = std::clamp(left, 0.0f, availableWidth - width);

        auto m = Margin();
        m.Left = left;
        Margin(m);
    }

    void StreamingSuggestionsControl::_setDirection(bool openUpward)
    {
        _recalculateHorizontalPlacement();

        auto currentMargin = Margin();
        const auto controlHeight = ActualHeight();

        if (openUpward)
        {
            currentMargin.Top = (_anchor.Y - controlHeight);
        }
        else
        {
            currentMargin.Top = (_anchor.Y + _characterHeight + 5);
        }
        Margin(currentMargin);
    }
}
