// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "FontSizeChangedArgs.g.h"
#include "TitleChangedEventArgs.g.h"
#include "ContextMenuRequestedEventArgs.g.h"
#include "PasteFromClipboardEventArgs.g.h"
#include "OpenHyperlinkEventArgs.g.h"
#include "NoticeEventArgs.g.h"
#include "ScrollPositionChangedArgs.g.h"
#include "RendererWarningArgs.g.h"
#include "TransparencyChangedEventArgs.g.h"
#include "ShowWindowArgs.g.h"
#include "UpdateSelectionMarkersEventArgs.g.h"
#include "ExitVimModeEventArgs.g.h"
#include "VimTextChangedEventArgs.g.h"
#include "ShowFuzzySearchEventArgs.g.h"
#include "StartVimSearchEventArgs.g.h"
#include "ToggleRowNumbersEventArgs.g.h"
#include "CompletionsChangedEventArgs.g.h"
#include "KeySentEventArgs.g.h"
#include "CharSentEventArgs.g.h"
#include "StringSentEventArgs.g.h"
#include "SearchMissingCommandEventArgs.g.h"
#include "WindowSizeChangedEventArgs.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{

    struct FontSizeChangedArgs : public FontSizeChangedArgsT<FontSizeChangedArgs>
    {
    public:
        FontSizeChangedArgs(int32_t width,
                            int32_t height) :
            Width(width),
            Height(height)
        {
        }

        til::property<int32_t> Width;
        til::property<int32_t> Height;
    };

    struct TitleChangedEventArgs : public TitleChangedEventArgsT<TitleChangedEventArgs>
    {
    public:
        TitleChangedEventArgs(hstring title) :
            _Title(title) {}

        WINRT_PROPERTY(hstring, Title);
    };

    struct ContextMenuRequestedEventArgs : public ContextMenuRequestedEventArgsT<ContextMenuRequestedEventArgs>
    {
    public:
        ContextMenuRequestedEventArgs(winrt::Windows::Foundation::Point pos) :
            _Position(pos) {}

        WINRT_PROPERTY(winrt::Windows::Foundation::Point, Position);
    };

    struct PasteFromClipboardEventArgs : public PasteFromClipboardEventArgsT<PasteFromClipboardEventArgs>
    {
    public:
        PasteFromClipboardEventArgs(std::function<void(const hstring&)> clipboardDataHandler, bool bracketedPasteEnabled) :
            m_clipboardDataHandler(clipboardDataHandler),
            _BracketedPasteEnabled{ bracketedPasteEnabled } {}

        void HandleClipboardData(hstring value)
        {
            m_clipboardDataHandler(value);
        };

        WINRT_PROPERTY(bool, BracketedPasteEnabled, false);

    private:
        std::function<void(const hstring&)> m_clipboardDataHandler;
    };

    struct OpenHyperlinkEventArgs : public OpenHyperlinkEventArgsT<OpenHyperlinkEventArgs>
    {
    public:
        OpenHyperlinkEventArgs(hstring uri) :
            _uri(uri) {}

        hstring Uri() { return _uri; };

    private:
        hstring _uri;
    };

    struct NoticeEventArgs : public NoticeEventArgsT<NoticeEventArgs>
    {
    public:
        NoticeEventArgs(const NoticeLevel level, const hstring& message) :
            _level(level),
            _message(message) {}

        NoticeLevel Level() { return _level; };
        hstring Message() { return _message; };

    private:
        const NoticeLevel _level;
        const hstring _message;
    };

    struct ScrollPositionChangedArgs : public ScrollPositionChangedArgsT<ScrollPositionChangedArgs>
    {
    public:
        ScrollPositionChangedArgs(const int viewTop,
                                  const int viewHeight,
                                  const int bufferSize) :
            _ViewTop(viewTop),
            _ViewHeight(viewHeight),
            _BufferSize(bufferSize)
        {
        }

        WINRT_PROPERTY(int32_t, ViewTop);
        WINRT_PROPERTY(int32_t, ViewHeight);
        WINRT_PROPERTY(int32_t, BufferSize);
    };

    struct RendererWarningArgs : public RendererWarningArgsT<RendererWarningArgs>
    {
    public:
        RendererWarningArgs(const HRESULT hr, winrt::hstring parameter) :
            _Result{ hr },
            _Parameter{ std::move(parameter) }
        {
        }

        WINRT_PROPERTY(HRESULT, Result);
        WINRT_PROPERTY(winrt::hstring, Parameter);
    };

    struct TransparencyChangedEventArgs : public TransparencyChangedEventArgsT<TransparencyChangedEventArgs>
    {
    public:
        TransparencyChangedEventArgs(const float opacity) :
            _Opacity(opacity)
        {
        }

        WINRT_PROPERTY(float, Opacity);
    };

    struct ShowWindowArgs : public ShowWindowArgsT<ShowWindowArgs>
    {
    public:
        ShowWindowArgs(const bool showOrHide) :
            _ShowOrHide(showOrHide)
        {
        }

        WINRT_PROPERTY(bool, ShowOrHide);
    };

    struct UpdateSelectionMarkersEventArgs : public UpdateSelectionMarkersEventArgsT<UpdateSelectionMarkersEventArgs>
    {
    public:
        UpdateSelectionMarkersEventArgs(const bool clearMarkers) :
            _ClearMarkers(clearMarkers)
        {
        }

        WINRT_PROPERTY(bool, ClearMarkers, false);
    };

    struct ExitVimModeEventArgs: public ExitVimModeEventArgsT<ExitVimModeEventArgs>
    {
    public:
        ExitVimModeEventArgs(const bool enable) :
            _Enable(enable)
        {
        }

        WINRT_PROPERTY(bool, Enable, false);
    };

    struct VimTextChangedEventArgs : public VimTextChangedEventArgsT<VimTextChangedEventArgs>
    {
    public:
        VimTextChangedEventArgs(const winrt::hstring text, const winrt::hstring searchString, const winrt::hstring mode) :
            _Text(text),
            _SearchString(searchString),
            _Mode(mode)
        {
        }

        WINRT_PROPERTY(winrt::hstring, Text, false);
        WINRT_PROPERTY(winrt::hstring, SearchString, false);
        WINRT_PROPERTY(winrt::hstring, Mode, false);
    };

    struct ShowFuzzySearchEventArgs : public ShowFuzzySearchEventArgsT<ShowFuzzySearchEventArgs>
    {
    public:
        ShowFuzzySearchEventArgs(const winrt::hstring searchString) :
            _SearchString(searchString)
        {
        }

        WINRT_PROPERTY(winrt::hstring, SearchString, L"");
    };

    struct StartVimSearchEventArgs : public StartVimSearchEventArgsT<StartVimSearchEventArgs>
    {
    public:
        StartVimSearchEventArgs(const bool isReverse) :
            _IsReverse(isReverse)
        {
        }

        WINRT_PROPERTY(bool, IsReverse, false);
    };

    struct ToggleRowNumbersEventArgs : public ToggleRowNumbersEventArgsT<ToggleRowNumbersEventArgs>
    {
    public:
        ToggleRowNumbersEventArgs(bool enable) :
            _Enable(enable)
        {
        }

        WINRT_PROPERTY(bool, Enable, false);
    };

    struct CompletionsChangedEventArgs : public CompletionsChangedEventArgsT<CompletionsChangedEventArgs>
    {
    public:
        CompletionsChangedEventArgs(const winrt::hstring menuJson, const unsigned int replaceLength) :
            _MenuJson(menuJson),
            _ReplacementLength(replaceLength)
        {
        }

        WINRT_PROPERTY(winrt::hstring, MenuJson, L"");
        WINRT_PROPERTY(uint32_t, ReplacementLength, 0);
    };

    struct KeySentEventArgs : public KeySentEventArgsT<KeySentEventArgs>
    {
    public:
        KeySentEventArgs(const WORD vkey, const WORD scanCode, const winrt::Microsoft::Terminal::Core::ControlKeyStates modifiers, const bool keyDown) :
            _VKey(vkey),
            _ScanCode(scanCode),
            _Modifiers(modifiers),
            _KeyDown(keyDown) {}

        WINRT_PROPERTY(WORD, VKey);
        WINRT_PROPERTY(WORD, ScanCode);
        WINRT_PROPERTY(winrt::Microsoft::Terminal::Core::ControlKeyStates, Modifiers);
        WINRT_PROPERTY(bool, KeyDown, false);
    };

    struct CharSentEventArgs : public CharSentEventArgsT<CharSentEventArgs>
    {
    public:
        CharSentEventArgs(const wchar_t character, const WORD scanCode, const winrt::Microsoft::Terminal::Core::ControlKeyStates modifiers) :
            _Character(character),
            _ScanCode(scanCode),
            _Modifiers(modifiers) {}

        WINRT_PROPERTY(wchar_t, Character);
        WINRT_PROPERTY(WORD, ScanCode);
        WINRT_PROPERTY(winrt::Microsoft::Terminal::Core::ControlKeyStates, Modifiers);
    };

    struct StringSentEventArgs : public StringSentEventArgsT<StringSentEventArgs>
    {
    public:
        StringSentEventArgs(const winrt::hstring& text) :
            _Text(text) {}

        WINRT_PROPERTY(winrt::hstring, Text);
    };

    struct SearchMissingCommandEventArgs : public SearchMissingCommandEventArgsT<SearchMissingCommandEventArgs>
    {
    public:
        SearchMissingCommandEventArgs(const winrt::hstring& missingCommand, const til::CoordType& bufferRow) :
            MissingCommand(missingCommand),
            BufferRow(bufferRow) {}

        til::property<winrt::hstring> MissingCommand;
        til::property<til::CoordType> BufferRow;
    };

    struct WindowSizeChangedEventArgs : public WindowSizeChangedEventArgsT<WindowSizeChangedEventArgs>
    {
    public:
        WindowSizeChangedEventArgs(int32_t width,
                                   int32_t height) :
            _Width(width),
            _Height(height)
        {
        }

        WINRT_PROPERTY(int32_t, Width);
        WINRT_PROPERTY(int32_t, Height);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(OpenHyperlinkEventArgs);
}
