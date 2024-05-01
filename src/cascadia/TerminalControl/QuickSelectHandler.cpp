#include "pch.h"
#include "QuickSelectHandler.h"
#include "VimModeProxy.h"

class VimModeProxy;

QuickSelectHandler::QuickSelectHandler(
        std::shared_ptr<Microsoft::Terminal::Core::Terminal> terminal,
        std::shared_ptr<VimModeProxy> vimProxy,
        std::shared_ptr<QuickSelectAlphabet> quickSelectAlphabet)
{
    _terminal = terminal;
    _vimProxy = vimProxy;
    _quickSelectAlphabet = quickSelectAlphabet;
}

void QuickSelectHandler::EnterQuickSelectMode(
    std::wstring_view text,
    bool copyMode,
    Search& searcher,
    Microsoft::Console::Render::Renderer* renderer,
    winrt::Microsoft::Terminal::Control::implementation::ControlCore *controlCore)
{
    _vimProxy->ExitVimMode();
    _quickSelectAlphabet->Enabled(true);
    _copyMode = copyMode;
    searcher.QuickSelectRegex(*_terminal, text, true);

    _terminal->SetSearchHighlights({});
    _terminal->SetQuickSelectHighlights(searcher.Results());
    renderer->TriggerRedrawAll();
    controlCore->UpdateBar();
    _terminal->GetTextBuffer().GetCursor().SetIsVisible(false);
    //controlCore->UpdateSelectionFromVim();
}

bool QuickSelectHandler::Enabled()
{
    return _quickSelectAlphabet->Enabled();
}

void QuickSelectHandler::HandleChar(
    uint32_t vkey,
    const ::Microsoft::Terminal::Core::ControlKeyStates mods,
    Microsoft::Console::Render::Renderer* renderer,
    winrt::Microsoft::Terminal::Control::implementation::ControlCore *controlCore)
{
    if (vkey == VK_ESCAPE)
    {
        _quickSelectAlphabet->Enabled(false);
        _quickSelectAlphabet->ClearChars();
        _terminal->SetQuickSelectHighlights({});
        renderer->TriggerRedrawAll();
        controlCore->UpdateBar();
        _terminal->GetTextBuffer().GetCursor().SetIsVisible(true);
        return;
    }

    if (vkey == VK_BACK)
    {
        _quickSelectAlphabet->RemoveChar();
        renderer->TriggerRedrawAll();
        controlCore->UpdateBar();
        return;
    }

    wchar_t vkeyText[2] = { 0 };
    BYTE keyboardState[256];
    if (!GetKeyboardState(keyboardState))
    {
        return;
    }

    keyboardState[VK_SHIFT] = 0x80;
    keyboardState[VK_CONTROL] = 0x00;
    ToUnicode(vkey, MapVirtualKey(vkey, MAPVK_VK_TO_VSC), keyboardState, vkeyText, 2, 0);

    _quickSelectAlphabet->AppendChar(vkeyText);

    if (_quickSelectAlphabet->AllCharsSet(_terminal->NumberOfVisibleSearchSelections()))
    {
        const auto index = _quickSelectAlphabet->GetIndexForChars();
        const auto quickSelectResult = _terminal->GetViewportSelectionAtIndex(index);
        if (quickSelectResult.has_value())
        {
            const auto startPoint = std::get<0>(quickSelectResult.value());
            const auto endPoint = std::get<1>(quickSelectResult.value());

            if (mods.IsShiftPressed())
            {
                const auto req = TextBuffer::CopyRequest::FromConfig(_terminal->GetTextBuffer(), startPoint, endPoint, true, false, false);
                const auto text = _terminal->GetTextBuffer().GetPlainText(req);
                controlCore->UserScrollViewport(_terminal->GetTextBuffer().GetLastNonSpaceCharacter().y);
                _exitQuickSelectMode(renderer);
                const auto suspension = _terminal->SuspendLock();
                controlCore->SendInput(winrt::hstring{ text });
            }
            else if (!_copyMode && !mods.IsCtrlPressed())
            {
                _exitQuickSelectMode(renderer);
                _terminal->SelectChar(startPoint);
                _vimProxy->EnterVimMode(false);
                renderer->TriggerRedrawAll();
            }
            else
            {
                const auto req = TextBuffer::CopyRequest::FromConfig(_terminal->GetTextBuffer(), startPoint, endPoint, true, false, false);
                auto text = _terminal->GetTextBuffer().GetPlainText(req);
                _terminal->CopyToClipboard(text);

                std::thread hideTimerThread([this, renderer]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(250));
                    {
                        auto lock = _terminal->LockForWriting();
                        //This isn't technically safe.  There is a slight chance that the renderer is deleted
                        //I think the fix is to make the renderer a shared pointer but I am not ready to mess with change core terminal stuff
                        _exitQuickSelectMode(renderer);
                    }
                });
                hideTimerThread.detach();
            }
        }
        else
        {
            _exitQuickSelectMode(renderer);
        }
    }
    renderer->TriggerRedrawAll();
    renderer->NotifyPaintFrame();
    controlCore->UpdateBar();
}

void QuickSelectHandler::_exitQuickSelectMode(Microsoft::Console::Render::Renderer* renderer) const
{
    _quickSelectAlphabet->Enabled(false);
    _quickSelectAlphabet->ClearChars();
    _terminal->SetQuickSelectHighlights({});
    renderer->TriggerRedrawAll();
    renderer->NotifyPaintFrame();
    _terminal->GetTextBuffer().GetCursor().SetIsVisible(true);
}
