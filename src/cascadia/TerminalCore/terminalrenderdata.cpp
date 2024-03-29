// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include <DefaultSettings.h>

#include "QuickSelectAlphabet.h"

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;

Viewport Terminal::GetViewport() noexcept
{
    return _GetVisibleViewport();
}

til::point Terminal::GetTextBufferEndPosition() const noexcept
{
    // We use the end line of mutableViewport as the end
    // of the text buffer, it always moves with the written
    // text
    return { _GetMutableViewport().Width() - 1, ViewEndIndex() };
}

const TextBuffer& Terminal::GetTextBuffer() const noexcept
{
    return _activeBuffer();
}

const FontInfo& Terminal::GetFontInfo() const noexcept
{
    _assertLocked();
    return _fontInfo;
}

void Terminal::SetFontInfo(const FontInfo& fontInfo)
{
    _assertLocked();
    _fontInfo = fontInfo;
}

til::point Terminal::GetCursorPosition() const noexcept
{
    const auto& cursor = _activeBuffer().GetCursor();
    return cursor.GetPosition();
}

bool Terminal::IsCursorVisible() const noexcept
{
    const auto& cursor = _activeBuffer().GetCursor();
    return cursor.IsVisible() && !cursor.IsPopupShown();
}

bool Terminal::IsCursorOn() const noexcept
{
    const auto& cursor = _activeBuffer().GetCursor();
    return cursor.IsOn();
}

ULONG Terminal::GetCursorPixelWidth() const noexcept
{
    return 1;
}

ULONG Terminal::GetCursorHeight() const noexcept
{
    return _activeBuffer().GetCursor().GetSize();
}

CursorType Terminal::GetCursorStyle() const noexcept
{
    return _activeBuffer().GetCursor().GetType();
}

bool Terminal::IsCursorDoubleWidth() const
{
    const auto& buffer = _activeBuffer();
    const auto position = buffer.GetCursor().GetPosition();
    return buffer.GetRowByOffset(position.y).DbcsAttrAt(position.x) != DbcsAttribute::Single;
}

const std::vector<RenderOverlay> Terminal::GetOverlays() const noexcept
{
    return {};
}

const bool Terminal::IsGridLineDrawingAllowed() noexcept
{
    return true;
}

const std::wstring Microsoft::Terminal::Core::Terminal::GetHyperlinkUri(uint16_t id) const
{
    return _activeBuffer().GetHyperlinkUriFromId(id);
}

const std::wstring Microsoft::Terminal::Core::Terminal::GetHyperlinkCustomId(uint16_t id) const
{
    return _activeBuffer().GetCustomIdFromId(id);
}

// Method Description:
// - Gets the regex pattern ids of a location
// Arguments:
// - The location
// Return value:
// - The pattern IDs of the location
const std::vector<size_t> Terminal::GetPatternId(const til::point location) const
{
    _assertLocked();

    // Look through our interval tree for this location
    const auto intervals = _patternIntervalTree.findOverlapping({ location.x + 1, location.y }, location);
    if (intervals.size() == 0)
    {
        return {};
    }
    else
    {
        std::vector<size_t> result{};
        for (const auto& interval : intervals)
        {
            result.emplace_back(interval.value);
        }
        return result;
    }
    return {};
}

std::pair<COLORREF, COLORREF> Terminal::GetAttributeColors(const TextAttribute& attr) const noexcept
{
    return GetRenderSettings().GetAttributeColors(attr);
}

std::vector<Microsoft::Console::Types::Viewport> Terminal::GetSelectionRects() noexcept
try
{
    std::vector<Viewport> result;

    for (const auto& lineRect : _GetSelectionRects())
    {
        result.emplace_back(Viewport::FromInclusive(lineRect));
    }

    return result;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return {};
}

std::vector<Microsoft::Console::Types::Viewport> Terminal::GetYankSelectionRects() noexcept
try
{
    std::vector<Viewport> result;

    for (const auto& lineRect : _GetYankSelectionRects())
    {
        result.emplace_back(Viewport::FromInclusive(lineRect));
    }

    return result;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return {};
}

std::vector<Microsoft::Console::Types::Viewport> Terminal::GetSearchSelectionRects() noexcept
try
{
    std::vector<Viewport> result;

    for (const auto& lineRect : _GetSearchSelectionRects(_GetVisibleViewport()))
    {
        result.emplace_back(Viewport::FromInclusive(lineRect));
    }

    return result;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return {};
}

Microsoft::Console::Render::QuickSelectState Terminal::GetQuickSelectState() noexcept
try
{
    auto result = QuickSelectState{};
    if (!InQuickSelectMode())
    {
        return result;
    }

    auto lowerIt = std::lower_bound(_searchSelections.begin(), _searchSelections.end(), _GetVisibleViewport().Top(), [](const til::inclusive_rect& rect, til::CoordType value) {
        return rect.top < value;
    });

    auto upperIt = std::upper_bound(_searchSelections.begin(), _searchSelections.end(), _GetVisibleViewport().BottomExclusive(), [](til::CoordType value, const til::inclusive_rect& rect) {
        return value < rect.top;
    });

    auto num = static_cast<int32_t>(std::distance(lowerIt, upperIt));
    auto chars = _quickSelectAlphabet->GetQuickSelectChars(num);

    til::CoordType lastY = -1;
    for (int i = 0; i < num; i++)
    {
        auto ch = chars[i];
        if(ch.isCurrentMatch)
        {
            const til::inclusive_rect selection = lowerIt[i];
            const auto start = til::point{ selection.left, selection.top };
            const auto end = til::point{ selection.right, selection.bottom };
            const auto adj = _activeBuffer().GetTextRects(start, end, _blockSelection, false);

            if (adj[0].right - adj[0].left + 1 >= ch.chars.size())
            {
                for (auto j = 0; j < adj.size(); j++)
                {
                    QuickSelectSelection toAdd;
                    if (j > 0)
                    {
                        toAdd = QuickSelectSelection{};
                    }
                    else
                    {
                        toAdd = ch;
                    }
                    toAdd.selection = Viewport::FromInclusive(adj[j]);
                    toAdd.isCurrentMatch = ch.isCurrentMatch;
                    if (lastY == toAdd.selection.Top())
                    {
                        result.selectionMap.at(toAdd.selection.Top()).emplace_back(toAdd);
                    }
                    else
                    {
                        auto rowSelections = std::vector<QuickSelectSelection>{};
                        rowSelections.emplace_back(toAdd);
                        result.selectionMap.emplace(toAdd.selection.Top(), rowSelections);
                        lastY = toAdd.selection.Top();
                    }
                }
            }
        }
    }

    return result;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return {};
}

bool Terminal::InQuickSelectMode()
{
    return _quickSelectAlphabet->Enabled();
}

void Terminal::SelectNewRegion(const til::point coordStart, const til::point coordEnd)
{
#pragma warning(push)
#pragma warning(disable : 26496) // cpp core checks wants these const, but they're decremented below.
    auto realCoordStart = coordStart;
    auto realCoordEnd = coordEnd;
#pragma warning(pop)

    auto notifyScrollChange = false;
    if (coordStart.y < _VisibleStartIndex())
    {
        // recalculate the scrollOffset
        _scrollOffset = ViewStartIndex() - coordStart.y;
        notifyScrollChange = true;
    }
    else if (coordEnd.y > _VisibleEndIndex())
    {
        // recalculate the scrollOffset, note that if the found text is
        // beneath the current visible viewport, it may be within the
        // current mutableViewport and the scrollOffset will be smaller
        // than 0
        _scrollOffset = std::max(0, ViewStartIndex() - coordStart.y);
        notifyScrollChange = true;
    }

    if (notifyScrollChange)
    {
        _activeBuffer().TriggerScroll();
        _NotifyScrollEvent();
    }

    realCoordStart.y -= _VisibleStartIndex();
    realCoordEnd.y -= _VisibleStartIndex();

    SetSelectionAnchor(realCoordStart);
    SetSelectionEnd(realCoordEnd, SelectionExpansion::Char);
}

void Terminal::SelectYankRegion()
{
    _yankSelection = SelectionAnchors{};

    _yankSelection->end = _selection->end;
    _yankSelection->start = _selection->start;
    _yankSelection->pivot = _selection->pivot;
}

void Terminal::ClearYankRegion()
{
    _yankSelection.reset();
}

void Terminal::SelectSearchRegions(std::vector<til::inclusive_rect> rects)
{
    _searchSelections.clear();
    for (auto& rect : rects)
    {
        rect.top -= _VisibleStartIndex();
        rect.bottom -= _VisibleStartIndex();

        const auto realStart = _ConvertToBufferCell(til::point{ rect.left, rect.top });
        const auto realEnd = _ConvertToBufferCell(til::point{ rect.right, rect.bottom });

        auto rr = til::inclusive_rect{ realStart.x, realStart.y, realEnd.x, realEnd.y };

        _searchSelections.emplace_back(rr);
    }
}

const std::wstring_view Terminal::GetConsoleTitle() const noexcept
{
    _assertLocked();
    if (_title.has_value())
    {
        return *_title;
    }
    return _startingTitle;
}

// Method Description:
// - Lock the terminal for reading the contents of the buffer. Ensures that the
//      contents of the terminal won't be changed in the middle of a paint
//      operation.
//   Callers should make sure to also call Terminal::UnlockConsole once
//      they're done with any querying they need to do.
void Terminal::LockConsole() noexcept
{
    _readWriteLock.lock();
}

// Method Description:
// - Unlocks the terminal after a call to Terminal::LockConsole.
void Terminal::UnlockConsole() noexcept
{
    _readWriteLock.unlock();
}

const bool Terminal::IsUiaDataInitialized() const noexcept
{
    // GH#11135: Windows Terminal needs to create and return an automation peer
    // when a screen reader requests it. However, the terminal might not be fully
    // initialized yet. So we use this to check if any crucial components of
    // UiaData are not yet initialized.
    _assertLocked();
    return !!_mainBuffer;
}
