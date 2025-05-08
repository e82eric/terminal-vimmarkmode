#include "pch.h"
#include "VimMotions.hpp"

#include "../Terminal.hpp"

namespace vim
{
    namespace motions
    {
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

        std::optional<til::point_span> _findBlockStartFromEnd(Microsoft::Terminal::Core::Terminal &terminal, til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter)
        {
            til::CoordType startX = -1;
            til::CoordType startY = -1;
            auto found = false;

            auto innerPairs = 0;
            for (til::CoordType j = pos.y; j >= 0; j--)
            {
                const auto x = pos.y == j ? pos.x - 1 : terminal.GetTextBuffer().GetRowByOffset(j).size();
                for (int i = x; i >= 0; i--)
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
                return std::nullopt;
            }

            return til::point_span { til::point{ startX, startY }, til::point{ pos.x, pos.y } };
        }

        std::optional<til::point_span> _findBlockEndFromStart(Microsoft::Terminal::Core::Terminal &terminal, til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter)
        {
            til::CoordType endX = -1;
            til::CoordType endY = -1;

            auto innerPairs = 0;
            auto lastNonSpaceChar = _getLastNonSpaceChar(terminal);
            auto found = false;
            for (til::CoordType j = pos.y; j <= lastNonSpaceChar.y; j++)
            {
                const auto startX = pos.y == j ? pos.x + 1 : 0;
                for (int i = startX; i < terminal.GetTextBuffer().GetRowByOffset(j).size(); i++)
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
                return std::nullopt;
            }

            return til::point_span{ { pos.x, pos.y }, { endX, endY } };
        }

        void _matchingCharFromEnd(Microsoft::Terminal::Core::Terminal &terminal, til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool isVisual)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            auto findResult = _findBlockStartFromEnd(terminal, startPos, startDelimiter, endDelimiter);
            if (findResult.has_value())
            {
                const auto pairStart = findResult.value().start;
                if (isVisual)
                {
                    if (selection.start < pairStart)
                    {
                        selection.end = pairStart;
                        selection.pivot = selection.start;
                    }
                    else
                    {
                        selection.start = pairStart;
                        selection.pivot = selection.end;
                    }
                }
                else
                {
                    selection.start = pairStart;
                    selection.end = pairStart;
                    selection.pivot = pairStart;
                }
                terminal.SetVimSelectionAnchors(&selection);
            }
        }

        void _matchingCharFromStart(Microsoft::Terminal::Core::Terminal &terminal, til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool isVisual)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            auto findResult = _findBlockEndFromStart(terminal, startPos, startDelimiter, endDelimiter);
            if (findResult.has_value())
            {
                const auto pairEnd = findResult.value().end;
                if (isVisual)
                {
                    if (selection.end >= pairEnd)
                    {
                        selection.start = pairEnd;
                        selection.pivot = selection.end;
                    }
                    else
                    {
                        selection.end = pairEnd;
                        selection.pivot = selection.start;
                    }
                }
                else
                {
                    selection.start = pairEnd;
                    selection.end = pairEnd;
                    selection.pivot = selection.start;
                }
                terminal.SetVimSelectionAnchors(&selection);
            }
        }

        void MatchingChar(Microsoft::Terminal::Core::Terminal &terminal, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool isVisual)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            auto cursor = GetVimCursor(terminal, &selection);
            const auto pos = cursor.Span.start;
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
            auto selection = terminal.GetVimSelectionAnchors();
            std::optional<til::point_span> findResult;
            if (onStartDelimiter)
            {
                findResult = _findBlockEndFromStart(terminal, startPos, startDelimiter, endDelimiter);
            }
            else
            {
                findResult = _findBlockStartFromEnd(terminal, startPos, startDelimiter, endDelimiter);
            }
            if (findResult.has_value())
            {
                selection.start = findResult.value().start;
                selection.end = findResult.value().end;
                selection.end.x++;
                if (inBlock)
                {
                    if (selection.end.x == 0)
                    {
                        selection.end.y--;
                        selection.end.x = terminal.GetTextBuffer().GetRowByOffset(selection.end.y).GetLastNonSpaceColumn() - 1;
                    }
                    else
                    {
                        selection.end.x--;
                    }
                    if (selection.start.x >= terminal.GetTextBuffer().GetRowByOffset(selection.end.y).GetLastNonSpaceColumn() - 1)
                    {
                        selection.start.y++;
                        selection.start.x = 0;
                    }
                    else
                    {
                        selection.start.x++;
                    }
                }
                selection.pivot = selection.end;
                terminal.SetVimSelectionAnchors(&selection);
            }
        }

        std::optional<til::point> _GetEndOfWord(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            // can't expand right
            if (target.x == bufferSize.RightInclusive())
            {
                return std::nullopt;
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
                return std::nullopt;
            }

            bufferSize.DecrementInBounds(result);

            return til::point{ result.x, result.y };
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

        std::optional<til::point> _GetStartOfNextWord(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            const auto lastNonSpaceChar = _getLastNonSpaceChar(terminal);
            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            // can't expand right
            if (target.x == bufferSize.RightInclusive())
            {
                return std::nullopt;
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
                return std::nullopt;
            }

            return result;
        }

        std::optional<til::point> _GetStartOfWord(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            if (target.x == 0)
            {
                return std::nullopt;
            }
            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            auto result = target;
            bool found = false;
            DelimiterClass previousClass;

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
                return til::point {0, target.y};
            }

            bufferSize.IncrementInBounds(result);

            return result;
        }

        std::optional<til::point> _GetEndOfPreviousWord(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            auto wordStart = _GetStartOfWord(terminal, { std::max(0, target.x - 1), target.y }, wordDelimiters);

            if (!wordStart.has_value())
            {
                return wordStart;
            }

            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            til::point result = wordStart.value();
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
                return std::nullopt;
            }

            return result;
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

                for (int i = startX; i >= 0; i--)
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
            auto endPair = _GetEndOfWord(terminal, pos, delimiters);
            auto startPair = _GetStartOfWord(terminal, pos, delimiters);
            if (!startPair.has_value())
            {
                startPair = { 0, pos.y };
            }

            auto selection = terminal.GetVimSelectionAnchors();
            if (startPair.has_value() && endPair.has_value())
            {
                selection.start = startPair.value();
                selection.end = { endPair.value().x + 1, endPair.value().y };
                selection.pivot = selection.start;
            }
            terminal.SetVimSelectionAnchors(&selection);
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
                for (int i = startX; i < row.size(); i++)
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

        std::optional<til::point> _GetLineFirstNonBlankChar(Microsoft::Terminal::Core::Terminal& terminal, const til::point target)
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
                return std::nullopt;
            }

            return result;
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

        void _moveLeftByCell(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, bool moveToPreviousLine)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto startOfLine = _getStartLineOfRow(terminal.GetTextBuffer(), cursor.Span.start.y);

            if (selection->start.x > 0)
            {
                selection->start.x--;
                selection->end.x--;
                selection->pivot = selection->start;
            }
            else if (startOfLine < cursor.Span.start.y || moveToPreviousLine)
            {
                auto previousRow = cursor.Span.start.y - 1;
                const auto lastNonSpaceColumn = std::max(1, terminal.GetTextBuffer().GetRowByOffset(previousRow).GetLastNonSpaceColumn());
                selection->end = { lastNonSpaceColumn, previousRow };
                selection->start = { lastNonSpaceColumn - 1, previousRow };
                selection->pivot = selection->start;
            }
        }

        void _moveLeftVisual(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, bool movePreviousLine)
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
                else if (cursor.Span.start.y > startOfLine || movePreviousLine)
                {
                    auto previousRow = cursor.Span.start.y - 1;
                    const auto lastNonSpaceColumn = std::max(1, terminal.GetTextBuffer().GetRowByOffset(previousRow).GetLastNonSpaceColumn());
                    selection->start = { lastNonSpaceColumn - 1, previousRow };
                    selection->pivot = selection->end;
                }
                break;
            case VimCursorPosition::End:
                selection->end.x--;
                break;
            }
        }

        void _moveLeft(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, bool isVisual, bool moveToPreviousLine)
        {
            if (!isVisual)
            {
                _moveLeftByCell(terminal, selection, moveToPreviousLine);
            }
            else if (selection->blockSelection)
            {
                _moveLeftBlock(terminal, selection);
            }
            else
            {
                _moveLeftVisual(terminal, selection, moveToPreviousLine);
            }
        }

        void _moveLeftToPoint(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, til::point target, bool isVisual, bool moveToPreviousLine)
        {
            auto cursor = GetVimCursor(terminal, selection);
            while (cursor.Span.start != target && cursor.Span.start >= target)
            {
                _moveLeft(terminal, selection, isVisual, moveToPreviousLine);
                cursor = GetVimCursor(terminal, selection);
            }
            terminal.SetVimSelectionAnchors(selection);
        }

        void MoveLeft(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto vimSelection = terminal.GetVimSelectionAnchors();
            if (!isVisual)
            {
                _moveLeftByCell(terminal, &vimSelection, false);
            }
            else if (vimSelection.blockSelection)
            {
                _moveLeftBlock(terminal, &vimSelection);
            }
            else
            {
                _moveLeftVisual(terminal, &vimSelection, false);
            }
            terminal.SetVimSelectionAnchors(&vimSelection);
        }

        void _moveRightVisualBlock(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection, bool moveNextLine)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto lineEnd = terminal.GetTextBuffer().GetLineWidth(cursor.Span.start.y);
            if (cursor.Span.end.x >= lineEnd)
            {
                if (moveNextLine)
                {
                    selection->end = { 1, selection->end.y + 1 };
                    selection->pivot = selection->start;
                }
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

        void _moveRightByCell(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, bool movePreviousLine)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto lineEnd = terminal.GetTextBuffer().GetLineWidth(cursor.Span.start.y);
            if (cursor.Span.end.x + 1 > lineEnd)
            {
                auto wrapped = terminal.GetTextBuffer().GetRowByOffset(cursor.Span.start.y).WasWrapForced();
                if (wrapped || movePreviousLine)
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

        void _moveRightVisual(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, bool movePreviousLine)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto lineEnd = terminal.GetTextBuffer().GetLineWidth(cursor.Span.start.y);

            if (cursor.Position == SingleCell || cursor.Position == VimCursorPosition::End)
            {
                if (cursor.Span.end.x >= lineEnd)
                {
                    auto wrapped = terminal.GetTextBuffer().GetRowByOffset(cursor.Span.start.y).WasWrapForced();
                    if (wrapped || movePreviousLine)
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

        void _moveRight(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, bool isVisual, bool movePreviousLine)
        {
            if (!isVisual)
            {
                _moveRightByCell(terminal, selection, movePreviousLine);
            }
            else if (selection->blockSelection)
            {
                _moveRightVisualBlock(terminal, selection, movePreviousLine);
            }
            else
            {
                _moveRightVisual(terminal, selection, movePreviousLine);
            }
        }

        void _moveRightToPoint(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, til::point point, bool isVisual, bool moveToNextLine)
        {
            auto cursor = GetVimCursor(terminal, selection);
            while (cursor.Span.start != point && cursor.Span.start <= point)
            {
                _moveRight(terminal, selection, isVisual, moveToNextLine);
                cursor = GetVimCursor(terminal, selection);
            }
            terminal.SetVimSelectionAnchors(selection);
        }

        void MoveRight(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            _moveRight(terminal, &selection, isVisual, false);
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

        void _moveDownVisual(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection, til::CoordType lastY)
        {
            auto cursor = GetVimCursor(terminal, selection);
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

        void _moveDownEntireLine(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            if (selection->end.y > selection->pivot.y)
            {
                auto end = _GetLineEnd(terminal, til::point{ 0, selection->end.y + 1 });
                selection->end = end;
            }
            else if (selection->end.y == selection->start.y)
            {
                auto currentEnd = _GetLineEnd(terminal, til::point{ 0, selection->end.y + 1});
                selection->end = { currentEnd.x, currentEnd.y };
                selection->pivot = selection->start;
            }
            else
            {
                selection->start = til::point{ 0, selection->start.y + 1 };
            }
        }

        void _moveDownByCell(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            auto cursor = GetVimCursor(terminal, selection);
            til::point point = { cursor.Span.start };
            point.y++;
            selection->start = { point.x, point.y };
            selection->end = { point.x + 1, point.y };
            selection->pivot = selection->start;
        }

        void _moveDown(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection, bool isVisual, bool entireLine, til::CoordType lastY)
        {
            auto cursor = GetVimCursor(terminal, selection);
            if (cursor.IsBlock)
            {
                _moveDownBlock(terminal, selection);
            }
            else if (isVisual)
            {
                _moveDownVisual(terminal, selection, lastY);
            }
            else if (entireLine)
            {
                _moveDownEntireLine(terminal, selection);
            }
            else
            {
                _moveDownByCell(terminal, selection);
            }
        }

        void _moveDownToPoint(Microsoft::Terminal::Core::Terminal &terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection, til::point point, bool isVisual, bool entireLine)
        {
            auto cursor = GetVimCursor(terminal, selection);
            auto lastY = _getLastNonSpaceChar(terminal).y;
            while (cursor.Span.start != point && cursor.Span.start.y < point.y)
            {
                _moveDown(terminal, selection, isVisual, entireLine, lastY);
                cursor = GetVimCursor(terminal, selection);
            }
            terminal.SetVimSelectionAnchors(selection);
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
                _moveDownByCell(terminal, &vimSelection);
            }
            else
            {

                auto lastY = _getLastNonSpaceChar(terminal).y;
                _moveDownVisual(terminal, &vimSelection, lastY);
            }
            terminal.SetVimSelectionAnchors(&vimSelection);
        }

        VimCursor GetVimCursor(Microsoft::Terminal::Core::Terminal & terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection) noexcept
        {
            if (selection->start == selection->end)
            {
                selection->end = { selection->start.x + 1, selection->start.y };
                selection->pivot = selection->start;
                terminal.SetVimSelectionAnchors(selection);
            }

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
            auto result = GetVimCursor(terminal, &selection);
            return result;
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


        void _moveUpEntireLine(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo* selection)
        {
            if (selection->end.y > selection->pivot.y)
            {
                auto end = _GetLineEnd(terminal, til::point{ 0, selection->end.y - 1 });
                selection->end = end;
            }
            else if (selection->end.y == selection->pivot.y)
            {
                auto currentEnd = _GetLineEnd(terminal, til::point{ 0, selection->end.y });
                selection->end = currentEnd;
                selection->start = til::point{ 0, selection->start.y - 1 };
            }
            else
            {
                selection->start = til::point{ 0, selection->start.y - 1 };
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
            _moveLeftToPoint(terminal, &vimSelection, til::point{ 0, startOfLine }, isVisual, false);
        }

        void MoveToEndOfLine(Microsoft::Terminal::Core::Terminal& terminal, bool isvisual)
        {
            auto cursor = GetVimCursor(terminal);
            auto vimSelection = terminal.GetVimSelectionAnchors();
            til::point target;
            if (terminal.IsBlockSelection())
            {
                auto maxNonSpaceChar = 0;
                for (til::CoordType i = vimSelection.start.y; i <= vimSelection.end.y; i++)
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

            _moveRightToPoint(terminal, &vimSelection, target, isvisual, false);
        }

        void TilChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            til::point target;
            auto cursor = GetVimCursor(terminal);
            auto vimSelection = terminal.GetVimSelectionAnchors();

            if (_FindChar(terminal, vkey, true, target))
            {
                _moveRightToPoint(terminal, &vimSelection, target, isVisual, false);
            }
        }

        void TilCharBackwards(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            til::point target;
            auto cursor = GetVimCursor(terminal);
            auto vimSelection = terminal.GetVimSelectionAnchors();

            if (_FindCharBack(terminal, vkey, true, target))
            {
                _moveLeftToPoint(terminal, &vimSelection, target, isVisual, false);
            }
        }

        void FindChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            auto cursor = GetVimCursor(terminal);
            auto vimSelection = terminal.GetVimSelectionAnchors();
            til::point target;

            if (_FindChar(terminal, vkey, false, target))
            {
                _moveRightToPoint(terminal, &vimSelection, target, isVisual, false);
            }
        }

        void FindCharBackwards(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            til::point target;
            auto cursor = GetVimCursor(terminal);
            auto vimSelection = terminal.GetVimSelectionAnchors();
            if (_FindCharBack(terminal, vkey, false, target))
            {
                _moveLeftToPoint(terminal, &vimSelection, target, isVisual, false);
            }
        }

        void MoveWordLeft(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord, bool isVisual)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            auto cursor = GetVimCursor(terminal, &selection);
            auto delimiters = isLargeWord ? L"" : _getVimDelimiters();

            if (cursor.Span.start.x > 0)
            {
                auto pos = til::point{ std::max(cursor.Span.start.x, 0) - 1, cursor.Span.start.y };
                auto endPair = _GetStartOfWord(terminal, pos, delimiters);
                if (endPair.has_value())
                {
                    _moveLeftToPoint(terminal, &selection, *endPair, isVisual, true);
                    return;
                }
            }

            auto yToMove = cursor.Span.start.y;
            if (cursor.Span.start.y > 0)
            {
                --yToMove;
                auto startOfNextLine = til::point{ 0, yToMove };
                auto startOfNextLinePair = _GetLineEnd(terminal, startOfNextLine);
                auto endPair = _GetStartOfWord(terminal, { std::max(0, startOfNextLinePair.x - 1), startOfNextLinePair.y }, delimiters);
                if (endPair.has_value())
                {
                    _moveLeftToPoint(terminal, &selection, endPair.value(), isVisual, true);
                    return;
                }
                _moveLeftToPoint(terminal, &selection, startOfNextLine, isVisual, true);
                return;
            }
                
            _moveLeftToPoint(terminal, &selection, { 0, 0 }, isVisual, true);
        }

        void MoveWordRight(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord, bool isVisual)
        {
            auto delimiters = isLargeWord ? L"" : _getVimDelimiters();

            auto lastPoint = _getLastNonSpaceChar(terminal);
            auto selection = terminal.GetVimSelectionAnchors();
            auto cursor = GetVimCursor(terminal);

            auto endPair = _GetEndOfWord(terminal, cursor.Span.end, delimiters);
            if (endPair.has_value())
            {
                _moveRightToPoint(terminal, &selection, endPair.value(), isVisual, true);
                return;
            }

            auto yToMove = cursor.Span.start.y;
            while (yToMove + 1 <= lastPoint.y)
            {
                ++yToMove;
                auto startOfNextLine = til::point{ 0, yToMove };
                auto startOfNextLinePair = _GetLineFirstNonBlankChar(terminal, startOfNextLine);
                if (startOfNextLinePair.has_value())
                {
                    endPair = _GetEndOfWord(terminal, *startOfNextLinePair, delimiters);
                    if (endPair.has_value())
                    {
                        _moveRightToPoint(terminal, &selection, endPair.value(), isVisual, true);
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
            auto selection = terminal.GetVimSelectionAnchors();
            auto cursor = GetVimCursor(terminal);
            auto start = _GetStartOfNextWord(terminal, cursor.Span.start, delimiters);

            til::point target;
            auto found = false;
            if (start.has_value())
            {
                target = *start;
                found = true;
            }
            else
            {
                auto lastNonSpaceChar = _getLastNonSpaceChar(terminal);
                auto yToMove = cursor.Span.start.y;
                if (yToMove + 1 <= lastNonSpaceChar.y)
                {
                    yToMove++;
                    auto startOfNextLine = til::point{ 0, yToMove };
                    target = { startOfNextLine.x, startOfNextLine.y };
                    found = true;
                }
            }

            if (found)
            {
                _moveRightToPoint(terminal, &selection, target, isVisual, true);
            }
        }

        void SelectInWord(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord)
        {
            auto delimiters = _getVimDelimiters();
            if (isLargeWord)
            {
                delimiters = L"";
            }

            auto cursor = GetVimCursor(terminal);
            auto targetPos = cursor.Span.start;
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
            for (til::CoordType j = pos.y; j >= 0; j--)
            {
                auto x = j == pos.y ? pos.x : terminal.GetTextBuffer().GetRowByOffset(j).size();
                auto glyph = terminal.GetTextBuffer().GetRowByOffset(j).GlyphAt(x);
                if (glyph == endDelimiter)
                {
                    x--;
                }

                for (int i = x; i >= 0; i--)
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
            auto selection = terminal.GetVimSelectionAnchors();
            auto cursor = GetVimCursor(terminal, &selection);

            auto pos = cursor.Span.start;

            const auto glyph = terminal.GetTextBuffer().GetRowByOffset(pos.y).GlyphAt(pos.x);
            const auto posIsDelimiter = glyph == delimiter;

            auto foundMatchGoingBack = false;
            til::CoordType matchGoingBack;
            for (int i = pos.x - 1; i >= 0; i--)
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
            for (int i = pos.x + 1; i <= terminal.GetTextBuffer().GetRowByOffset(pos.y).size(); i++)
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
                    selection.end = til::point{ pos.x, pos.y };
                    selection.start = til::point{ matchGoingBack, pos.y };
                    if (!includeDelimiter)
                    {
                        selection.start.x++;
                        selection.end.x;
                    }
                    else
                    {
                        selection.end.x++;
                    }
                    terminal.SetVimSelectionAnchors(&selection);
                }
                else if (foundMatchGoingForward)
                {
                    selection.start = til::point{ pos.x, pos.y };
                    selection.end = til::point{ matchGoingForward, pos.y };
                    if (!includeDelimiter)
                    {
                        selection.start.x++;
                        selection.end.x;
                    }
                    else
                    {
                        selection.end.x++;
                    }
                    terminal.SetVimSelectionAnchors(&selection);
                }
            }
            else if (foundMatchGoingBack && foundMatchGoingForward)
            {
                selection.start = til::point{ matchGoingBack, pos.y };
                selection.end = til::point{ matchGoingForward, pos.y };
                if (!includeDelimiter)
                {
                    selection.start.x++;
                    selection.end.x;
                }
                else
                {
                    selection.end.x++;
                }
                terminal.SetVimSelectionAnchors(&selection);
            }
        }

        void MatchingChar(Microsoft::Terminal::Core::Terminal& terminal, til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool inBlock)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            std::optional<til::point_span> findResult;
            if (onStartDelimiter)
            {
                findResult = _findBlockEndFromStart(terminal, startPos, startDelimiter, endDelimiter);
            }
            else
            {
                findResult = _findBlockStartFromEnd(terminal, startPos, startDelimiter, endDelimiter);
            }
            if (findResult.has_value())
            {
                selection.start = findResult->start;
                selection.end = findResult->end;
                if (inBlock)
                {
                    if (selection.end.x == 0)
                    {
                        selection.end.y--;
                        selection.end.x = terminal.GetTextBuffer().GetRowByOffset(selection.end.y).GetLastNonSpaceColumn() - 1;
                    }
                    else
                    {
                        selection.end.x--;
                    }
                    if (selection.start.x >= terminal.GetTextBuffer().GetRowByOffset(selection.end.y).GetLastNonSpaceColumn() - 1)
                    {
                        selection.start.y++;
                        selection.start.x = 0;
                    }
                    else
                    {
                        selection.start.x++;
                    }
                }
                selection.pivot = selection.end;
                terminal.SetVimSelectionAnchors(&selection);
            }
        }

        void MoveToFirstNonBlankChar(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            auto cursor = GetVimCursor(terminal, &selection);

            auto startLine = _getStartLineOfRow(terminal.GetTextBuffer(), cursor.Span.start.y);

            if (terminal.IsBlockSelection())
            {
                auto maxNonSpaceChar = 0;
                for (til::CoordType i = selection.start.y; i <= selection.end.y; i++)
                {
                    auto lastNonSpaceColumn = 0;
                    const auto nonBlankCharResult = _GetLineFirstNonBlankChar(terminal, { 0, i });
                    if (nonBlankCharResult.has_value())
                    {
                        lastNonSpaceColumn = nonBlankCharResult.value().x;
                    }

                    if (lastNonSpaceColumn < maxNonSpaceChar)
                    {
                        maxNonSpaceChar = lastNonSpaceColumn;
                    }
                }

                for (til::CoordType i = cursor.Span.start.x; i > maxNonSpaceChar; i--)
                {
                    _moveLeftBlock(terminal, &selection);
                }
                terminal.SetVimSelectionAnchors(&selection);
                return;
            }
            auto startOfLine = til::point{ 0, startLine };
            auto firstNonBlankChar = _GetLineFirstNonBlankChar(terminal, startOfLine);

            til::point newStart;
            if (firstNonBlankChar.has_value())
            {
                newStart = { firstNonBlankChar.value().x, startLine };
            }
            else
            {
                newStart = { 0, startLine };
            }

            _moveLeftToPoint(terminal, &selection, newStart, isVisual, false);
        }

        void SelectHalfPageUp(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            auto cursor = GetVimCursor(terminal, &selection);
            const auto bufferSize{ terminal.GetTextBuffer().GetSize() };
            const auto viewportHeight = terminal.GetViewport().Height();
            if (cursor.IsBlock)
            {
                auto rowsToMoveUp = viewportHeight / 2;
                for (int i = 0; i < rowsToMoveUp; i++)
                {
                    _moveUpBlock(terminal, &selection);
                }
                terminal.SetVimSelectionAnchors(&selection);
                return;
            }

            auto targetPos = cursor.Span.start;

            const auto newY = targetPos.y - viewportHeight / 2;
            const auto y = newY < bufferSize.Top() ? 0 : newY;
            const til::CoordType x = targetPos.x;
            if (entireLine)
            {
                while (cursor.Span.start.y > y)
                {
                    _moveUpEntireLine(terminal, &selection);
                    cursor = GetVimCursor(terminal, &selection);
                }
                terminal.SetVimSelectionAnchors(&selection);
                return;
            }
            const auto point = til::point{ x, y };
            while (cursor.Span.start != point && cursor.Span.start.y >= point.y)
            {
                if (isVisual)
                {
                    _moveUpVisual(terminal, &selection);
                }
                else
                {
                    _moveUpCell(terminal, &selection);
                }
                cursor = GetVimCursor(terminal, &selection);
            }
            terminal.SetVimSelectionAnchors(&selection);
        }

        void SelectHalfPageDown(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine)
        {
            const auto viewportHeight{ terminal.GetViewport().Height() };
            const auto lastRow = _getLastNonSpaceChar(terminal).y;
            auto cursor = GetVimCursor(terminal);
            auto vimSelection = terminal.GetVimSelectionAnchors();
            auto pos = cursor.Span.start;
            const auto newY = pos.y + viewportHeight / 2;
            const auto y = newY > lastRow ? lastRow : newY;
            if (cursor.IsBlock)
            {
                for (int i = 0; i < y; i++)
                {
                    _moveDownBlock(terminal, &vimSelection);
                }
                terminal.SetVimSelectionAnchors(&vimSelection);

                return;
            }

            if (entireLine)
            {
                while (cursor.Span.start.y != y)
                {
                    _moveDownEntireLine(terminal, &vimSelection);
                    cursor = GetVimCursor(terminal, &vimSelection);
                }
                terminal.SetVimSelectionAnchors(&vimSelection);
                return;
            }

            const til::CoordType x = pos.x;
            const auto point = til::point{ x, y };
            _moveDownToPoint(terminal, &vimSelection, point, isVisual, entireLine);
        }

        void SelectBottom(Microsoft::Terminal::Core::Terminal& terminal, bool isvisual, bool entireLine)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            auto cursor = GetVimCursor(terminal, &selection);
            auto lastChar = _getLastNonSpaceChar(terminal);

            auto target = til::point{ selection.start.x, lastChar.y };
            if (target > lastChar || entireLine)
            {
                target = lastChar;
            }

            if (entireLine)
            {
                while (cursor.Span.start.y < target.y)
                {
                    _moveDownEntireLine(terminal, &selection);
                    cursor = GetVimCursor(terminal, &selection);
                }
                terminal.SetVimSelectionAnchors(&selection);
            }
            else
            {
                _moveDownToPoint(terminal, &selection, target, isvisual, false);
            }
            terminal.UserScrollViewport(lastChar.y);
        }

        void SelectTop(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            auto cursor = GetVimCursor(terminal, &selection);
            auto selectionAnchors = terminal.GetSelectionAnchors();

            if (entireLine)
            {
                while (cursor.Span.start.y != 0)
                {
                    _moveUpEntireLine(terminal, &selection);
                    cursor = GetVimCursor(terminal, &selection);
                }
                terminal.SetVimSelectionAnchors(&selection);
                terminal.UserScrollViewport(0);
                return;
            }
            auto pos = til::point{ selection.start.x, 0 };
            while (cursor.Span.start != pos && cursor.Span.start.y > 0)
            {
                if (isVisual)
                {
                    _moveUpVisual(terminal, &selection);
                }
                else
                {
                    _moveUpCell(terminal, &selection);
                }
                cursor = GetVimCursor(terminal, &selection);
            }

            terminal.SetVimSelectionAnchors(&selection);
            terminal.UserScrollViewport(0);
        }

        void SelectLineDown(Microsoft::Terminal::Core::Terminal& terminal)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            _moveDownEntireLine(terminal, &selection);
            terminal.SetVimSelectionAnchors(&selection);
        }

        void SelectLineUp(Microsoft::Terminal::Core::Terminal& terminal)
        {
            auto selection = terminal.GetVimSelectionAnchors();
            _moveUpEntireLine(terminal, &selection);
            terminal.SetVimSelectionAnchors(&selection);
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

        void SelectEntireLine(Microsoft::Terminal::Core::Terminal &terminal)
        {
            auto cursor = GetVimCursor(terminal);
            auto startOfLine = _getStartLineOfRow(terminal.GetTextBuffer(), cursor.Span.start.y);

            auto pos = cursor.Span.start.y;
            til::CoordType endLine = pos;
            while (terminal.GetTextBuffer().GetRowByOffset(endLine).WasWrapForced())
            {
                endLine++;
                if (terminal.GetTextBuffer().GetRowByOffset(endLine).GetLastNonSpaceColumn() > 0)
                {
                    pos = endLine;
                }
            }

            const auto lastNonSpaceColumn = std::max(0, terminal.GetTextBuffer().GetRowByOffset(pos).GetLastNonSpaceColumn());
            auto end = til::point{ lastNonSpaceColumn, pos };

            auto vimSelection = terminal.GetVimSelectionAnchors();
            vimSelection.start = { 0, startOfLine };
            vimSelection.end = end;
            vimSelection.pivot = vimSelection.start;
            terminal.SetVimSelectionAnchors(&vimSelection);
        }
    }
}
