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
    Microsoft::Console::Render::Renderer* renderer)
{
    _quickSelectAlphabet->Enabled(true);
    _copyMode = copyMode;
    searcher.QuickSelectRegex(*_terminal, text, true);
    searcher.HighlightResults();
    renderer->TriggerSelection();
}

bool QuickSelectHandler::Enabled()
{
    return _quickSelectAlphabet->Enabled();
}

void QuickSelectHandler::HandleChar(uint32_t vkey, bool isShiftPressed, Microsoft::Console::Render::Renderer* renderer, winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection& connection)
{
    if (vkey == VK_ESCAPE)
    {
        _quickSelectAlphabet->Enabled(false);
        _quickSelectAlphabet->ClearChars();
        _terminal->ClearSelection();
        renderer->TriggerSelection();
        return;
    }

    if (vkey == VK_BACK)
    {
        _quickSelectAlphabet->RemoveChar();
        renderer->TriggerSelection();
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

            if (isShiftPressed)
            {
                _quickSelectAlphabet->Enabled(false);
                _quickSelectAlphabet->ClearChars();
                const auto req = TextBuffer::CopyRequest::FromConfig(_terminal->GetTextBuffer(), startPoint, endPoint, true, false, false);
                const auto text = _terminal->GetTextBuffer().GetPlainText(req);
                connection.WriteInput(text);
                _terminal->ClearSelection();
                renderer->TriggerSelection();
                renderer->TriggerRedrawAll();
                renderer->NotifyPaintFrame();
            }
            else if (!_copyMode)
            {
                _quickSelectAlphabet->Enabled(false);
                _quickSelectAlphabet->ClearChars();
                _terminal->ClearSelection();
                _vimProxy->EnterVimMode();
                _terminal->SelectChar(startPoint);
                renderer->TriggerSelection();
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
                        _quickSelectAlphabet->Enabled(false);
                        _quickSelectAlphabet->ClearChars();
                        _terminal->ClearSelection();
                        //This isn't technically safe.  There is a slight chance that the renderer is deleted
                        //I think the fix is to make the renderer a shared pointer but I am not ready to mess with change core terminal stuff
                        renderer->TriggerSelection();
                        renderer->TriggerRedrawAll();
                        renderer->NotifyPaintFrame();
                    }
                });
                hideTimerThread.detach();
            }
        }
    }
    renderer->TriggerRedrawAll();
    renderer->NotifyPaintFrame();
}
