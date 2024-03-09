#include "pch.h"
#include "VimModeProxy.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"

VimModeProxy::VimModeProxy(Microsoft::Terminal::Core::Terminal *terminal)
{
    _terminal = terminal;
}

void VimModeProxy::TilChar(std::wstring_view vkey, bool isVisual)
{
    til::point target;
    if (_FindChar(vkey, true, target))
    {
        _UpdateSelection(isVisual, target);
    }
}

void VimModeProxy::FindChar(std::wstring_view vkey, bool isVisual)
{
    til::point target;
    if (_FindChar(vkey, false, target))
    {
        _UpdateSelection(isVisual, target);
    }
}

void VimModeProxy::FindCharBack(std::wstring_view vkey, bool isVisual)
{
    til::point target;
    if (_FindCharBack(vkey, false, target))
    {
        _UpdateSelection(isVisual, target);
    }
}

void VimModeProxy::TilCharBack(std::wstring_view vkey, bool isVisual)
{
    til::point target;
    if (_FindCharBack(vkey, true, target))
    {
        _UpdateSelection(isVisual, target);
    }
}

void VimModeProxy::InDelimiter(std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter)
{
    auto selection = _terminal->GetSelectionAnchors();
    _InDelimiter(selection->start, startDelimiter, endDelimiter, includeDelimiter);
}

void VimModeProxy::SelectWordRight(bool isVisual, bool isLargeWord)
{
    auto selection = _terminal->GetSelectionAnchors();
    auto delimiters = _wordDelimiters;
    if (isLargeWord)
    {
        delimiters = L"";
    }

    auto startPoint = selection->start < selection->pivot ? selection->start : selection->end;

    auto endPair = _GetEndOfWord(startPoint, delimiters);
    if (endPair.second)
    {
        if (endPair.first == startPoint)
        {
            endPair = _GetStartOfNextWord(endPair.first, delimiters);
            if (endPair.second)
            {
                endPair = _GetEndOfWord(endPair.first, delimiters);
            }
        }

        if (endPair.second)
        {
            _UpdateSelection(isVisual, endPair.first);
        }
        else
        {
            auto yToMove = selection->start < selection->pivot ? selection->start.y : selection->end.y;
            auto startOfNextLine = til::point{ 0, yToMove + 1 };
            endPair = _GetEndOfWord(startOfNextLine, delimiters);
            if (endPair.second)
            {
                _UpdateSelection(isVisual, endPair.first);
            }
        }
    }
}

void VimModeProxy::SelectWordLeft(bool isVisual, bool isLargeWord)
{
    auto delimiters = _wordDelimiters;
    if (isLargeWord)
    {
        delimiters = L"";
    }

    auto selection = _terminal->GetSelectionAnchors();
    auto startPoint = selection->end > selection->pivot ? selection->end : selection->start;

    auto startPair = _GetStartOfWord(startPoint, delimiters);
    if (startPair.second)
    {
        if (startPair.first == startPoint)
        {
            startPair = _GetEndOfPreviousWord(startPair.first, delimiters);
            if (startPair.second)
            {
                startPair = _GetStartOfWord(startPair.first, delimiters);
            }
        }

        if (startPair.second)
        {
            _UpdateSelection(isVisual, startPair.first);
        }
        else
        {
            auto startOfPreviousLine = til::point{ 0, selection->end.y - 1 };
            auto endOfLine = _GetLineEnd(startOfPreviousLine);
            startPair = _GetStartOfWord(endOfLine, delimiters);
            if (startPair.second)
            {
                _UpdateSelection(isVisual, startPair.first);
            }
        }
    }
}

void VimModeProxy::SelectWordStartRight(bool isVisual, bool isLargeWord)
{
    auto delimiters = _wordDelimiters;
    if (isLargeWord)
    {
        delimiters = L"";
    }

    auto selection = _terminal->GetSelectionAnchors();
    auto startPoint = selection->start < selection->pivot ? selection->start : selection->end;

    auto start = _GetStartOfNextWord(startPoint, delimiters);

    if (start.second)
    {
        _UpdateSelection(isVisual, start.first);
    }
    else
    {
        auto yToMove = selection->start < selection->pivot ? selection->start.y : selection->end.y;
        auto startOfNextLine = til::point{ 0, yToMove + 1 };
        _UpdateSelection(isVisual, startOfNextLine);
    }
}

void VimModeProxy::SelectInWord(bool largeWord)
{
    auto delimiters = _wordDelimiters;
    if (largeWord)
    {
        delimiters = L"";
    }

    auto targetPos{ _terminal->GetSelectionAnchors()->end };
    _InWord(targetPos, delimiters);
}

void VimModeProxy::SelectLineRight(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    til::CoordType endLine = selection->end.y;
    if (_terminal->IsBlockSelection())
    {
        endLine = selection->start.y == selection->pivot.y ? selection->end.y : selection->start.y;
    }
    while (_terminal->GetTextBuffer().GetRowByOffset(endLine).WasWrapForced())
    {
        endLine++;
    }

    auto lastNonSpaceColumn = std::max(0, _terminal->GetTextBuffer().GetRowByOffset(endLine).GetLastNonSpaceColumn() - 1);
    _UpdateSelection(isVisual, til::point{ lastNonSpaceColumn, endLine });
}

til::CoordType _getStartLineOfRow2(TextBuffer& textBuffer, til::CoordType row)
{
    auto result = row;
    while (textBuffer.GetRowByOffset(result - 1).WasWrapForced())
    {
        result--;
    }
    return result;
}

void VimModeProxy::SelectLineLeft(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    til::CoordType startLine = _getStartLineOfRow2(_terminal->GetTextBuffer(), selection->end.y);
    if (_terminal->IsBlockSelection())
    {
        startLine = selection->start.y == selection->pivot.y ? selection->end.y : selection->start.y;
    }
    _UpdateSelection(isVisual, til::point{ 0, startLine });
}

void VimModeProxy::SelectLineUp()
{
    auto selection = _terminal->GetSelectionAnchors();
    if (selection->start.y > 0 || selection->end.y > 0)
    {
        if (selection->end.y > selection->pivot.y)
        {
            auto end = _GetLineEnd(til::point{ 0, selection->end.y - 1 });
            selection->end = end;
        }
        else if (selection->end.y == selection->pivot.y)
        {
            auto currentEnd = _GetLineEnd(til::point{ 0, selection->end.y });
            selection->end = currentEnd;
            selection->start = til::point{ 0, selection->start.y - 1 };
            selection->pivot = currentEnd;
        }
        else
        {
            selection->start = til::point{ 0, selection->start.y - 1 };
        }
    }
}

void VimModeProxy::SelectLineDown()
{
    auto selection = _terminal->GetSelectionAnchors();
    if (selection->start.y < selection->pivot.y)
    {
        auto start = til::point{ 0, selection->start.y + 1 };
        selection->start = start;
    }
    else if (selection->start.y == selection->pivot.y)
    {
        auto currentStart = til::point{ 0, selection->start.y };
        auto currentEnd = _GetLineEnd(til::point{ 0, selection->end.y + 1 });

        selection->end = currentEnd;
        selection->start = currentStart;
        selection->pivot = currentStart;
    }
    else
    {
        auto end = _GetLineEnd(til::point{ 0, selection->end.y + 1 });
        selection->end = end;
    }
}

void VimModeProxy::SelectTop(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    if (isVisual)
    {
        selection->start = til::point{ 0, 0 };
        _terminal->UserScrollViewport(0);
    }
    else
    {
        selection->start = til::point{ 0, 0 };
        selection->pivot = til::point{ 0, 0 };
        selection->end = til::point{ 0, 0 };
        _terminal->UserScrollViewport(0);
    }
}

void VimModeProxy::SelectBottom(bool isVisual)
{
    auto lastChar = _terminal->GetTextBuffer().GetLastNonSpaceCharacter();
    _UpdateSelection(isVisual, lastChar);
    _terminal->UserScrollViewport(lastChar.y);
}

void VimModeProxy::SelectHalfPageUp(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };
    auto targetPos{ selection->start.y < selection->pivot.y ? selection->start : selection->end };

    const auto viewportHeight{ _terminal->GetViewport().Height() };
    const auto newY{ targetPos.y - (viewportHeight / 2) };
    const auto newPos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ targetPos.x, newY };

    _UpdateSelection(isVisual, newPos);
    _terminal->UserScrollViewport(newPos.y);
}

void VimModeProxy::SelectHalfPageDown(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };
    auto targetPos{ selection->end.y > selection->pivot.y ? selection->end : selection->start };

    const auto viewportHeight{ _terminal->GetViewport().Height() };
    const auto mutableBottom{ _terminal->GetViewport().BottomInclusive() };
    const auto newY{ targetPos.y + (viewportHeight / 2) };
    const auto newPos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ targetPos.x, newY };

    _UpdateSelection(isVisual, newPos);
    _terminal->UserScrollViewport(newPos.y);
}

void VimModeProxy::SelectPageUp(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };
    auto targetPos{ selection->start.y < selection->pivot.y ? selection->start : selection->end };

    const auto viewportHeight{ _terminal->GetViewport().Height() };
    const auto newY{ targetPos.y - viewportHeight };
    const auto newPos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ targetPos.x, newY };

    _UpdateSelection(isVisual, newPos);
    _terminal->UserScrollViewport(newPos.y);
}

void VimModeProxy::SelectPageDown(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };
    auto targetPos{ selection->end.y > selection->pivot.y ? selection->end : selection->start };

    const auto viewportHeight{ _terminal->GetViewport().Height() };
    const auto mutableBottom{ _terminal->GetViewport().BottomInclusive() };
    const auto newY{ targetPos.y + viewportHeight };
    const auto newPos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ targetPos.x, newY };

    _UpdateSelection(isVisual, newPos);
    _terminal->UserScrollViewport(newPos.y);
}

void VimModeProxy::SelectCharRight(bool isVisual)
{
    DWORD mods = isVisual ? 280 : 0;
    _terminal->UpdateSelection(::Microsoft::Terminal::Core::Terminal::SelectionDirection ::Right, ::Microsoft::Terminal::Core::Terminal::SelectionExpansion::Char, Microsoft::Terminal::Core::ControlKeyStates{ mods });
}

void VimModeProxy::SelectCharLeft(bool isVisual)
{
    DWORD mods = isVisual ? 280 : 0;
    _terminal->UpdateSelection(::Microsoft::Terminal::Core::Terminal::SelectionDirection::Left, ::Microsoft::Terminal::Core::Terminal::SelectionExpansion::Char, ::Microsoft::Terminal::Core::ControlKeyStates{ mods });
}

void VimModeProxy::SelectDown(bool isVisual)
{
    DWORD mods = isVisual ? 280 : 0;
    _terminal->UpdateSelection(::Microsoft::Terminal::Core::Terminal::SelectionDirection::Down, ::Microsoft::Terminal::Core::Terminal::SelectionExpansion::Char, ::Microsoft::Terminal::Core::ControlKeyStates{ mods });
}

void VimModeProxy::SelectUp(bool isVisual)
{
    DWORD mods = isVisual ? 280 : 0;
    _terminal->UpdateSelection(::Microsoft::Terminal::Core::Terminal::SelectionDirection::Up, ::Microsoft::Terminal::Core::Terminal::SelectionExpansion::Char, ::Microsoft::Terminal::Core::ControlKeyStates{ mods });
}

std::pair<til::point, bool> VimModeProxy::_GetLineFirstNonBlankChar(const til::point target) const
{
    const auto bufferSize = _terminal->GetTextBuffer().GetSize();

    auto result = target;
    bool found = false;

    while (result.x < bufferSize.RightInclusive())
    {
        auto classAt = _terminal->GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, L"");
        if (classAt != DelimiterClass::ControlChar)
        {
            found = true;
            break;
        }
        bufferSize.IncrementInBounds(result);
    }

    if (!found)
    {
        return { {}, false };
    }

    return { result, true };
}

void VimModeProxy::SelectLineFirstNonBlankChar(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    auto startLine = _getStartLineOfRow2(_terminal->GetTextBuffer(), selection->start.y);
    if (_terminal->IsBlockSelection())
    {
        startLine = selection->start.y == selection->pivot.y ? selection->end.y : selection->start.y;
    }
    auto startOfLine = til::point{ 0, startLine };
    auto firstNonBlankChar = _GetLineFirstNonBlankChar(startOfLine);

    if (firstNonBlankChar.second == true)
    {
        _UpdateSelection(isVisual, firstNonBlankChar.first);
    }
    else
    {
        _UpdateSelection(isVisual, startOfLine);
    }
}

bool VimModeProxy::_FindChar(std::wstring_view vkey, bool isTil, til::point& target)
{
    const auto selection = _terminal->GetSelectionAnchors();
    auto startPoint = selection->end == selection->pivot ?
                          til::point{ selection->start.x + 1, selection->start.y } :
                          til::point{ selection->end.x + 1, selection->end.y };

    if (isTil)
    {
        startPoint.x++;
    }

    auto newY = startPoint.y;
    auto lineWrapped = false;

    while (lineWrapped || newY == startPoint.y)
    {
        const auto startX = newY > startPoint.y ? 0 : startPoint.x;
        auto& row = _terminal->GetTextBuffer().GetRowByOffset(newY);
        for (auto i = startX; i < row.size(); i++)
        {
            const auto glyphAt = row.GlyphAt(i);
            if (glyphAt == vkey)
            {
                target = til::point{ i, newY };
                if (isTil)
                {
                    target.x--;
                }
                return true;
            }
        }

        lineWrapped = row.WasWrapForced();
        newY++;
    }
    return false;
}

bool VimModeProxy::_FindCharBack(std::wstring_view vkey, bool isTil, til::point& target)
{
    auto selection = _terminal->GetSelectionAnchors();
    const auto startPoint = selection->end == selection->pivot ?
                                til::point{ selection->start.x - 2, selection->start.y } :
                                til::point{ selection->end.x - 2, selection->end.y };

    auto newY = startPoint.y;

    while (_terminal->GetTextBuffer().GetRowByOffset(newY).WasWrapForced() || newY == startPoint.y)
    {
        auto& row = _terminal->GetTextBuffer().GetRowByOffset(newY);
        const auto startX = newY != startPoint.y ? row.size() : startPoint.x;

        for (auto i = startX; i >= 0; i--)
        {
            const auto glyphAt = row.GlyphAt(i);
            if (glyphAt == vkey)
            {
                target = til::point{ i, newY };
                if (isTil)
                {
                    target.x++;
                }
                return true;
            }
        }

        newY--;
    }

    return false;
}

void VimModeProxy::_UpdateSelection(bool isVisual, til::point adjusted)
{
    auto selection = _terminal->GetSelectionAnchors();
    if (isVisual)
    {
        auto pivotIsStart = selection->start == selection->pivot;
        //This means that the end is moving
        if (pivotIsStart)
        {
            if (adjusted < selection->pivot)
            {
                selection->start = adjusted;
                selection->end = selection->pivot;
            }
            else
            {
                selection->end = adjusted;
            }
        }
        //This means that the start is moving
        else
        {
            if (adjusted > selection->pivot)
            {
                selection->start = selection->pivot;
                selection->end = adjusted;
            }
            else
            {
                selection->start = adjusted;
            }
        }
    }
    else
    {
        selection->start = adjusted;
        selection->end = adjusted;
        selection->pivot = adjusted;
    }
    _terminal->SetSelectionAnchors(selection);
}

void VimModeProxy::_InDelimiter(til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter)
{
    til::CoordType startX = -1;
    til::CoordType endX = -1;

    for (auto i = 0; i < _terminal->GetTextBuffer().GetRowByOffset(pos.y).size(); i++)
    {
        auto g = _terminal->GetTextBuffer().GetRowByOffset(pos.y).GlyphAt(i);
        if (g == startDelimiter && startX == -1)
        {
            if (startX > i)
            {
                return;
            }
            else
            {
                startX = i;
                endX = -1;
            }
        }
        else if (g == endDelimiter && startX != -1)
        {
            if (i >= pos.x)
            {
                endX = i;
                break;
            }
            startX = -1;
            endX = -1;
        }
    }

    if (startX == -1 || endX == -1)
    {
        return;
    }

    auto selection = _terminal->GetSelectionAnchors();
    if (includeDelimiter)
    {
        selection->start = til::point{ startX, pos.y };
        selection->end = til::point{ endX, pos.y };
    }
    else
    {
        selection->start = til::point{ startX + 1, pos.y };
        selection->end = til::point{ endX - 1, pos.y };
    }
    selection->pivot = selection->start;
    //_terminal->SetSelectionAnchors(selection);
}

std::pair<til::point, bool> VimModeProxy::_GetEndOfWord(const til::point target, const std::wstring_view wordDelimiters) const
{
    const auto bufferSize = _terminal->GetTextBuffer().GetSize();

    // can't expand right
    if (target.x == bufferSize.RightInclusive())
    {
        return { {}, false };
    }

    auto result = target;
    bool found = false;

    // expand right until we hit the right boundary or a different delimiter class
    while (result.x < bufferSize.RightInclusive())
    {
        bufferSize.IncrementInBounds(result);
        auto classAt = _terminal->GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
        if (classAt == DelimiterClass::ControlChar || classAt == DelimiterClass::DelimiterChar)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        return { {}, false };
    }

    bufferSize.DecrementInBounds(result);

    return { result, true };
}

std::pair<til::point, bool> VimModeProxy::_GetStartOfWord(const til::point target, const std::wstring_view wordDelimiters) const
{
    const auto bufferSize = _terminal->GetTextBuffer().GetSize();

    auto result = target;
    bool found = false;

    while (result.x >= 0)
    {
        bufferSize.DecrementInBounds(result);
        auto classAt = _terminal->GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
        if (classAt == DelimiterClass::ControlChar || classAt == DelimiterClass::DelimiterChar || result.x == 0)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        return { {}, false };
    }

    if (result.x != 0)
    {
        bufferSize.IncrementInBounds(result);
    }

    return { result, true };
}

std::pair<til::point, bool> VimModeProxy::_GetStartOfNextWord(const til::point target, const std::wstring_view wordDelimiters) const
{
    auto wordEnd = _GetEndOfWord(target, wordDelimiters);

    if (!wordEnd.second)
    {
        return wordEnd;
    }

    const auto bufferSize = _terminal->GetTextBuffer().GetSize();

    // can't expand right
    if (target.x == bufferSize.RightInclusive())
    {
        return { {}, false };
    }

    auto result = wordEnd.first;
    bool found = false;

    bufferSize.IncrementInBounds(result);

    // expand right until we hit the right boundary or a different delimiter class
    while (result.x < bufferSize.RightInclusive())
    {
        auto classAt = _terminal->GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
        if (classAt != DelimiterClass::ControlChar || classAt == DelimiterClass::DelimiterChar)
        {
            found = true;
            break;
        }

        bufferSize.IncrementInBounds(result);
    }

    if (!found)
    {
        return { {}, false };
    }

    return { result, true };
}

til::point VimModeProxy::_GetLineEnd(const til::point target) const
{
    const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };

    // can't expand right
    if (target.x == bufferSize.RightInclusive())
    {
        return {};
    }

    auto result = target;
    til::CoordType lastNonControlChar = 0;

    // expand right until we hit the right boundary or a different delimiter class
    while (result.x < bufferSize.RightInclusive())
    {
        auto classAt = _terminal->GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, L"");
        if (classAt != DelimiterClass::ControlChar)
        {
            lastNonControlChar = result.x;
        }
        bufferSize.IncrementInBounds(result);
    }

    return til::point{ lastNonControlChar, target.y };
}

void VimModeProxy::_InWord(til::point& pos, std::wstring_view delimiters)
{
    auto endPair = _GetEndOfWord(pos, delimiters);
    auto startPair = _GetStartOfWord(pos, delimiters);

    auto selection = _terminal->GetSelectionAnchors();
    if (endPair.second && endPair.second)
    {
        selection->end = endPair.first;
        selection->pivot = endPair.first;
        selection->start = startPair.first;
    }
    //_terminal->SetSelectionAnchors(selection);
}

void VimModeProxy::_MoveByViewport(::Microsoft::Terminal::Core::Terminal::SelectionDirection direction, til::point& pos) noexcept
{
    const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };
    switch (direction)
    {
    case ::Microsoft::Terminal::Core::Terminal::SelectionDirection::Left:
        pos = { bufferSize.Left(), pos.y };
        break;
    case ::Microsoft::Terminal::Core::Terminal::SelectionDirection::Right:
        pos = { bufferSize.RightInclusive(), pos.y };
        break;
    case ::Microsoft::Terminal::Core::Terminal::SelectionDirection::Up:
    {
        const auto viewportHeight{ _terminal->GetViewport().Height() };
        const auto newY{ pos.y - viewportHeight };
        pos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ pos.x, newY };
        break;
    }
    case ::Microsoft::Terminal::Core::Terminal::SelectionDirection::Down:
    {
        const auto viewportHeight{ _terminal->GetViewport().Height() };
        const auto mutableBottom{ _terminal->GetViewport().BottomInclusive() };
        const auto newY{ pos.y + viewportHeight };
        pos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ pos.x, newY };
        break;
    }
    }
}

std::pair<til::point, bool> VimModeProxy::_GetEndOfPreviousWord(const til::point target, const std::wstring_view wordDelimiters) const
{
    auto wordStart = _GetStartOfWord(target, wordDelimiters);

    if (!wordStart.second)
    {
        return wordStart;
    }

    const auto bufferSize = _terminal->GetTextBuffer().GetSize();

    auto result = wordStart.first;
    bool found = false;

    bufferSize.DecrementInBounds(result);

    while (result.x >= 0)
    {
        auto classAt = _terminal->GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
        if (classAt != DelimiterClass::ControlChar || classAt == DelimiterClass::DelimiterChar || result.x == 0)
        {
            found = true;
            break;
        }

        bufferSize.DecrementInBounds(result);
    }

    if (!found)
    {
        return { {}, false };
    }

    return { result, true };
}

void VimModeProxy::_MoveByHalfViewport(::Microsoft::Terminal::Core::Terminal::SelectionDirection direction, til::point& pos) noexcept
{
    const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };
    switch (direction)
    {
    case ::Microsoft::Terminal::Core::Terminal::SelectionDirection::Left:
        pos = { bufferSize.Left(), pos.y };
        break;
    case ::Microsoft::Terminal::Core::Terminal::SelectionDirection::Right:
        pos = { bufferSize.RightInclusive(), pos.y };
        break;
    case ::Microsoft::Terminal::Core::Terminal::SelectionDirection::Up:
    {
        const auto viewportHeight{ _terminal->GetViewport().Height() };
        const auto newY{ pos.y - (viewportHeight / 2) };
        pos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ pos.x, newY };
        break;
    }
    case ::Microsoft::Terminal::Core::Terminal::SelectionDirection::Down:
    {
        const auto viewportHeight{ _terminal->GetViewport().Height() };
        const auto mutableBottom{ _terminal->GetViewport().BottomInclusive() };
        const auto newY{ pos.y + (viewportHeight / 2) };
        pos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ pos.x, newY };
        break;
    }
    }

    auto selection = _terminal->GetSelectionAnchors();
    selection->start = pos;
    selection->end = pos;
    selection->pivot = pos;

    //_ScrollToPoint(pos);
    _terminal->UserScrollViewport(pos.y);
}
