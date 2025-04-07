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
            selection->start = til::point{ point.x - 1, point.y };
            selection->pivot = selection->start;
            selection->end = til::point{ point.x, point.y };
            terminal.SetSelectionAnchors(selection);
        }

        void SelectLastNonSpaceChar(Microsoft::Terminal::Core::Terminal& terminal)
        {
            auto lastNonSpaceChar = _getLastNonSpaceChar(terminal);
            SelectPoint(terminal, lastNonSpaceChar);
        }

        void MoveLeft(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            if (selectionAnchors->start.x == 0)
            {
                return;
            }
            auto selection{ selectionAnchors.write() };

            if (!isVisual)
            {
                selection->start.x--;
                selection->end.x--;
                selection->pivot = selection->end;
            }
            else
            {
                bool singleCell = (selectionAnchors->end.y == selectionAnchors->start.y &&
                                   selectionAnchors->end.x - 1 == selectionAnchors->start.x);
                if (singleCell)
                {
                    selection->start.x--;
                    selection->pivot = selection->end;
                }
                else if (selection->end > selection->pivot)
                {
                    selection->end.x--;
                    selection->pivot = selection->start;
                }
                else
                {
                    selection->start.x--;
                    selection->pivot = selection->end;
                }
            }

            terminal.SetSelectionAnchors(selection);
        }

        void MoveRight(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();

            if (selectionAnchors->end.x >= terminal.GetTextBuffer().GetLineWidth(selectionAnchors->end.y))
            {
                return;
            }
            const auto selection{ selectionAnchors.write() };
            if (!isVisual)
            {
                selection->start.x++;
                selection->end = {selection->start.x + 1, selection->start.y};
                selection->pivot = selection->end;
            }
            else
            {
                bool singleCell = (selectionAnchors->end.y == selectionAnchors->start.y &&
                                   selectionAnchors->end.x - 1 == selectionAnchors->start.x);
                if (singleCell)
                {
                    selection->end.x++;
                    selection->pivot = selection->start;
                }
                else if (selection->end > selection->pivot)
                {
                    selection->end.x++;
                    selection->pivot = selection->start;
                }
                else
                {
                    selection->start.x++;
                    selection->pivot = selection->end;
                }
            }

            terminal.SetSelectionAnchors(selection);
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
            auto selectionAnchors = terminal.GetSelectionAnchors();
            auto selection{ selectionAnchors.write() };
            auto startLine = _getStartLineOfRow(terminal.GetTextBuffer(), selection->start.y);
            if (terminal.IsBlockSelection())
            {
                startLine = selection->start.y == selection->pivot.y ? selection->end.y : selection->start.y;
            }
            auto startOfLine = til::point{ 0, startLine };

            if (!isVisual)
            {
                selection->start = startOfLine;
                selection->end = til::point{ 1, startLine };
                selection->pivot = selection->start;
                terminal.SetSelectionAnchors(selection);
                return;
            }

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

        void MoveToEndOfLine(Microsoft::Terminal::Core::Terminal& terminal, bool /*isVisual*/)
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
                selection->start = s;
                selection->end = til::point{ s.x + 1, s.y };
                selection->pivot = selection->end;
                terminal.SetSelectionAnchors(selection);
                //_UpdateSelection(terminal, isVisual, s);
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
            auto selectionAnchors = terminal.GetSelectionAnchors();
            auto selection{ selectionAnchors.write() };
            auto delimiters = isLargeWord ? L"" : _wordDelimiters;

            const bool isSingleCell = (selection->end.x - 1 == selection->start.x && selection->start.y == selection->end.y);
            const bool pivotAtStart = (selection->start == selection->pivot);
            const bool pivotAtEnd = (selection->end == selection->pivot);

            auto lastPoint = _getLastNonSpaceChar(terminal);
            auto startPoint = selection->start;
            if (isSingleCell)
            {
                startPoint = { selection->start.x, selection->start.y };
            }
            else if (pivotAtStart)
            {
                startPoint = { selection->end.x - 1, selection->start.y };
            }

            auto updateSelection = [&](til::point wordEnd) {
                if (!isVisual)
                {
                    selection->start = wordEnd;
                    selection->end = til::point{wordEnd.x + 1, wordEnd.y};
                    selection->pivot = selection->start;
                }
                else if (isSingleCell)
                {
                    selection->start = {wordEnd.x, wordEnd.y};
                    selection->pivot = selection->end;
                }
                else if (pivotAtStart)
                {
                    selection->end = {wordEnd.x + 1, wordEnd.y};
                    selection->pivot = selection->start;
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
            auto startPoint = selection->end;
            if (isSingleCell || pivotAtEnd)
            {
                startPoint = { selection->start.x + 1, selection->start.y };
            }

            // Helper lambda to update the selection based on the word's end point.
            auto updateSelection = [&](til::point wordEnd) {
                if (!isVisual)
                {
                    selection->start = { wordEnd.x - 1, wordEnd.y };
                    selection->end = wordEnd;
                    selection->pivot = selection->end;
                }
                else if (isSingleCell || pivotAtStart)
                {
                    selection->end = wordEnd;
                    selection->pivot = selection->start;
                }
                else if (pivotAtEnd)
                {
                    selection->start = { wordEnd.x - 1, wordEnd.y };
                    selection->pivot = selection->end;
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

        void InDelimiterSameLine(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view delimiter, bool includeDelimiter)
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

        void MoveToFirstNonBlankChar(Microsoft::Terminal::Core::Terminal& terminal, bool /*isVisual*/)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            auto selection{ selectionAnchors.write() };
            auto startLine = _getStartLineOfRow(terminal.GetTextBuffer(), selection->start.y);
            if (terminal.IsBlockSelection())
            {
                startLine = selection->start.y == selection->pivot.y ? selection->end.y : selection->start.y;
            }
            auto startOfLine = til::point{ 0, startLine };
            auto firstNonBlankChar = _GetLineFirstNonBlankChar(terminal, startOfLine);

            if (firstNonBlankChar.second == true)
            {
                selection->start = { 0, startLine };
                selection->end = { 1, startLine };
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
            const auto targetPos = startIsPivot ? selection->end : selection->start;

            const auto viewportHeight = terminal.GetViewport().Height();
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
                        selection->end.x = terminal.GetTextBuffer().GetRowByOffset(y).GetLastNonSpaceColumn() - 1;
                    }
                    else
                    {
                        selection->end.y = selection->start.y;
                        selection->end.x = terminal.GetTextBuffer().GetRowByOffset(selection->start.y).GetLastNonSpaceColumn() - 1;
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
            const auto newPos = til::point{ x, y };

            _UpdateSelection(terminal, isVisual, newPos);
        }

        void SelectHalfPageDown(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            const auto startIsPivot = selection->start.y == selection->pivot.y && selection->start.x == selection->pivot.x;
            const auto pos = startIsPivot ? selection->end : selection->start;

            const auto viewportHeight{ terminal.GetViewport().Height() };
            const auto lastRow = _getLastNonSpaceChar(terminal).y;
            const auto newY = pos.y + viewportHeight / 2;
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
                        selection->end.x = terminal.GetTextBuffer().GetRowByOffset(selection->start.y).GetLastNonSpaceColumn() - 1;
                    }
                }
                else
                {
                    selection->end.y = y;
                    selection->end.x = terminal.GetTextBuffer().GetRowByOffset(y).GetLastNonSpaceColumn() - 1;
                    ;
                }
                terminal.SetSelectionAnchors(selection);
                return;
            }

            const auto newPos = til::point{ x, y };

            _UpdateSelection(terminal, isVisual, newPos);
        }

        void SelectBottom(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto lastChar = _getLastNonSpaceChar(terminal);
            _UpdateSelection(terminal, isVisual, lastChar);
            terminal.UserScrollViewport(lastChar.y);
        }

        void SelectTop(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual)
        {
            auto selectionAnchors = terminal.GetSelectionAnchors();
            const auto selection{ selectionAnchors.write() };
            if (isVisual)
            {
                selection->start = til::point{ selection->start.x, 0 };
                terminal.UserScrollViewport(0);
            }
            else
            {
                selection->start = til::point{ selection->start.x, 0 };
                selection->pivot = til::point{ selection->start.x, 0 };
                selection->end = til::point{ selection->start.x, 0 };
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
                auto end = _GetLineEnd(terminal, til::point{ 0, selection->end.y - 1 });
                selection->end = end;
            }
            else if (selection->end.y == selection->pivot.y)
            {
                auto currentEnd = _GetLineEnd(terminal, til::point{ 0, selection->end.y });
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
                terminal.SetSelectionAnchors(selection);
            }
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
                selection->pivot = currentEnd;
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
