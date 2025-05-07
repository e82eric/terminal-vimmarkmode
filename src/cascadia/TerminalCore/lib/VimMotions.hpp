#pragma once

#include "../Terminal.hpp"
#include "..\terminal.hpp"

namespace vim
{
    namespace motions
    {
        enum VimCursorPosition
        {
            SingleCell,
            Start,
            End
        };

        enum BlockVimCursorVerticalPosition
        {
            SingleRow,
            Top,
            Bottom
        };

        enum BlockVimCursorHorizontalPosition
        {
            SingleColumn,
            Left,
            Right
        };

        struct VimCursor
        {
            bool IsBlock;
            VimCursorPosition Position;
            BlockVimCursorHorizontalPosition BlockHorizontalPosition;
            BlockVimCursorVerticalPosition BlockVerticalPosition;
            til::point_span Span;
        };

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
        void SelectHalfPageUp(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine);
        void SelectHalfPageDown(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine);
        void SelectBottom(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine);
        void SelectTop(Microsoft::Terminal::Core::Terminal& terminal, bool isVisual, bool entireLine);
        void SelectLineDown(Microsoft::Terminal::Core::Terminal& terminal);
        void SelectLineUp(Microsoft::Terminal::Core::Terminal& terminal);
        void MatchingChar(Microsoft::Terminal::Core::Terminal& terminal, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool isVisual);
        void SelectPoint(Microsoft::Terminal::Core::Terminal& terminal, til::point point);
        void SelectCurrentChar(Microsoft::Terminal::Core::Terminal& terminal);
        void SelectEntireLine(Microsoft::Terminal::Core::Terminal& terminal);
        VimCursor GetVimCursor(Microsoft::Terminal::Core::Terminal& terminal) noexcept;
        VimCursor GetVimCursor(Microsoft::Terminal::Core::Terminal& terminal, Microsoft::Terminal::Core::Terminal::VimSelectionInfo *selection) noexcept;
    }
}
