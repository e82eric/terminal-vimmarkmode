#include "pch.h"
#include "VimModeProxy.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"

VimModeProxy::VimModeProxy(std::shared_ptr<Microsoft::Terminal::Core::Terminal> terminal, winrt::Microsoft::Terminal::Control::implementation::ControlCore *controlCore, Search* searcher)
{
    _terminal = terminal;
    _controlCore = controlCore;
    _searcher = searcher;
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

void VimModeProxy::_inDelimiter(std::wstring_view startDelimiter, std::wstring_view endDelimiter, bool includeDelimiter)
{
    auto selection = _terminal->GetSelectionAnchors();
    _InDelimiter(selection->start, startDelimiter, endDelimiter, includeDelimiter);
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
    til::point s;
    if (_terminal->IsBlockSelection())
    {
        auto maxNonSpaceChar = 0;
        for (auto i = selection->start.y; i <= selection->end.y; i++)
        {
            auto lastNonSpaceColumn = std::max(0, _terminal->GetTextBuffer().GetRowByOffset(i).GetLastNonSpaceColumn() - 1);
            if (lastNonSpaceColumn > maxNonSpaceChar)
            {
                maxNonSpaceChar = lastNonSpaceColumn;
            }
        }
        s = til::point{ maxNonSpaceChar, selection->end.y };
    }
    else
    {
        til::CoordType endLine = selection->end.y;
        while (_terminal->GetTextBuffer().GetRowByOffset(endLine).WasWrapForced())
        {
            endLine++;
        }

        auto lastNonSpaceColumn = std::max(0, _terminal->GetTextBuffer().GetRowByOffset(endLine).GetLastNonSpaceColumn() - 1);
        s = til::point{ lastNonSpaceColumn, endLine };
    }
    _UpdateSelection(isVisual, s);
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
    til::CoordType startLine = _getStartLineOfRow2(_terminal->GetTextBuffer(), selection->end.y);
    if (_terminal->IsBlockSelection())
    {
        startLine = selection->start.y == selection->pivot.y ? selection->end.y : selection->start.y;
    }
    _UpdateSelection(isVisual, til::point{ 0, startLine });
}

void VimModeProxy::_selectLineUp()
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
    _terminal->SetSelectionAnchors(selection);
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
    _terminal->SetSelectionAnchors(selection);
}

void VimModeProxy::_selectBottom(bool isVisual)
{
    auto lastChar = _terminal->GetTextBuffer().GetLastNonSpaceCharacter();
    _UpdateSelection(isVisual, lastChar);
    _terminal->UserScrollViewport(lastChar.y);
}

void VimModeProxy::_selectHalfPageUp(bool isVisual)
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

void VimModeProxy::_selectHalfPageDown(bool isVisual)
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
    DWORD mods = isVisual ? 280 : 0;
    _terminal->UpdateSelection(::Microsoft::Terminal::Core::Terminal::SelectionDirection ::Right, ::Microsoft::Terminal::Core::Terminal::SelectionExpansion::Char, Microsoft::Terminal::Core::ControlKeyStates{ mods });
}

void VimModeProxy::_selectCharLeft(bool isVisual)
{
    DWORD mods = isVisual ? 280 : 0;
    _terminal->UpdateSelection(::Microsoft::Terminal::Core::Terminal::SelectionDirection::Left, ::Microsoft::Terminal::Core::Terminal::SelectionExpansion::Char, ::Microsoft::Terminal::Core::ControlKeyStates{ mods });
}

void VimModeProxy::_selectDown(bool isVisual)
{
    DWORD mods = isVisual ? 280 : 0;
    _terminal->UpdateSelection(::Microsoft::Terminal::Core::Terminal::SelectionDirection::Down, ::Microsoft::Terminal::Core::Terminal::SelectionExpansion::Char, ::Microsoft::Terminal::Core::ControlKeyStates{ mods });
}

void VimModeProxy::_selectUp(bool isVisual)
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

    for (int i = 0; i < times; i++)
    {
        switch (textObject)
        {
        case VimTextObjectType::inSquareBracePair:
            _inDelimiter(L"[", L"]", false);
            break;
        case VimTextObjectType::inRoundBracePair:
            _inDelimiter(L"(", L")", false);
            break;
        case VimTextObjectType::inSingleQuotePair:
            _inDelimiter(L"'", L"'", false);
            break;
        case VimTextObjectType::inDoubleQuotePair:
            _inDelimiter(L"\"", L"\"", false);
            break;
        case VimTextObjectType::inAngleBracketPair:
            _inDelimiter(L"<", L">", false);
            break;
        case VimTextObjectType::aroundSquareBracePair:
            _inDelimiter(L"[", L"]", true);
            break;
        case VimTextObjectType::aroundRoundBracePair:
            _inDelimiter(L"(", L")", true);
            break;
        case VimTextObjectType::aroundSingleQuotePair:
            _inDelimiter(L"'", L"'", true);
            break;
        case VimTextObjectType::aroundDoubleQuotePair:
            _inDelimiter(L"\"", L"\"", true);
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
                _selectHalfPageUp(true);
                break;
            case VimMotionType::halfPageDown:
                _selectHalfPageDown(true);
                break;
            case VimMotionType::pageUp:
                _selectPageUp(true);
                break;
            case VimMotionType::pageDown:
                _selectPageDown(true);
            default:
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
                _selectDown(selectFromStart);
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
                _selectHalfPageUp(selectFromStart);
                break;
            case VimMotionType::halfPageDown:
                _selectHalfPageDown(selectFromStart);
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
    case VimActionType::enterQuickCopyMode:
    case VimActionType::enterQuickSelectMode:
        ResetVimState();
        _controlCore->EnterQuickSelectMode(L"[\\w\\d\\S]+", action == VimActionType::enterQuickCopyMode);
        break;
    case VimActionType::toggleRowNumbersOn:
    {
        _showRowNumbers = true;
        _controlCore->ToggleRowNumbers(true);
        break;
    }
    case VimActionType::toggleRowNumbersOff:
    {
        _showRowNumbers = false;
        _controlCore->ToggleRowNumbers(false);
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
    case VimActionType::search:
    {
        auto moveForward = motion != VimMotionType::forward;
        if (textObject == VimTextObjectType::word)
        {
            _selectInWord(false);
            const auto bufferData = _terminal->RetrieveSelectedTextFromBuffer(moveForward);
            auto searchString = bufferData.plainText;
            _searchString = searchString;
            if (_searcher->ResetIfStale(*_terminal, searchString, moveForward, true))
            {
                _searcher->HighlightResults();
                _searcher->MoveToCurrentSelection();
            }

            auto current = _searcher->GetCurrent();
            if (current)
            {
                _terminal->SelectNewRegion(current->start, current->start);
                _terminal->ToggleMarkMode();
            }
        }
        else
        {
            if (_searcher->ResetIfStaleRegex(*_terminal, searchString, moveForward, true))
            {
                _searcher->HighlightResults();
                _searcher->MoveToCurrentSelection();
                auto results = _searcher->Results();
                if (!results.empty())
                {
                    if (moveForward)
                    {
                        auto current = results[results.size() - 1];
                        _terminal->SelectNewRegion(current.start, current.start);
                        _terminal->ToggleMarkMode();
                    }
                    else
                    {
                        auto current = results[0];
                        _terminal->SelectNewRegion(current.start, current.start);
                        _terminal->ToggleMarkMode();
                    }
                }
            }
            else
            {
                _searcher->MoveToCurrentSelection();
                _searcher->FindNext();
                auto current = _searcher->GetCurrent();
                if (current)
                {
                    _terminal->SelectNewRegion(current->start, current->start);
                    _terminal->ToggleMarkMode();
                }
            }
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

    if (vkey == L'F' && mods.IsShiftPressed() && mods.IsCtrlPressed())
    {
        _controlCore->StartFuzzySearch(L"");
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

    if (vkey >= 0x30 && vkey <= 0x39 && !mods.IsShiftPressed() && _vimMode != VimMode::search && _textObject != VimTextObjectType::findChar && _textObject != VimTextObjectType::findCharReverse && _textObject != VimTextObjectType::tilChar && _textObject != VimTextObjectType::tilCharReverse)
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
    else if (vkey == VK_ESCAPE && _showRowNumbers)
    {
        _action = VimActionType::toggleRowNumbersOff;
        sequenceCompleted = true;
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
                _searchString += vkeyText;
            }
        }
    }
    else if (vkey == L' ')
    {
        if (_leaderSequence)
        {
            _action = VimActionType::toggleRowNumbersOn;
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

        sequenceCompleted = true;
    }
    // / ?
    else if (vkey == 0xBF)
    {
        if (mods.IsShiftPressed())
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
        hideMarkers = true;

        _action = VimActionType::search;
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
        _action = VimActionType::search;
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
    // []
    else if ((vkey == VK_OEM_4 || vkey == VK_OEM_6))
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

    if (vkey != VK_RETURN && _vimMode != VimMode::search && vkey != L'\r')
    {
        _sequenceText += vkeyText;
    }

    auto shouldExit = false;

    if (sequenceCompleted && !skipExecute)
    {
        shouldExit = _executeVimSelection(_action, _textObject, _times, _motion, _vimMode == VimMode::visual, _searchString, vkeyText);
    }

    std::wstring statusBarSearchString;

    if (_vimMode != VimMode::search && _searchString.empty())
    {
        statusBarSearchString = L"";
    }
    else if (_searchString.empty())
    {
        statusBarSearchString = _reverseSearch ? L"?" : L"/";
    }
    else
    {
        statusBarSearchString = _reverseSearch ? L"?" + _searchString : L"/" + _searchString;
    }

    if (_sequenceText != L"\r")
    {
        auto modeText = _vimMode == VimMode::search ? L"Search" : _vimMode == VimMode::normal ? L"Normal" :
                                                                                                L"VisualLine";
        _controlCore->UpdateVimText(modeText, statusBarSearchString, _sequenceText);
    }

    wcsncpy_s(_lastVkey, vkeyText, sizeof(_lastVkey) / sizeof(_lastVkey[0]));

    if (sequenceCompleted)
    {
        if (_action == VimActionType::yank)
        {
            _vimMode = VimMode::normal;
        }

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

        _sequenceText = L"";
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

int32_t VimModeProxy::ViewportRowToHighlight()
{
    const auto offset = _terminal->GetScrollOffset();
    return _terminal->GetSelectionEnd().y - offset;
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

void VimModeProxy::ResetVimState()
{
    const auto lock = _terminal->LockForWriting();
    _vimMode = VimMode::none;
    _lastTextObject = VimTextObjectType::none;
    _lastAction = VimActionType::none;
    _lastMotion = VimMotionType::none;
    _textObject = VimTextObjectType::none;
    _lastTimes = 0;
    _sequenceText = L"";
    _searchString = L"";
    _lastVkey[0] = L'\0';
    _terminal->ClearYankRegion();
    _terminal->ClearSelection();
    _controlCore->UpdateSelectionFromVim();
    _controlCore->ExitVim();
}

VimModeProxy::VimMode VimModeProxy::_getVimMode()
{
    return _vimMode;
}

void VimModeProxy::ExitVimMode()
{
    _vimMode = VimMode::none;
    if (_showRowNumbers)
    {
        _showRowNumbers = false;
        _controlCore->ToggleRowNumbers(false);
    }
}

void VimModeProxy::EnterVimMode()
{
    _vimMode = VimMode::normal;
    ResetVimModeForSizeChange(true);
    _controlCore->UpdateVimText(L"Normal", _searchString, _sequenceText);
}

bool VimModeProxy::IsInVimMode()
{
    return _getVimMode() != VimModeProxy::VimMode::none;
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
            selectLastChar = true;
        }

        if (selectLastChar)
        {
            _terminal->SelectLastChar();
        }

        _vimScrollScreenPosition(VimTextObjectType::centerOfScreen);
        _controlCore->UpdateSelectionFromVim();
    }
}

void VimModeProxy::SelectRow(int32_t row, int32_t col)
{
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
