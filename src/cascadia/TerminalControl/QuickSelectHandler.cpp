#include "pch.h"
#include "QuickSelectHandler.h"
#include "VimModeProxy.h"

class VimModeProxy;

QuickSelectHandler::
QuickSelectHandler(
    Microsoft::Terminal::Core::Terminal* terminal,
    VimModeProxy *vimProxy,
    Microsoft::Console::Render::Renderer *renderer,
    QuickSelectAlphabet *quickSelectAlphabet)
{
    _terminal = terminal;
    _vimProxy = vimProxy;
    _renderer = renderer;
    _quickSelectAlphabet = quickSelectAlphabet;
}

void QuickSelectHandler::EnterQuickSelectMode(bool copyMode)
{
    _quickSelectAlphabet->Enabled(true);
    _copyMode = copyMode;
}

bool QuickSelectHandler::Enabled()
{
    return _quickSelectAlphabet->Enabled();
}

void QuickSelectHandler::HandleChar(uint32_t vkey)
{
    if (vkey == VK_ESCAPE)
    {
        _quickSelectAlphabet->Enabled(false);
        _quickSelectAlphabet->ClearChars();
        _terminal->ClearSelection();
        _renderer->TriggerSelection();
        return;
    }

    if (vkey == VK_BACK)
    {
        _quickSelectAlphabet->RemoveChar();
        _renderer->TriggerSelection();
        return;
    }

    wchar_t vkeyText[2] = { 0 };
    BYTE keyboardState[256];
    if (!GetKeyboardState(keyboardState))
    {
        return;
    }

    keyboardState[VK_SHIFT] = 0x80;
    ToUnicode(vkey, MapVirtualKey(vkey, MAPVK_VK_TO_VSC), keyboardState, vkeyText, 2, 0);

    _quickSelectAlphabet->AppendChar(vkeyText);

    if (_quickSelectAlphabet->AllCharsSet(_terminal->NumberOfVisibleSearchSelections()))
    {
        auto index = _quickSelectAlphabet->GetIndexForChars();
        auto quickSelectResult = _terminal->GetViewportSelectionAtIndex(index);
        if (quickSelectResult.has_value())
        {
            auto startPoint = std::get<0>(quickSelectResult.value());
            auto endPoint = std::get<1>(quickSelectResult.value());

            if (!_copyMode)
            {
                _quickSelectAlphabet->Enabled(false);
                _quickSelectAlphabet->ClearChars();
                _terminal->ClearSelection();
                _vimProxy->EnterVimMode();
                _terminal->SelectChar(startPoint);
                _renderer->TriggerSelection();
            }
            else
            {
                const auto req = TextBuffer::CopyRequest::FromConfig(_terminal->GetTextBuffer(), startPoint, endPoint, true, false, false);
                auto text = _terminal->GetTextBuffer().GetPlainText(req);
                _terminal->CopyToClipboard(text);

                std::thread hideTimerThread([this]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                    {
                        auto lock = _terminal->LockForWriting();
                        _quickSelectAlphabet->Enabled(false);
                        _quickSelectAlphabet->ClearChars();
                        _terminal->ClearSelection();
                        _renderer->TriggerSelection();
                        _renderer->TriggerRedrawAll();
                        _renderer->NotifyPaintFrame();
                    }
                });
                hideTimerThread.detach();
            }
        }
    }
    _renderer->TriggerRedrawAll();
    _renderer->NotifyPaintFrame();
}
