#pragma once
#include "../../renderer/base/Renderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"

namespace Microsoft::Console::Render
{
    class Renderer;
}

class VimModeProxy;

class QuickSelectHandler
{
private:
    Microsoft::Terminal::Core::Terminal* _terminal;
    bool _copyMode = false;
    VimModeProxy *_vimProxy;
    Microsoft::Console::Render::Renderer* _renderer;
    QuickSelectAlphabet *_quickSelectAlphabet;

public:
    QuickSelectHandler(
        Microsoft::Terminal::Core::Terminal* terminal,
        VimModeProxy *vimProxy,
        Microsoft::Console::Render::Renderer *renderer,
        QuickSelectAlphabet *quickSelectAlphabet);
    void EnterQuickSelectMode(bool copyMode);
    bool Enabled();
    void HandleChar(uint32_t vkey);
};
