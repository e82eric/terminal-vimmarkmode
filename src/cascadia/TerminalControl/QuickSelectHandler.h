#pragma once
#include "../../renderer/base/Renderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../../cascadia/TerminalCore/lib/QuickSelectAlphabet.h"

class Search;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ControlCore;
}

namespace Microsoft::Console::Render
{
    class Renderer;
}

class VimModeProxy;

class QuickSelectHandler
{
private:
    std::shared_ptr<Microsoft::Terminal::Core::Terminal> _terminal;
    bool _copyMode = false;
    std::shared_ptr<VimModeProxy> _vimProxy;
    std::shared_ptr<QuickSelectAlphabet> _quickSelectAlphabet;
    void _exitQuickSelectMode(Microsoft::Console::Render::Renderer* renderer) const;

public:
    QuickSelectHandler(
        std::shared_ptr<Microsoft::Terminal::Core::Terminal> terminal,
        std::shared_ptr<VimModeProxy> vimProxy,
        std::shared_ptr<QuickSelectAlphabet> quickSelectAlphabet);
    void EnterQuickSelectMode(
        std::wstring_view text,
        bool copyMode,
        Search& searcher,
        Microsoft::Console::Render::Renderer* renderer,
        winrt::Microsoft::Terminal::Control::implementation::ControlCore* controlCore);
    bool Enabled();
    void HandleChar(
        uint32_t vkey,
        const ::Microsoft::Terminal::Core::ControlKeyStates mods,
        Microsoft::Console::Render::Renderer* renderer,
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection &connection,
        winrt::Microsoft::Terminal::Control::implementation::ControlCore *controlCore);
};
