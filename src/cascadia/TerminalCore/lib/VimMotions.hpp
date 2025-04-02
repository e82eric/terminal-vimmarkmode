#pragma once

#include "..\terminal.hpp"

namespace vim
{
    namespace motions
    {
        void SelectLastNonSpaceChar(Microsoft::Terminal::Core::Terminal& terminal);
        void MoveLeft(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void MoveRight(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void MoveToStartOfLine(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void MoveToEndOfLine(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void TilChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual);
        void TilCharBackwards(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual);
        void FindChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual);
        void FindCharBackwards(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view vkey, bool isVisual);
        void MoveWordLeft(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord, bool isVisual);
        void MoveWordRight(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord, bool isVisual);
        void MoveWordStartRight(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord, bool isVisual);
        void SelectInWord(Microsoft::Terminal::Core::Terminal& terminal, bool isLargeWord);
        void MoveDown(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void MoveUp(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void InDelimiter(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter);
        void InDelimiterSameLine(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view delimiter, bool includeDelimiter);
        void MatchingChar(Microsoft::Terminal::Core::Terminal& terminal, til::point startPos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool inBlock);
        void MoveToFirstNonBlankChar(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void SelectPageDown(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void SelectPageUp(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void SelectHalfPageUp(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine);
        void SelectHalfPageDown(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine);
        void SelectBottom(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void SelectTop(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual);
        void SelectLineDown(Microsoft::Terminal::Core::Terminal& terminal);
        void SelectLineUp(Microsoft::Terminal::Core::Terminal& terminal);
        void MatchingChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool isVisual);
    }
}
