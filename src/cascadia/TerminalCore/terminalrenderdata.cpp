// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include <DefaultSettings.h>

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

    int columns = 1;
    while (std::pow(_quickSelectAlphabet.size(), columns) < std::distance(lowerIt, upperIt))
    {
        columns++;
    }

    std::vector<int> indices(columns, 0);

    til::CoordType lastY = -1;
    std::vector<til::rect> dirtySearchRectanglesToPaint;

    for (auto selection = lowerIt; selection != upperIt; ++selection)
    {
        const auto start = til::point{ selection->left, selection->top };
        const auto end = til::point{ selection->right, selection->bottom };
        const auto adj = _activeBuffer().GetTextRects(start, end, _blockSelection, false);

        auto charsInFirstRow = 0;
        if (start.y == end.y)
        {
            charsInFirstRow = end.x + 1 - start.x;
        }
        else
        {
            charsInFirstRow = _GetVisibleViewport().RightInclusive() - start.x + 1; 
        }

        bool allMatching = true;
        std::vector<QuickSelectChar> chs;
        for (int i = 0; i < indices.size(); i++)
        {
            auto idx = indices[i];
            auto ch = QuickSelectChar{};
            ch.val = _quickSelectAlphabet[idx];
            if (i < _quickSelectChars.size())
            {
                if (_quickSelectAlphabet[idx] != _quickSelectChars[i])
                {
                    allMatching = false;
                    //We are going to throw this away anyways
                    break;
                }

                ch.isMatch = true;
            }
            else
            {
                ch.isMatch = false;
            }
            chs.emplace_back(ch);
        }

        auto isCurrentMatch = false;
        if ((_quickSelectChars.size() == 0 || chs.size() >= _quickSelectChars.size()) &&
            (charsInFirstRow >= columns) &&
            allMatching)
        {
            isCurrentMatch = true;
        }

        for (auto j = 0; j < adj.size(); j++)
        {
            auto toAdd = QuickSelectSelection{};
            toAdd.selection = Viewport::FromInclusive(adj[j]);
            toAdd.isCurrentMatch = isCurrentMatch;

            if (j == 0)
            {
                for (auto ch : chs)
                {
                    toAdd.chars.emplace_back(ch);
                }

                for (int j = columns - 1; j >= 0; --j)
                {
                    indices[j]++;
                    if (indices[j] < _quickSelectAlphabet.size())
                    {
                        break; // No carry over, break the loop
                    }
                    indices[j] = 0; // Carry over to the previous column
                    //if (j == 0)
                    //{
                    //    // If it's the first column, reset to all zeros (optional based on your use case)
                    //    std::fill(indices.begin(), indices.end(), 0);
                    //}
                }
            }

            if (toAdd.isCurrentMatch)
            {
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

    return result;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return {};
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
