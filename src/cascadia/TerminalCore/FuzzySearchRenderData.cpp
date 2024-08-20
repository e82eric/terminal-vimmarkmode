#pragma once
#include "pch.h"
#include "FuzzySearchRenderData.hpp"

FuzzySearchRenderData::FuzzySearchRenderData(IRenderData* pData) :
    _pData(pData)
{
    auto vp = Microsoft::Console::Types::Viewport{};
    _viewPort = vp.FromDimensions(til::point{ 0, 5 }, _size);
}

void FuzzySearchRenderData::SetSize(til::size size)
{
    _size = size;
    _viewPort = _viewPort.FromDimensions(til::point{ 0, 0 }, til::size{ _size.width, _size.height });
}

void FuzzySearchRenderData::SetTopRow(til::CoordType row)
{
    _row = row;
    til::CoordType newY;
    const auto textBufferHeight = _pData->GetTextBuffer().GetSize().Height();
    const auto viewPortHeight = _viewPort.Height();
    if (row + viewPortHeight > textBufferHeight)
    {
        newY = textBufferHeight - viewPortHeight;
    }
    else
    {
        newY = std::max(0, row - 3);
    }

    _viewPort = _viewPort.FromDimensions(til::point{ 0, std::max(0, newY) }, til::size{ _size.width, _size.height });
}

void FuzzySearchRenderData::ResetTopRow()
{
    _viewPort = _viewPort.FromDimensions(til::point{ 0, 0 }, til::size{ _size.width, _size.height });
}

Microsoft::Console::Types::Viewport FuzzySearchRenderData::GetViewport() noexcept
{
    return _viewPort;
}

til::point FuzzySearchRenderData::GetTextBufferEndPosition() const noexcept
{
    return {};
}

const FontInfo& FuzzySearchRenderData::GetFontInfo() const noexcept
{
    return _pData->GetFontInfo();
}

std::span<const til::point_span> FuzzySearchRenderData::GetYankSelectionRects() noexcept
{
    return {};
}

void FuzzySearchRenderData::SelectYankRegion()
{
}

void FuzzySearchRenderData::ClearYankRegion()
{
}

bool FuzzySearchRenderData::InQuickSelectMode()
{
    return false;
}

Microsoft::Console::Render::QuickSelectState FuzzySearchRenderData::GetQuickSelectState() noexcept
{
    return {};
}

std::span<const til::point_span> FuzzySearchRenderData::GetSelectionSpans() const noexcept
{
    auto startPoint = til::point{ 0, _row };
    til::CoordType endRow = _row;

    while (_pData->GetTextBuffer().GetRowByOffset(endRow).WasWrapForced())
    {
        endRow++;
    }
    const auto rects = _pData->GetTextBuffer().GetTextSpans(til::point{ 0, _row }, til::point{ _viewPort.Width() - 1, endRow }, false, false);
    _lastSelectionSpans = rects;

    return _lastSelectionSpans;
}

[[nodiscard]] std::unique_lock<til::recursive_ticket_lock> FuzzySearchRenderData::LockForReading() const noexcept
{
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function 'recursive_ticket_lock>()' which may throw exceptions (f.6).
#pragma warning(suppress : 26492) // Don't use const_cast to cast away const or volatile
    return std::unique_lock{ const_cast<til::recursive_ticket_lock&>(_readWriteLock) };
}

[[nodiscard]] std::unique_lock<til::recursive_ticket_lock> FuzzySearchRenderData::LockForWriting() noexcept
{
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function 'recursive_ticket_lock>()' which may throw exceptions (f.6).
    return std::unique_lock{ _readWriteLock };
}

void FuzzySearchRenderData::LockConsole() noexcept
{
    _pData->LockConsole();
}

void FuzzySearchRenderData::UnlockConsole() noexcept
{
    _pData->UnlockConsole();
}

std::pair<COLORREF, COLORREF> FuzzySearchRenderData::GetAttributeColors(const TextAttribute& attr) const noexcept
{
    return _pData->GetAttributeColors(attr);
}

til::point FuzzySearchRenderData::GetCursorPosition() const noexcept
{
    return {};
}

bool FuzzySearchRenderData::IsCursorVisible() const noexcept
{
    return false;
}

bool FuzzySearchRenderData::IsCursorOn() const noexcept
{
    return false;
}

ULONG FuzzySearchRenderData::GetCursorHeight() const noexcept
{
    return 42ul;
}

CursorType FuzzySearchRenderData::GetCursorStyle() const noexcept
{
    return CursorType::FullBox;
}

ULONG FuzzySearchRenderData::GetCursorPixelWidth() const noexcept
{
    return 12ul;
}

bool FuzzySearchRenderData::IsCursorDoubleWidth() const
{
    return false;
}

const bool FuzzySearchRenderData::IsGridLineDrawingAllowed() noexcept
{
    return false;
}

const std::wstring_view FuzzySearchRenderData::GetConsoleTitle() const noexcept
{
    return std::wstring_view{};
}

const bool FuzzySearchRenderData::IsSelectionActive() const
{
    return false;
}

const bool FuzzySearchRenderData::IsBlockSelection() const noexcept
{
    return false;
}

void FuzzySearchRenderData::ClearSelection()
{
}

void FuzzySearchRenderData::SelectNewRegion(const til::point /*coordStart*/, const til::point /*coordEnd*/)
{
}

const til::point FuzzySearchRenderData::GetSelectionAnchor() const noexcept
{
    return {};
}

const til::point FuzzySearchRenderData::GetSelectionEnd() const noexcept
{
    return {};
}

const bool FuzzySearchRenderData::IsUiaDataInitialized() const noexcept
{
    return true;
}

const std::wstring FuzzySearchRenderData::GetHyperlinkUri(uint16_t /*id*/) const
{
    return {};
}

const std::wstring FuzzySearchRenderData::GetHyperlinkCustomId(uint16_t /*id*/) const
{
    return {};
}

const std::vector<size_t> FuzzySearchRenderData::GetPatternId(const til::point /*location*/) const
{
    return {};
}

TextBuffer& FuzzySearchRenderData::GetTextBuffer(void) const noexcept
{
    return _pData->GetTextBuffer();
}

std::span<const til::point_span> FuzzySearchRenderData::GetSearchHighlights() const noexcept
{
    return {};
}

const til::point_span* FuzzySearchRenderData::GetSearchHighlightFocused(void) const noexcept
{
    return {};
}
