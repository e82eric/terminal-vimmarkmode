#pragma once
#include "../../renderer/base/Renderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../../cascadia/TerminalCore/lib/QuickSelectAlphabet.h"

class Search;

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

public:
    QuickSelectHandler(
        std::shared_ptr<Microsoft::Terminal::Core::Terminal> terminal,
        std::shared_ptr<VimModeProxy> vimProxy,
        std::shared_ptr<QuickSelectAlphabet> quickSelectAlphabet);
    void EnterQuickSelectMode(std::wstring_view text,
                              bool copyMode,
                              Search& searcher,
                              Microsoft::Console::Render::Renderer* renderer);
    bool Enabled();
    void HandleChar(uint32_t vkey, Microsoft::Console::Render::Renderer* renderer);
};
