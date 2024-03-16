#pragma once
#include "FuzzySearchBoxControl.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../buffer/out/search.h"
#include "../../renderer/inc/FontInfoDesired.hpp"

class VimModeProxy;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ControlSettings;
}

namespace Microsoft::Console::Types
{
    class Viewport;
}

class FontInfo;
class FontInfoDesired;

namespace Microsoft::Console::Render
{
    class IRenderEngine;
    class Renderer;
}

class FuzzySearchRenderData;

namespace Microsoft::Terminal::Core
{
    class Terminal;
}

class FuzzySearchHandler
{
    float _panelWidth{ 0 };
    float _panelHeight{ 0 };
    float _compositionScale{ 0 };
    bool _active = false;
    Search* _searcher;
    FontInfoDesired& _desiredFont;
    FontInfo& _actualFont;
    winrt::com_ptr<winrt::Microsoft::Terminal::Control::implementation::ControlSettings> _settings{ nullptr };

    std::shared_ptr<Microsoft::Terminal::Core::Terminal> _terminal;
    std::unique_ptr<::Microsoft::Console::Render::IRenderEngine> _fuzzySearchRenderEngine{ nullptr };
    std::unique_ptr<::Microsoft::Console::Render::Renderer> _fuzzySearchRenderer{ nullptr };
    std::shared_ptr<VimModeProxy> _vimProxy;
    std::shared_ptr<FuzzySearchRenderData> _fuzzySearchRenderData{ nullptr };
    winrt::handle _fuzzySearchLastSwapChainHandle{ nullptr };
    winrt::fire_and_forget _fuzzySearchRenderEngineSwapChainChanged(const HANDLE handle);
public:
    FuzzySearchHandler(
        std::shared_ptr<Microsoft::Terminal::Core::Terminal> terminal,
        Search* searcher,
        winrt::com_ptr<winrt::Microsoft::Terminal::Control::implementation::ControlSettings> _settings,
        FontInfoDesired& fontInfoDesired,
        FontInfo& fontInfo);
    void UpdateFont(
        int32_t dpi,
        const FontInfoDesired& pfiFontInfoDesired,
        FontInfo& fiFontInfo);
    void SetVimProxy(std::shared_ptr<VimModeProxy> vimMode);
    void FuzzySearchPreviewSizeChanged(const float width,
                                                           const float height);
    void SelectRow(int32_t row, int32_t col);
    void FuzzySearchSelectionChanged(int32_t row);
    void EnterFuzzySearchMode();
    void CloseFuzzySearchNoSelection();
    winrt::Microsoft::Terminal::Control::FuzzySearchResult FuzzySearch(const winrt::hstring& text);
    void SizeOrScaleChanged(const float width, const float height, const float scale);

private:
    void _sizeFuzzySearchPreview();
    void _rendererBackgroundColorChanged();
    void _rendererTabColorChanged();
};
