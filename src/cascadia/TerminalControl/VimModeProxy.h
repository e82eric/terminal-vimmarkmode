#pragma once
#include "../../cascadia/TerminalCore/Terminal.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class ITerminalApi;
}

class VimModeProxy
{
public:
    VimModeProxy(Microsoft::Terminal::Core::Terminal* terminal);
    void FindChar(std::wstring_view vkey, bool isVisual);
    void TilChar(std::wstring_view vkey, bool isVisual);
    void FindCharBack(std::wstring_view vkey, bool isVisual);
    void TilCharBack(std::wstring_view vkey, bool isVisual);
    void InDelimiter(std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter);
    void SelectWordRight(bool isVisual, bool isLargeWord);
    void SelectWordLeft(bool isVisual, bool isLargeWord);
    void SelectWordStartRight(bool isVisual, bool isLargeWord);
    void SelectInWord(bool largeWord);
    void SelectLineRight(bool isVisual);
    void SelectLineLeft(bool isVisual);
    void SelectLineFirstNonBlankChar(bool isVisual);
    void SelectLineUp();
    void SelectLineDown();
    void SelectTop(bool isVisual);
    void SelectBottom(bool isVisual);
    void SelectHalfPageUp(bool isVisual);
    void SelectHalfPageDown(bool isVisual);
    void SelectPageUp(bool isVisual);
    void SelectPageDown(bool isVisual);
    void SelectCharRight(bool isVisual);
    void SelectCharLeft(bool isVisual);
    void SelectDown(bool isVisual);
    void SelectUp(bool isVisual);

private:
    bool _FindChar(std::wstring_view vkey, bool isTil, til::point& target);
    bool _FindCharBack(std::wstring_view vkey, bool isTil, til::point& target);
    void _UpdateSelection(bool isVisual, til::point adjusted);
    void _InDelimiter(til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter);
    void _InWord(til::point& pos, std::wstring_view delimiters);
    std::pair<til::point, bool> _GetStartOfNextWord(const til::point target, const std::wstring_view wordDelimiters) const;
    std::pair<til::point, bool> _GetEndOfWord(const til::point target, const std::wstring_view wordDelimiters) const;
    std::pair<til::point, bool> _GetStartOfWord(const til::point target, const std::wstring_view wordDelimiters) const;
    std::pair<til::point, bool> _GetEndOfPreviousWord(const til::point target, const std::wstring_view wordDelimiters) const;
    til::point _GetLineEnd(const til::point target) const;
    std::pair<til::point, bool> _GetLineFirstNonBlankChar(const til::point target) const;
    void _MoveByViewport(::Microsoft::Terminal::Core::Terminal::SelectionDirection direction, til::point& pos) noexcept;
    void _MoveByHalfViewport(::Microsoft::Terminal::Core::Terminal::SelectionDirection direction, til::point& pos) noexcept;

    Microsoft::Terminal::Core::Terminal* _terminal;
    std::wstring _wordDelimiters = L"/\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?│";
};
