#include "pch.h"
#include "FuzzySearchHandler.h"

#include <ControlSettings.h>

#include <DefaultSettings.h>
#include "FuzzySearchTextSegment.h"
#include "FuzzySearchBoxControl.h"
#include "VimModeProxy.h"
#include "../../cascadia/TerminalCore/FuzzySearchRenderData.hpp"
#include "../../renderer/base/Renderer.hpp"


struct FuzzySearchResultRow;

FuzzySearchHandler::FuzzySearchHandler(
    std::shared_ptr<Microsoft::Terminal::Core::Terminal> terminal,
    Search* searcher,
    winrt::com_ptr<winrt::Microsoft::Terminal::Control::implementation::ControlSettings> settings,
    FontInfoDesired& fontInfoDesired,
    FontInfo& fontInfo) :
    _desiredFont(fontInfoDesired),
    _actualFont(fontInfo)
{
    //_desiredFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, DEFAULT_FONT_SIZE, CP_UTF8 },
    //_actualFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false };
    _settings = settings;

    _terminal = terminal;
    _searcher = searcher;

    auto fuzzySearchRenderThread = std::make_unique<::Microsoft::Console::Render::RenderThread>();
    auto* const localPointerToFuzzySearchThread = fuzzySearchRenderThread.get();

    //Now create the renderer and initialize the render thread.
    //const auto& renderSettings = _terminal->GetRenderSettings();
    _fuzzySearchRenderData = std::make_unique<FuzzySearchRenderData>(_terminal.get());
    const auto& renderSettings = _terminal->GetRenderSettings();
    _fuzzySearchRenderer = std::make_unique<::Microsoft::Console::Render::Renderer>(renderSettings, _fuzzySearchRenderData.get(), nullptr, 0, std::move(fuzzySearchRenderThread));
    _fuzzySearchRenderData->SetRenderer(_fuzzySearchRenderer.get());

    _fuzzySearchRenderer->SetBackgroundColorChangedCallback([this]() { _rendererBackgroundColorChanged(); });
    _fuzzySearchRenderer->SetFrameColorChangedCallback([this]() { _rendererTabColorChanged(); });
    _fuzzySearchRenderer->SetRendererEnteredErrorStateCallback([this]() { });

    THROW_IF_FAILED(localPointerToFuzzySearchThread->Initialize(_fuzzySearchRenderer.get()));
}

void FuzzySearchHandler::SetVimProxy(std::shared_ptr<VimModeProxy> vimProxy)
{
    _vimProxy = vimProxy;
}

void FuzzySearchHandler::SizeOrScaleChanged(const float width, const float height, const float scale)
{
    _panelWidth = width;
    _panelHeight = height;
    _compositionScale = scale;
}

void FuzzySearchHandler::UpdateFont(
    int32_t dpi,
    const FontInfoDesired& pfiFontInfoDesired,
    FontInfo& fiFontInfo)
{
    _desiredFont = pfiFontInfoDesired;
    _actualFont = fiFontInfo;

    std::unordered_map<std::wstring_view, uint32_t> featureMap;
    if (const auto fontFeatures = _settings->FontFeatures())
    {
        featureMap.reserve(fontFeatures.Size());

        for (const auto& [tag, param] : fontFeatures)
        {
            featureMap.emplace(tag, param);
        }
    }
    std::unordered_map<std::wstring_view, float> axesMap;
    if (const auto fontAxes = _settings->FontAxes())
    {
        axesMap.reserve(fontAxes.Size());

        for (const auto& [axis, value] : fontAxes)
        {
            axesMap.emplace(axis, value);
        }
    }

    LOG_IF_FAILED(_fuzzySearchRenderEngine->UpdateDpi(dpi));
    LOG_IF_FAILED(_fuzzySearchRenderEngine->UpdateFont(pfiFontInfoDesired, fiFontInfo, featureMap, axesMap));
}

void FuzzySearchHandler::FuzzySearchPreviewSizeChanged(const float width,
                                                const float height)
{
    _panelWidth = width;
    _panelHeight = height;

    _fuzzySearchRenderer->EnablePainting();
    _sizeFuzzySearchPreview();
}

void FuzzySearchHandler::SelectRow(int32_t row, int32_t col)
{
    const auto lock = _terminal->LockForWriting();
    _vimProxy->SelectRow(row, col);
    _active = false;
}

void FuzzySearchHandler::FuzzySearchSelectionChanged(int32_t row)
{
    _fuzzySearchRenderData->SetTopRow(row);
    LOG_IF_FAILED(_fuzzySearchRenderEngine->InvalidateAll());
    _fuzzySearchRenderer->NotifyPaintFrame();
}

void FuzzySearchHandler::EnterFuzzySearchMode()
{
    _vimProxy->EnterVimMode();
    _active = true;
    _sizeFuzzySearchPreview();
}

void FuzzySearchHandler::CloseFuzzySearchNoSelection()
{
    _active = false;
}


std::wstring GetRowFullText(FuzzySearchResultRow& fuzzySearchResult2, TextBuffer& textBuffer)
{
    std::wstring result;

    auto i = fuzzySearchResult2.startRowNumber;
    while (textBuffer.GetRowByOffset(i).WasWrapForced())
    {
        result += textBuffer.GetRowByOffset(i).GetText();
        i++;
    }
    result += textBuffer.GetRowByOffset(i).GetText();

    return result;
}

//winrt::Microsoft::Terminal::Control::FuzzySearchResult FuzzySearchHandler::FuzzySearch(const winrt::hstring& text)
//{
//    const auto lock = _terminal->LockForWriting();
//
//    const auto fuzzySearchResultRows = _searcher->FuzzySearch(*_terminal, text);
//
//    auto searchResults = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>();
//
//    for (auto p : fuzzySearchResultRows)
//    {
//        auto rowFullText = GetRowFullText(p, _terminal->GetTextBuffer());
//
//        //sort the positions descending so that it is easier to create text segments from them
//        std::ranges::sort(p.positions, [](int32_t a, int32_t b) {
//            return a < b;
//        });
//
//        //Covert row text to text runs
//        auto runs = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Control::FuzzySearchTextSegment>();
//        std::wstring currentRun;
//        bool isCurrentRunHighlighted = false;
//        size_t highlightIndex = 0;
//
//        for (int32_t i = 0; i < rowFullText.length(); ++i)
//        {
//            if (highlightIndex < p.positions.size() && i == p.positions[highlightIndex])
//            {
//                if (!isCurrentRunHighlighted)
//                {
//                    if (!currentRun.empty())
//                    {
//                        auto textSegmentHString = winrt::hstring(currentRun);
//                        auto textSegment = winrt::make<winrt::Microsoft::Terminal::Control::implementation::FuzzySearchTextSegment>(textSegmentHString, false);
//                        runs.Append(textSegment);
//                        currentRun.clear();
//                    }
//                    isCurrentRunHighlighted = true;
//                }
//                highlightIndex++;
//            }
//            else
//            {
//                if (isCurrentRunHighlighted)
//                {
//                    if (!currentRun.empty())
//                    {
//                        winrt::hstring textSegmentHString = winrt::hstring(currentRun);
//                        auto textSegment = winrt::make<winrt::Microsoft::Terminal::Control::implementation::FuzzySearchTextSegment>(textSegmentHString, true);
//                        runs.Append(textSegment);
//                        currentRun.clear();
//                    }
//                    isCurrentRunHighlighted = false;
//                }
//            }
//            currentRun += rowFullText[i];
//        }
//
//        if (!currentRun.empty())
//        {
//            auto textSegmentHString = winrt::hstring(currentRun);
//            auto textSegment = winrt::make<winrt::Microsoft::Terminal::Control::implementation::FuzzySearchTextSegment>(textSegmentHString, isCurrentRunHighlighted);
//            runs.Append(textSegment);
//        }
//
//        auto firstPosition = p.positions[0];
//        auto line = winrt::make<winrt::Microsoft::Terminal::Control::implementation::FuzzySearchTextLine>(runs, p.startRowNumber, firstPosition);
//
//        searchResults.Append(line);
//    }
//
//    auto fuzzySearchResult = winrt::make<winrt::Microsoft::Terminal::Control::FuzzySearchResult>(searchResults, static_cast<int32_t>(fuzzySearchResultRows.size()), static_cast<int32_t>(searchResults.Size()));
//    return fuzzySearchResult;
//}

void FuzzySearchHandler::_sizeFuzzySearchPreview()
{
    auto lock = _terminal->LockForWriting();
    auto lock2 = _fuzzySearchRenderData->LockForWriting();

    auto cx = gsl::narrow_cast<til::CoordType>(lrint(_panelWidth * _compositionScale));
    auto cy = gsl::narrow_cast<til::CoordType>(lrint(_panelHeight * _compositionScale));

    cx = std::max(cx, _actualFont.GetSize().width);
    cy = std::max(cy, _actualFont.GetSize().height);

    const auto viewInPixels = Microsoft::Console::Types::Viewport::FromDimensions({ 0, 0 }, { cx, cy });
    //Is this right
    const auto vp = _fuzzySearchRenderEngine->GetViewportInCharacters(viewInPixels);
    _fuzzySearchRenderData->SetSize(vp.Dimensions());

    auto size = til::size{ til::math::rounding, static_cast<float>(_terminal->GetViewport().Width()), static_cast<float>(_terminal->GetTextBuffer().TotalRowCount()) };

    auto newTextBuffer = std::make_unique<TextBuffer>(size,
                                                      TextAttribute{},
                                                      0,
                                                      true,
                                                      *_fuzzySearchRenderer);

    TextBuffer::Reflow(_terminal->GetTextBuffer(), *newTextBuffer.get());
    _fuzzySearchRenderData->SetTextBuffer(std::move(newTextBuffer));
    THROW_IF_FAILED(_fuzzySearchRenderEngine->SetWindowSize({ cx, cy }));
    LOG_IF_FAILED(_fuzzySearchRenderEngine->InvalidateAll());
    _fuzzySearchRenderer->NotifyPaintFrame();
}

void FuzzySearchHandler::_rendererBackgroundColorChanged()
{
}

void FuzzySearchHandler::_rendererTabColorChanged()
{
}

//winrt::fire_and_forget FuzzySearchHandler::_fuzzySearchRenderEngineSwapChainChanged(const HANDLE sourceHandle)
//{
//    // `sourceHandle` is a weak ref to a HANDLE that's ultimately owned by the
//    // render engine's own unique_handle. We'll add another ref to it here.
//    // This will make sure that we always have a valid HANDLE to give to
//    // callers of our own SwapChainHandle method, even if the renderer is
//    // currently in the process of discarding this value and creating a new
//    // one. Callers should have already set up the SwapChainChanged
//    // callback, so this all works out.
//
//    winrt::handle duplicatedHandle;
//    const auto processHandle = GetCurrentProcess();
//    THROW_IF_WIN32_BOOL_FALSE(DuplicateHandle(processHandle, sourceHandle, processHandle, duplicatedHandle.put(), 0, FALSE, DUPLICATE_SAME_ACCESS));
//
//    const auto weakThis{ get_weak() };
//
//    // Concurrent read of _dispatcher is safe, because Detach() calls WaitForPaintCompletionAndDisable()
//    // which blocks until this call returns. _dispatcher will only be changed afterwards.
//    co_await wil::resume_foreground(_dispatcher);
//
//    if (auto core{ weakThis.get() })
//    {
//        // `this` is safe to use now
//
//        _fuzzySearchLastSwapChainHandle = std::move(duplicatedHandle);
//        // Now bubble the event up to the control.
//        _FuzzySearchSwapChainChangedHandlers(*this, winrt::box_value<uint64_t>(reinterpret_cast<uint64_t>(_fuzzySearchLastSwapChainHandle.get())));
//    }
//}
