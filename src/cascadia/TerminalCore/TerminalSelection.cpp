// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "unicode.hpp"

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Types;

DEFINE_ENUM_FLAG_OPERATORS(Terminal::SelectionEndpoint);

/* Selection Pivot Description:
 *   The pivot helps properly update the selection when a user moves a selection over itself
 *   See SelectionTest::DoubleClickDrag_Left for an example of the functionality mentioned here
 *   As an example, consider the following scenario...
 *     1. Perform a word selection (double-click) on a word
 *
 *                  |-position where we double-clicked
 *                 _|_
 *               |word|
 *                |--|
 *  start & pivot-|  |-end
 *
 *     2. Drag your mouse down a line
 *
 *
 *  start & pivot-|__________
 *             __|word_______|
 *            |______|
 *                  |
 *                  |-end & mouse position
 *
 *     3. Drag your mouse up two lines
 *
 *                  |-start & mouse position
 *                  |________
 *             ____|   ______|
 *            |___w|ord
 *                |-end & pivot
 *
 *    The pivot never moves until a new selection is created. It ensures that that cell will always be selected.
 */

// Method Description:
// - Helper to determine the selected region of the buffer. Used for rendering.
// Return Value:
// - A vector of rectangles representing the regions to select, line by line. They are absolute coordinates relative to the buffer origin.
std::vector<til::inclusive_rect> Terminal::_GetSelectionRects() const noexcept
{
    std::vector<til::inclusive_rect> result;

    if (!IsSelectionActive())
    {
        return result;
    }

    try
    {
        return _activeBuffer().GetTextRects(_selection->start, _selection->end, _blockSelection, false);
    }
    CATCH_LOG();
    return result;
}

// Method Description:
// - Helper to determine the selected region of the buffer. Used for rendering.
// Return Value:
// - A vector of rectangles representing the regions to select, line by line. They are absolute coordinates relative to the buffer origin.
std::vector<til::inclusive_rect> Terminal::_GetSearchSelectionRects(Microsoft::Console::Types::Viewport viewport) const noexcept
{
    std::vector<til::inclusive_rect> result;
    try
    {
        auto lowerIt = std::lower_bound(_searchSelections.begin(), _searchSelections.end(), viewport.Top(), [](const til::inclusive_rect& rect, til::CoordType value) {
            return rect.top < value;
        });

        auto upperIt = std::upper_bound(_searchSelections.begin(), _searchSelections.end(), viewport.BottomExclusive(), [](til::CoordType value, const til::inclusive_rect& rect) {
            return value < rect.top;
        });

        for (auto selection = lowerIt; selection != upperIt; ++selection)
        {
            const auto start = til::point{ selection->left, selection->top };
            const auto end = til::point{ selection->right, selection->top };
            const auto adj = _activeBuffer().GetTextRects(start, end, _blockSelection, false);
            for (auto a : adj)
            {
                result.emplace_back(a);
            }
        }

        return result;
    }
    CATCH_LOG();
    return result;
}

// Method Description:
// - Identical to GetTextRects if it's a block selection, else returns a single span for the whole selection.
// Return Value:
// - A vector of one or more spans representing the selection. They are absolute coordinates relative to the buffer origin.
std::vector<til::point_span> Terminal::_GetSelectionSpans() const noexcept
{
    std::vector<til::point_span> result;

    if (!IsSelectionActive())
    {
        return result;
    }

    try
    {
        return _activeBuffer().GetTextSpans(_selection->start, _selection->end, _blockSelection, false);
    }
    CATCH_LOG();
    return result;
}

// Method Description:
// - Get the current anchor position relative to the whole text buffer
// Arguments:
// - None
// Return Value:
// - None
const til::point Terminal::GetSelectionAnchor() const noexcept
{
    _assertLocked();
    return _selection->start;
}

// Method Description:
// - Get the current end anchor position relative to the whole text buffer
// Arguments:
// - None
// Return Value:
// - None
const til::point Terminal::GetSelectionEnd() const noexcept
{
    _assertLocked();
    return _selection->end;
}

// Method Description:
// - Gets the viewport-relative position of where to draw the marker
//    for the selection's start endpoint
til::point Terminal::SelectionStartForRendering() const
{
    auto pos{ _selection->start };
    const auto bufferSize{ GetTextBuffer().GetSize() };
    if (pos.x != bufferSize.Left())
    {
        // In general, we need to draw the marker one before the
        // beginning of the selection.
        // When we're at the left boundary, we want to
        // flip the marker, so we skip this step.
        bufferSize.DecrementInBounds(pos);
    }
    pos.y = base::ClampSub(pos.y, _VisibleStartIndex());
    return til::point{ pos };
}

// Method Description:
// - Gets the viewport-relative position of where to draw the marker
//    for the selection's end endpoint
til::point Terminal::SelectionEndForRendering() const
{
    auto pos{ _selection->end };
    const auto bufferSize{ GetTextBuffer().GetSize() };
    if (pos.x != bufferSize.RightInclusive())
    {
        // In general, we need to draw the marker one after the
        // end of the selection.
        // When we're at the right boundary, we want to
        // flip the marker, so we skip this step.
        bufferSize.IncrementInBounds(pos);
    }
    pos.y = base::ClampSub(pos.y, _VisibleStartIndex());
    return til::point{ pos };
}

const Terminal::SelectionEndpoint Terminal::SelectionEndpointTarget() const noexcept
{
    return _selectionEndpoint;
}

// Method Description:
// - Checks if selection is active
// Return Value:
// - bool representing if selection is active. Used to decide copy/paste on right click
const bool Terminal::IsSelectionActive() const noexcept
{
    _assertLocked();
    return _selection.has_value();
}

const bool Terminal::IsBlockSelection() const noexcept
{
    _assertLocked();
    return _blockSelection;
}

// Method Description:
// - Perform a multi-click selection at viewportPos expanding according to the expansionMode
// Arguments:
// - viewportPos: the (x,y) coordinate on the visible viewport
// - expansionMode: the SelectionExpansion to dictate the boundaries of the selection anchors
void Terminal::MultiClickSelection(const til::point viewportPos, SelectionExpansion expansionMode)
{
    // set the selection pivot to expand the selection using SetSelectionEnd()
    _selection = SelectionAnchors{};
    _selection->pivot = _ConvertToBufferCell(viewportPos);

    _multiClickSelectionMode = expansionMode;
    SetSelectionEnd(viewportPos);

    // we need to set the _selectionPivot again
    // for future shift+clicks
    _selection->pivot = _selection->start;
}

// Method Description:
// - Record the position of the beginning of a selection
// Arguments:
// - position: the (x,y) coordinate on the visible viewport
void Terminal::SetSelectionAnchor(const til::point viewportPos)
{
    _assertLocked();

    _selection = SelectionAnchors{};
    _selection->pivot = _ConvertToBufferCell(viewportPos);

    _multiClickSelectionMode = SelectionExpansion::Char;
    SetSelectionEnd(viewportPos);

    _selection->start = _selection->pivot;
}

// Method Description:
// - Update selection anchors when dragging to a position
// - based on the selection expansion mode
// Arguments:
// - viewportPos: the (x,y) coordinate on the visible viewport
// - newExpansionMode: overwrites the _multiClickSelectionMode for this function call. Used for ShiftClick
void Terminal::SetSelectionEnd(const til::point viewportPos, std::optional<SelectionExpansion> newExpansionMode)
{
    if (!IsSelectionActive())
    {
        // capture a log for spurious endpoint sets without an active selection
        LOG_HR(E_ILLEGAL_STATE_CHANGE);
        return;
    }

    const auto textBufferPos = _ConvertToBufferCell(viewportPos);

    // if this is a shiftClick action, we need to overwrite the _multiClickSelectionMode value (even if it's the same)
    // Otherwise, we may accidentally expand during other selection-based actions
    _multiClickSelectionMode = newExpansionMode.has_value() ? *newExpansionMode : _multiClickSelectionMode;

    auto targetStart = false;
    const auto anchors = _PivotSelection(textBufferPos, targetStart);
    const auto expandedAnchors = _ExpandSelectionAnchors(anchors);

    if (newExpansionMode.has_value())
    {
        // shift-click operations only expand the target side
        auto& anchorToExpand = targetStart ? _selection->start : _selection->end;
        anchorToExpand = targetStart ? expandedAnchors.first : expandedAnchors.second;

        // the other anchor should then become the pivot (we don't expand it)
        auto& anchorToPivot = targetStart ? _selection->end : _selection->start;
        anchorToPivot = _selection->pivot;
    }
    else
    {
        // expand both anchors
        std::tie(_selection->start, _selection->end) = expandedAnchors;
    }
    _selectionMode = SelectionInteractionMode::Mouse;
    _selectionIsTargetingUrl = false;
}

// Method Description:
// - returns a new pair of selection anchors for selecting around the pivot
// - This ensures start < end when compared
// Arguments:
// - targetPos: the (x,y) coordinate we are moving to on the text buffer
// - targetStart: if true, target will be the new start. Otherwise, target will be the new end.
// Return Value:
// - the new start/end for a selection
std::pair<til::point, til::point> Terminal::_PivotSelection(const til::point targetPos, bool& targetStart) const noexcept
{
    if (targetStart = _activeBuffer().GetSize().CompareInBounds(targetPos, _selection->pivot) <= 0)
    {
        // target is before pivot
        // treat target as start
        return std::make_pair(targetPos, _selection->pivot);
    }
    else
    {
        // target is after pivot
        // treat pivot as start
        return std::make_pair(_selection->pivot, targetPos);
    }
}

// Method Description:
// - Update the selection anchors to expand according to the expansion mode
// Arguments:
// - anchors: a pair of selection anchors representing a desired selection
// Return Value:
// - the new start/end for a selection
std::pair<til::point, til::point> Terminal::_ExpandSelectionAnchors(std::pair<til::point, til::point> anchors) const
{
    auto start = anchors.first;
    auto end = anchors.second;

    const auto bufferSize = _activeBuffer().GetSize();
    switch (_multiClickSelectionMode)
    {
    case SelectionExpansion::Line:
        start = { bufferSize.Left(), start.y };
        end = { bufferSize.RightInclusive(), end.y };
        break;
    case SelectionExpansion::Word:
        start = _activeBuffer().GetWordStart(start, _wordDelimiters);
        end = _activeBuffer().GetWordEnd(end, _wordDelimiters);
        break;
    case SelectionExpansion::Char:
    default:
        // no expansion is necessary
        break;
    }
    return std::make_pair(start, end);
}

// Method Description:
// - enable/disable block selection (ALT + selection)
// Arguments:
// - isEnabled: new value for _blockSelection
void Terminal::SetBlockSelection(const bool isEnabled) noexcept
{
    _blockSelection = isEnabled;
}

void Terminal::ToggleMarkers(const bool isEnabled) noexcept
{
    if (isEnabled)
    {
        _selectionEndpoint = static_cast<SelectionEndpoint>(0);
    }
    else
    {
        _selectionEndpoint = SelectionEndpoint::End;
    }
}

Terminal::SelectionInteractionMode Terminal::SelectionMode() const noexcept
{
    return _selectionMode;
}

void Terminal::ToggleMarkMode()
{
    if (_selectionMode == SelectionInteractionMode::Mark)
    {
        // Exit Mark Mode
        ClearSelection();
    }
    else
    {
        // Enter Mark Mode
        // NOTE: directly set cursor state. We already should have locked before calling this function.
        _activeBuffer().GetCursor().SetIsOn(false);
        if (!IsSelectionActive())
        {
            // No selection --> start one at the cursor
            const auto cursorPos{ _activeBuffer().GetCursor().GetPosition() };
            _selection = SelectionAnchors{};
            _selection->start = cursorPos;
            _selection->end = cursorPos;
            _selection->pivot = cursorPos;
            _blockSelection = false;
            WI_SetAllFlags(_selectionEndpoint, SelectionEndpoint::Start | SelectionEndpoint::End);
        }
        else if (WI_AreAllFlagsClear(_selectionEndpoint, SelectionEndpoint::Start | SelectionEndpoint::End))
        {
            // Selection already existed
            WI_SetFlag(_selectionEndpoint, SelectionEndpoint::End);
        }
        _ScrollToPoint(_selection->start);
        _selectionMode = SelectionInteractionMode::Mark;
        _selectionIsTargetingUrl = false;
    }
}

// Method Description:
// - switch the targeted selection endpoint with the other one (i.e. start <--> end)
void Terminal::SwitchSelectionEndpoint() noexcept
{
    if (IsSelectionActive())
    {
        if (WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) && WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::End))
        {
            // moving cursor --> anchor start, move end
            _selectionEndpoint = SelectionEndpoint::End;
            _anchorInactiveSelectionEndpoint = true;
        }
        else if (WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::End))
        {
            // moving end --> now we're moving start
            _selectionEndpoint = SelectionEndpoint::Start;
            _selection->pivot = _selection->end;
        }
        else if (WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start))
        {
            // moving start --> now we're moving end
            _selectionEndpoint = SelectionEndpoint::End;
            _selection->pivot = _selection->start;
        }
    }
}

void Terminal::ExpandSelectionToWord()
{
    if (IsSelectionActive())
    {
        const auto& buffer = _activeBuffer();
        _selection->start = buffer.GetWordStart(_selection->start, _wordDelimiters);
        _selection->pivot = _selection->start;
        _selection->end = buffer.GetWordEnd(_selection->end, _wordDelimiters);

        // if we're targeting both endpoints, instead just target "end"
        if (WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) && WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::End))
        {
            _selectionEndpoint = SelectionEndpoint::End;
        }
    }
}

// Method Description:
// - selects the next/previous hyperlink, if one is available
// Arguments:
// - dir: the direction we're scanning the buffer in to find the hyperlink of interest
// Return Value:
// - true if we found a hyperlink to select (and selected it). False otherwise.
void Terminal::SelectHyperlink(const SearchDirection dir)
{
    if (_selectionMode != SelectionInteractionMode::Mark)
    {
        // This feature only works in mark mode
        _selectionIsTargetingUrl = false;
        return;
    }

    // 0. Useful tools/vars
    const auto bufferSize = _activeBuffer().GetSize();
    const auto viewportHeight = _GetMutableViewport().Height();

    // The patterns are stored relative to the "search area". Initially, this search area will be the viewport,
    // but as we progressively search through more of the buffer, this will change.
    // Keep track of the search area here, and use the lambda below to convert points to the search area coordinate space.
    auto searchArea = _GetVisibleViewport();
    auto convertToSearchArea = [&searchArea](const til::point pt) noexcept {
        auto copy = pt;
        searchArea.ConvertToOrigin(&copy);
        return copy;
    };

    // extracts the next/previous hyperlink from the list of hyperlink ranges provided
    auto extractResultFromList = [&](std::vector<interval_tree::Interval<til::point, size_t>>& list) noexcept {
        const auto selectionStartInSearchArea = convertToSearchArea(_selection->start);

        std::optional<std::pair<til::point, til::point>> resultFromList;
        if (!list.empty())
        {
            if (dir == SearchDirection::Forward)
            {
                // pattern tree includes the currently selected range when going forward,
                // so we need to check if we're pointing to that one before returning it.
                auto range = list.front();
                if (_selectionIsTargetingUrl && range.start == selectionStartInSearchArea)
                {
                    if (list.size() > 1)
                    {
                        // if we're pointing to the currently selected URL,
                        // pick the next one.
                        range = til::at(list, 1);
                        resultFromList = { range.start, range.stop };
                    }
                    else
                    {
                        // LOAD-BEARING: the only range here is the one that's currently selected.
                        // Make sure this is set to nullopt so that we keep searching through the buffer.
                        resultFromList = std::nullopt;
                    }
                }
                else
                {
                    // not on currently selected range, return the first one
                    resultFromList = { range.start, range.stop };
                }
            }
            else if (dir == SearchDirection::Backward)
            {
                // moving backwards excludes the currently selected range,
                // simply return the last one in the list as it's ordered
                const auto& range = list.back();
                resultFromList = { range.start, range.stop };
            }
        }

        // pattern tree stores everything as viewport coords,
        // so we need to convert them on the way out
        if (resultFromList)
        {
            searchArea.ConvertFromOrigin(&resultFromList->first);
            searchArea.ConvertFromOrigin(&resultFromList->second);
        }
        return resultFromList;
    };

    // 1. Look for the hyperlink
    til::point searchStart = dir == SearchDirection::Forward ? _selection->start : til::point{ bufferSize.Left(), _VisibleStartIndex() };
    til::point searchEnd = dir == SearchDirection::Forward ? til::point{ bufferSize.RightInclusive(), _VisibleEndIndex() } : _selection->start;

    // 1.A) Try searching the current viewport (no scrolling required)
    auto resultList = _patternIntervalTree.findContained(convertToSearchArea(searchStart), convertToSearchArea(searchEnd));
    std::optional<std::pair<til::point, til::point>> result = extractResultFromList(resultList);
    if (!result)
    {
        // 1.B) Incrementally search through more of the space
        if (dir == SearchDirection::Forward)
        {
            searchStart = { bufferSize.Left(), searchEnd.y + 1 };
            searchEnd = { bufferSize.RightInclusive(), std::min(searchStart.y + viewportHeight, ViewEndIndex()) };
        }
        else
        {
            searchEnd = { bufferSize.RightInclusive(), searchStart.y - 1 };
            searchStart = { bufferSize.Left(), std::max(searchStart.y - viewportHeight, bufferSize.Top()) };
        }
        searchArea = Viewport::FromDimensions(searchStart, searchEnd.x + 1, searchEnd.y + 1);

        const til::point bufferStart{ bufferSize.Origin() };
        const til::point bufferEnd{ bufferSize.RightInclusive(), ViewEndIndex() };
        while (!result && bufferSize.IsInBounds(searchStart) && bufferSize.IsInBounds(searchEnd) && searchStart <= searchEnd && bufferStart <= searchStart && searchEnd <= bufferEnd)
        {
            auto patterns = _getPatterns(searchStart.y, searchEnd.y);
            resultList = patterns.findContained(convertToSearchArea(searchStart), convertToSearchArea(searchEnd));
            result = extractResultFromList(resultList);
            if (!result)
            {
                if (dir == SearchDirection::Forward)
                {
                    searchStart.y += 1;
                    searchEnd.y = std::min(searchStart.y + viewportHeight, ViewEndIndex());
                }
                else
                {
                    searchEnd.y -= 1;
                    searchStart.y = std::max(searchEnd.y - viewportHeight, bufferSize.Top());
                }
                searchArea = Viewport::FromDimensions(searchStart, searchEnd.x + 1, searchEnd.y + 1);
            }
        }

        // 1.C) Nothing was found. Bail!
        if (!result.has_value())
        {
            return;
        }
    }

    // 2. Select the hyperlink
    _selection->start = result->first;
    _selection->pivot = result->first;
    _selection->end = result->second;
    bufferSize.DecrementInBounds(_selection->end);
    _selectionIsTargetingUrl = true;
    _selectionEndpoint = SelectionEndpoint::End;

    // 3. Scroll to the selected area (if necessary)
    _ScrollToPoint(_selection->end);
}

Terminal::UpdateSelectionParams Terminal::ConvertKeyEventToUpdateSelectionParams(const ControlKeyStates mods, const WORD vkey) const noexcept
{
    if ((_selectionMode == SelectionInteractionMode::Mark || mods.IsShiftPressed()) && !mods.IsAltPressed())
    {
        if (mods.IsCtrlPressed())
        {
            // Ctrl + Shift + _
            // (Mark Mode) Ctrl + _
            switch (vkey)
            {
            case VK_LEFT:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Left, SelectionExpansion::Word };
            case VK_RIGHT:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Right, SelectionExpansion::Word };
            case VK_HOME:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Left, SelectionExpansion::Buffer };
            case VK_END:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Right, SelectionExpansion::Buffer };
            default:
                break;
            }
        }
        else
        {
            // Shift + _
            // (Mark Mode) Just the vkeys
            switch (vkey)
            {
            case VK_HOME:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Left, SelectionExpansion::Viewport };
            case VK_END:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Right, SelectionExpansion::Viewport };
            case VK_PRIOR:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Up, SelectionExpansion::Viewport };
            case VK_NEXT:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Down, SelectionExpansion::Viewport };
            case VK_LEFT:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Left, SelectionExpansion::Char };
            case VK_RIGHT:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Right, SelectionExpansion::Char };
            case VK_UP:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Up, SelectionExpansion::Char };
            case VK_DOWN:
                return UpdateSelectionParams{ std::in_place, SelectionDirection::Down, SelectionExpansion::Char };
            default:
                break;
            }
        }
    }
    return std::nullopt;
}

void Terminal::InDelimiter(std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter)
{
    auto targetPos{ WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) ? _selection->start : _selection->end };
    _InDelimiter(targetPos, startDelimiter, endDelimiter, includeDelimiter);
}

void Terminal::TilChar(WORD vkey, bool isVisual, bool isUpperCase)
{
    auto targetPos = _selection->end;
    _TilChar(targetPos, vkey, isVisual, isUpperCase);
}

void Terminal::FindChar(WORD vkey, bool isVisual, bool isUpperCase)
{
    auto targetPos{ WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) ? _selection->start : _selection->end };
    _FindChar(targetPos, vkey, isVisual, isUpperCase);
}

void Terminal::FindCharBack(WORD vkey, bool isVisual, bool isUpperCase)
{
    auto targetPos{ WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) ? _selection->start : _selection->end };
    _FindCharBack(targetPos, vkey, isVisual, isUpperCase);
}

void Terminal::TilCharBack(WORD vkey, bool isVisual, bool isUpperCase)
{
    auto targetPos{ _selection->end };
    _TilCharBack(targetPos, vkey, isVisual, isUpperCase);
}

void Terminal::SetPivot()
{
    auto targetPos{ WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) ? _selection->start : _selection->end };
    _selection->pivot = targetPos;
}

void Terminal::SelectTop(bool isVisual)
{
    if (isVisual)
    {
        _selection->start = til::point{ 0, 0 };
        _ScrollToPoint(til::point{ 0, 0 });
    }
    else
    {
        DWORD mods = 0;

        if (isVisual)
        {
            mods = 280;
        }

        UpdateSelection(SelectionDirection::Up, SelectionExpansion::Buffer, ControlKeyStates{ mods });
    }
}

void Terminal::SelectBottom(bool isVisual)
{
    DWORD mods = 0;

    if (isVisual)
    {
        mods = 280;
    }

    UpdateSelection(SelectionDirection::Down, SelectionExpansion::Buffer, ControlKeyStates{ mods });
}

void Terminal::SelectHalfPageUp(bool isVisual)
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    auto targetPos{  _selection->start.y < _selection->pivot.y ? _selection->start : _selection->end  };

    const auto viewportHeight{ _GetMutableViewport().Height() };
    const auto newY{ targetPos.y - (viewportHeight / 2) };
    const auto newPos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ targetPos.x, newY };

    if (!isVisual)
    {
        _selection->start = newPos;
        _selection->end = newPos;
    }
    else if (newPos < _selection->pivot)
    {
        _selection->start = newPos;
        _selection->end = _selection->end;
    }
    else
    {
        _selection->start = _selection->pivot;
        _selection->end = newPos;
    }

    _ScrollToPoint(newPos);
}

void Terminal::SelectHalfPageDown(bool isVisual)
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    auto targetPos{ _selection->end.y > _selection->pivot.y ? _selection->end : _selection->start };

    const auto viewportHeight{ _GetMutableViewport().Height() };
    const auto mutableBottom{ _GetMutableViewport().BottomInclusive() };
    const auto newY{ targetPos.y + (viewportHeight / 2) };
    const auto newPos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ targetPos.x, newY };

    if (!isVisual)
    {
        _selection->start = newPos;
        _selection->end = newPos;
    }
    else if (newPos < _selection->pivot)
    {
        _selection->start = newPos;
        _selection->end = _selection->end;
    }
    else
    {
        _selection->start = _selection->pivot;
        _selection->end = newPos;
    }

    _ScrollToPoint(newPos);
}

void Terminal::SelectPageUp(bool isVisual)
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    auto targetPos{ _selection->start.y < _selection->pivot.y ? _selection->start : _selection->end };

    const auto viewportHeight{ _GetMutableViewport().Height() };
    const auto newY{ targetPos.y - viewportHeight };
    const auto newPos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ targetPos.x, newY };

    if (!isVisual)
    {
        _selection->start = newPos;
        _selection->end = newPos;
    }
    else if (newPos < _selection->pivot)
    {
        _selection->start = newPos;
        _selection->end = _selection->end;
    }
    else
    {
        _selection->start = _selection->pivot;
        _selection->end = newPos;
    }

    _ScrollToPoint(newPos);
}

void Terminal::SelectPageDown(bool isVisual)
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    auto targetPos{ _selection->end.y > _selection->pivot.y ? _selection->end : _selection->start };

    const auto viewportHeight{ _GetMutableViewport().Height() };
    const auto mutableBottom{ _GetMutableViewport().BottomInclusive() };
    const auto newY{ targetPos.y + viewportHeight };
    const auto newPos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ targetPos.x, newY };

    if (!isVisual)
    {
        _selection->start = newPos;
        _selection->end = newPos;
    }
    else if (newPos < _selection->pivot)
    {
        _selection->start = newPos;
        _selection->end = _selection->end;
    }
    else
    {
        _selection->start = _selection->pivot;
        _selection->end = newPos;
    }

    _ScrollToPoint(newPos);
}

void Terminal::SelectCharLeft(bool isVisual)
{
    DWORD mods = 0;

    if (isVisual)
    {
        mods = 280;
    }

    UpdateSelection(SelectionDirection::Left, SelectionExpansion::Char, ControlKeyStates{ mods });
}

void Terminal::SelectCharRight(bool isVisual)
{
    DWORD mods = 0;

    if (isVisual)
    {
        mods = 280;
    }

    UpdateSelection(SelectionDirection::Right, SelectionExpansion::Char, ControlKeyStates{ mods });
}

void Terminal::SelectDown(bool isVisual)
{
    DWORD mods = 0;

    if (isVisual)
    {
        mods = 280;
    }

    UpdateSelection(SelectionDirection::Down, SelectionExpansion::Char, ControlKeyStates{ mods });
}

void Terminal::SelectUp(bool isVisual)
{
    DWORD mods = 0;

    if (isVisual)
    {
        mods = 280;
    }

    UpdateSelection(SelectionDirection::Up, SelectionExpansion::Char, ControlKeyStates{ mods });
}

void Terminal::SelectLineRight(bool isVisual)
{
    auto targetPos{ WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) ? _selection->start : _selection->end };
    auto endPair = _activeBuffer().GetLineEnd(targetPos);
    _selection->end = endPair;
    if (isVisual)
    {
        _selection->start = targetPos;
    }
    else
    {
        _selection->start = endPair;
    }
}

void Terminal::SelectLineUp(bool /*isVisual*/)
{
    if (_selection->start.y > 0)
    {
        if (_selection->end.y > _selection->pivot.y)
        {
            auto end = _activeBuffer().GetLineEnd(til::point{ 0, _selection->end.y - 1 });
            _selection->end = end;
        }
        else
        {
            _selection->start = til::point{ 0, _selection->start.y - 1 };
        }
    }
}

void Terminal::SelectLineDown(bool /*isVisual*/)
{
    if (_selection->start.y < _selection->pivot.y)
    {
        auto start = til::point{ 0, _selection->start.y + 1 };
        _selection->start = start;
    }
    else
    {
        auto end = _activeBuffer().GetLineEnd(til::point{ 0, _selection->end.y + 1 });
        _selection->end = end;
    }
}

void Terminal::SelectLineLeft(bool isVisual)
{
    DWORD mods = 0;

    if (isVisual)
    {
        mods = 280;
    }
    UpdateSelection(SelectionDirection::Left, SelectionExpansion::Viewport, ControlKeyStates{ mods });
}

void Terminal::SelectWordLeft(bool isVisual)
{
    DWORD mods = 0;

    if (isVisual)
    {
        mods = 280;
    }

    UpdateSelection(SelectionDirection::Left, SelectionExpansion::Word, ControlKeyStates{ mods });
}

void Terminal::SelectWordStartRight(bool isVisual, bool isLargeWord)
{
    auto delimiters = _wordDelimiters;
    if (isLargeWord)
    {
        delimiters = L"";
    }

    auto startPoint = _selection->start < _selection->pivot ? _selection->start : _selection->end;

    auto start = _activeBuffer().GetStartOfNextWord(startPoint, delimiters);

    if (start.second)
    {
        _selection->end = start.first;
        if (!isVisual)
        {
            _selection->start = _selection->end;
        }
        else if (start.first < _selection->pivot)
        {
            _selection->end = _selection->pivot;
            _selection->start = start.first;
        }
        else
        {
            _selection->end = start.first;
            _selection->start = _selection->pivot;
        }
    }
    else
    {
        auto startOfNextLine = til::point{ 0, _selection->end.y + 1 };
        _selection->end = startOfNextLine;
        if (!isVisual)
        {
            _selection->start = _selection->end;
        }
    }
}

void Terminal::SelectWordLeft(bool isVisual, bool isLargeWord)
{
    auto delimiters = _wordDelimiters;
    if (isLargeWord)
    {
        delimiters = L"";
    }

    auto startPoint = _selection->end > _selection->pivot ? _selection->end : _selection->start;

    auto startPair = _activeBuffer().GetStartOfWord(startPoint, delimiters);
    if (startPair.second)
    {
        if (startPair.first == startPoint)
        {
            startPair = _activeBuffer().GetEndOfPreviousWord(startPair.first, delimiters);
            if (startPair.second)
            {
                startPair = _activeBuffer().GetStartOfWord(startPair.first, delimiters);
            }
        }

        if (startPair.second)
        {
            if (!isVisual)
            {
                _selection->end = startPair.first;
                _selection->start = _selection->end;
            }
            else if (startPair.first < _selection->pivot)
            {
                _selection->end = _selection->pivot;
                _selection->start = startPair.first;
            }
            else
            {
                _selection->end = startPair.first;
            }
        }
        else
        {
            auto startOfPreviousLine = til::point{ 0, _selection->end.y - 1 };
            auto endOfLine = _activeBuffer().GetLineEnd(startOfPreviousLine);
            startPair = _activeBuffer().GetStartOfWord(endOfLine, delimiters);
            if (startPair.second)
            {
                _selection->start = startPair.first;
                if (!isVisual)
                {
                    _selection->end = _selection->end;
                }
            }
        }
    }
}

void Terminal::SelectWordRight(bool isVisual, bool isLargeWord)
{
    auto delimiters = _wordDelimiters;
    if (isLargeWord)
    {
        delimiters = L"";
    }

    auto startPoint = _selection->start < _selection->pivot ? _selection->start : _selection->end;

    auto endPair = _activeBuffer().GetEndOfWord(startPoint, delimiters);
    if (endPair.second)
    {
        if (endPair.first == startPoint)
        {
            endPair = _activeBuffer().GetStartOfNextWord(endPair.first, delimiters);
            if (endPair.second)
            {
                endPair = _activeBuffer().GetEndOfWord(endPair.first, delimiters);
            }
        }

        if (endPair.second)
        {
            if (!isVisual)
            {
                _selection->end = endPair.first;
                _selection->start = _selection->end;
            }
            else if (endPair.first < _selection->pivot)
            {
                _selection->end = _selection->pivot;
                _selection->start = endPair.first;
            }
            else
            {
                _selection->end = endPair.first;
                _selection->start = _selection->pivot;
            }
        }
        else
        {
            auto startOfNextLine = til::point{ 0, _selection->end.y + 1 };
            endPair = _activeBuffer().GetEndOfWord(startOfNextLine, delimiters);
            if (endPair.second)
            {
                _selection->end = endPair.first;
                if (!isVisual)
                {
                    _selection->start = _selection->end;
                }
            }
        }
    }
}

void Terminal::SelectInWord(bool largeWord)
{
    auto delimiters = _wordDelimiters;
    if (largeWord)
    {
        delimiters = L"";
    }

    auto targetPos{ _selection->end };
    _InWord(targetPos, delimiters);
}

// Method Description:
// - updates the selection endpoints based on a direction and expansion mode. Primarily used for keyboard selection.
// Arguments:
// - direction: the direction to move the selection endpoint in
// - mode: the type of movement to be performed (i.e. move by word)
// - mods: the key modifiers pressed when performing this update
void Terminal::UpdateSelection(SelectionDirection direction, SelectionExpansion mode, ControlKeyStates mods)
{
    // This is a special variable used to track if we should move the cursor when in mark mode.
    //   We have special functionality where if you use the "switchSelectionEndpoint" action
    //   when in mark mode (moving the cursor), we anchor an endpoint and you can use the
    //   plain arrow keys to move the endpoint. This way, you don't have to hold shift anymore!
    const bool shouldMoveBothEndpoints = _selectionMode == SelectionInteractionMode::Mark && !_anchorInactiveSelectionEndpoint && !mods.IsShiftPressed();

    // 1. Figure out which endpoint to update
    // [Mark Mode]
    // - shift pressed --> only move "end" (or "start" if "pivot" == "end")
    // - otherwise --> move both "start" and "end" (moving cursor)
    // [Quick Edit]
    // - just move "end" (or "start" if "pivot" == "end")
    _selectionEndpoint = static_cast<SelectionEndpoint>(0);
    if (shouldMoveBothEndpoints)
    {
        WI_SetAllFlags(_selectionEndpoint, SelectionEndpoint::Start | SelectionEndpoint::End);
    }
    else if (_selection->start == _selection->pivot)
    {
        WI_SetFlag(_selectionEndpoint, SelectionEndpoint::End);
    }
    else if (_selection->end == _selection->pivot)
    {
        WI_SetFlag(_selectionEndpoint, SelectionEndpoint::Start);
    }
    auto targetPos{ WI_IsFlagSet(_selectionEndpoint, SelectionEndpoint::Start) ? _selection->start : _selection->end };

    // 2 Perform the movement
    switch (mode)
    {
    case SelectionExpansion::Char:
        _MoveByChar(direction, targetPos);
        break;
    case SelectionExpansion::Word:
        _MoveByWord(direction, targetPos);
        break;
    case SelectionExpansion::Viewport:
        _MoveByViewport(direction, targetPos);
        break;
    case SelectionExpansion::Buffer:
        _MoveByBuffer(direction, targetPos);
        break;
    }

    // 3. Actually modify the selection state
    _selectionIsTargetingUrl = false;
    _selectionMode = std::max(_selectionMode, SelectionInteractionMode::Keyboard);
    if (shouldMoveBothEndpoints)
    {
        // [Mark Mode] + shift unpressed --> move all three (i.e. just use arrow keys)
        _selection->start = targetPos;
        _selection->end = targetPos;
        _selection->pivot = targetPos;
    }
    else
    {
        // [Mark Mode] + shift --> updating a standard selection
        // This is also standard quick-edit modification
        bool targetStart = false;
        std::tie(_selection->start, _selection->end) = _PivotSelection(targetPos, targetStart);

        // IMPORTANT! Pivoting the selection here might have changed which endpoint we're targeting.
        // So let's set the targeted endpoint again.
        WI_UpdateFlag(_selectionEndpoint, SelectionEndpoint::Start, targetStart);
        WI_UpdateFlag(_selectionEndpoint, SelectionEndpoint::End, !targetStart);
    }

    // 4. Scroll (if necessary)
    _ScrollToPoint(targetPos);
}

void Terminal::SelectAll()
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    _selection = SelectionAnchors{};
    _selection->start = bufferSize.Origin();
    _selection->end = { bufferSize.RightInclusive(), _GetMutableViewport().BottomInclusive() };
    _selection->pivot = _selection->end;
    _selectionMode = SelectionInteractionMode::Keyboard;
    _selectionIsTargetingUrl = false;
    _ScrollToPoint(_selection->start);
}

void Terminal::_MoveByChar(SelectionDirection direction, til::point& pos)
{
    switch (direction)
    {
    case SelectionDirection::Left:
        _activeBuffer().GetSize().DecrementInBounds(pos);
        pos = _activeBuffer().GetGlyphStart(pos);
        break;
    case SelectionDirection::Right:
        _activeBuffer().GetSize().IncrementInBounds(pos);
        pos = _activeBuffer().GetGlyphEnd(pos);
        break;
    case SelectionDirection::Up:
    {
        const auto bufferSize{ _activeBuffer().GetSize() };
        const auto newY{ pos.y - 1 };
        pos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ pos.x, newY };
        break;
    }
    case SelectionDirection::Down:
    {
        const auto bufferSize{ _activeBuffer().GetSize() };
        const auto mutableBottom{ _GetMutableViewport().BottomInclusive() };
        const auto newY{ pos.y + 1 };
        pos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ pos.x, newY };
        break;
    }
    }
}

void Terminal::_MoveByWord(SelectionDirection direction, til::point& pos)
{
    switch (direction)
    {
    case SelectionDirection::Left:
    {
        const auto wordStartPos{ _activeBuffer().GetWordStart(pos, _wordDelimiters) };
        if (_activeBuffer().GetSize().CompareInBounds(_selection->pivot, pos) < 0)
        {
            // If we're moving towards the pivot, move one more cell
            pos = wordStartPos;
            _activeBuffer().GetSize().DecrementInBounds(pos);
        }
        else if (wordStartPos == pos)
        {
            // already at the beginning of the current word,
            // move to the beginning of the previous word
            _activeBuffer().GetSize().DecrementInBounds(pos);
            pos = _activeBuffer().GetWordStart(pos, _wordDelimiters);
        }
        else
        {
            // move to the beginning of the current word
            pos = wordStartPos;
        }
        break;
    }
    case SelectionDirection::Right:
    {
        const auto wordEndPos{ _activeBuffer().GetWordEnd(pos, _wordDelimiters) };
        if (_activeBuffer().GetSize().CompareInBounds(pos, _selection->pivot) < 0)
        {
            // If we're moving towards the pivot, move one more cell
            pos = _activeBuffer().GetWordEnd(pos, _wordDelimiters);
            _activeBuffer().GetSize().IncrementInBounds(pos);
        }
        else if (wordEndPos == pos)
        {
            // already at the end of the current word,
            // move to the end of the next word
            _activeBuffer().GetSize().IncrementInBounds(pos);
            pos = _activeBuffer().GetWordEnd(pos, _wordDelimiters);
        }
        else
        {
            // move to the end of the current word
            pos = wordEndPos;
        }
        break;
    }
    case SelectionDirection::Up:
        _MoveByChar(direction, pos);
        pos = _activeBuffer().GetWordStart(pos, _wordDelimiters);
        break;
    case SelectionDirection::Down:
        _MoveByChar(direction, pos);
        pos = _activeBuffer().GetWordEnd(pos, _wordDelimiters);
        break;
    }
}

void Terminal::_FindChar(til::point& /*pos*/, WORD vkey, bool isVisual, bool isUpperCase)
{
    BYTE keyboardState[256];
    GetKeyboardState(keyboardState);
    if (isUpperCase)
    {
        keyboardState[VK_SHIFT] = 0x80;
    }
    WCHAR charBuffer[2] = { 0 };
    ToUnicode(vkey, 0, keyboardState, charBuffer, 2, 0);

    auto adjustedEnd = til::point{ _selection->end.x + 1, _selection->end.y };
    auto adjustedStart = til::point{ _selection->start.x + 1, _selection->start.y };

    auto startPoint = _selection->start < _selection->pivot ? adjustedStart : adjustedEnd;

    til::point originalStart = _selection->start;

    auto result = _activeBuffer().FindChar(startPoint, charBuffer);

    if (!result.second)
    {
        return;
    }

    auto adjustedResult = til::point{ result.first.x, result.first.y };

    if (!isVisual)
    {
        _selection->start = adjustedResult;
        _selection->end = adjustedResult;
    }
    else if (adjustedStart < _selection->pivot)
    {
        _selection->start = adjustedResult;
        _selection->end = _selection->pivot;
    }
    else
    {
        _selection->end = adjustedResult;
        _selection->start = _selection->pivot;
    }
}

void Terminal::_TilChar(til::point& /*pos*/, WORD vkey, bool isVisual, bool isUpperCase)
{
    BYTE keyboardState[256];
    GetKeyboardState(keyboardState);
    if (isUpperCase)
    {
        keyboardState[VK_SHIFT] = 0x80;
    }
    WCHAR charBuffer[2] = { 0 };
    ToUnicode(vkey, 0, keyboardState, charBuffer, 2, 0);

    auto adjustedEnd = til::point{ _selection->end.x + 2, _selection->end.y };
    auto adjustedStart = til::point{ _selection->start.x + 2, _selection->start.y };

    auto startPoint = _selection->start < _selection->pivot ? adjustedStart : adjustedEnd;

    til::point originalStart = _selection->start;

    auto result = _activeBuffer().FindChar(startPoint, charBuffer);

    if (!result.second)
    {
        return;
    }

    auto adjustedResult = til::point{ result.first.x - 1, result.first.y };

    if (!isVisual)
    {
        _selection->start = adjustedResult;
        _selection->end = adjustedResult;
    }
    else if (adjustedStart < _selection->pivot)
    {
        _selection->start = adjustedResult;
        _selection->end = _selection->pivot;
    }
    else
    {
        _selection->end = adjustedResult;
        _selection->start = _selection->pivot;
    }
}

void Terminal::_FindCharBack(til::point& /*pos*/, WORD vkey, bool isVisual, bool isUpperCase)
{
    BYTE keyboardState[256];
    GetKeyboardState(keyboardState);
    if (isUpperCase)
    {
        keyboardState[VK_SHIFT] = 0x80;
    }
    WCHAR charBuffer[2] = { 0 };
    ToUnicode(vkey, 0, keyboardState, charBuffer, 2, 0);
    
    auto adjustedEnd = til::point{ _selection->end.x - 1, _selection->end.y };
    auto adjustedStart = til::point{ _selection->start.x - 1, _selection->start.y };

    auto startPoint = _selection->end > _selection->pivot ? adjustedEnd : adjustedStart;

    auto result = _activeBuffer().FindCharReverse(startPoint, charBuffer);

    if (!result.second)
    {
        return;
    }

    auto adjustedResult = til::point{ result.first.x - 1, result.first.y };

    if (!isVisual)
    {
        _selection->start = adjustedResult;
        _selection->end = adjustedResult;
    }
    else if (adjustedEnd > _selection->pivot)
    {
        _selection->start = _selection->pivot;
        _selection->end = adjustedResult;
    }
    else
    {
        _selection->end = _selection->pivot;
        _selection->start = adjustedResult;
    }
}

void Terminal::_TilCharBack(til::point& /*pos*/, WORD vkey, bool isVisual, bool isUpperCase)
{
    BYTE keyboardState[256];
    GetKeyboardState(keyboardState);
    if (isUpperCase)
    {
        keyboardState[VK_SHIFT] = 0x80;
    }
    WCHAR charBuffer[2] = { 0 };
    ToUnicode(vkey, 0, keyboardState, charBuffer, 2, 0);

    auto adjustedEnd = til::point{ _selection->end.x - 2, _selection->end.y };
    auto adjustedStart = til::point{ _selection->start.x - 2, _selection->start.y };

    auto startPoint = _selection->end > _selection->pivot ? adjustedEnd : adjustedStart;

    auto result = _activeBuffer().FindCharReverse(startPoint, charBuffer);

    if (!result.second)
    {
        return;
    }

    auto adjustedResult = til::point{ result.first.x, result.first.y };

    if (!isVisual)
    {
        _selection->start = adjustedResult;
        _selection->end = adjustedResult;
    }
    else if (adjustedEnd > _selection->pivot)
    {
        _selection->start = _selection->pivot;
        _selection->end = adjustedResult;
    }
    else
    {
        _selection->end = _selection->pivot;
        _selection->start = adjustedResult;
    }
}

void Terminal::_InDelimiter(til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter)
{
    auto start = _activeBuffer().FindCharReverse(pos, startDelimiter);
    auto end = _activeBuffer().FindChar(pos, endDelimiter);

    if (!start.second || !end.second)
    {
        return;
    }

    if (includeDelimiter)
    {
        _selection->start = til::point{ start.first.x - 1, start.first.y };
        _selection->end = til::point{ end.first.x + 1, end.first.y };
    }
    else
    {
        _selection->start = start.first;
        _selection->end = end.first;
    }
}

void Terminal::_InWord(til::point& pos, std::wstring_view delimiters)
{
    auto endPair = _activeBuffer().GetWordEnd2(pos, delimiters);
    if (endPair.second)
    {
        _selection->end = endPair.first;
        _selection->start = _activeBuffer().GetWordStart(pos, delimiters);
    }
}

void Terminal::_MoveByViewport(SelectionDirection direction, til::point& pos) noexcept
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    switch (direction)
    {
    case SelectionDirection::Left:
        pos = { bufferSize.Left(), pos.y };
        break;
    case SelectionDirection::Right:
        pos = { bufferSize.RightInclusive(), pos.y };
        break;
    case SelectionDirection::Up:
    {
        const auto viewportHeight{ _GetMutableViewport().Height() };
        const auto newY{ pos.y - viewportHeight };
        pos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ pos.x, newY };
        break;
    }
    case SelectionDirection::Down:
    {
        const auto viewportHeight{ _GetMutableViewport().Height() };
        const auto mutableBottom{ _GetMutableViewport().BottomInclusive() };
        const auto newY{ pos.y + viewportHeight };
        pos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ pos.x, newY };
        break;
    }
    }
}

void Terminal::_MoveByHalfViewport(SelectionDirection direction, til::point& pos) noexcept
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    switch (direction)
    {
    case SelectionDirection::Left:
        pos = { bufferSize.Left(), pos.y };
        break;
    case SelectionDirection::Right:
        pos = { bufferSize.RightInclusive(), pos.y };
        break;
    case SelectionDirection::Up:
    {
        const auto viewportHeight{ _GetMutableViewport().Height() };
        const auto newY{ pos.y - (viewportHeight / 2)};
        pos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ pos.x, newY };
        break;
    }
    case SelectionDirection::Down:
    {
        const auto viewportHeight{ _GetMutableViewport().Height() };
        const auto mutableBottom{ _GetMutableViewport().BottomInclusive() };
        const auto newY{ pos.y + (viewportHeight / 2) };
        pos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ pos.x, newY };
        break;
    }
    }

    _selection->start = pos;
    _selection->end = pos;
    _selection->pivot = pos;

    _ScrollToPoint(pos);
}

void Terminal::_MoveByBuffer(SelectionDirection direction, til::point& pos) noexcept
{
    const auto bufferSize{ _activeBuffer().GetSize() };
    switch (direction)
    {
    case SelectionDirection::Left:
    case SelectionDirection::Up:
        pos = bufferSize.Origin();
        break;
    case SelectionDirection::Right:
    case SelectionDirection::Down:
        pos = { bufferSize.RightInclusive(), _GetMutableViewport().BottomInclusive() };
        break;
    }
}

// Method Description:
// - clear selection data and disable rendering it
#pragma warning(disable : 26440) // changing this to noexcept would require a change to ConHost's selection model
void Terminal::ClearSelection()
{
    _assertLocked();
    _searchSelections.clear();
    _selection = std::nullopt;
    _selectionMode = SelectionInteractionMode::None;
    _selectionIsTargetingUrl = false;
    _selectionEndpoint = static_cast<SelectionEndpoint>(0);
    _anchorInactiveSelectionEndpoint = false;
}

// Method Description:
// - get wstring text from highlighted portion of text buffer
// Arguments:
// - singleLine: collapse all of the text to one line
// Return Value:
// - wstring text from buffer. If extended to multiple lines, each line is separated by \r\n
const TextBuffer::TextAndColor Terminal::RetrieveSelectedTextFromBuffer(bool singleLine)
{
    const auto selectionRects = _GetSelectionRects();

    const auto GetAttributeColors = [&](const auto& attr) {
        return _renderSettings.GetAttributeColors(attr);
    };

    // GH#6740: Block selection should preserve the visual structure:
    // - CRLFs need to be added - so the lines structure is preserved
    // - We should apply formatting above to wrapped rows as well (newline should be added).
    // GH#9706: Trimming of trailing white-spaces in block selection is configurable.
    const auto includeCRLF = !singleLine || _blockSelection;
    const auto trimTrailingWhitespace = !singleLine && (!_blockSelection || _trimBlockSelection);
    const auto formatWrappedRows = _blockSelection;
    return _activeBuffer().GetText(includeCRLF, trimTrailingWhitespace, selectionRects, GetAttributeColors, formatWrappedRows);
}

// Method Description:
// - convert viewport position to the corresponding location on the buffer
// Arguments:
// - viewportPos: a coordinate on the viewport
// Return Value:
// - the corresponding location on the buffer
til::point Terminal::_ConvertToBufferCell(const til::point viewportPos) const
{
    const auto yPos = _VisibleStartIndex() + viewportPos.y;
    til::point bufferPos = { viewportPos.x, yPos };
    _activeBuffer().GetSize().Clamp(bufferPos);
    return bufferPos;
}

// Method Description:
// - if necessary, scroll the viewport such that the given point is visible
// Arguments:
// - pos: a coordinate relative to the buffer (not viewport)
void Terminal::_ScrollToPoint(const til::point pos)
{
    if (const auto visibleViewport = _GetVisibleViewport(); !visibleViewport.IsInBounds(pos))
    {
        if (const auto amtAboveView = visibleViewport.Top() - pos.y; amtAboveView > 0)
        {
            // anchor is above visible viewport, scroll by that amount
            _scrollOffset += amtAboveView;
        }
        else
        {
            // anchor is below visible viewport, scroll by that amount
            const auto amtBelowView = pos.y - visibleViewport.BottomInclusive();
            _scrollOffset -= amtBelowView;
        }
        _NotifyScrollEvent();
        _activeBuffer().TriggerScroll();
    }
}
