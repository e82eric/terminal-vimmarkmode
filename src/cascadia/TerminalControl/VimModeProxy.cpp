#include "pch.h"
#include "VimModeProxy.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"

VimModeProxy::VimModeProxy(std::shared_ptr<Microsoft::Terminal::Core::Terminal> terminal, winrt::Microsoft::Terminal::Control::implementation::ControlCore *controlCore, Search* searcher)
{
    _terminal = terminal;
    _controlCore = controlCore;
    _searcher = searcher;
}

bool comparePointsLessThanOrEqual(const til::point_span& span, const til::point& pt) {
    return std::tie(span.start.y, span.start.x) <= std::tie(pt.y, pt.x);
}

bool comparePointsLess(const til::point_span& span, const til::point& pt) {
    return std::tie(span.start.y, span.start.x) < std::tie(pt.y, pt.x);
}

void VimModeProxy::_moveToNextSearchResult(bool moveForward, bool isVisual)
{
    if (_searcher->Results().size() > 0)
    {
        const auto point = _terminal->GetSelectionAnchors()->start;
        const auto& results = _searcher->Results();

        std::vector<til::point_span>::const_iterator toSelect;
        if (!moveForward)
        {
            const auto it = std::lower_bound(results.begin(), results.end(), point, comparePointsLessThanOrEqual);

            if (it != results.end())
            {
                toSelect = it;
            }
            else
            {
                toSelect = _searcher->Results().begin();
            }
        }
        else
        {
            auto it = std::lower_bound(results.begin(), results.end(), point, comparePointsLess);
            if (it != results.begin())
            {
                it = std::prev(it);
                toSelect = it;
            }
            else
            {
                toSelect = std::prev(_searcher->Results().end());
            }
        }

        _ScrollIfNeeded(toSelect->start);
        _UpdateSelection(isVisual, toSelect->start);
    }
}

void VimModeProxy::_highlightClosestSearchResult(bool moveForward)
{
    std::vector<til::point_span> oldResults;
    if (_searcher->Results().size() > 0)
    {
        const auto point = _terminal->GetSelectionAnchors()->start;
        const auto& results = _searcher->Results();

        std::vector<til::point_span>::const_iterator toSelect;
        if (!moveForward)
        {
            const auto it = std::lower_bound(results.begin(), results.end(), point, comparePointsLess);

            if (it != results.end())
            {
                toSelect = it;
            }
            else
            {
                toSelect = _searcher->Results().begin();
            }
        }
        else
        {
            auto it = std::lower_bound(results.begin(), results.end(), point, comparePointsLessThanOrEqual);

            if (it != results.begin())
            {
                it = std::prev(it);
                toSelect = it;
            }
            else
            {
                toSelect = std::prev(_searcher->Results().end());
            }
        }

        const auto idx = std::distance(results.begin(), toSelect);
        _terminal->SetSearchHighlightFocused(idx, 0);
        _ScrollIfNeeded(toSelect->start);
    }
}

void VimModeProxy::_handleSearch(bool moveForward)
{
    std::vector<til::point_span> oldResults;
    _searcher->ResetIfStaleRegex(*_terminal, _searchString, moveForward, true, &oldResults);
    _terminal->SetSearchHighlights(_searcher->Results());
    const auto results = _searcher->Results();
    if (!results.empty())
    {
        _terminal->SetSearchHighlightFocused(results.size() - 1, 0);
        _highlightClosestSearchResult(moveForward);
    }
    _controlCore->UpdateSelectionFromVim(oldResults);
}

void VimModeProxy::_tilChar(std::wstring_view vkey, bool isVisual)
{
    til::point target;
    if (_FindChar(vkey, true, target))
    {
        _UpdateSelection(isVisual, target);
    }
}

void VimModeProxy::_findChar(std::wstring_view vkey, bool isVisual)
{
    til::point target;
    if (_FindChar(vkey, false, target))
    {
        _UpdateSelection(isVisual, target);
    }
}

void VimModeProxy::_findCharBack(std::wstring_view vkey, bool isVisual)
{
    til::point target;
    if (_FindCharBack(vkey, false, target))
    {
        _UpdateSelection(isVisual, target);
    }
}

void VimModeProxy::_tilCharBack(std::wstring_view vkey, bool isVisual)
{
    til::point target;
    if (_FindCharBack(vkey, true, target))
    {
        _UpdateSelection(isVisual, target);
    }
}

void VimModeProxy::_matchingChar(std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool isVisual)
{
    const auto selection = _terminal->GetSelectionAnchors();
    const auto pos = selection->start == selection->pivot ? selection->end : selection->start;
    if (onStartDelimiter)
    {
        _matchingCharFromStart(pos, startDelimiter, endDelimiter, isVisual);
    }
    else
    {
        _matchingCharFromEnd(pos, startDelimiter, endDelimiter, isVisual);
    }
}

void VimModeProxy::_matchingCharFromStart(til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    const std::tuple<bool, til::point, til::point> findResult = _findBlockEndFromStart(startPos, startDelimiter, endDelimiter);
    if (std::get<0>(findResult))
    {
        const auto pairEnd = std::get<2>(findResult);
        if (isVisual)
        {
            if (selection->end >= pairEnd)
            {
                selection->start = pairEnd;
                selection->pivot = selection->end;
            }
            else
            {
                selection->end = pairEnd;
                selection->pivot = selection->start;
            }
        }
        else
        {
            selection->start = pairEnd;
            selection->end = pairEnd;
            selection->pivot = selection->start;
        }
        _terminal->SetSelectionAnchors(selection);
    }
}

void VimModeProxy::_matchingCharFromEnd(til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    const std::tuple<bool, til::point, til::point> findResult = _findBlockStartFromEnd(startPos, startDelimiter, endDelimiter);
    if (std::get<0>(findResult))
    {
        const auto pairStart = std::get<1>(findResult);
        if (isVisual)
        {
            if (selection->start < pairStart)
            {
                selection->end = pairStart;
                selection->pivot = selection->start;
            }
            else
            {
                selection->start = pairStart;
                selection->pivot = selection->end;
            }
        }
        else
        {
            selection->start = pairStart;
            selection->end = pairStart;
            selection->pivot = pairStart;
        }
        _terminal->SetSelectionAnchors(selection);
    }
}

void VimModeProxy::_matchingChar(til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool inBlock)
{
    auto selection = _terminal->GetSelectionAnchors();
    std::tuple<bool, til::point, til::point> findResult;
    if (onStartDelimiter)
    {
        findResult = _findBlockEndFromStart(startPos, startDelimiter, endDelimiter);
    }
    else
    {
        findResult = _findBlockStartFromEnd(startPos, startDelimiter, endDelimiter);
    }
    if (std::get<0>(findResult))
    {
        selection->start = std::get<1>(findResult);
        selection->end = std::get<2>(findResult);
        if (inBlock)
        {
            if (selection->end.x == 0)
            {
                selection->end.y--;
                selection->end.x = _terminal->GetTextBuffer().GetRowByOffset(selection->end.y).GetLastNonSpaceColumn() - 1;
            }
            else
            {
                selection->end.x--;
            }
            if (selection->start.x >= _terminal->GetTextBuffer().GetRowByOffset(selection->end.y).GetLastNonSpaceColumn() - 1)
            {
                selection->start.y++;
                selection->start.x = 0;
            }
            else
            {
                selection->start.x++;
            }
        }
        selection->pivot = selection->end;
        _terminal->SetSelectionAnchors(selection);
    }
}

void VimModeProxy::_inDelimiterSameLine(std::wstring_view delimiter, bool includeDelimiter)
{
    auto selection = _terminal->GetSelectionAnchors();
    const auto pos = selection->start == selection->pivot ? selection->end : selection->start;

    const auto posIsDelimiter = _terminal->GetTextBuffer().GetRowByOffset(pos.y).GlyphAt(pos.x) == delimiter;

    auto foundMatchGoingBack = false;
    til::CoordType matchGoingBack;
    for (auto i = pos.x - 1; i >= 0; i--)
    {
        auto g = _terminal->GetTextBuffer().GetRowByOffset(pos.y).GlyphAt(i);
        if (g == delimiter)
        {
            foundMatchGoingBack = true;
            matchGoingBack = i;
            break;
        }
    }

    auto foundMatchGoingForward = false;
    til::CoordType matchGoingForward;
    for (auto i = pos.x + 1; i <= _terminal->GetTextBuffer().GetRowByOffset(pos.y).size(); i++)
    {
        auto g = _terminal->GetTextBuffer().GetRowByOffset(pos.y).GlyphAt(i);
        if (g == delimiter)
        {
            foundMatchGoingForward = true;
            matchGoingForward = i;
            break;
        }
    }

    if (posIsDelimiter)
    {
        if (foundMatchGoingBack)
        {
            selection->end = til::point{ pos.x, pos.y };
            selection->start = til::point{ matchGoingBack, pos.y };
            if (!includeDelimiter)
            {
                selection->start.x++;
                selection->end.x--;
            }
            _terminal->SetSelectionAnchors(selection);
        }
        else if (foundMatchGoingForward)
        {
            selection->start = til::point{ pos.x, pos.y };
            selection->end = til::point{ matchGoingForward, pos.y };
            if (!includeDelimiter)
            {
                selection->start.x++;
                selection->end.x--;
            }
            _terminal->SetSelectionAnchors(selection);
        }
    }
    else if (foundMatchGoingBack && foundMatchGoingForward)
    {
        selection->start = til::point{ matchGoingBack, pos.y };
        selection->end = til::point{ matchGoingForward, pos.y };
        if (!includeDelimiter)
        {
            selection->start.x++;
            selection->end.x--;
        }
        _terminal->SetSelectionAnchors(selection);
    }
}

void VimModeProxy::_inDelimiter(std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter)
{
    auto delimitersAreSame = startDelimiter == endDelimiter;

    const auto selection = _terminal->GetSelectionAnchors();
    const auto pos = selection->start == selection->pivot ? selection->end : selection->start;

    auto numberOfInnerPairs = 0;
    for (auto j = pos.y; j >= 0; j--)
    {
        auto x = j == pos.y ? pos.x - 1 : _terminal->GetTextBuffer().GetRowByOffset(j).size();
        for (auto i = x; i >= 0; i--)
        {
            auto g = _terminal->GetTextBuffer().GetRowByOffset(j).GlyphAt(i);
            //If delimiters are same there is no way to understand inner pairs
            if (g == endDelimiter && !delimitersAreSame)
            {
                numberOfInnerPairs++;
            }
            else if (g == startDelimiter)
            {
                if (numberOfInnerPairs > 0)
                {
                    numberOfInnerPairs--;
                }
                else
                {
                    _matchingChar(til::point{ i, j }, startDelimiter, endDelimiter, true, !includeDelimiter);
                    return;
                }
            }
        }
    }
}

void VimModeProxy::_selectWordRight(bool isVisual, bool isLargeWord)
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

void VimModeProxy::_selectWordLeft(bool isVisual, bool isLargeWord)
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

void VimModeProxy::_selectWordStartRight(bool isVisual, bool isLargeWord)
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

void VimModeProxy::_selectInWord(bool largeWord)
{
    auto delimiters = _wordDelimiters;
    if (largeWord)
    {
        delimiters = L"";
    }

    auto targetPos{ _terminal->GetSelectionAnchors()->end };
    _InWord(targetPos, delimiters);
}

void VimModeProxy::_selectLineRight(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    if (_terminal->IsBlockSelection())
    {
        auto maxNonSpaceChar = 0;
        for (auto i = selection->start.y; i <= selection->end.y; i++)
        {
            const auto lastNonSpaceColumn = std::max(0, _terminal->GetTextBuffer().GetRowByOffset(i).GetLastNonSpaceColumn() - 1);
            if (lastNonSpaceColumn > maxNonSpaceChar)
            {
                maxNonSpaceChar = lastNonSpaceColumn;
            }
        }
        auto s = til::point{ maxNonSpaceChar, selection->end.y };
        selection->end = s;
        _terminal->SetSelectionAnchors(selection);
    }
    else
    {
        const auto pos = selection->pivot == selection->start ? selection->end : selection->start;
        til::CoordType endLine = pos.y;
        while (_terminal->GetTextBuffer().GetRowByOffset(endLine).WasWrapForced())
        {
            endLine++;
        }

        const auto lastNonSpaceColumn = std::max(0, _terminal->GetTextBuffer().GetRowByOffset(endLine).GetLastNonSpaceColumn() - 1);
        auto s = til::point{ lastNonSpaceColumn, endLine };
        _UpdateSelection(isVisual, s);
    }
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

void VimModeProxy::_selectLineLeft(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    const auto pos = selection->pivot == selection->start ? selection->end : selection->start;
    til::CoordType startLine = _getStartLineOfRow2(_terminal->GetTextBuffer(), pos.y);
    if (_terminal->IsBlockSelection())
    {
        startLine = selection->start.y == selection->pivot.y ? selection->end.y : selection->start.y;
    }
    _UpdateSelection(isVisual, til::point{ 0, startLine });
}

void VimModeProxy::_selectLineUp()
{
    auto selection = _terminal->GetSelectionAnchors();
    auto endIsMoving = false;
    if (selection->end.y > selection->pivot.y)
    {
        endIsMoving = true;
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
    if (selection->start.y >= 0 && selection->end.y > 0)
    {
        _terminal->SetSelectionAnchors(selection);
    }
}

void VimModeProxy::_selectLineDown()
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
    _terminal->SetSelectionAnchors(selection);
}

void VimModeProxy::_selectTop(bool isVisual)
{
    auto selection = _terminal->GetSelectionAnchors();
    if (isVisual)
    {
        selection->start = til::point{ selection->start.x, 0 };
        _terminal->UserScrollViewport(0);
    }
    else
    {
        selection->start = til::point{ selection->start.x, 0 };
        selection->pivot = til::point{ selection->start.x, 0 };
        selection->end = til::point{ selection->start.x, 0 };
        _terminal->UserScrollViewport(0);
    }
    _terminal->SetSelectionAnchors(selection);
}

void VimModeProxy::_selectBottom(bool isVisual)
{
    auto lastChar = _terminal->GetTextBuffer().GetLastNonSpaceCharacter();
    _UpdateSelection(isVisual, lastChar);
    _terminal->UserScrollViewport(lastChar.y);
}

void VimModeProxy::_selectHalfPageUp(bool isVisual, bool entireLine)
{
    auto selection = _terminal->GetSelectionAnchors();
    const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };
    const auto startIsPivot = selection->start.y == selection->pivot.y && selection->start.x == selection->pivot.x;
    const auto targetPos= startIsPivot ? selection->end : selection->start;

    const auto viewportHeight = _terminal->GetViewport().Height();
    const auto newY= targetPos.y - viewportHeight / 2;
    const auto y = newY < bufferSize.Top() ? 0: newY;
    const til::CoordType x = targetPos.x;
    if (entireLine)
    {
        if (startIsPivot)
        {
            if (y > selection->start.y)
            {
                selection->end.y = y;
                selection->end.x = _terminal->GetTextBuffer().GetRowByOffset(y).GetLastNonSpaceColumn() - 1;
            }
            else
            {
                selection->end.y = selection->start.y;
                selection->end.x = _terminal->GetTextBuffer().GetRowByOffset(selection->start.y).GetLastNonSpaceColumn() - 1;
                selection->pivot = selection->end;
                selection->start.y = y;
                selection->start.x = 0;
            }
        }
        else
        {
            selection->start.y = y;
            selection->start.x = 0;
        }
        _terminal->SetSelectionAnchors(selection);
        return;
    }
    const auto newPos = til::point{ x, y };

    _UpdateSelection(isVisual, newPos);
}

void VimModeProxy::_selectHalfPageDown(bool isVisual, bool entireLine)
{
    auto selection = _terminal->GetSelectionAnchors();
    const auto startIsPivot = selection->start.y == selection->pivot.y && selection->start.x == selection->pivot.x;
    const auto pos = startIsPivot ? selection->end : selection->start;

    const auto viewportHeight{ _terminal->GetViewport().Height() };
    const auto lastRow = _terminal->GetTextBuffer().GetLastNonSpaceCharacter().y;
    const auto newY= pos.y + viewportHeight / 2 ;
    const auto y = newY > lastRow ? lastRow : newY;
    const til::CoordType x = pos.x;
    if (entireLine)
    {
        if (!startIsPivot)
        {
            if (y < selection->end.y)
            {
                selection->start.y = y;
                selection->start.x = 0;
            }
            else
            {
                selection->start.y = selection->end.y;
                selection->start.x = 0;
                selection->pivot = selection->start;
                selection->end.y = newY;
                selection->end.x = _terminal->GetTextBuffer().GetRowByOffset(selection->start.y).GetLastNonSpaceColumn() - 1;
            }
        }
        else
        {
            selection->end.y = y;
            selection->end.x = _terminal->GetTextBuffer().GetRowByOffset(y).GetLastNonSpaceColumn() - 1;;
        }
        _terminal->SetSelectionAnchors(selection);
        return;
    }

    const auto newPos = til::point{ x, y };

    _UpdateSelection(isVisual, newPos);
}

void VimModeProxy::_selectPageUp(bool isVisual)
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

void VimModeProxy::_selectPageDown(bool isVisual)
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

void VimModeProxy::_selectCharRight(bool isVisual)
{
    const auto selection = _terminal->GetSelectionAnchors();
    auto point = selection->end > selection->pivot ? selection->end : selection->start;
    point.x++;
    if (point.x < _terminal->GetTextBuffer().GetLineWidth(point.y))
    {
        _UpdateSelection(isVisual, point);
    }
}

void VimModeProxy::_selectCharLeft(bool isVisual)
{
    const auto selection = _terminal->GetSelectionAnchors();
    auto point = selection->end > selection->pivot ? selection->end : selection->start;
    point.x--;
    if (point.x >= 0)
    {
        _UpdateSelection(isVisual, point);
    }
}

void VimModeProxy::_selectDown(bool isVisual, til::CoordType lastY)
{
    const auto selection = _terminal->GetSelectionAnchors();
    auto point = selection->end > selection->pivot ? selection->end : selection->start;
    point.y++;
    if (point.y <= lastY)
    {
        _UpdateSelection(isVisual, point);
    }
}

void VimModeProxy::_selectUp(bool isVisual)
{
    const auto selection = _terminal->GetSelectionAnchors();
    auto point = selection->end > selection->pivot ? selection->end : selection->start;
    point.y--;
    if (point.y >= 0)
    {
        _UpdateSelection(isVisual, point);
    }
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

void VimModeProxy::_selectLineFirstNonBlankChar(bool isVisual)
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

bool VimModeProxy::_executeVimSelection(
    const VimActionType action,
    const VimTextObjectType textObject,
    const int times,
    const VimMotionType motion,
    const bool isVisual,
    const std::wstring searchString,
    std::wstring_view vkey)
{
    bool exitAfter = false;
    bool selectFromStart = isVisual || action == VimActionType::yank;
    const auto lastNonSpaceChar = _terminal->GetTextBuffer().GetLastNonSpaceCharacter();

    for (int i = 0; i < times; i++)
    {
        auto pairIsVisual = isVisual || _vimMode == VimMode::visualLine;
        switch (textObject)
        {
        case VimTextObjectType::startCurlyBracePair:
            _matchingChar(L"{", L"}", true, pairIsVisual);
            break;
        case VimTextObjectType::startRoundBracePair:
            _matchingChar(L"(", L")", true, pairIsVisual);
            break;
        case VimTextObjectType::startSquareBracePair:
            _matchingChar(L"[", L"]", true, pairIsVisual);
            break;
        case VimTextObjectType::endCurlyBracePair:
            _matchingChar(L"{", L"}", false, pairIsVisual);
            break;
        case VimTextObjectType::endRoundBracePair:
            _matchingChar(L"(", L")", false, pairIsVisual);
            break;
        case VimTextObjectType::endSquareBracePair:
            _matchingChar(L"[", L"]", false, pairIsVisual);
            break;
        case VimTextObjectType::inCurlyBracePair:
            _inDelimiter(L"{", L"}", false);
            break;
        case VimTextObjectType::inSquareBracePair:
            _inDelimiter(L"[", L"]", false);
            break;
        case VimTextObjectType::inRoundBracePair:
            _inDelimiter(L"(", L")", false);
            break;
        case VimTextObjectType::inSingleQuotePair:
            _inDelimiterSameLine(L"'", false);
            break;
        case VimTextObjectType::inDoubleQuotePair:
            _inDelimiterSameLine(L"\"", false);
            break;
        case VimTextObjectType::inAngleBracketPair:
            _inDelimiter(L"<", L">", false);
            break;
        case VimTextObjectType::aroundCurlyBracePair:
            _inDelimiter(L"{", L"}", true);
            break;
        case VimTextObjectType::aroundSquareBracePair:
            _inDelimiter(L"[", L"]", true);
            break;
        case VimTextObjectType::aroundRoundBracePair:
            _inDelimiter(L"(", L")", true);
            break;
        case VimTextObjectType::aroundSingleQuotePair:
            _inDelimiterSameLine(L"'", true);
            break;
        case VimTextObjectType::aroundDoubleQuotePair:
            _inDelimiterSameLine(L"\"", true);
            break;
        case VimTextObjectType::aroundAngleBracketPair:
            _inDelimiter(L"<", L">", true);
            break;
        case VimTextObjectType::findChar:
            if (motion == VimMotionType::forward)
            {
                _findChar(vkey, selectFromStart);
            }
            else
            {
                _findCharBack(vkey, selectFromStart);
            }
            break;
        case VimTextObjectType::tilChar:
            if (motion == VimMotionType::forward)
            {
                _tilChar(vkey, selectFromStart);
            }
            else
            {
                _tilCharBack(vkey, selectFromStart);
            }
            break;
        case VimTextObjectType::findCharReverse:
            if (motion == VimMotionType::forward)
            {
                _findCharBack(vkey, selectFromStart);
            }
            else
            {
                _findChar(vkey, selectFromStart);
            }
            break;
        case VimTextObjectType::tilCharReverse:
            if (motion == VimMotionType::forward)
            {
                _tilCharBack(vkey, selectFromStart);
            }
            else
            {
                _tilChar(vkey, selectFromStart);
            }
            break;
        case VimTextObjectType::largeWord:
        case VimTextObjectType::word:
            if (motion == VimMotionType::moveForwardToEnd)
            {
                _selectWordRight(selectFromStart, textObject == VimTextObjectType::largeWord);
            }
            else if (motion == VimMotionType::moveBackToBegining)
            {
                _selectWordLeft(selectFromStart, textObject == VimTextObjectType::largeWord);
            }
            else if (motion == VimMotionType::moveForwardToStart)
            {
                _selectWordStartRight(selectFromStart, textObject == VimTextObjectType::largeWord);
            }
            break;
        case VimTextObjectType::inLargeWord:
        case VimTextObjectType::inWord:
            _selectInWord(textObject == VimTextObjectType::inLargeWord);
            break;
        case VimTextObjectType::line:
            if (motion == VimMotionType::moveForwardToEnd)
            {
                _selectLineRight(selectFromStart);
            }
            else if (motion == VimMotionType::moveBackToBegining)
            {
                _selectLineLeft(selectFromStart);
            }
            else if (motion == VimMotionType::backToFirstNonSpaceChar)
            {
                _selectLineFirstNonBlankChar(selectFromStart);
            }
            break;
        case VimTextObjectType::entireLine:
            switch (motion)
            {
            case VimMotionType::moveUp:
                _selectLineUp();
                break;
            case VimMotionType::moveDown:
                _selectLineDown();
                break;
            case VimMotionType::moveToTopOfBuffer:
                _selectTop(true);
                break;
            case VimMotionType::moveToBottomOfBuffer:
                _selectBottom(true);
                break;
            case VimMotionType::halfPageUp:
                _selectHalfPageUp(true, true);
                break;
            case VimMotionType::halfPageDown:
                _selectHalfPageDown(true, true);
                break;
            case VimMotionType::pageUp:
                _selectPageUp(true);
                break;
            case VimMotionType::pageDown:
                _selectPageDown(true);
            case VimMotionType::selectCurrentLine:
                _selectLineLeft(false);
                _selectLineRight(true);
                break;
            }
            break;
        case VimTextObjectType::charTextObject:
            switch (motion)
            {
            case VimMotionType::none:
                _selectCharRight(false);
                _selectCharLeft(false);
                break;
            case VimMotionType::moveLeft:
                _selectCharLeft(selectFromStart);
                break;
            case VimMotionType::moveDown:
                _selectDown(selectFromStart, lastNonSpaceChar.y);
                break;
            case VimMotionType::moveUp:
                _selectUp(selectFromStart);
                break;
            case VimMotionType::moveRight:
                _selectCharRight(selectFromStart);
                break;
            }
            break;
        case VimTextObjectType::none:
            switch (motion)
            {
            case VimMotionType::moveToTopOfBuffer:
                _selectTop(selectFromStart);
                break;
            case VimMotionType::moveToBottomOfBuffer:
                _selectBottom(selectFromStart);
                break;
            case VimMotionType::halfPageUp:
                _selectHalfPageUp(selectFromStart, false);
                break;
            case VimMotionType::halfPageDown:
                _selectHalfPageDown(selectFromStart, false);
                break;
            case VimMotionType::pageUp:
                _selectPageUp(selectFromStart);
                break;
            case VimMotionType::pageDown:
                _selectPageDown(selectFromStart);
                break;
            }
            break;
        }
    }

    switch (action)
    {
    case VimActionType::swapPivot:
    {
        auto selection = _terminal->GetSelectionAnchors();
        selection->pivot = selection->pivot == selection->start ? selection->end : selection->start;
        _terminal->SetSelectionAnchors(selection);
        break;
    }
    case VimActionType::enterQuickCopyMode:
    case VimActionType::enterQuickSelectMode:
        ResetVimState();
        _controlCore->EnterQuickSelectMode(L"[\\w\\d\\S]+", action == VimActionType::enterQuickCopyMode);
        break;
    case VimActionType::toggleRowNumbers:
    {
        _showRowNumbers = !_showRowNumbers;
        _controlCore->ToggleRowNumberMode(_showRowNumbers);
        break;
    }
    case VimActionType::scroll:
        _vimScrollScreenPosition(_textObject);
        break;
    case VimActionType::fuzzyFind:
    {
        const auto bufferData = _terminal->RetrieveSelectedTextFromBuffer(false);
        auto searchString = bufferData.plainText;
        _controlCore->StartFuzzySearch(searchString);
        break;
    }
    case VimActionType::commitSearch:
    {
        auto results = _searcher->Results();
        if (results.size() > 0)
        {
            auto focused = _terminal->GetSearchHighlightFocused();
            _terminal->SelectChar(focused->start);
            _terminal->SetSearchHighlightFocused(-1, 0);
            _controlCore->UpdateSelectionFromVim(results);
        }
        break;
    }
    case VimActionType::nextSearchResult:
    {
        auto moveForward = motion != VimMotionType::forward;
        _moveToNextSearchResult(moveForward, isVisual);
        break;
    }
    case VimActionType::search:
    {
        auto moveForward = motion != VimMotionType::forward;
        if (textObject == VimTextObjectType::word)
        {
            _selectInWord(false);
            const auto bufferData = _terminal->RetrieveSelectedTextFromBuffer(moveForward);
            _searchString = L"\\b" + bufferData.plainText + L"\\b";
            _handleSearch(moveForward);
            _moveToNextSearchResult(moveForward, isVisual);
            //_terminal->SetSearchHighlightFocused(-1);
        }
        else
        {
            _handleSearch(moveForward);
        }
        break;
    }
    case VimActionType::yank:
    {
        _terminal->SelectYankRegion();
        std::thread hideTimerThread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            {
                ResetVimState();
            }
        });
        hideTimerThread.detach();
        _controlCore->CopySelectionToClipboard(false, nullptr);
        exitAfter = false;
        break;
    }
    case VimActionType::toggleVisualOn:
        _terminal->SetPivot();
        break;
    case VimActionType::enterBlockSelectionMode:
        _vimMode = _terminal->IsBlockSelection() ? VimMode::normal : VimMode::visual;
        _controlCore->ToggleBlockSelection();
        break;
    case VimActionType::exit:
        exitAfter = true;
        break;
    }

    return exitAfter;
}

bool VimModeProxy::TryVimModeKeyBinding(
    const WORD vkey,
    const ::Microsoft::Terminal::Core::ControlKeyStates mods)
{
    bool sequenceCompleted = false;
    bool hideMarkers = false;
    bool clearStateOnSequenceCompleted = true;
    bool skipExecute = false;

    if (vkey == 16 || vkey == 17 || vkey == 18)
    {
        return true;
    }

    wchar_t vkeyText[2] = { 0 };
    BYTE keyboardState[256];
    if (!GetKeyboardState(keyboardState))
    {
        return true;
    }
    if (mods.IsShiftPressed())
    {
        keyboardState[VK_SHIFT] = 0x80;
    }
    ToUnicode(vkey, MapVirtualKey(vkey, MAPVK_VK_TO_VSC), keyboardState, vkeyText, 2, 0);

    std::wstringstream timesStringStream(_timesString);
    if (!_timesString.empty())
    {
        timesStringStream >> _times;
    }
    else
    {
        _times = 1;
    }

    if (vkey == L'R' && mods.IsCtrlPressed())
    {
        _action = VimActionType::toggleRowNumbers;
        _textObject = VimTextObjectType::none;
        sequenceCompleted = true;
    }
    else if (vkey >= 0x30 && vkey <= 0x39 && !mods.IsShiftPressed() && _vimMode != VimMode::search && _textObject != VimTextObjectType::findChar && _textObject != VimTextObjectType::findCharReverse && _textObject != VimTextObjectType::tilChar && _textObject != VimTextObjectType::tilCharReverse)
    {
        _timesString += vkeyText;
        std::wstringstream timesStringStream(_timesString);
        int times;
        timesStringStream >> times;
    }
    else if (vkey == VK_ESCAPE && _terminal->IsBlockSelection())
    {
        _action = VimActionType::enterBlockSelectionMode;
        sequenceCompleted = true;
    }
    // '%'
    else if (vkeyText[0] == L'%')
    {
        const auto bufferData = _terminal->RetrieveSelectedTextFromBuffer(false);
        if (bufferData.plainText.size() > 0)
        {
            auto selection = _terminal->GetSelectionAnchors();
            auto pos = selection->start == selection->pivot ? selection->end : selection->start;
            auto firstChar = _terminal->GetTextBuffer().GetRowByOffset(pos.y).GlyphAt(pos.x)[0];
            if (firstChar == L'(')
            {
                _textObject = VimTextObjectType::startRoundBracePair;
                sequenceCompleted = true;
            }
            else if (firstChar == L')')
            {
                _textObject = VimTextObjectType::endRoundBracePair;
                sequenceCompleted = true;
            }
            else if (firstChar == L'{')
            {
                _textObject = VimTextObjectType::startCurlyBracePair;
                sequenceCompleted = true;
            }
            else if (firstChar == L'}')
            {
                _textObject = VimTextObjectType::endCurlyBracePair;
                sequenceCompleted = true;
            }
            else if (firstChar == L'[')
            {
                _textObject = VimTextObjectType::startSquareBracePair;
                sequenceCompleted = true;
            }
            else if (firstChar == L']')
            {
                _textObject = VimTextObjectType::endSquareBracePair;
                sequenceCompleted = true;
            }
        }
    }
    else if (_vimMode == VimMode::visualLine)
    {
        _textObject = VimTextObjectType::entireLine;
        sequenceCompleted = true;
        if ((vkey == L'U' && mods.IsCtrlPressed()) || vkey == VK_PRIOR)
        {
            _motion = VimMotionType::halfPageUp;
        }
        else if ((vkey == L'D' && mods.IsCtrlPressed()) || vkey == VK_NEXT)
        {
            _motion = VimMotionType::halfPageDown;
        }
        else if (vkey == L'G' && mods.IsShiftPressed() && _motion == VimMotionType::none)
        {
            _motion = VimMotionType::moveToBottomOfBuffer;
        }
        else if (vkey == L'G' && !mods.IsShiftPressed())
        {
            if (_motion == VimMotionType::g)
            {
                _motion = VimMotionType::moveToTopOfBuffer;
            }
            else
            {
                _motion = VimMotionType::g;
                sequenceCompleted = false;
            }
        }
        else if ((vkey == L'K' || vkey == VK_UP) && _motion == VimMotionType::none)
        {
            _motion = VimMotionType::moveUp;
        }
        else if ((vkey == L'J' || vkey == VK_DOWN || vkey == VK_RETURN) && _motion == VimMotionType::none)
        {
            _motion = VimMotionType::moveDown;
        }
        else if (vkey == L'Y' && _motion == VimMotionType::none)
        {
            _textObject = VimTextObjectType::none;
            _action = VimActionType::yank;
        }
        else if (vkey == L'F' && mods.IsCtrlPressed())
        {
            _motion = VimMotionType::pageDown;
        }
        else if (vkey == L'B' && mods.IsCtrlPressed())
        {
            _motion = VimMotionType::pageUp;
        }
        else if (vkey == VK_ESCAPE)
        {
            _vimMode = VimMode::normal;
            _textObject = VimTextObjectType::charTextObject;
        }
    }
    else if (_textObject == VimTextObjectType::tilChar || _textObject == VimTextObjectType::tilCharReverse || _textObject == VimTextObjectType::findChar || _textObject == VimTextObjectType::findCharReverse)
    {
        sequenceCompleted = true;
    }
    else if (_vimMode == VimMode::search)
    {
        if (vkey == VK_RETURN || vkey == VK_ESCAPE)
        {
            _motion = VimMotionType::none;
            _vimMode = VimMode::normal;
            if (vkey == VK_RETURN)
            {
                _action = VimActionType::commitSearch;
                sequenceCompleted = true;
            }
            else
            {
                _terminal->SetSearchHighlights({});
                _controlCore->UpdateSelectionFromVim(_searcher->Results());
            }
        }
        else
        {
            _action = VimActionType::search;
            _motion = _reverseSearch ? VimMotionType::back : VimMotionType::forward;
            sequenceCompleted = true;
            clearStateOnSequenceCompleted = false;
            hideMarkers = true;

            if (vkey == VK_BACK)
            {
                if (!_searchString.empty())
                {
                    _searchString.pop_back();
                }
            }
            else
            {
                if (vkey == L'V' && mods.IsCtrlPressed())
                {
                    auto pasteString = _getClipboardText();
                    _searchString += pasteString;
                }
                else
                {
                    _searchString += vkeyText;
                }
            }
        }
    }
    else if (vkey == L' ')
    {
        if (_leaderSequence)
        {
            _action = VimActionType::toggleRowNumbers;
            sequenceCompleted = true;
        }
        else
        {
            _leaderSequence = true;
        }
    }
    // * #
    else if ((vkey == 0x38 && mods.IsShiftPressed()) || (vkey == 0x33 && mods.IsShiftPressed()))
    {
        _action = VimActionType::search;
        _amount = VimTextAmount::in;
        _textObject = VimTextObjectType::word;

        _reverseSearch = vkey == 0x33;
        _motion = vkey == 0x38 ? VimMotionType::forward : VimMotionType::back;

        sequenceCompleted = true;
    }
    // / ?
    else if (vkey == 0xBF)
    {
        StartSearch(mods.IsShiftPressed());
        hideMarkers = true;
        _controlCore->StartVimSearch(_reverseSearch);
    }
    else if (vkey == L'O' && _vimMode == VimMode::visual)
    {
        _action = VimActionType::swapPivot;
        _textObject = VimTextObjectType::none;
        sequenceCompleted = true;
    }
    else if (vkey == L'Q' && mods.IsCtrlPressed())
    {
        _action = VimActionType::enterBlockSelectionMode;
        sequenceCompleted = true;
    }
    else if ((vkey == L'U' && mods.IsCtrlPressed()) || vkey == VK_PRIOR)
    {
        _motion = VimMotionType::halfPageUp;
        _textObject = VimTextObjectType::none;
        sequenceCompleted = true;
    }
    else if ((vkey == L'D' && mods.IsCtrlPressed()) || vkey == VK_NEXT)
    {
        _motion = VimMotionType::halfPageDown;
        _textObject = VimTextObjectType::none;
        sequenceCompleted = true;
    }
    else if (vkey == L'G' && mods.IsShiftPressed() && _motion == VimMotionType::none)
    {
        _motion = VimMotionType::moveToBottomOfBuffer;
        _textObject = VimTextObjectType::none;
        sequenceCompleted = true;
    }
    else if (vkey == L'G' && !mods.IsShiftPressed())
    {
        if (_motion == VimMotionType::g)
        {
            _motion = VimMotionType::moveToTopOfBuffer;
            _textObject = VimTextObjectType::none;
            sequenceCompleted = true;
        }
        else
        {
            _motion = VimMotionType::g;
        }
    }
    else if (vkey == L'N')
    {
        _action = VimActionType::nextSearchResult;
        if (_reverseSearch)
        {
            _motion = mods.IsShiftPressed() ? VimMotionType::forward : VimMotionType::back;
        }
        else
        {
            _motion = mods.IsShiftPressed() ? VimMotionType::back : VimMotionType::forward;
        }

        sequenceCompleted = true;
    }
    else if (vkey == L'I')
    {
        _amount = VimTextAmount::in;
    }
    else if (vkey == L'A')
    {
        _amount = VimTextAmount::around;
    }
    else if (vkey == L'F')
    {
        //Ooes this work?
        if (_leaderSequence)
        {
            _action = VimActionType::fuzzyFind;
        }
        else if (mods.IsShiftPressed())
        {
            _textObject = VimTextObjectType::findCharReverse;
            _motion = VimMotionType::forward;
        }
        else
        {
            if (mods.IsCtrlPressed())
            {
                _motion = VimMotionType::pageDown;
                _textObject = VimTextObjectType::none;
                sequenceCompleted = true;
            }
            else
            {
                _textObject = VimTextObjectType::findChar;
                _motion = VimMotionType::forward;
            }
        }
    }
    else if (vkey == L'T')
    {
        if (_action == VimActionType::scroll)
        {
            _textObject = VimTextObjectType::topOfScreen;
            sequenceCompleted = true;
        }
        else
        {
            _motion = VimMotionType::forward;
            if (mods.IsShiftPressed())
            {
                _textObject = VimTextObjectType::tilCharReverse;
            }
            else
            {
                _textObject = VimTextObjectType::tilChar;
            }
        }
    }
    else if (vkey == L'Y' && _motion == VimMotionType::none)
    {
        _action = VimActionType::yank;
        if (mods.IsShiftPressed())
        {
            _textObject = VimTextObjectType::line;
            _motion = VimMotionType::moveForwardToEnd;
            sequenceCompleted = true;
        }
        else if (_lastVkey[0] == L'y')
        {
            _motion = VimMotionType::selectCurrentLine;
            _textObject = VimTextObjectType::entireLine;
            sequenceCompleted = true;
        }
        else if (_vimMode == VimMode::visual)
        {
            _textObject = VimTextObjectType::none;
            sequenceCompleted = true;
        }
    }
    else if (vkey == L'E' && _motion == VimMotionType::none)
    {
        _textObject = mods.IsShiftPressed() ? VimTextObjectType::largeWord : VimTextObjectType::word;

        sequenceCompleted = true;
        _motion = VimMotionType::moveForwardToEnd;
    }
    else if (vkey == L'B' && (_motion == VimMotionType::none))
    {
        if (_action == VimActionType::scroll)
        {
            _textObject = VimTextObjectType::bottomOfScreen;
            sequenceCompleted = true;
        }
        else
        {
            if (mods.IsCtrlPressed())
            {
                _motion = VimMotionType::pageUp;
                _textObject = VimTextObjectType::none;
                sequenceCompleted = true;
            }
            else
            {
                _motion = VimMotionType::moveBackToBegining;
                _textObject = mods.IsShiftPressed() ? VimTextObjectType::largeWord : VimTextObjectType::word;
                sequenceCompleted = true;
            }
        }
    }
    // $
    else if (vkey == 0x34 && mods.IsShiftPressed())
    {
        _motion = VimMotionType::moveForwardToEnd;
        _textObject = VimTextObjectType::line;
        sequenceCompleted = true;
    }
    // ^
    else if (vkey == 0x36 && mods.IsShiftPressed())
    {
        _motion = VimMotionType::backToFirstNonSpaceChar;
        _textObject = VimTextObjectType::line;
        sequenceCompleted = true;
    }
    // |
    else if (vkey == VK_OEM_5 && mods.IsShiftPressed())
    {
        _motion = VimMotionType::moveBackToBegining;
        _textObject = VimTextObjectType::line;
        sequenceCompleted = true;
    }
    else if ((vkey == L'K' || vkey == VK_UP) && _motion == VimMotionType::none)
    {
        _motion = VimMotionType::moveUp;
        _textObject = _vimMode == VimMode::visualLine || (_times > 1 && _action == VimActionType::yank) ? VimTextObjectType::entireLine : VimTextObjectType::charTextObject;
        sequenceCompleted = true;
    }
    else if ((vkey == L'J' || vkey == VK_DOWN || vkey == VK_RETURN) && _motion == VimMotionType::none)
    {
        _textObject = _vimMode == VimMode::visualLine || (_times > 1 && _action == VimActionType::yank) ? VimTextObjectType::entireLine : VimTextObjectType::charTextObject;
        _motion = VimMotionType::moveDown;
        sequenceCompleted = true;
    }
    else if ((vkey == L'L' || vkey == VK_RIGHT) && _motion == VimMotionType::none)
    {
        _motion = VimMotionType::moveRight;
        _textObject = VimTextObjectType::charTextObject;
        sequenceCompleted = true;
    }
    else if ((vkey == L'H' || vkey == VK_LEFT) && _motion == VimMotionType::none)
    {
        _motion = VimMotionType::moveLeft;
        _textObject = VimTextObjectType::charTextObject;
        sequenceCompleted = true;
    }
    else if (vkey == L'W')
    {
        if (_leaderSequence)
        {
            _action = mods.IsShiftPressed() ? VimActionType::enterQuickCopyMode : VimActionType::enterQuickSelectMode;
            sequenceCompleted = true;
        }
        else if (_amount == VimTextAmount::in)
        {
            _textObject = mods.IsShiftPressed() ? VimTextObjectType::inLargeWord : VimTextObjectType::inWord;
        }
        else
        {
            _textObject = mods.IsShiftPressed() ? VimTextObjectType::largeWord : VimTextObjectType::word;
            _motion = VimMotionType::moveForwardToStart;
        }

        sequenceCompleted = true;
    }
    // {}
    else if ((vkey == VK_OEM_4 || vkey == VK_OEM_6) && mods.IsShiftPressed())
    {
        _textObject = _amount == VimTextAmount::in ? VimTextObjectType::inCurlyBracePair : _amount == VimTextAmount::around ? VimTextObjectType::aroundCurlyBracePair :
                                                                                                                               VimTextObjectType::charTextObject;
        sequenceCompleted = true;
    }
    // []
    else if (vkey == VK_OEM_4 || vkey == VK_OEM_6)
    {
        _textObject = _amount == VimTextAmount::in ? VimTextObjectType::inSquareBracePair : _amount == VimTextAmount::around ? VimTextObjectType::aroundSquareBracePair :
                                                                                                                               VimTextObjectType::charTextObject;
        sequenceCompleted = true;
    }
    // '
    else if (vkey == 0xDE && !mods.IsShiftPressed())
    {
        _textObject = _amount == VimTextAmount::in ? VimTextObjectType::inSingleQuotePair : _amount == VimTextAmount::around ? VimTextObjectType::aroundSingleQuotePair :
                                                                                                                               VimTextObjectType::charTextObject;
        sequenceCompleted = true;
    }
    // "
    else if (vkey == 0xDE && mods.IsShiftPressed())
    {
        _textObject = _amount == VimTextAmount::in ? VimTextObjectType::inDoubleQuotePair : _amount == VimTextAmount::around ? VimTextObjectType::aroundDoubleQuotePair :
                                                                                                                               VimTextObjectType::charTextObject;
        sequenceCompleted = true;
    }
    // ()
    else if (vkey == 0x39 && mods.IsShiftPressed() || vkey == 0x30 && mods.IsShiftPressed())
    {
        _textObject = _amount == VimTextAmount::in ? VimTextObjectType::inRoundBracePair : _amount == VimTextAmount::around ? VimTextObjectType::aroundRoundBracePair :
                                                                                                                              VimTextObjectType::charTextObject;
        sequenceCompleted = true;
    }
    // <>
    else if ((vkey == VK_OEM_COMMA && mods.IsShiftPressed()) || (vkey == VK_OEM_PERIOD && mods.IsShiftPressed()))
    {
        _textObject = _amount == VimTextAmount::in ? VimTextObjectType::inAngleBracketPair : _amount == VimTextAmount::around ? VimTextObjectType::aroundAngleBracketPair :
                                                                                                                                VimTextObjectType::charTextObject;
        sequenceCompleted = true;
    }
    // ; ,
    else if (vkey == 186 || vkey == 188)
    {
        _motion = vkey == 186 ? VimMotionType::forward : VimMotionType::back;

        _action = _lastAction;
        wcsncpy_s(vkeyText, _lastVkey, sizeof(vkeyText) / sizeof(vkeyText[0]));
        sequenceCompleted = true;
        _textObject = _lastTextObject;

        sequenceCompleted = true;
    }
    else if (vkey == L'V' && _vimMode == VimMode::normal && _motion == VimMotionType::none)
    {
        _vimMode = VimMode::visual;
        _action = VimActionType::toggleVisualOn;
        if (mods.IsShiftPressed())
        {
            _vimMode = VimMode::visualLine;
            _motion = VimMotionType::selectCurrentLine;
            _textObject = VimTextObjectType::entireLine;
            sequenceCompleted = true;
        }
        sequenceCompleted = true;
    }
    else if (vkey == L'S')
    {
        if (_leaderSequence && _action == VimActionType::fuzzyFind)
        {
            _textObject = VimTextObjectType::inWord;
            sequenceCompleted = true;
        }
    }
    else if (vkey == L'Z')
    {
        if (_action == VimActionType::scroll)
        {
            _textObject = VimTextObjectType::centerOfScreen;
            sequenceCompleted = true;
        }
        else
        {
            _action = VimActionType::scroll;
        }
    }
    else if (vkey == VK_ESCAPE)
    {
        if (_vimMode == VimMode::search || _vimMode == VimMode::visual)
        {
            _vimMode = VimMode::normal;
            _textObject = VimTextObjectType::charTextObject;
            _motion = VimMotionType::none;
            sequenceCompleted = true;
        }
        else if (_vimMode == VimMode::normal)
        {
            _action = VimActionType::exit;
            _textObject = VimTextObjectType::none;
            sequenceCompleted = true;
        }
    }
    else
    {
        sequenceCompleted = true;
        skipExecute = true;
    }

    auto shouldExit = false;

    if (sequenceCompleted && !skipExecute)
    {
        shouldExit = _executeVimSelection(_action, _textObject, _times, _motion, _vimMode == VimMode::visual, _searchString, vkeyText);
    }

    wcsncpy_s(_lastVkey, vkeyText, sizeof(_lastVkey) / sizeof(_lastVkey[0]));

    if (sequenceCompleted)
    {
        if (_action == VimActionType::yank)
        {
            _vimMode = VimMode::normal;
        }
        _setStateForCompletedSequence();

        if (shouldExit)
        {
            ResetVimState();
            return true;
        }
    }

    _controlCore->UpdateSelectionFromVim();

    return true;
}

bool VimModeProxy::ShowRowNumbers()
{
    return _showRowNumbers;
}

void VimModeProxy::ShowRowNumbers(bool val)
{
    _showRowNumbers = val;
}

int32_t VimModeProxy::ViewportRowToHighlight()
{
    auto lock = _terminal->LockForReading();
    const auto offset = _terminal->GetScrollOffset();
    const auto selection = _terminal->GetSelectionAnchors();
    if (!selection.has_value())
    {
        return _terminal->GetCursorPosition().y;
    }
    const auto pivotIsStart = selection->start == selection->pivot;
    const til::point point = pivotIsStart ? selection->end : selection->start;
    return point.y - offset;
}

void VimModeProxy::_setPosForResize(til::point pos, til::point& target) const
{
    const auto selection = _terminal->GetSelectionAnchors();
    auto lineNumber = 0;
    for (auto i = 0; i < pos.y; i++)
    {
        if (!_terminal->GetTextBuffer().GetRowByOffset(i).WasWrapForced())
        {
            lineNumber++;
        }
    }
    target.x = pos.x;
    auto y = pos.y - 1;
    const auto width = _terminal->GetTextBuffer().GetRowByOffset(y).size();
    while (_terminal->GetTextBuffer().GetRowByOffset(y).WasWrapForced())
    {
        target.x += width;
        y--;
    }

    target.y = lineNumber;
}

void VimModeProxy::StoreSelectionForResize()
{
    const auto selection = _terminal->GetSelectionAnchors();
    _setPosForResize(selection->start, _tempStart);
    _setPosForResize(selection->end, _tempEnd);
    _setPosForResize(selection->pivot, _tempPivot);
    const auto offSet = _terminal->GetScrollOffset();
    _tempTop = selection->start.y - offSet;
}

til::point VimModeProxy::_updateFromResize(til::point from) const
{
    auto selection = _terminal->GetSelectionAnchors();
    auto lineNumber = 0;
    auto i = 0;
    while (lineNumber < from.y)
    {
        if (!_terminal->GetTextBuffer().GetRowByOffset(i).WasWrapForced())
        {
            lineNumber++;
        }
        i++;
    }

    const auto width = _terminal->GetTextBuffer().GetRowByOffset(i).size();
    til::CoordType extraI = from.x / width;
    til::CoordType x = from.x % width;

    const auto point = til::point{ x, i + extraI };
    return point;
}

void VimModeProxy::UpdateSelectionFromResize() const
{
    auto selection = _terminal->GetSelectionAnchors();
    selection->start = _updateFromResize(_tempStart);
    selection->pivot = _updateFromResize(_tempPivot);
    selection->end = _updateFromResize(_tempEnd);
    _terminal->SetSelectionAnchors(selection);
    _terminal->UserScrollViewport(selection->start.y - _tempTop);
}

void VimModeProxy::CommitSearch()
{
    _vimMode = VimMode::normal;
    _executeVimSelection(VimActionType::commitSearch, VimTextObjectType::none, 1, VimMotionType::none, false, _searchString, L"");
    _controlCore->UpdateSelectionFromVim(_searcher->Results());
    _setStateForCompletedSequence();
}

void VimModeProxy::_setStateForCompletedSequence()
{
    _lastTextObject = _textObject;
    _lastAction = _action;
    _lastMotion = _motion;
    _textObject = VimTextObjectType::charTextObject;
    _action = VimActionType::none;
    _motion = VimMotionType::none;
    _lastTimes = _times;
    _timesString = L"";
    _amount = VimTextAmount::none;
    _leaderSequence = false;
}

void VimModeProxy::ExitVimSearch()
{
    _vimMode = VimMode::normal;
    auto selection = _terminal->GetSelectionAnchors();
    _ScrollIfNeeded(selection->start);
    _setStateForCompletedSequence();
    _controlCore->ClearSearch();
}

void VimModeProxy::SetSearchString(std::wstring_view searchString)
{
    _searchString = searchString;
    _executeVimSelection(VimActionType::search, VimTextObjectType::none, 1, _motion, false, _searchString, L"");
}

void VimModeProxy::StartSearch(bool isReverse)
{
        if (isReverse)
        {
            _motion = VimMotionType::back;
            _reverseSearch = true;
        }
        else
        {
            _motion = VimMotionType::forward;
            _reverseSearch = false;
        }
        _action = VimActionType::search;
        _vimMode = VimMode::search;
        _searchString = L"";

        _action = VimActionType::search;
}

bool VimModeProxy::_FindChar(std::wstring_view vkey, bool isTil, til::point& target) const
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

void VimModeProxy::_vimScrollScreenPosition(VimTextObjectType textObjectType)
{
    static const int32_t paddingRows = 5;
    int offset;
    switch (textObjectType)
    {
    case VimTextObjectType::bottomOfScreen:
        offset = _terminal->GetViewport().Height() - paddingRows;
        break;
    case VimTextObjectType::centerOfScreen:
        offset = _terminal->GetViewport().Height() / 2;
        break;
    case VimTextObjectType::topOfScreen:
        offset = paddingRows;
        break;
    }
    _terminal->UserScrollViewport(_terminal->GetSelectionEnd().y - offset);
}

bool VimModeProxy::_FindCharBack(std::wstring_view vkey, bool isTil, til::point& target)
{
    auto selection = _terminal->GetSelectionAnchors();
    auto startPoint = selection->end == selection->pivot ?
                                til::point{ selection->start.x - 1, selection->start.y } :
                                til::point{ selection->end.x - 1, selection->end.y };

    if (isTil && startPoint.x > 0)
    {
        startPoint.x--;
    }

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

std::tuple<bool, til::point, til::point> VimModeProxy::_findBlockEndFromStart(til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter) const
{
    til::CoordType endX = -1;
    til::CoordType endY = -1;

    auto innerPairs = 0;
    auto lastNonSpaceChar = _terminal->GetTextBuffer().GetLastNonSpaceCharacter();
    auto found = false;
    for (auto j = pos.y; j <= lastNonSpaceChar.y; j++)
    {
        const auto startX = pos.y == j ? pos.x + 1 : 0;
        for (auto i = startX; i < _terminal->GetTextBuffer().GetRowByOffset(j).size(); i++)
        {
            auto g = _terminal->GetTextBuffer().GetRowByOffset(j).GlyphAt(i);
            if (g == startDelimiter && startDelimiter != endDelimiter)
            {
                innerPairs++;
            }
            else if (g == endDelimiter)
            {
                if (innerPairs > 0)
                {
                    innerPairs--;
                }
                else
                {
                    found = true;
                    endX = i;
                    endY = j;
                    break;
                }
            }
        }
        if (found)
        {
            break;
        }
    }

    if (!found)
    {
        return std::make_tuple<bool, til::point, til::point>(false, {}, {});
    }

    return std::make_tuple<bool, til::point, til::point>(true, til::point{ pos.x, pos.y }, til::point{ endX, endY });
}

std::tuple<bool, til::point, til::point> VimModeProxy::_findBlockStartFromEnd(til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter) const
{
    til::CoordType startX = -1;
    til::CoordType startY = -1;
    auto found = false;

    auto innerPairs = 0;
    for (auto j = pos.y; j >= 0; j--)
    {
        const auto x = pos.y == j ? pos.x - 1 : _terminal->GetTextBuffer().GetRowByOffset(j).size();
        for (auto i = x; i >= 0; i--)
        {
            auto g = _terminal->GetTextBuffer().GetRowByOffset(j).GlyphAt(i);
            if (g == endDelimiter)
            {
                innerPairs++;
            }
            else if (g == startDelimiter)
            {
                if (innerPairs > 0)
                {
                    innerPairs--;
                }
                else
                {
                    startX = i;
                    startY = j;
                    found = true;
                    break;
                }
            }
        }
        if (found)
        {
            break;
        }
    }

    if (startX == -1)
    {
        return std::make_tuple<bool, til::point, til::point>(false, {}, {});
    }

    return std::make_tuple<bool, til::point, til::point>(true, til::point{ startX, startY }, til::point{ pos.x, pos.y });
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
    _terminal->SetSelectionAnchors(selection);
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

void VimModeProxy::_ScrollIfNeeded(const til::point& pos) noexcept
{
    const auto viewport = _terminal->GetViewport();
    const auto scrollOffset = _terminal->GetViewport().Top();

    if (pos.y >= scrollOffset && pos.y < scrollOffset + viewport.Height())
    {
        
    }
    else
    {
        auto newY = std::max(0, pos.y - 5);
        _terminal->UserScrollViewport(newY);
    }
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

    _terminal->UserScrollViewport(pos.y);
}

void VimModeProxy::ResetVimState()
{
    const auto lock = _terminal->LockForWriting();
    ExitVimMode();
    _lastTextObject = VimTextObjectType::none;
    _lastAction = VimActionType::none;
    _lastMotion = VimMotionType::none;
    _textObject = VimTextObjectType::none;
    _lastTimes = 0;
    _searchString = L"";
    _lastVkey[0] = L'\0';
    _terminal->ClearYankRegion();
    _terminal->ClearSelection();
    _terminal->SetSearchHighlights({});
    _controlCore->UpdateSelectionFromVim(_searcher->Results());
    _controlCore->ExitVim();
}

VimModeProxy::VimMode VimModeProxy::_getVimMode()
{
    return _vimMode;
}

void VimModeProxy::ExitVimMode()
{
    _vimMode = VimMode::none;
}

void VimModeProxy::EnterVimMode(bool selectLastChar)
{
    _vimMode = VimMode::normal;
    ResetVimModeForSizeChange(selectLastChar);
}

bool VimModeProxy::IsInVimMode()
{
    return _getVimMode() != VimMode::none;
}

void VimModeProxy::ResetVimModeForSizeChange(bool selectLastChar)
{
    if (_vimMode != VimMode::none)
    {
        auto lock = _terminal->LockForWriting();

        if (!_terminal->IsSelectionActive())
        {
            _terminal->ToggleMarkMode();
            if (_terminal->SelectionMode() != ::Microsoft::Terminal::Core::Terminal::SelectionInteractionMode::Mark)
            {
                _terminal->ToggleMarkMode();
            }
            selectLastChar = true;
        }

        if (_terminal->SelectionMode() != ::Microsoft::Terminal::Core::Terminal::SelectionInteractionMode::Mark)
        {
            _terminal->ToggleMarkMode();
        }

        if (selectLastChar)
        {
            _terminal->SelectLastChar();
        }

        if (selectLastChar)
        {
            _vimScrollScreenPosition(VimTextObjectType::centerOfScreen);
        }
        _controlCore->UpdateSelectionFromVim();
    }
}

void VimModeProxy::SelectRow(int32_t row, int32_t col)
{
    EnterVimMode(false);
    if (_terminal->SelectionMode() != ::Microsoft::Terminal::Core::Terminal::SelectionInteractionMode::Mark)
    {
        _terminal->ToggleMarkMode();
    }

    auto vp = _terminal->GetViewport();
    if (col > vp.Width())
    {
        int32_t rows = col / vp.Width();
        col %= vp.Width();
        row += rows;
    }

    _terminal->SelectChar(til::point{ col, row });
    _vimScrollScreenPosition(VimTextObjectType::centerOfScreen);
    _controlCore->UpdateSelectionFromVim();
}

std::wstring VimModeProxy::_getClipboardText()
{
    const auto clipboard = _openClipboard();
    if (!clipboard)
    {
        LOG_LAST_ERROR();
        return L"";
    }

    // This handles most cases of pasting text as the OS converts most formats to CF_UNICODETEXT automatically.
    if (const auto handle = GetClipboardData(CF_UNICODETEXT))
    {
        const wil::unique_hglobal_locked lock{ handle };
        const auto str = static_cast<const wchar_t*>(lock.get());
        if (!str)
        {
            return L"";
        }

        // As per: https://learn.microsoft.com/en-us/windows/win32/dataxchg/standard-clipboard-formats
        //   CF_UNICODETEXT: [...] A null character signals the end of the data.
        // --> Use wcsnlen() to determine the actual length.
        // NOTE: Some applications don't add a trailing null character. This includes past conhost versions.
        const auto maxLen = GlobalSize(handle) / sizeof(wchar_t);
        auto result = std::wstring(str, maxLen);
        if (!result.empty() && result.back() == '\0')
        {
            result.pop_back(); // Removes the last character if it's a null character
        }
        return result;
    }
    return L"";
}

wil::unique_close_clipboard_call VimModeProxy::_openClipboard()
{
    auto hwnd = GetConsoleWindow();

    bool success = false;

    // OpenClipboard may fail to acquire the internal lock --> retry.
    for (DWORD sleep = 10;; sleep *= 2)
    {
        if (OpenClipboard(hwnd))
        {
            success = true;
            break;
        }
        // 10 iterations
        if (sleep > 10000)
        {
            break;
        }
        Sleep(sleep);
    }

    return wil::unique_close_clipboard_call{ success };
}
