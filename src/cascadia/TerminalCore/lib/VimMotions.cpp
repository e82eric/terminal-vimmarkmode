#include "pch.h"
#include "VimMotions.hpp"

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
            return til::point{ lastColumn, maybeLastRowNumber };
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

        void _matchingChar(Microsoft::Terminal::Core::Terminal &terminal, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool isVisual)
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

            // expand right until we hit the right boundary or a different delimiter class
            while (result.x < bufferSize.RightInclusive())
            {
                bufferSize.IncrementInBounds(result);
                auto classAt = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
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

            return til::point{ lastNonControlChar, target.y };
        }

        std::pair<til::point, bool> _GetStartOfNextWord(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            auto wordEnd = _GetEndOfWord(terminal, target, wordDelimiters);

            if (!wordEnd.second)
            {
                return wordEnd;
            }

            const auto bufferSize = terminal.GetTextBuffer().GetSize();

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
                auto classAt = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
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

        std::pair<til::point, bool> _GetStartOfWord(Microsoft::Terminal::Core::Terminal &terminal, const til::point target, const std::wstring_view wordDelimiters)
        {
            const auto bufferSize = terminal.GetTextBuffer().GetSize();

            auto result = target;
            bool found = false;

            while (result.x >= 0)
            {
                bufferSize.DecrementInBounds(result);
                auto classAt = terminal.GetTextBuffer().GetRowByOffset(result.y).DelimiterClassAt(result.x, wordDelimiters);
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
            auto selection = terminal.GetSelectionAnchors();
            auto startPoint = selection->end == selection->pivot ?
                                  til::point{ selection->start.x - 1, selection->start.y } :
                                  til::point{ selection->end.x - 1, selection->end.y };

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
            auto endPair = _GetEndOfWord(terminal, pos, delimiters);
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
            const auto selection = terminal.GetSelectionAnchors();
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

        til::CoordType _getStartLineOfRow2(TextBuffer& textBuffer, til::CoordType row)
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
                auto pivotIsStart = selection->start == til::point{ selection->pivot.x - 1, selection->pivot.y };
                //This means that the end is moving
                if (pivotIsStart)
                {
                    if (adjusted < selection->pivot)
                    {
                        selection->start = adjusted;
                        selection->end = til::point{ selection->pivot.x, selection->pivot.y };
                    }
                    else
                    {
                        selection->end = til::point{ adjusted.x + 1, adjusted.y };
                    }
                }
                //This means that the start is moving
                else
                {
                    if (adjusted > selection->pivot)
                    {
                        selection->start = selection->pivot;
                        selection->end = til::point{ adjusted.x + 1, adjusted.y };
                    }
                    else
                    {
                        selection->start = til::point{ adjusted.x + 1, adjusted.y };
                    }
                }
            }
            else
            {
                selection->start = adjusted;
                selection->end = til::point{ adjusted.x + 1, adjusted.y };
                selection->pivot = selection->end;
            }
            terminal.SetSelectionAnchors(selection);
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

        void SelectLastNonSpaceChar(Microsoft::Terminal::Core::Terminal& terminal)
        {
            auto lastNonSpaceChar = _getLastNonSpaceChar(terminal);
            terminal.SelectChar(lastNonSpaceChar);
        }

        void MoveLeft(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            const auto selection = terminal.GetSelectionAnchors();
            auto point = selection->end > selection->pivot ? selection->end : selection->start;
            point.x--;
            if (point.x >= 0)
            {
                _UpdateSelection(terminal, isVisual, point);
            }
        }

        void MoveRight(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            const auto selection = terminal.GetSelectionAnchors();
            auto point = selection->end > selection->pivot ? selection->end : selection->start;
            point.x++;
            if (point.x < terminal.GetTextBuffer().GetLineWidth(point.y))
            {
                _UpdateSelection(terminal, isVisual, point);
            }
        }

        void MoveDown(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto lastY = _getLastNonSpaceChar(terminal).y;
            const auto selection = terminal.GetSelectionAnchors();
            auto point = selection->end > selection->pivot ? selection->end : selection->start;
            point.y++;
            if (point.y <= lastY)
            {
                _UpdateSelection(terminal, isVisual, point);
            }
        }

        void MoveUp(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            const auto selection = terminal.GetSelectionAnchors();
            auto point = selection->end > selection->pivot ? selection->end : selection->start;
            point.y--;
            if (point.y >= 0)
            {
                _UpdateSelection(terminal, isVisual, point);
            }
        }

        void MoveToStartOfLine(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selection = terminal.GetSelectionAnchors();
            auto startLine = _getStartLineOfRow2(terminal.GetTextBuffer(), selection->start.y);
            if (terminal.IsBlockSelection())
            {
                startLine = selection->start.y == selection->pivot.y ? selection->end.y : selection->start.y;
            }
            auto startOfLine = til::point{ 0, startLine };
            auto firstNonBlankChar = _GetLineFirstNonBlankChar(terminal, startOfLine);

            if (firstNonBlankChar.second == true)
            {
                _UpdateSelection(terminal, isVisual, firstNonBlankChar.first);
            }
            else
            {
                _UpdateSelection(terminal, isVisual, startOfLine);
            }
        }

        void MoveToEndOfLine(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            if (terminal.IsBlockSelection())
            {
                auto maxNonSpaceChar = 0;
                for (auto i = selection->start.y; i <= selection->end.y; i++)
                {
                    const auto lastNonSpaceColumn = std::max(0, terminal.GetTextBuffer().GetRowByOffset(i).GetLastNonSpaceColumn() - 1);
                    if (lastNonSpaceColumn > maxNonSpaceChar)
                    {
                        maxNonSpaceChar = lastNonSpaceColumn;
                    }
                }
                auto s = til::point{ maxNonSpaceChar, selection->end.y };
                selection->end = s;
                terminal.SetSelectionAnchors(selection);
            }
            else
            {
                const auto pos = selection->pivot == selection->start ? selection->end : selection->start;
                auto lastRowWithChars = pos.y;
                til::CoordType endLine = pos.y;
                while (terminal.GetTextBuffer().GetRowByOffset(endLine).WasWrapForced())
                {
                    endLine++;
                    if (terminal.GetTextBuffer().GetRowByOffset(endLine).GetLastNonSpaceColumn() > 0)
                    {
                        lastRowWithChars = endLine;
                    }
                }

                const auto lastNonSpaceColumn = std::max(0, terminal.GetTextBuffer().GetRowByOffset(lastRowWithChars).GetLastNonSpaceColumn() - 1);
                auto s = til::point{ lastNonSpaceColumn, lastRowWithChars };
                _UpdateSelection(terminal, isVisual, s);
            }
        }

        void TilChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            til::point target;
            if (_FindChar(terminal, vkey, true, target))
            {
                _UpdateSelection(terminal, isVisual, target);
            }
        }

        void TilCharBackwards(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            til::point target;
            if (_FindCharBack(terminal, vkey, true, target))
            {
                _UpdateSelection(terminal, isVisual, target);
            }
        }

        void FindChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            til::point target;
            if (_FindChar(terminal, vkey, false, target))
            {
                _UpdateSelection(terminal, isVisual, target);
            }
        }

        void FindCharBackwards(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual)
        {
            til::point target;
            if (_FindCharBack(terminal, vkey, false, target))
            {
                _UpdateSelection(terminal, isVisual, target);
            }
        }

        void MoveWordLeft(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord, bool isVisual)
        {
            auto delimiters = _wordDelimiters;
            if (isLargeWord)
            {
                delimiters = L"";
            }

            auto selection = terminal.GetSelectionAnchors();
            auto startPoint = selection->end > selection->pivot ? selection->end : selection->start;

            auto startPair = _GetStartOfWord(terminal, startPoint, delimiters);
            if (startPair.second)
            {
                if (startPair.first == startPoint)
                {
                    startPair = _GetEndOfPreviousWord(terminal, startPair.first, delimiters);
                    if (startPair.second)
                    {
                        startPair = _GetStartOfWord(terminal, startPair.first, delimiters);
                    }
                }

                if (startPair.second)
                {
                    _UpdateSelection(terminal, isVisual, startPair.first);
                }
                else
                {
                    auto startOfPreviousLine = til::point{ 0, selection->end.y - 1 };
                    auto endOfLine = _GetLineEnd(terminal, startOfPreviousLine);
                    startPair = _GetStartOfWord(terminal, endOfLine, delimiters);
                    if (startPair.second)
                    {
                        _UpdateSelection(terminal, isVisual, startPair.first);
                    }
                }
            }
        }

        void MoveWordRight(Microsoft::Terminal::Core::Terminal &terminal, bool isLargeWord, bool isVisual)
        {
            auto selection = terminal.GetSelectionAnchors();
            auto delimiters = _wordDelimiters;
            if (isLargeWord)
            {
                delimiters = L"";
            }

            auto startPoint = selection->start < selection->pivot ? selection->start : selection->end;

            auto endPair = _GetEndOfWord(terminal, startPoint, delimiters);
            if (endPair.second)
            {
                if (endPair.first == startPoint)
                {
                    endPair = _GetStartOfNextWord(terminal, endPair.first, delimiters);
                    if (endPair.second)
                    {
                        endPair = _GetEndOfWord(terminal, endPair.first, delimiters);
                    }
                }

                if (endPair.second)
                {
                    _UpdateSelection(terminal, isVisual, endPair.first);
                }
                else
                {
                    auto yToMove = selection->start < selection->pivot ? selection->start.y : selection->end.y;
                    auto startOfNextLine = til::point{ 0, yToMove + 1 };
                    endPair = _GetEndOfWord(terminal, startOfNextLine, delimiters);
                    if (endPair.second)
                    {
                        _UpdateSelection(terminal, isVisual, endPair.first);
                    }
                }
            }
        }

        void MoveWordStartRight(Microsoft::Terminal::Core::Terminal &terminal, bool isLargeWord, bool isVisual)
        {
            auto delimiters = _wordDelimiters;
            if (isLargeWord)
            {
                delimiters = L"";
            }

            auto selection = terminal.GetSelectionAnchors();
            auto startPoint = selection->start < selection->pivot ? selection->start : selection->end;

            auto start = _GetStartOfNextWord(terminal, startPoint, delimiters);

            if (start.second)
            {
                _UpdateSelection(terminal, isVisual, start.first);
            }
            else
            {
                auto yToMove = selection->start < selection->pivot ? selection->start.y : selection->end.y;
                auto startOfNextLine = til::point{ 0, yToMove + 1 };
                _UpdateSelection(terminal, isVisual, startOfNextLine);
            }
        }

        void SelectInWord(Microsoft::Terminal::Core::Terminal &terminal, bool isLargeWord)
        {
            auto delimiters = _wordDelimiters;
            if (isLargeWord)
            {
                delimiters = L"";
            }

            auto targetPos{ terminal.GetSelectionAnchors()->end };
            _InWord(terminal, targetPos, delimiters);
        }

        void InDelimiter(Microsoft::Terminal::Core::Terminal &terminal, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter)
        {
            auto delimitersAreSame = startDelimiter == endDelimiter;

            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            const auto pos = selection->start == selection->pivot ? selection->end : selection->start;

            auto numberOfInnerPairs = 0;
            for (auto j = pos.y; j >= 0; j--)
            {
                auto x = j == pos.y ? pos.x - 1 : terminal.GetTextBuffer().GetRowByOffset(j).size();
                for (auto i = x; i >= 0; i--)
                {
                    auto g = terminal.GetTextBuffer().GetRowByOffset(j).GlyphAt(i);
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
                            _matchingChar(terminal, til::point{ i, j }, startDelimiter, endDelimiter, true, !includeDelimiter);
                            return;
                        }
                    }
                }
            }
        }

        void InDelimiterSameLine(Microsoft::Terminal::Core::Terminal &terminal, std::wstring_view delimiter, bool includeDelimiter)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            const auto pos = selection->start == selection->pivot ? selection->end : selection->start;

            const auto posIsDelimiter = terminal.GetTextBuffer().GetRowByOffset(pos.y).GlyphAt(pos.x) == delimiter;

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
                        selection->end.x--;
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
                        selection->end.x--;
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
                    selection->end.x--;
                }
                terminal.SetSelectionAnchors(selection);
            }
        }
    }
}
