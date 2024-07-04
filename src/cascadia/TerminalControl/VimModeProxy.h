#pragma once
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "ControlCore.h"

namespace Microsoft::Console::VirtualTerminal
{
    class ITerminalApi;
}
namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ControlCore;
}

class VimModeProxy
{
public:
    enum class VimMode : int32_t
    {
        none = 0,
        normal = 1,
        visual = 2,
        search = 3,
        visualLine = 4,
    };

    enum class VimTextAmount : int32_t
    {
        none = 0,
        in = 1,
        around = 2,
    };

    enum class VimActionType : int32_t
    {
        none = 0,
        yank = 1,
        search = 2,
        toggleVisualOn = 3,
        fuzzyFind = 4,
        exit = 5,
        scroll = 6,
        toggleRowNumbers,
        enterQuickSelectMode,
        enterQuickCopyMode,
        enterBlockSelectionMode,
        swapPivot,
        commitSearch,
        nextSearchResult
    };

    enum class VimTextObjectType : int32_t
    {
        none = 0,
        charTextObject = 1,
        word = 2,
        largeWord = 3,
        line = 4,
        startSquareBracePair = 5,
        startRoundBracePair = 6,
        startDoubleQuotePair = 7,
        startSingleQuotePair = 8,
        startAngleBracketPair = 9,
        startCurlyBracePair = 10,
        endSquareBracePair = 11,
        endRoundBracePair = 12,
        endDoubleQuotePair = 13,
        endSingleQuotePair = 14,
        endAngleBracketPair = 15,
        endCurlyBracePair = 16,
        inSquareBracePair = 17,
        inRoundBracePair = 18,
        inDoubleQuotePair = 19,
        inSingleQuotePair = 20,
        inAngleBracketPair = 21,
        inCurlyBracePair = 22,
        aroundSquareBracePair = 23,
        aroundRoundBracePair = 24,
        aroundDoubleQuotePair = 25,
        aroundSingleQuotePair = 26,
        aroundAngleBracketPair = 27,
        aroundCurlyBracePair = 28,
        tilChar = 29,
        findChar = 30,
        tilCharReverse = 31,
        findCharReverse = 32,
        inWord = 33,
        inLargeWord = 34,
        entireLine = 35,
        centerOfScreen = 36,
        topOfScreen = 37,
        bottomOfScreen = 38
    };

    enum class VimMotionType : int32_t
    {
        none = 0,
        moveRight = 1,
        moveLeft = 2,
        moveUp = 3,
        moveDown = 4,
        moveBackToBegining = 5,
        moveForwardToStart = 6,
        moveForwardToEnd = 7,
        g = 8,
        forward = 9,
        back = 10,
        moveToTopOfBuffer = 11,
        moveToBottomOfBuffer = 12,
        halfPageUp = 13,
        halfPageDown = 14,
        pageUp = 15,
        pageDown = 16,
        backToFirstNonSpaceChar = 17,
        selectCurrentLine
    };

    VimModeProxy(
        std::shared_ptr<Microsoft::Terminal::Core::Terminal> terminal,
        winrt::Microsoft::Terminal::Control::implementation::ControlCore *controlCore,
        Search *searcher);
    bool TryVimModeKeyBinding(
        const WORD vkey,
        const ::Microsoft::Terminal::Core::ControlKeyStates mods);
    void ResetVimState();
    void ExitVimMode();
    void EnterVimMode(bool selectLastChar);
    bool IsInVimMode();
    void ResetVimModeForSizeChange(bool selectLastChar);
    void SelectRow(int32_t row, int32_t col);
    bool ShowRowNumbers();
    void ShowRowNumbers(bool val);
    int32_t ViewportRowToHighlight();
    void StoreSelectionForResize();
    void UpdateSelectionFromResize() const;
    void SetSearchString(std::wstring_view searchString);
    void StartSearch(bool isReverse);
    void ExitVimSearch();
    void CommitSearch();

private:
    void _setStateForCompletedSequence();
    til::point _updateFromResize(til::point from) const;
    void _setPosForResize(til::point pos, til::point& target) const;
    VimMode _getVimMode();
    void _handleSearch(bool moveForward, bool isVisual);
    void _nextSearchResult(bool moveForward, bool isVisual);
    void _findChar(std::wstring_view vkey, bool isVisual);
    void _tilChar(std::wstring_view vkey, bool isVisual);
    void _findCharBack(std::wstring_view vkey, bool isVisual);
    void _tilCharBack(std::wstring_view vkey, bool isVisual);
    void _matchingChar(std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool isVisual);
    void _matchingChar(til::point pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool onStartDelimiter, bool inBlock);
    void _matchingCharFromStart(til::point pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool isVisual);
    void _matchingCharFromEnd(til::point pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool isVisual);
    void _inDelimiter(std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter);
    void _inDelimiterSameLine(std::wstring_view delimiter, bool includeDelimiter);
    void _selectWordRight(bool isVisual, bool isLargeWord);
    void _selectWordLeft(bool isVisual, bool isLargeWord);
    void _selectWordStartRight(bool isVisual, bool isLargeWord);
    void _selectInWord(bool largeWord);
    void _selectLineRight(bool isVisual);
    void _selectLineLeft(bool isVisual);
    void _selectLineFirstNonBlankChar(bool isVisual);
    void _selectLineUp();
    void _selectLineDown();
    void _selectTop(bool isVisual);
    void _selectBottom(bool isVisual);
    void _selectHalfPageUp(bool isVisual, bool entireLine);
    void _selectHalfPageDown(bool isVisual, bool entireLine);
    void _selectPageUp(bool isVisual);
    void _selectPageDown(bool isVisual);
    void _selectCharRight(bool isVisual);
    void _selectCharLeft(bool isVisual);
    void _selectDown(bool isVisual, til::CoordType lastY);
    void _selectUp(bool isVisual);
    bool _executeVimSelection(
        const VimActionType action,
        const VimTextObjectType textObject,
        const int times,
        const VimMotionType motion,
        const bool isVisual,
        const std::wstring searchString,
        std::wstring_view vkey);
    bool _FindChar(std::wstring_view vkey, bool isTil, til::point& target) const;
    bool _FindCharBack(std::wstring_view vkey, bool isTil, til::point& target);
    void _UpdateSelection(bool isVisual, til::point adjusted);
    std::tuple<bool, til::point, til::point> _findBlockEndFromStart(til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter) const;
    std::tuple<bool, til::point, til::point> _findBlockStartFromEnd(til::point& pos, std::wstring_view startDelimiter, std::wstring_view endDelimiter) const;
    void _InWord(til::point& pos, std::wstring_view delimiters);
    std::pair<til::point, bool> _GetStartOfNextWord(const til::point target, const std::wstring_view wordDelimiters) const;
    std::pair<til::point, bool> _GetEndOfWord(const til::point target, const std::wstring_view wordDelimiters) const;
    std::pair<til::point, bool> _GetStartOfWord(const til::point target, const std::wstring_view wordDelimiters) const;
    std::pair<til::point, bool> _GetEndOfPreviousWord(const til::point target, const std::wstring_view wordDelimiters) const;
    til::point _GetLineEnd(const til::point target) const;
    std::pair<til::point, bool> _GetLineFirstNonBlankChar(const til::point target) const;
    void _MoveByViewport(::Microsoft::Terminal::Core::Terminal::SelectionDirection direction, til::point& pos) noexcept;
    void _MoveByHalfViewport(::Microsoft::Terminal::Core::Terminal::SelectionDirection direction, til::point& pos) noexcept;
    void _ScrollIfNeeded(til::point& pos) noexcept;
    void _vimScrollScreenPosition(VimTextObjectType textObjectType);
    wil::unique_close_clipboard_call _openClipboard();
    std::wstring _getClipboardText();

    std::shared_ptr<Microsoft::Terminal::Core::Terminal> _terminal;
    winrt::Microsoft::Terminal::Control::implementation::ControlCore* _controlCore;
    Search* _searcher;
    std::wstring _wordDelimiters = L"/\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?│";

    til::CoordType _tempTop = 0;
    til::point _tempStart;
    til::point _tempEnd;
    til::point _tempPivot;
    bool _showRowNumbers = false;
    void _enterVimMode();
    VimTextObjectType _textObject = VimTextObjectType::none;
    VimMotionType _motion = VimMotionType::none;
    VimMode _vimMode = VimMode::none;
    VimMotionType _lastMotion = VimMotionType::none;
    VimActionType _action = VimActionType::none;
    VimTextAmount _amount = VimTextAmount::none;
    bool _leaderSequence = false;
    std::wstring _timesString = L"";
    int _times = 0;
    bool _reverseSearch = false;
    std::wstring _searchString = L"";
    std::wstring _sequenceText = L"";
    bool _quickSelectCopy = false;
    wchar_t _lastVkey[2] = { L'\0' };
    int _lastTimes = 0;
    VimActionType _lastAction = VimActionType::none;
    VimTextObjectType _lastTextObject = VimTextObjectType::none;
};
