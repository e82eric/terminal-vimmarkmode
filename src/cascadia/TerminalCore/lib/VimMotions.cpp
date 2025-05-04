#include "pch.h"
#include "VimMotions.hpp"

#include "../Terminal.hpp"
#include "../Terminal.hpp"
#include "../Terminal.hpp"
#include "../Terminal.hpp"

namespace vim
{
    namespace motions
    {
        std::wstring _wordDelimiters = L"/\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?│";

        til::point _getLastNonSpaceChar(Microsoft::Terminal::Core::Terminal &terminal)
        {
            auto& buffer = terminal.GetTextBuffer();
            auto maybeLastChar = buffer.GetLastNonSpaceCharacter();
            auto maybeLastRowNumber = maybeLastChar.y;
            while (buffer.GetRowByOffset(maybeLastRowNumber).GetLastNonSpaceColumn() == 0)
            {
                maybeLastRowNumber--;
            }
            auto lastColumn = buffer.GetRowByOffset(maybeLastRowNumber).GetLastNonSpaceColumn();
            return til::point{ lastColumn - 1, maybeLastRowNumber };
        }

        std::tuple<bool, til::point, til::point> _findBlockStartFromEnd(Microsoft::Terminal::Core::Terminal &terminal, til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter)
        {
            til::CoordType startX = -1;
            til::CoordType startY = -1;
            auto found = false;

            auto innerPairs = 0;
            for (auto j = pos.y; j >= 0; j--)
            {
                const auto x = pos.y == j ? pos.x - 1 : terminal.GetTextBuffer().GetRowByOffset(j).size();
                for (auto i = x; i >= 0; i--)
                {
                    auto g = terminal.GetTextBuffer().GetRowByOffset(j).GlyphAt(i);
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

        std::tuple<bool, til::point, til::point> _findBlockEndFromStart(Microsoft::Terminal::Core::Terminal &terminal, til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter)
        {
            til::CoordType endX = -1;
            til::CoordType endY = -1;

            auto innerPairs = 0;
            auto lastNonSpaceChar = _getLastNonSpaceChar(terminal);
            auto found = false;
            for (auto j = pos.y; j <= lastNonSpaceChar.y; j++)
            {
                const auto startX = pos.y == j ? pos.x + 1 : 0;
                for (auto i = startX; i < terminal.GetTextBuffer().GetRowByOffset(j).size(); i++)
                {
                    auto g = terminal.GetTextBuffer().GetRowByOffset(j).GlyphAt(i);
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

        void _matchingCharFromEnd(Microsoft::Terminal::Core::Terminal &terminal, til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool isVisual)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            const std::tuple<bool, til::point, til::point> findResult = _findBlockStartFromEnd(terminal, startPos, startDelimiter, endDelimiter);
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
                terminal.SetSelectionAnchors(selection);
            }
        }

        void _matchingCharFromStart(Microsoft::Terminal::Core::Terminal &terminal, til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool isVisual)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            const std::tuple<bool, til::point, til::point> findResult = _findBlockEndFromStart(terminal, startPos, startDelimiter, endDelimiter);
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
                terminal.SetSelectionAnchors(selection);
            }
        }

        void MatchingChar(Microsoft::Terminal::Core::Terminal &terminal, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool isVisual)
        {
            const auto selection = terminal.GetSelectionAnchors();
            const auto pos = selection->start == selection->pivot ? selection->end : selection->start;
            if (onStartDelimiter)
            {
                _matchingCharFromStart(terminal, pos, startDelimiter, endDelimiter, isVisual);
            }
            else
            {
                _matchingCharFromEnd(terminal, pos, startDelimiter, endDelimiter, isVisual);
            }
        }

        void _matchingChar(Microsoft::Terminal::Core::Terminal &terminal, til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool inBlock)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            auto selection{ selectionAnchors.write() };
            std::tuple<bool, til::point, til::point> findResult;
            if (onStartDelimiter)
            {
                findResult = _findBlockEndFromStart(terminal, startPos, startDelimiter, endDelimiter);
            }
            else
            {
                findResult = _findBlockStartFromEnd(terminal, startPos, startDelimiter, endDelimiter);
            }
            if (std::get<0>(findResult))
            {
                selection->start = std::get<1>(findResult);
                selection->end = std::get<2>(findResult);
                selection->end.x++;
                if (inBlock)
                {
                    if (selection->end.x == 0)
                    {
                        selection->end.y--;
                        selection->end.x = terminal.GetTextBuffer().GetRowByOffset(selection->end.y).GetLastNonSpaceColumn() - 1;
                    }
                    else
                    {
                        selection->end.x--;
                    }
                    if (selection->start.x >= terminal.GetTextBuffer().GetRowByOffset(selection->end.y).GetLastNonSpaceColumn() - 1)
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
                terminal.SetSelectionAnchors(selection);
            }
        }

        std::pair<til::point, bool> _GetEndOfWord(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            // can't expand right
            if (target.x == bufferSize.RightInclusive())
            {
                return { {}, false };
            }

            auto result = target;
            bool found = false;
            bufferSize.IncrementInBounds(result);

            while (result.x < bufferSize.RightInclusive())
            {
                DelimiterClass previousClass = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt( result.x - 1, wordDelimiters);
                bufferSize.IncrementInBounds(result);
                auto classAt = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x - 1, wordDelimiters);
                if (classAt != previousClass && previousClass != DelimiterClass::ControlChar)
                {
                    bufferSize.DecrementInBounds(result);
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                return { {}, false };
            }

            //bufferSize.DecrementInBounds(result);

            return { til::point{ result.x, result.y }, true };
        }

        std::pair<til::point, bool> _GetEndOfWord2(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            // can't expand right
            if (target.x == bufferSize.RightInclusive())
            {
                return { {}, false };
            }

            auto result = target;
            bool found = false;

            while (result.x < bufferSize.RightInclusive())
            {
                DelimiterClass previousClass = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt( result.x - 1, wordDelimiters);
                bufferSize.IncrementInBounds(result);
                auto classAt = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x - 1, wordDelimiters);
                if (classAt != previousClass && previousClass != DelimiterClass::ControlChar)
                {
                    bufferSize.DecrementInBounds(result);
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                return { {}, false };
            }

            //bufferSize.DecrementInBounds(result);

            return { til::point{ result.x, result.y }, true };
        }

        til::point _GetLineEnd(Microsoft::Terminal::Core::Terminal &terminal, const til::point target)
        {
            const auto bufferSize{ terminal.GetTextBuffer().GetSize() };

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
                auto classAt = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, L"");
                if (classAt != DelimiterClass::ControlChar)
                {
                    lastNonControlChar = result.x;
                }
                bufferSize.IncrementInBounds(result);
            }

            return til::point{ lastNonControlChar + 1, target.y };
        }

        std::pair<til::point, bool> _GetStartOfNextWord(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            //auto wordEnd = _GetEndOfWord(terminal, target, wordDelimiters);

            //if (!wordEnd.second)
            //{
            //    return wordEnd;
            //}

            const auto lastNonSpaceChar = _getLastNonSpaceChar(terminal);
            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            // can't expand right
            if (target.x == bufferSize.RightInclusive())
            {
                return { {}, false };
            }

            auto result = target;
            bool found = false;

            auto startDelimiterClass = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt( result.x, wordDelimiters);

            // expand right until we hit the right boundary or a different delimiter class
            while (result.x < bufferSize.RightInclusive())
            {
                auto previousClass = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
                bufferSize.IncrementInBounds(result);
                auto classAt = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
                if (classAt != startDelimiterClass && classAt != DelimiterClass::ControlChar || (previousClass == DelimiterClass::ControlChar && classAt != DelimiterClass::ControlChar) || result == lastNonSpaceChar)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                return { {}, false };
            }

            return { result, true };
        }

        std::pair<til::point, bool> _GetStartOfWord(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            if (target.x == 0)
            {
                return { {}, false };
            }
            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            auto result = target;
            bool found = false;
            DelimiterClass previousClass;
            bufferSize.DecrementInBounds(result);

            while (result.x > 0)
            {
                previousClass = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
                bufferSize.DecrementInBounds(result);
                auto classAt = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
                if (classAt != previousClass && previousClass != DelimiterClass::ControlChar)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                return { {0, target.y}, true };
            }

            bufferSize.IncrementInBounds(result);

            return { result, true };
        }

        std::pair<til::point, bool> _GetEndOfPreviousWord(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            auto wordStart = _GetStartOfWord(terminal, target, wordDelimiters);

            if (!wordStart.second)
            {
                return wordStart;
            }

            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            auto result = wordStart.first;
            bool found = false;

            bufferSize.DecrementInBounds(result);

            while (result.x >= 0)
            {
                auto classAt = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
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

        bool _FindCharBack(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isTil, til::point& target)
        {
            auto cursor = GetVimCursor(terminal);
            auto selection = terminal.GetSelectionAnchors();
            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            const bool pivotAtStart = (selection->start == selection->pivot);
            auto startPoint = selection->start;
            if (!isSingleCell && pivotAtStart)
            {
                startPoint = selection->end;
            }
            if (cursor.IsBlock)
            {
                startPoint = cursor.Span.start;
            }

            startPoint.x--;
            if (isTil && startPoint.x > 0)
            {
                startPoint.x--;
            }

            auto newY = startPoint.y;

            while (terminal.GetTextBuffer().GetRowByOffset(newY).WasWrapForced() || newY == startPoint.y)
            {
                auto& row = terminal.GetTextBuffer().GetRowByOffset(newY);
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

        void _InWord(Microsoft::Terminal::Core::Terminal &terminal, til::point& pos, std::wstring_view delimiters)
        {
            auto endPair = _GetEndOfWord2(terminal, pos, delimiters);
            auto startPair = _GetStartOfWord(terminal, pos, delimiters);

            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            if (endPair.second && endPair.second)
            {
                selection->end = endPair.first;
                selection->pivot = endPair.first;
                selection->start = startPair.first;
            }
            terminal.SetSelectionAnchors(selection);
        }

        bool _FindChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isTil, til::point& target)
        {
            auto cursor = GetVimCursor(terminal);
            const auto selection = terminal.GetSelectionAnchors();
            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            const bool pivotAtStart = (selection->start == selection->pivot);

            auto startPoint = selection->start;
            if (!isSingleCell && pivotAtStart)
            {
                startPoint = selection->end;
            }
            if (cursor.IsBlock)
            {
                startPoint = cursor.Span.start;
            }

            startPoint.x++;
            if (isTil)
            {
                startPoint.x++;
            }

            auto newY = startPoint.y;
            auto lineWrapped = false;

            while (lineWrapped || newY == startPoint.y)
            {
                const auto startX = newY > startPoint.y ? 0 : startPoint.x;
                auto& row = terminal.GetTextBuffer().GetRowByOffset(newY);
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

        std::pair<til::point, bool> _GetLineFirstNonBlankChar(Microsoft::Terminal::Core::Terminal& terminal, const til::point target)
        {
            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            auto result = target;
            bool found = false;

            while (result.x < bufferSize.RightInclusive())
            {
                auto classAt = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, L"");
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

        til::CoordType _getStartLineOfRow(TextBuffer& textBuffer, til::CoordType row)
        {
            auto result = row;
            while (textBuffer.GetRowByOffset(result - 1).WasWrapForced())
            {
                result--;
            }
            return result;
        }

        void _UpdateSelection(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, til::point adjusted)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
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
                terminal.SetSelectionAnchors(selection);
            }
            else
            {
                SelectPoint(terminal, adjusted);
            }
        }

        til::point _getLastNonSpaceChar(const Microsoft::Terminal::Core::Terminal& terminal)
        {
            auto& buffer = terminal.GetTextBuffer();
            auto maybeLastChar = buffer.GetLastNonSpaceCharacter();
            auto maybeLastRowNumber = maybeLastChar.y;
            while (buffer.GetRowByOffset(maybeLastRowNumber).GetLastNonSpaceColumn() == 0)
            {
                maybeLastRowNumber--;
            }
            auto lastColumn = buffer.GetRowByOffset(maybeLastRowNumber).GetLastNonSpaceColumn();
            return til::point{ lastColumn, maybeLastRowNumber };
        }

        void SelectPoint(Microsoft::Terminal::Core::Terminal &terminal, til::point point)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            selection->active = true;
            selection->start = til::point{ point.x, point.y };
            selection->pivot = selection->start;
            selection->end = til::point{ point.x + 1, point.y };
            terminal.SetSelectionAnchors(selection);
        }

        void SelectLastNonSpaceChar(Microsoft::Terminal::Core::Terminal& terminal)
        {
            auto lastNonSpaceChar = _getLastNonSpaceChar(terminal);
            SelectPoint(terminal, lastNonSpaceChar);
        }

        void _moveToStartOfNextLineBlock(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            if (cursor.BlockHorizontalPosition == Right || cursor.BlockHorizontalPosition == SingleColumn)
            {
                selection->end = { 1, cursor.Span.start.y + 1 };
                selection->pivot = selection->start;
            }
            else
            {
                selection->start = { 0, cursor.Span.start.y + 1 };
                selection->pivot = selection->start;
            }
        }

        void _moveLeftBlock(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection)
        {
            if (selection->blockSelection)
            {
                auto cursor = GetVimCursor(terminal, selection);

                if (cursor.BlockVerticalPosition == Top)
                {
                    if (cursor.BlockHorizontalPosition == SingleColumn || cursor.BlockHorizontalPosition == Left)
                    {
                        selection->start = { selection->start.x - 1, selection->start.y };
                        selection->pivot = { selection->end.x, selection->end.y };
                    }
                    else
                    {
                        selection->end = { selection->end.x - 1, selection->end.y };
                    }
                }
                else
                {
                    if (cursor.BlockHorizontalPosition == Left || cursor.BlockHorizontalPosition == SingleColumn)
                    {
                        selection->start = { selection->start.x - 1, selection->start.y };
                        selection->pivot = { selection->end.x, selection->start.y };
                    }
                    else
                    {
                        selection->end = { selection->end.x - 1, selection->end.y };
                    }
                }
            }
        }

        void _moveLeftByCell(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto startOfLine = _getStartLineOfRow(terminal.GetTextBuffer(), cursor.Span.start.y);

            if (selection->start.x > 0)
            {
                selection->start.x--;
                selection->end.x--;
                selection->pivot = selection->start;
            }
            else if (startOfLine < cursor.Span.start.y)
            {
                auto previousRow = cursor.Span.start.y - 1;
                const auto lastNonSpaceColumn = std::max(0, terminal.GetTextBuffer().GetRowByOffset(previousRow).GetLastNonSpaceColumn() - 1);
                selection->end = { lastNonSpaceColumn, previousRow };
                selection->start = { lastNonSpaceColumn - 1, previousRow };
                selection->pivot = selection->start;
            }
        }

        void _moveLeftVisual(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto startOfLine = _getStartLineOfRow(terminal.GetTextBuffer(), cursor.Span.start.y);

            switch (cursor.Position)
            {
            case VimCursorPosition::Start:
            case VimCursorPosition::SingleCell:
                if (selection->start.x > 0)
                {
                    selection->start.x--;
                    selection->pivot = selection->end;
                }
                else if (cursor.Span.start.y > startOfLine)
                {
                    auto previousRow = cursor.Span.start.y - 1;
                    const auto lastNonSpaceColumn = std::max(0, terminal.GetTextBuffer().GetRowByOffset(previousRow).GetLastNonSpaceColumn() - 1);
                    selection->start = { lastNonSpaceColumn - 1, previousRow };
                    selection->pivot = selection->end;
                }
                break;
            case VimCursorPosition::End:
                selection->end.x--;
                break;
            }
        }

        void _moveLeft(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, bool isVisual)
        {
            if (!isVisual)
            {
                _moveLeftByCell(terminal, selection);
            }
            else if (selection->blockSelection)
            {
                _moveLeftBlock(terminal, selection);
            }
            else
            {
                _moveLeftVisual(terminal, selection);
            }
        }

        void MoveLeft(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto vimSelection = terminal.GetVimSelectionAnchors();
            if (!isVisual)
            {
                _moveLeftByCell(terminal, &vimSelection);
            }
            else if (vimSelection.blockSelection)
            {
                _moveLeftBlock(terminal, &vimSelection);
            }
            else
            {
                _moveLeftVisual(terminal, &vimSelection);
            }
            terminal.SetVimSelectionAnchors(&vimSelection);
        }

        void _moveRightVisualBlock(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto lineEnd = terminal.GetTextBuffer().GetLineWidth(cursor.Span.start.y);
            if (cursor.Span.end.x >= lineEnd)
            {
                return;
            }

            if (cursor.BlockVerticalPosition == Top)
            {
                if (cursor.BlockHorizontalPosition != SingleColumn && cursor.BlockHorizontalPosition == Left)
                {
                    selection->start = { selection->start.x + 1, selection->start.y };
                }
                else
                {
                    selection->end = { selection->end.x + 1, selection->end.y };
                    selection->pivot = { selection->start.x, selection->end.y };
                }
            }
            else
            {
                if (cursor.BlockHorizontalPosition == SingleColumn || cursor.BlockHorizontalPosition == Right)
                {
                    selection->end = { selection->end.x + 1, selection->end.y };
                    selection->pivot = selection->start;
                }
                else if (cursor.BlockHorizontalPosition == Left)
                {
                    selection->start = { selection->start.x + 1, selection->start.y };
                }
            }
        }

        void _moveRightByCell(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto lineEnd = terminal.GetTextBuffer().GetLineWidth(cursor.Span.start.y);
            if (cursor.Span.end.x + 1 > lineEnd)
            {
                auto wrapped = terminal.GetTextBuffer().GetRowByOffset(cursor.Span.start.y).WasWrapForced();
                if (wrapped)
                {
                    selection->start = { 0, cursor.Span.start.y + 1 };
                    selection->end = { 1, cursor.Span.start.y + 1};
                    selection->pivot = selection->start;
                }
                return;
            }
            selection->start.x++;
            selection->end = { selection->start.x + 1, selection->start.y };
            selection->pivot = selection->start;
        }

        void _moveRightVisual(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto lineEnd = terminal.GetTextBuffer().GetLineWidth(cursor.Span.start.y);

            if (cursor.Position == SingleCell || cursor.Position == VimCursorPosition::End)
            {
                if (cursor.Span.end.x >= lineEnd)
                {
                    auto wrapped = terminal.GetTextBuffer().GetRowByOffset(cursor.Span.start.y).WasWrapForced();
                    if (wrapped)
                    {
                        selection->end = { 1, cursor.Span.start.y + 1 };
                        selection->pivot = selection->start;
                    }
                    return;
                }
                selection->end.x++;
                selection->pivot = selection->start;
            }
            else
            {
                selection->start.x++;
                selection->pivot = selection->end;
            }
        }

        void _moveRight(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, bool isVisual)
        {
            if (!isVisual)
            {
                _moveRightByCell(terminal, selection);
            }
            else if (selection->blockSelection)
            {
                _moveRightVisualBlock(terminal, selection);
            }
            else
            {
                _moveRightVisual(terminal, selection);
            }
        }

        void MoveRight(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            _moveRight(terminal, &selection, isVisual);
            terminal.SetVimSelectionAnchors(&selection);
        }

        void _moveDownBlock(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection)
        {
            auto lastY = _getLastNonSpaceChar(terminal).y;
            auto cursor = GetVimCursor(terminal, selection);
            auto point = cursor.Span.start;

            if (point.y + 1 <= lastY)
            {
                if (cursor.IsBlock)
                {
                    if (cursor.BlockVerticalPosition == SingleRow || cursor.BlockVerticalPosition == Bottom)
                    {
                        selection->end = { selection->end.x, cursor.Span.end.y + 1 };
                        if (cursor.BlockHorizontalPosition == Left || cursor.BlockHorizontalPosition == SingleColumn)
                        {
                            selection->pivot = { selection->end.x, selection->start.y };
                        }
                        else
                        {
                            selection->pivot = { selection->start.x, selection->start.y };
                        }
                    }
                    else
                    {
                        if (cursor.BlockHorizontalPosition == Left)
                        {
                            selection->start = { selection->start.x, selection->start.y + 1 };
                            selection->pivot = selection->end;
                        }
                        else
                        {
                            selection->start = { selection->start.x, selection->start.y + 1 };
                            selection->pivot = { selection->start.x, selection->end.y };
                        }
                    }
                }
            }
        }

        void _moveDownVisual(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto lastY = _getLastNonSpaceChar(terminal).y;
            auto newY = cursor.Span.start.y + 1;

            if (newY <= lastY)
            {
                switch (cursor.Position)
                {
                case VimCursorPosition::SingleCell:
                    selection->end = { cursor.Span.end.x, cursor.Span.end.y + 1 };
                    selection->pivot = selection->start;
                    break;
                case VimCursorPosition::Start:
                    if (newY > selection->pivot.y)
                    {
                        selection->start = { selection->pivot.x - 1, selection->pivot.y };
                        selection->end = { cursor.Span.end.x, cursor.Span.end.y + 1 };
                        selection->pivot = selection->start;
                    }
                    else
                    {

                        selection->start = {cursor.Span.start.x, newY};
                        selection->pivot = selection->end;
                    }
                    break;
                case VimCursorPosition::End:
                    selection->end = { cursor.Span.end.x, cursor.Span.end.y + 1 };
                    selection->pivot = selection->start;
                    break;
                }
            }
        }

        void _moveDown(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            til::point point = { cursor.Span.start };
            point.y++;
            selection->start = { point.x, point.y };
            selection->end = { point.x + 1, point.y };
            selection->pivot = selection->start;
        }

        void MoveDown(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto vimSelection = terminal.GetVimSelectionAnchors();
            if (vimSelection.blockSelection)
            {
                _moveDownBlock(terminal, &vimSelection);
                terminal.SetVimSelectionAnchors(&vimSelection);
                return;
            }
            if (!isVisual)
            {
                _moveDown(terminal, &vimSelection);
            }
            else
            {
                _moveDownVisual(terminal, &vimSelection);
            }
            terminal.SetVimSelectionAnchors(&vimSelection);
        }

        VimCursor GetVimCursor(Microsoft::Terminal::Core::Terminal &/*terminal*/, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection) noexcept
        {
            VimCursorPosition position = {};
            BlockVimCursorHorizontalPosition blockVimCursorHorizontalPosition = {};
            BlockVimCursorVerticalPosition blockVimCursorVerticalPosition = {};
            auto isBlock = selection->blockSelection;
            if (isBlock)
            {
                if (selection->start.y == selection->end.y)
                {
                    blockVimCursorVerticalPosition = SingleRow;
                }
                else if (selection->start.y == selection->pivot.y)
                {
                    blockVimCursorVerticalPosition = Bottom;
                }
                else
                {
                    blockVimCursorVerticalPosition = Top;
                }

                if (selection->start.x + 1 == selection->end.x)
                {
                    blockVimCursorHorizontalPosition = SingleColumn;
                }
                else if (selection->start.x == selection->pivot.x)
                {
                    blockVimCursorHorizontalPosition = Right;
                }
                else
                {
                    blockVimCursorHorizontalPosition = Left;
                }
            }
            else
            {
                if (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y)
                {
                    position = SingleCell;
                }
                else if (selection->start == selection->pivot)
                {
                    position = End;
                }
                else
                {
                    position = Start;
                }
            }

            til::point start;
            til::point end;

            if (selection->blockSelection)
            {
                if (blockVimCursorHorizontalPosition == SingleColumn)
                {
                    if (blockVimCursorVerticalPosition == SingleRow)
                    {
                        start = selection->start;
                        end = selection->end;
                    }
                    else if (blockVimCursorVerticalPosition == Top)
                    {
                        start = selection->start;
                        end = { selection->start.x + 1, selection->start.y };
                    }
                    else if (blockVimCursorVerticalPosition == Bottom)
                    {
                        start = { selection->end.x - 1, selection->end.y };
                        end = selection->end;
                    }
                }
                else if (blockVimCursorHorizontalPosition == Left)
                {
                    if (blockVimCursorVerticalPosition == SingleRow || blockVimCursorVerticalPosition == Top)
                    {
                        start = selection->start;
                        end = { selection->start.x + 1, selection->start.y };
                    }
                    else if (blockVimCursorVerticalPosition == Bottom)
                    {
                        start = { selection->start.x, selection->end.y };
                        end = { selection->start.x + 1, selection->end.y };
                    }
                }
                else if (blockVimCursorHorizontalPosition == Right)
                {
                    if (blockVimCursorVerticalPosition == SingleRow || blockVimCursorVerticalPosition == Top)
                    {
                        start = { selection->end.x - 1, selection->start.y };
                        end = { selection->end.x, selection->start.y };
                    }
                    else if (blockVimCursorVerticalPosition == Bottom)
                    {
                        start = { selection->end.x - 1, selection->end.y };
                        end = { selection->end.x, selection->end.y };
                    }
                }
            }
            else if (position == SingleCell)
            {
                start = selection->start;
                end = selection->end;
            }
            else if (position == End)
            {
                start = { selection->end.x - 1, selection->end.y };
                end = selection->end;
            }
            else
            {
                start = selection->start;
                end = { selection->start.x + 1, selection->start.y };
            }

            return VimCursor{ isBlock,  position, blockVimCursorHorizontalPosition, blockVimCursorVerticalPosition, til::point_span{ start, end } };
        }

        VimCursor GetVimCursor(Microsoft::Terminal::Core::Terminal &terminal) noexcept
        {
            auto selection = terminal.GetVimSelectionAnchors();

            if (selection.start == selection.end)
            {
                selection.end = { selection.start.x + 1, selection.start.y };
                selection.pivot = selection.start;
                terminal.SetVimSelectionAnchors(&selection);
            }

            VimCursorPosition position = {};
            BlockVimCursorHorizontalPosition blockVimCursorHorizontalPosition = {};
            BlockVimCursorVerticalPosition blockVimCursorVerticalPosition = {};
            auto isBlock = selection.blockSelection;
            if (isBlock)
            {
                if (selection.start.y == selection.end.y)
                {
                    blockVimCursorVerticalPosition = SingleRow;
                }
                else if (selection.start.y == selection.pivot.y)
                {
                    blockVimCursorVerticalPosition = Bottom;
                }
                else
                {
                    blockVimCursorVerticalPosition = Top;
                }

                if (selection.start.x + 1 == selection.end.x)
                {
                    blockVimCursorHorizontalPosition = SingleColumn;
                }
                else if (selection.start.x == selection.pivot.x)
                {
                    blockVimCursorHorizontalPosition = Right;
                }
                else
                {
                    blockVimCursorHorizontalPosition = Left;
                }
            }
            else
            {
                if (selection.end.x - 1 == selection.start.x && selection.start.y == selection.end.y)
                {
                    position = SingleCell;
                }
                else if (selection.start == selection.pivot)
                {
                    position = End;
                }
                else
                {
                    position = Start;
                }
            }

            til::point start;
            til::point end;

            if (selection.blockSelection)
            {
                if (blockVimCursorHorizontalPosition == SingleColumn)
                {
                    if (blockVimCursorVerticalPosition == SingleRow)
                    {
                        start = selection.start;
                        end = selection.end;
                    }
                    else if (blockVimCursorVerticalPosition == Top)
                    {
                        start = selection.start;
                        end = { selection.start.x + 1, selection.start.y };
                    }
                    else if (blockVimCursorVerticalPosition == Bottom)
                    {
                        start = { selection.end.x - 1, selection.end.y };
                        end = selection.end;
                    }
                }
                else if (blockVimCursorHorizontalPosition == Left)
                {
                    if (blockVimCursorVerticalPosition == SingleRow || blockVimCursorVerticalPosition == Top)
                    {
                        start = selection.start;
                        end = { selection.start.x + 1, selection.start.y };
                    }
                    else if (blockVimCursorVerticalPosition == Bottom)
                    {
                        start = { selection.start.x, selection.end.y };
                        end = { selection.start.x + 1, selection.end.y };
                    }
                }
                else if (blockVimCursorHorizontalPosition == Right)
                {
                    if (blockVimCursorVerticalPosition == SingleRow || blockVimCursorVerticalPosition == Top)
                    {
                        start = { selection.end.x - 1, selection.start.y };
                        end = { selection.end.x, selection.start.y };
                    }
                    else if (blockVimCursorVerticalPosition == Bottom)
                    {
                        start = { selection.end.x - 1, selection.end.y };
                        end = { selection.end.x, selection.end.y };
                    }
                }
            }
            else if (position == SingleCell)
            {
                start = selection.start;
                end = { selection.start.x + 1, selection.start.y };
            }
            else if (position == End)
            {
                start = { selection.end.x - 1, selection.end.y };
                end = selection.end;
            }
            else
            {
                start = selection.start;
                end = { selection.start.x + 1, selection.start.y };
            }

            return VimCursor{ isBlock,  position, blockVimCursorHorizontalPosition, blockVimCursorVerticalPosition, til::point_span{ start, end } };
        }

        void _moveUpBlock(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            if (cursor.Span.start.y - 1 >= 0)
            {
                if (cursor.IsBlock)
                {
                    if (cursor.BlockVerticalPosition == SingleRow || cursor.BlockVerticalPosition == Top)
                    {
                        selection->start = { selection->start.x, cursor.Span.start.y - 1 };
                        if (cursor.BlockHorizontalPosition == Left || cursor.BlockHorizontalPosition == SingleColumn)
                        {
                            selection->pivot = { selection->end.x, selection->end.y };
                        }
                        else
                        {
                            selection->pivot = { selection->start.x, selection->end.y };
                        }
                    }
                    else
                    {
                        selection->end = { selection->end.x, cursor.Span.end.y - 1 };
                        selection->pivot = { selection->pivot.x, selection->start.y };
                    }
                }
            }
        }

        void _moveUpCell(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto newY = std::max(cursor.Span.start.y - 1, 0);
            selection->start = { cursor.Span.start.x, newY };
            selection->end = { cursor.Span.end.x, newY };
            selection->pivot = selection->start;
        }

        void _moveUpVisual(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto newY = std::max(cursor.Span.start.y - 1, 0);
            if (cursor.Position == VimCursorPosition::End)
            {
                if (til::point{cursor.Span.start.x, newY} <= selection->pivot)
                {
                    selection->start = { cursor.Span.start.x, newY };
                    selection->end = { selection->pivot.x + 1, selection->pivot.y };
                    selection->pivot = selection->end;
                }
                else
                {
                    selection->end = {selection->end.x, newY};
                    selection->pivot = selection->start;
                }
            }
            else
            {
                selection->start = { selection->start.x, newY };
                selection->pivot = selection->end;
            }
        }

        void MoveUp(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto cursor = GetVimCursor(terminal);
            auto vimSelection = terminal.GetVimSelectionAnchors();
            if (cursor.Span.start.y - 1 >= 0)
            {
                if (vimSelection.blockSelection)
                {
                    _moveUpBlock(terminal, &vimSelection);
                }
                else if (!isVisual)
                {
                    _moveUpCell(terminal, &vimSelection);
                }
                else
                {
                    _moveUpVisual(terminal, &vimSelection);
                }
                terminal.SetVimSelectionAnchors(&vimSelection);
            }
        }

        void MoveToStartOfLine(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto cursor = GetVimCursor(terminal);
            auto startOfLine = _getStartLineOfRow(terminal.GetTextBuffer(), cursor.Span.start.y);
            auto vimSelection = terminal.GetVimSelectionAnchors();
            while (cursor.Span.start != til::point{ 0, startOfLine })
            {
                _moveLeft(terminal, &vimSelection, isVisual);
                cursor = GetVimCursor(terminal, &vimSelection);
            }
            terminal.SetVimSelectionAnchors(&vimSelection);
        }

        void MoveToEndOfLine(Microsoft::Terminal::Core::Terminal& terminal, bool isvisual)
        {
            auto cursor = GetVimCursor(terminal);
            auto vimSelection = terminal.GetVimSelectionAnchors();
            til::point target;
            if (terminal.IsBlockSelection())
            {
                auto maxNonSpaceChar = 0;
                for (auto i = vimSelection.start.y; i <= vimSelection.end.y; i++)
                {
                    const auto lastNonSpaceColumn = std::max(0, terminal.GetTextBuffer().GetRowByOffset(i).GetLastNonSpaceColumn() - 1);
                    if (lastNonSpaceColumn > maxNonSpaceChar)
                    {
                        maxNonSpaceChar = lastNonSpaceColumn;
                    }
                }
                target = { maxNonSpaceChar, cursor.Span.start.y };
            }
            else
            {
                auto lastRowWithChars = cursor.Span.start.y;
                til::CoordType endLine = lastRowWithChars;
                while (terminal.GetTextBuffer().GetRowByOffset(endLine).WasWrapForced())
                {
                    endLine++;
                    if (terminal.GetTextBuffer().GetRowByOffset(endLine).GetLastNonSpaceColumn() > 0)
                    {
                        lastRowWithChars = endLine;
                    }
                }

                const auto lastNonSpaceColumn = std::max(0, terminal.GetTextBuffer().GetRowByOffset(lastRowWithChars).GetLastNonSpaceColumn() - 1);
                target = til::point{ lastNonSpaceColumn, lastRowWithChars };
            }

            while (cursor.Span.start != target)
            {
                _moveRight(terminal, &vimSelection, isvisual);
                cursor = GetVimCursor(terminal, &vimSelection);
            }
            terminal.SetVimSelectionAnchors(&vimSelection);
        }

        void TilChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            til::point target;
            auto cursor = GetVimCursor(terminal);
            auto vimSelection = terminal.GetVimSelectionAnchors();
            if (cursor.IsBlock)
            {
                if (_FindChar(terminal, vkey, true, target))
                {
                    for (auto i = cursor.Span.start.x; i < target.x; i++)
                    {
                        _moveRightVisualBlock(terminal, &vimSelection);
                    }
                }
                terminal.SetVimSelectionAnchors(&vimSelection);
                return;
            }

            if (_FindChar(terminal, vkey, true, target))
            {
                while (cursor.Span.start != target)
                {
                    _moveRight(terminal, &vimSelection, isVisual);
                    cursor = GetVimCursor(terminal, &vimSelection);
                }
                terminal.SetVimSelectionAnchors(&vimSelection);
            }
        }

        void TilCharBackwards(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            til::point target;
            auto cursor = GetVimCursor(terminal);
            if (cursor.IsBlock)
            {
                if (_FindCharBack(terminal, vkey,true, target))
                {
                    auto vimSelection = terminal.GetVimSelectionAnchors();
                    for (auto i = cursor.Span.start.x; i > target.x; i--)
                    {
                        _moveLeftBlock(terminal, &vimSelection);
                    }
                    terminal.SetVimSelectionAnchors(&vimSelection);
                }
                return;
            }


            if (_FindCharBack(terminal, vkey, true, target))
            {
                auto selectionAnchors = terminal.GetSelectionAnchors();
                auto selection{ selectionAnchors.write() };
                if (isVisual)
                {
                    const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
                    const bool pivotAtStart = (selection->start == selection->pivot);
                    const bool pivotAtEnd = (selection->end == selection->pivot);

                    if (isSingleCell || pivotAtEnd)
                    {
                        selection->start = target;
                        selection->pivot = selection->end;
                    }
                    else if (pivotAtStart)
                    {
                        if (target < selection->pivot)
                        {
                            selection->start = target;
                            selection->end = { selection->pivot.x + 1, selection->pivot.y };
                            selection->pivot = selection->end;
                        }
                        else
                        {
                            selection->end = { target.x + 1, target.y };
                            selection->pivot = selection->start;
                        }
                    }

                    terminal.SetSelectionAnchors(selection);
                }
                else
                {
                    _UpdateSelection(terminal, isVisual, target);
                }
            }
        }

        void FindChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            auto cursor = GetVimCursor(terminal);
            til::point target;
            if (cursor.IsBlock)
            {
                target = { cursor.Span.start };
                if (_FindChar(terminal, vkey, false, target))
                {
                    auto vimSelection = terminal.GetVimSelectionAnchors();
                    for (auto i = cursor.Span.start.x; i < target.x; i++)
                    {
                        _moveRightVisualBlock(terminal, &vimSelection);
                    }
                    terminal.SetVimSelectionAnchors(&vimSelection);
                }

                return;
            }

            if (_FindChar(terminal, vkey, false, target))
            {
                auto selectionAnchors = terminal.GetSelectionAnchors();
                auto selection{ selectionAnchors.write() };
                if (isVisual)
                {
                    const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
                    const bool pivotAtStart = (selection->start == selection->pivot);
                    const bool pivotAtEnd = (selection->end == selection->pivot);

                    if (isSingleCell || pivotAtStart)
                    {
                        selection->end = { target.x + 1, target.y };
                        selection->pivot = selection->start;
                    }
                    else if (pivotAtEnd)
                    {
                        if (target > selection->pivot)
                        {
                            selection->start = { selection->pivot.x - 1, selection->pivot.y };
                            selection->end = { target.x + 1, target.y };
                            selection->pivot = selection->start;
                        }
                        else
                        {
                            selection->start = target;
                            selection->pivot = selection->end;
                        }
                    }

                    terminal.SetSelectionAnchors(selection);
                }
                else
                {
                    _UpdateSelection(terminal, isVisual, target);
                }
            }
        }

        void FindCharBackwards(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            til::point target;
            auto cursor = GetVimCursor(terminal);
            if (cursor.IsBlock)
            {
                if (_FindCharBack(terminal, vkey, false, target))
                {
                    auto vimSelection = terminal.GetVimSelectionAnchors();
                    for (auto i = cursor.Span.start.x; i > target.x; i--)
                    {
                        _moveLeftBlock(terminal, &vimSelection);
                    }
                    terminal.SetVimSelectionAnchors(&vimSelection);
                }
                return;
            }
            if (_FindCharBack(terminal, vkey, false, target))
            {
                auto selectionAnchors = terminal.GetSelectionAnchors();
                auto selection{ selectionAnchors.write() };
                if (isVisual)
                {
                    const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
                    const bool pivotAtStart = (selection->start == selection->pivot);
                    const bool pivotAtEnd = (selection->end == selection->pivot);

                    if (isSingleCell || pivotAtEnd)
                    {
                        selection->start = target;
                        selection->pivot = selection->end;
                    }
                    else if (pivotAtStart)
                    {
                        if (target < selection->pivot)
                        {
                            selection->start = target;
                            selection->end = { selection->pivot.x + 1, selection->pivot.y };
                            selection->pivot = selection->end;
                        }
                        else
                        {
                            selection->end = { target.x + 1, target.y };
                            selection->pivot = selection->start;
                        }
                    }

                    terminal.SetSelectionAnchors(selection);
                }
                else
                {
                    _UpdateSelection(terminal, isVisual, target);
                }
            }
        }

        void MoveWordLeft(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord, bool isVisual)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            auto selection{ selectionAnchors.write() };
            auto delimiters = isLargeWord ? L"" : _wordDelimiters;

            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            const bool pivotAtStart = (selection->start == selection->pivot);
            const bool pivotAtEnd = (selection->end == selection->pivot);

            auto lastPoint = _getLastNonSpaceChar(terminal);
            auto startPoint = selection->start;
            //if (isSingleCell)
            //{
            //    startPoint = { selection->start.x, selection->start.y };
            //}
            //else if (pivotAtStart)
            //{
            //    startPoint = { selection->end.x - 1, selection->end.y };
            //}
            startPoint = terminal.GetVimCursor().front().start;

            auto updateSelection = [&](til::point wordEnd) {
                if (!isVisual)
                {
                    selection->start = wordEnd;
                    selection->end = til::point{wordEnd.x + 1, wordEnd.y};
                    selection->pivot = selection->start;
                }
                else if (selection->blockSelection)
                {
                    auto singleColumn = selection->start.x + 1 == selection->end.x;
                    auto pivotAtBottom = selection->pivot.y == selection->end.y;
                    auto pivotAtRight = selection->pivot.x == selection->end.x;

                    if (pivotAtBottom)
                    {
                        if (singleColumn || pivotAtRight)
                        {
                            selection->start = { wordEnd.x, selection->start.y };
                            selection->pivot = selection->end;
                        }
                        else
                        {
                            if (wordEnd.x < selection->pivot.x)
                            {
                                auto newEndX = selection->start.x + 1;
                                selection->start = { wordEnd.x, selection->start.y };
                                selection->end = { newEndX, selection->pivot.y };
                                selection->pivot = selection->end;
                            }
                            else
                            {
                                selection->end = { wordEnd.x + 1, selection->end.y };
                            }
                        }
                    }
                    else
                    {
                        if (singleColumn || pivotAtRight)
                        {
                            selection->start = { wordEnd.x, selection->start.y };
                            selection->pivot = { selection->end.x, selection->start.y };
                        }
                        else
                        {
                            if (wordEnd.x < selection->pivot.x)
                            {
                                selection->start = { wordEnd.x, selection->start.y };
                                selection->end = { selection->pivot.x + 1, selection->end.y };
                                selection->pivot = { selection->end.x, selection->start.y };
                            }
                            else
                            {
                                //selection->start = { wordEnd.x, selection->start.y };
                                selection->end = { wordEnd.x + 1, selection->end.y };
                                selection->pivot = selection->start;
                            }
                        }
                    }
                }
                else if (isSingleCell)
                {
                    selection->start = {wordEnd.x, wordEnd.y};
                    selection->pivot = selection->end;
                }
                else if (pivotAtStart)
                {
                    if (wordEnd < selection->pivot)
                    {
                        selection->start = wordEnd;
                        selection->end = { selection->pivot.x + 1, selection->pivot.y };
                        selection->pivot = selection->end;
                    }
                    else
                    {
                        selection->end = { wordEnd.x + 1, wordEnd.y };
                        selection->pivot = selection->start;
                    }
                }
                else if (pivotAtEnd)
                {
                    selection->start = { wordEnd.x, wordEnd.y };
                    selection->pivot = selection->end;
                }
                terminal.SetSelectionAnchors(selection);
            };

            auto endPair = _GetStartOfWord(terminal, startPoint, delimiters);
            if (endPair.second)
            {
                updateSelection(endPair.first);
                return;
            }

            auto yToMove = startPoint.y;
            while (yToMove - 1 >= 0)
            {
                --yToMove;
                auto startOfNextLine = til::point{ 0, yToMove };
                auto startOfNextLinePair = _GetLineEnd(terminal, startOfNextLine);
                endPair = _GetStartOfWord(terminal, startOfNextLinePair, delimiters);
                if (endPair.second)
                {
                    updateSelection(endPair.first);
                    return;
                }
            }

            updateSelection(til::point{ 0, 0 });
        }

        std::wstring _getVimDelimiters()
        {
            std::wstring delimiters;
            // Considering the standard ASCII range:
            for (wchar_t c = 0; c < 128; ++c)
            {
                if (!(std::iswalnum(c) || c == L'_'))
                {
                    delimiters.push_back(c);
                }
            }
            return delimiters;
        }

        void MoveWordRight(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord, bool isVisual)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            auto selection{ selectionAnchors.write() };
            auto delimiters = isLargeWord ? L"" : _wordDelimiters;

            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            const bool pivotAtStart = (selection->start == selection->pivot);
            const bool pivotAtEnd = (selection->end == selection->pivot);

            auto lastPoint = _getLastNonSpaceChar(terminal);
            auto vimCursor = terminal.GetVimCursor();
            auto startPoint = selection->end;
            if (isSingleCell || pivotAtEnd && !selection->blockSelection)
            {
                startPoint = { selection->start.x + 1, selection->start.y };
            }
            startPoint = vimCursor.back().end;

            // Helper lambda to update the selection based on the word's end point.
            auto updateSelection = [&](til::point wordEnd) {
                if (selection->blockSelection)
                {
                    auto singleColumn = selection->start.x + 1 == selection->end.x;
                    auto pivotAtBottom = selection->pivot.y == selection->end.y;
                    auto pivotAtRight = selection->pivot.x > selection->start.x;

                    if (pivotAtBottom)
                    {
                        if (pivotAtRight && !singleColumn)
                        {
                            if (wordEnd.x > selection->pivot.x && !singleColumn)
                            {
                                selection->start= { selection->pivot.x - 1, selection->start.y };
                                selection->end = { wordEnd.x, selection->end.y };
                                selection->pivot = { selection->start.x, selection->end.y };
                            }
                            else
                            {
                                selection->start = { wordEnd.x - 1, selection->start.y };
                                selection->pivot = { selection->end.x, selection->end.y };
                            }
                        }
                        else
                        {
                            selection->end = { wordEnd.x, selection->end.y };
                            selection->pivot = { selection->start.x, selection->end.y };
                        }
                    }
                    else
                    {
                        if (pivotAtRight && !singleColumn)
                        {
                            if (wordEnd.x > selection->pivot.x && !singleColumn)
                            {
                                selection->start= { selection->pivot.x - 1, selection->start.y };
                                selection->end = { wordEnd.x, selection->end.y };
                                selection->pivot = selection->start;
                            }
                            else
                            {
                                selection->start = { wordEnd.x - 1, selection->start.y };
                                selection->pivot = { selection->end.x, selection->start.y };
                            }
                        }
                        else
                        {
                            selection->end = { wordEnd.x, selection->end.y };
                            selection->pivot = { selection->start.x, selection->start.y };
                        }
                    }
                }
                else if (!isVisual)
                {
                    selection->start = { wordEnd.x - 1, wordEnd.y };
                    selection->end = wordEnd;
                    selection->pivot = selection->end;
                }
                else if (isSingleCell || pivotAtStart || selection->blockSelection)
                {
                    selection->end = wordEnd;
                    selection->pivot = selection->start;
                }
                else if (pivotAtEnd)
                {
                    if (wordEnd > selection->pivot)
                    {
                        selection->end = { wordEnd.x, wordEnd.y };
                        selection->start = { selection->pivot.x - 1, selection->pivot.y };
                        selection->pivot = selection->start;
                    }
                    else
                    {
                        selection->start = { wordEnd.x - 1, wordEnd.y };
                        selection->pivot = selection->end;
                    }
                }
                terminal.SetSelectionAnchors(selection);
            };

            auto endPair = _GetEndOfWord(terminal, startPoint, delimiters);
            if (endPair.second)
            {
                updateSelection(endPair.first);
                return;
            }

            auto yToMove = startPoint.y;
            while (yToMove + 1 <= lastPoint.y)
            {
                ++yToMove;
                auto startOfNextLine = til::point{ 0, yToMove };
                auto startOfNextLinePair = _GetLineFirstNonBlankChar(terminal, startOfNextLine);
                if (startOfNextLinePair.second)
                {
                    endPair = _GetEndOfWord(terminal, startOfNextLinePair.first, delimiters);
                    if (endPair.second)
                    {
                        updateSelection(endPair.first);
                        return;
                    }
                }
            }
        }

        void MoveWordStartRight(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord, bool isVisual)
        {
            auto delimiters = _getVimDelimiters();
            if (isLargeWord)
            {
                delimiters = L"";
            }

            auto selectionAnchors = terminal.GetSelectionAnchors();
            auto selection{ selectionAnchors.write() };

            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            const bool pivotAtStart = (selection->start == selection->pivot);
            const bool pivotAtEnd = (selection->end == selection->pivot);

            auto startPoint = selection->start;
            if (pivotAtStart && !isSingleCell)
            {
                startPoint = selection->end;
            }
            auto cursor = GetVimCursor(terminal);
            if (cursor.IsBlock)
            {
                startPoint = cursor.Span.start;
            }

            auto start = _GetStartOfNextWord(terminal, startPoint, delimiters);

            if (cursor.IsBlock)
            {
                auto vimSelection = terminal.GetVimSelectionAnchors();
                if (!start.second)
                {
                    //auto lastNonSpaceChar = _getLastNonSpaceChar(terminal);
                    //auto yToMove = startPoint.y;
                    //if (yToMove + 1 <= lastNonSpaceChar.y)
                    //{
                    //    yToMove++;
                    //    auto startOfNextLine = til::point{ 0, yToMove };
                    //    start = _GetLineFirstNonBlankChar(terminal, startOfNextLine);
                    //    if (start.second)
                    //    {
                    //        //if (isVisual)
                    //        //{
                    //        //    selection->end = { firstNonSpaceChar.first.x + 1, firstNonSpaceChar.first.y };
                    //        //    selection->pivot = selection->start;
                    //        //}
                    //        //else
                    //        //{
                    //        //    selection->start = firstNonSpaceChar.first;
                    //        //    selection->end = { firstNonSpaceChar.first.x + 1, firstNonSpaceChar.first.y };
                    //        //    selection->pivot = selection->start;
                    //        //}
                    //    }
                    //    else
                    //    {
                            _moveToStartOfNextLineBlock(terminal, &vimSelection);
                            terminal.SetVimSelectionAnchors(&vimSelection);
                            return;
                    //    }
                    //}
                }
                for (auto i = cursor.Span.start.x; i < start.first.x; i++)
                {
                    _moveRightVisualBlock(terminal, &vimSelection);
                }
                terminal.SetVimSelectionAnchors(&vimSelection);
                return;
            }

            if (start.second)
            {
                if (!isVisual)
                {
                    selection->start = start.first;
                    selection->end = { start.first.x + 1, start.first.y };
                    selection->pivot = selection->start;
                    terminal.SetSelectionAnchors(selection);
                }
                else
                {
                    if (pivotAtEnd && !isSingleCell)
                    {
                        if (start.first > selection->pivot)
                        {
                            selection->start = { selection->pivot.x - 1, selection->pivot.y };
                            selection->end = {start.first.x + 1, start.first.y};
                            selection->pivot = selection->start;
                        }
                    }
                    else
                    {
                        selection->end = { start.first.x + 1, start.first.y };
                        selection->pivot = selection->start;
                    }

                    terminal.SetSelectionAnchors(selection);
                }
            }
            else
            {
                auto lastNonSpaceChar = _getLastNonSpaceChar(terminal);
                auto yToMove = startPoint.y;
                if (yToMove + 1 <= lastNonSpaceChar.y)
                {
                    yToMove++;
                    auto startOfNextLine = til::point{ 0, yToMove };
                    auto firstNonSpaceChar = _GetLineFirstNonBlankChar(terminal, startOfNextLine);
                    if (firstNonSpaceChar.second)
                    {
                        if (isVisual)
                        {
                            selection->end = { firstNonSpaceChar.first.x + 1, firstNonSpaceChar.first.y };
                            selection->pivot = selection->start;
                        }
                        else
                        {
                            selection->start = firstNonSpaceChar.first;
                            selection->end = { firstNonSpaceChar.first.x + 1, firstNonSpaceChar.first.y };
                            selection->pivot = selection->start;
                        }
                    }
                    else
                    {
                        if (isVisual)
                        {
                            selection->end = { startOfNextLine.x + 1, startOfNextLine.y };
                            selection->pivot = selection->start;
                        }
                        else
                        {
                            selection->start = startOfNextLine;
                            selection->end = { startOfNextLine.x + 1, startOfNextLine.y };
                            selection->pivot = selection->start;
                        }
                    }
                    terminal.SetSelectionAnchors(selection);
                }
            }
        }

        void SelectInWord(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord)
        {
            auto delimiters = _wordDelimiters;
            if (isLargeWord)
            {
                delimiters = L"";
            }

            auto targetPos{ terminal.GetSelectionAnchors()->end };
            _InWord(terminal, targetPos, delimiters);
        }

        void InDelimiter(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter)
        {
            auto delimitersAreSame = startDelimiter == endDelimiter;

            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            auto pivotIsStart = selection->start == selection->pivot;
            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            auto pos = selection->start;
            if (pivotIsStart && !isSingleCell)
            {
                pos = selection->end;
            }

            auto numberOfInnerPairs = 0;
            for (auto j = pos.y; j >= 0; j--)
            {
                auto x = j == pos.y ? pos.x : terminal.GetTextBuffer().GetRowByOffset(j).size();
                auto glyph = terminal.GetTextBuffer().GetRowByOffset(j).GlyphAt(x);
                if (glyph == endDelimiter)
                {
                    x--;
                }

                for (auto i = x; i >= 0; i--)
                {
                    glyph = terminal.GetTextBuffer().GetRowByOffset(j).GlyphAt(i);
                    //If delimiters are same there is no way to understand inner pairs
                    if (glyph == endDelimiter && !delimitersAreSame)
                    {
                        numberOfInnerPairs++;
                    }
                    else if (glyph == startDelimiter)
                    {
                        if (numberOfInnerPairs > 0)
                        {
                            numberOfInnerPairs--;
                        }
                        else
                        {
                            _matchingChar(terminal, til::point{ i, j }, startDelimiter, endDelimiter, true, !includeDelimiter);
                            return;
                        }
                    }
                }
            }
        }

        void InDelimiterSameLine(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view delimiter, bool includeDelimiter)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };

            auto pivotIsStart = selection->start == selection->pivot;
            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            auto pos = selection->start;
            if (pivotIsStart && !isSingleCell)
            {
                pos = selection->end;
            }

            const auto glyph = terminal.GetTextBuffer().GetRowByOffset(pos.y).GlyphAt(pos.x);
            const auto posIsDelimiter = glyph == delimiter;

            auto foundMatchGoingBack = false;
            til::CoordType matchGoingBack;
            for (auto i = pos.x - 1; i >= 0; i--)
            {
                auto g = terminal.GetTextBuffer().GetRowByOffset(pos.y).GlyphAt(i);
                if (g == delimiter)
                {
                    foundMatchGoingBack = true;
                    matchGoingBack = i;
                    break;
                }
            }

            auto foundMatchGoingForward = false;
            til::CoordType matchGoingForward;
            for (auto i = pos.x + 1; i <= terminal.GetTextBuffer().GetRowByOffset(pos.y).size(); i++)
            {
                auto g = terminal.GetTextBuffer().GetRowByOffset(pos.y).GlyphAt(i);
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
                        selection->end.x;
                    }
                    else
                    {
                        selection->end.x++;
                    }
                    terminal.SetSelectionAnchors(selection);
                }
                else if (foundMatchGoingForward)
                {
                    selection->start = til::point{ pos.x, pos.y };
                    selection->end = til::point{ matchGoingForward, pos.y };
                    if (!includeDelimiter)
                    {
                        selection->start.x++;
                        selection->end.x;
                    }
                    else
                    {
                        selection->end.x++;
                    }
                    terminal.SetSelectionAnchors(selection);
                }
            }
            else if (foundMatchGoingBack && foundMatchGoingForward)
            {
                selection->start = til::point{ matchGoingBack, pos.y };
                selection->end = til::point{ matchGoingForward, pos.y };
                if (!includeDelimiter)
                {
                    selection->start.x++;
                    selection->end.x;
                }
                else
                {
                    selection->end.x++;
                }
                terminal.SetSelectionAnchors(selection);
            }
        }

        void MatchingChar(Microsoft::Terminal::Core::Terminal& terminal, til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool inBlock)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            auto selection{ selectionAnchors.write() };
            std::tuple<bool, til::point, til::point> findResult;
            if (onStartDelimiter)
            {
                findResult = _findBlockEndFromStart(terminal, startPos, startDelimiter, endDelimiter);
            }
            else
            {
                findResult = _findBlockStartFromEnd(terminal, startPos, startDelimiter, endDelimiter);
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
                        selection->end.x = terminal.GetTextBuffer().GetRowByOffset(selection->end.y).GetLastNonSpaceColumn() - 1;
                    }
                    else
                    {
                        selection->end.x--;
                    }
                    if (selection->start.x >= terminal.GetTextBuffer().GetRowByOffset(selection->end.y).GetLastNonSpaceColumn() - 1)
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
                terminal.SetSelectionAnchors(selection);
            }
        }

        void MoveToFirstNonBlankChar(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            auto selection{ selectionAnchors.write() };
            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            const bool pivotAtStart = (selection->start == selection->pivot);
            //const bool pivotAtEnd = (selection->end == selection->pivot);
            auto y = selection->start.y;
            if (!isSingleCell && pivotAtStart)
            {
                y = selection->end.y;
            }

            auto startLine = _getStartLineOfRow(terminal.GetTextBuffer(), y);

            if (terminal.IsBlockSelection())
            {
                auto cursor = GetVimCursor(terminal);

                auto maxNonSpaceChar = 0;
                for (auto i = selection->start.y; i <= selection->end.y; i++)
                {
                    auto lastNonSpaceColumn = 0;
                    const auto nonBlankCharResult = _GetLineFirstNonBlankChar(terminal, {0 , i});
                    if (nonBlankCharResult.second)
                    {
                        lastNonSpaceColumn = nonBlankCharResult.first.x;
                    }

                    if (lastNonSpaceColumn < maxNonSpaceChar)
                    {
                        maxNonSpaceChar = lastNonSpaceColumn;
                    }
                }

                auto selection = terminal.GetVimSelectionAnchors();
                for (auto i = cursor.Span.start.x; i > maxNonSpaceChar; i--)
                {
                    _moveLeftBlock(terminal, &selection);
                }
                terminal.SetVimSelectionAnchors(&selection);
                //auto s = til::point{ maxNonSpaceChar, selection->start.y };
                //selection->start = s;
                //selection->pivot = selection->end;
                //terminal.SetSelectionAnchors(selection);
                return;
            }
            auto startOfLine = til::point{ 0, startLine };
            auto firstNonBlankChar = _GetLineFirstNonBlankChar(terminal, startOfLine);

            if (isVisual)
            {
                til::point newStart;
                if (firstNonBlankChar.second == true)
                {
                    newStart = { firstNonBlankChar.first.x, startLine };
                    //selection->pivot = selection->end;
                }
                else
                {
                    newStart = { 0, startLine };
                    //selection->pivot = selection->end;
                }

                if (!isSingleCell && pivotAtStart)
                {
                    if (newStart < selection->pivot)
                    {
                        selection->end = { selection->pivot.x + 1, selection->pivot.y };
                        selection->pivot = selection->end;
                        selection->start = newStart;
                    }
                    else
                    {
                        selection->end = { newStart.x + 1, newStart.y };
                        selection->pivot = selection->start;
                    }
                }
                else
                {
                    selection->start = newStart;
                    selection->pivot = selection->end;
                }

                terminal.SetSelectionAnchors(selection);
            }
            else
            {
                if (firstNonBlankChar.second == true)
                {
                    selection->start = { firstNonBlankChar.first.x, startLine };
                    selection->end = { firstNonBlankChar.first.x + 1, startLine };
                    selection->pivot = selection->start;
                    terminal.SetSelectionAnchors(selection);
                }
                else
                {
                    selection->start = { 0, startLine };
                    selection->end = { 1, startLine };
                    selection->pivot = selection->start;
                    terminal.SetSelectionAnchors(selection);
                }
            }
        }

        void SelectPageDown(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selection = terminal.GetSelectionAnchors();
            const auto bufferSize{ terminal.GetTextBuffer().GetSize() };
            auto targetPos{ selection->end.y > selection->pivot.y ? selection->end : selection->start };

            const auto viewportHeight{ terminal.GetViewport().Height() };
            const auto mutableBottom{ terminal.GetViewport().BottomInclusive() };
            const auto newY{ targetPos.y + viewportHeight };
            const auto newPos = newY > mutableBottom ? til::point{ bufferSize.RightInclusive(), mutableBottom } : til::point{ targetPos.x, newY };

            _UpdateSelection(terminal, isVisual, newPos);
            terminal.UserScrollViewport(newPos.y);
        }

        void SelectPageUp(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selection = terminal.GetSelectionAnchors();
            const auto bufferSize{ terminal.GetTextBuffer().GetSize() };
            auto targetPos{ selection->start.y < selection->pivot.y ? selection->start : selection->end };

            const auto viewportHeight{ terminal.GetViewport().Height() };
            const auto newY{ targetPos.y - viewportHeight };
            const auto newPos = newY < bufferSize.Top() ? bufferSize.Origin() : til::point{ targetPos.x, newY };

            _UpdateSelection(terminal, isVisual, newPos);
            terminal.UserScrollViewport(newPos.y);
        }

        void SelectHalfPageUp(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            const auto bufferSize{ terminal.GetTextBuffer().GetSize() };
            const auto startIsPivot = selection->start.y == selection->pivot.y && selection->start.x == selection->pivot.x;
            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            auto cursor = GetVimCursor(terminal);
            const auto viewportHeight = terminal.GetViewport().Height();
            if (cursor.IsBlock)
            {
                auto rowsToMoveUp = viewportHeight / 2;
                auto vimSelection = terminal.GetVimSelectionAnchors();
                for (auto i = 0; i < rowsToMoveUp; i++)
                {
                    _moveUpBlock(terminal, &vimSelection);
                }
                terminal.SetVimSelectionAnchors(&vimSelection);
                return;
            }

            auto targetPos = selection->start;
            if (startIsPivot && !isSingleCell)
            {
                targetPos = selection->end;
            }

            const auto newY = targetPos.y - viewportHeight / 2;
            const auto y = newY < bufferSize.Top() ? 0 : newY;
            const til::CoordType x = targetPos.x;
            if (entireLine)
            {
                if (startIsPivot)
                {
                    if (y > selection->start.y)
                    {
                        selection->end.y = y;
                        selection->end.x = terminal.GetTextBuffer().GetRowByOffset(y).GetLastNonSpaceColumn();
                    }
                    else
                    {
                        selection->end.y = selection->start.y;
                        selection->end.x = terminal.GetTextBuffer().GetRowByOffset(selection->start.y).GetLastNonSpaceColumn();
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
                terminal.SetSelectionAnchors(selection);
                return;
            }
            const auto point = til::point{ x, y };

            if (isVisual)
            {
                if (startIsPivot)
                {
                    if (point < selection->pivot)
                    {
                        selection->start = { point.x - 1, point.y };
                        selection->end = { selection->pivot.x + 1, selection->pivot.y };
                        selection->pivot = selection->end;
                    }
                    else
                    {
                        selection->end = point;
                        //selection->start = { point.x - 1, point.y };
                        //selection->pivot = selection->end;
                    }
                }
                else
                {
                    selection->start = point;
                    //selection->end = { selection->pivot.x + 1, selection->pivot.y };
                    //selection->pivot = selection->end;
                }

                //selection->start = { point.x - 1, point.y };
                ////selection->end = { point.x, point.y };
                //selection->pivot = selection->end;
            }
            else
            {
                selection->start = { point.x, point.y };
                selection->end = { point.x + 1, point.y };
                selection->pivot = selection->start;
            }

            terminal.SetSelectionAnchors(selection);
        }

        void SelectHalfPageDown(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            const auto viewportHeight{ terminal.GetViewport().Height() };
            const auto lastRow = _getLastNonSpaceChar(terminal).y;
            const auto startIsPivot = selection->start.y == selection->pivot.y && selection->start.x == selection->pivot.x;
            auto cursor = GetVimCursor(terminal);

            if (cursor.IsBlock)
            {
                auto rowsToMoveDown = cursor.Span.start.y + viewportHeight / 2;
                auto vimSelection = terminal.GetVimSelectionAnchors();
                for (auto i = 0; i < rowsToMoveDown; i++)
                {
                    _moveDownBlock(terminal, &vimSelection);
                }
                terminal.SetVimSelectionAnchors(&vimSelection);

                return;
            }

            if (entireLine)
            {
                const auto pos = startIsPivot ? selection->end : selection->start;

                const auto newY = pos.y + viewportHeight / 2;
                const auto y = newY > lastRow ? lastRow : newY;
                //const til::CoordType x = pos.x;

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
                        selection->end.x = terminal.GetTextBuffer().GetRowByOffset(y).GetLastNonSpaceColumn();
                    }
                }
                else
                {
                    selection->end.y = y;
                    selection->end.x = terminal.GetTextBuffer().GetRowByOffset(y).GetLastNonSpaceColumn();
                    ;
                }
                terminal.SetSelectionAnchors(selection);
                return;
            }

            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            const bool pivotAtStart = (selection->start == selection->pivot);
            //const bool pivotAtEnd = (selection->end == selection->pivot);
            auto pos = selection->start;
            if (!isSingleCell && pivotAtStart)
            {
                pos = selection->end;
            }
            const auto newY = pos.y + viewportHeight / 2;
            const auto y = newY > lastRow ? lastRow : newY;
            const til::CoordType x = pos.x;

            const auto point = til::point{ x, y };

            if (isVisual)
            {
                if (isSingleCell)
                {
                    //selection->start = { point.x, point.y };
                    selection->end = { point.x + 1, point.y };
                    selection->pivot = selection->start;
                }
                else if (startIsPivot)
                {
                    selection->end = { point.x, point.y };
                }
                else
                {
                    if (point >= selection->pivot)
                    {
                        selection->start = { selection->pivot.x - 1, selection->pivot.y };
                        selection->end = { point.x + 1, point.y };
                        selection->pivot = selection->start;
                    }
                    else
                    {
                        selection->start = { point.x, point.y };
                        //selection->end = { point.x + 1, point.y };
                        //selection->pivot = selection->start;
                    }
                }

                //if (!isSingleCell && pivotAtEnd && point > selection->pivot)
                //{
                //    selection->start = { selection->pivot.x - 1, selection->pivot.y };
                //    selection->end = {point.x + 1, point.y};
                //    selection->pivot = selection->start;
                //}
                //else
                //{
                //    selection->end = { point.x + 1, point.y };
                //    selection->pivot = selection->start;
                //}
            }
            else
            {
                selection->start = { point.x, point.y };
                selection->end = { point.x + 1, point.y };
                selection->pivot = selection->start;
            }

            terminal.SetSelectionAnchors(selection);
        }

        void SelectBottom(Microsoft::Terminal::Core::Terminal& terminal, bool isvisual, bool entireLine)
        {
            auto lastChar = _getLastNonSpaceChar(terminal);
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };

            auto pos = til::point{ selection->start.x, lastChar.y };
            if (pos > lastChar || entireLine)
            {
                pos = lastChar;
            }

            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            const bool pivotAtEnd = (selection->end == selection->pivot);


            if (entireLine)
            {
                if (!isSingleCell && pivotAtEnd)
                {
                    if (pos > selection->pivot)
                    {
                        selection->start = { 0, selection->pivot.y };
                        selection->end = { pos.x + 1, pos.y };
                        selection->pivot = selection->start;
                    }
                }
                else
                {
                    selection->start = { 0, selection->start.y };
                    selection->end = { pos.x + 1, pos.y };
                    selection->pivot = selection->start;
                }
            }
            else if (isvisual)
            {
                if (!isSingleCell && pivotAtEnd)
                {
                    if (pos > selection->pivot)
                    {
                        selection->start = { selection->pivot.x - 1, selection->pivot.y };
                        selection->end = { pos.x + 1, pos.y };
                        selection->pivot = selection->start;
                    }
                }
                else
                {
                    selection->end = { pos.x + 1, pos.y };
                    selection->pivot = selection->start;
                }
            }
            else
            {
                selection->start = pos;
                selection->end = { pos.x + 1, pos.y };
                selection->pivot = selection->start;
            }
            terminal.SetSelectionAnchors(selection);

            terminal.UserScrollViewport(lastChar.y);
        }

        void SelectTop(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            const bool pivotAtStart = (selection->start == selection->pivot);

            if (entireLine)
            {
                auto pos = til::point{ 0, 0 };
                if (!isSingleCell && pivotAtStart)
                {
                    if (pos < selection->pivot)
                    {
                        auto lineEnd = _GetLineEnd(terminal, { 0, selection->pivot.y });
                        selection->start = pos;
                        selection->end = lineEnd;
                        selection->pivot = selection->end;
                    }
                }
                else
                {
                    auto lineEnd = _GetLineEnd(terminal, { 0, selection->end.y });
                    selection->start = pos;
                    selection->end = lineEnd;
                    selection->pivot = selection->end;
                }

                terminal.UserScrollViewport(0);
            }
            else if (isVisual)
            {
                auto pos = til::point{ selection->start.x, 0 };
                if (!isSingleCell && pivotAtStart)
                {
                    if (pos < selection->pivot)
                    {
                        selection->start = pos;
                        selection->end = { selection->pivot.x + 1, selection->pivot.y };
                        selection->pivot = selection->end;
                    }
                }
                else
                {
                    selection->start = til::point{ selection->start.x, 0 };
                    selection->pivot = selection->end;
                }

                terminal.UserScrollViewport(0);
            }
            else
            {
                selection->start = til::point{ selection->start.x, 0 };
                selection->pivot = til::point{ selection->start.x, 0 };
                selection->end = til::point{ selection->start.x + 1, 0 };
                terminal.UserScrollViewport(0);
            }
            terminal.SetSelectionAnchors(selection);
        }

        void SelectLineDown(Microsoft::Terminal::Core::Terminal& terminal)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            auto endIsMoving = false;
            if (selection->end.y > selection->pivot.y)
            {
                endIsMoving = true;
                auto end = _GetLineEnd(terminal, til::point{ 0, selection->end.y + 1 });
                selection->end = end;
            }
            else if (selection->end.y == selection->start.y)
            {
                auto currentEnd = _GetLineEnd(terminal, til::point{ 0, selection->end.y });
                selection->end = { currentEnd.x, currentEnd.y + 1 };
                //selection->start = til::point{ 0, selection->start.y + 1 };
                selection->pivot = selection->start;
            }
            else
            {
                selection->start = til::point{ 0, selection->start.y + 1 };
            }
            if (selection->start.y >= 0 && selection->end.y > 0)
            {
                terminal.SetSelectionAnchors(selection);
            }
            terminal.SetSelectionAnchors(selection);
        }

        void SelectLineUp(Microsoft::Terminal::Core::Terminal& terminal)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            auto endIsMoving = false;
            if (selection->end.y > selection->pivot.y)
            {
                endIsMoving = true;
                auto end = _GetLineEnd(terminal, til::point{ 0, selection->end.y - 1 });
                selection->end = end;
            }
            else if (selection->end.y == selection->pivot.y)
            {
                auto currentEnd = _GetLineEnd(terminal, til::point{ 0, selection->end.y });
                selection->end = currentEnd;
                selection->start = til::point{ 0, selection->start.y - 1 };
                //selection->pivot = currentEnd;
            }
            else
            {
                selection->start = til::point{ 0, selection->start.y - 1 };
            }
            if (selection->start.y >= 0 && selection->end.y > 0)
            {
                terminal.SetSelectionAnchors(selection);
            }
        }

        void SelectCurrentChar(Microsoft::Terminal::Core::Terminal &terminal)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            auto pivotAtStart = selection->start == selection->pivot;
            auto pivotAtEnd = selection->end == selection->pivot;
            if (pivotAtStart)
            {
                selection->start = { selection->end.x - 1, selection->end.y };
                selection->end = { selection->end.x, selection->end.y };
                selection->pivot = selection->end;
                terminal.SetSelectionAnchors(selection);
            }
            else if (pivotAtEnd)
            {
                selection->start = { selection->start.x, selection->start.y };
                selection->end = { selection->start.x + 1, selection->start.y };
                selection->pivot = selection->start;
                terminal.SetSelectionAnchors(selection);
            }
        }
    }
}
