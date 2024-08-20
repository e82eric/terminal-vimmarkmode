#pragma once

#include "../../buffer/out/textBuffer.hpp"
#include "../../renderer/inc/IRenderData.hpp"
#include "../../types/inc/Viewport.hpp"
#include <til/ticket_lock.h>

class FuzzySearchRenderData : public Microsoft::Console::Render::IRenderData
{
public:
    FuzzySearchRenderData(IRenderData* pData);
    void SetSize(til::size size);
    void SetTopRow(til::CoordType row);
    void ResetTopRow();
    Microsoft::Console::Types::Viewport GetViewport() noexcept override;
    til::point GetTextBufferEndPosition() const noexcept override;
    const FontInfo& GetFontInfo() const noexcept override;
    std::span<const til::point_span> GetYankSelectionRects() noexcept override;
    void SelectYankRegion() override;
    void ClearYankRegion() override;
    bool InQuickSelectMode() override;
    Microsoft::Console::Render::QuickSelectState GetQuickSelectState() noexcept override;
    std::span<const til::point_span> GetSelectionSpans() const noexcept override;
    [[nodiscard]] std::unique_lock<til::recursive_ticket_lock> LockForReading() const noexcept;
    [[nodiscard]] std::unique_lock<til::recursive_ticket_lock> LockForWriting() noexcept;
    void LockConsole() noexcept override;
    void UnlockConsole() noexcept override;
    std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& attr) const noexcept override;
    til::point GetCursorPosition() const noexcept override;
    bool IsCursorVisible() const noexcept override;
    bool IsCursorOn() const noexcept override;
    ULONG GetCursorHeight() const noexcept override;
    CursorType GetCursorStyle() const noexcept override;
    ULONG GetCursorPixelWidth() const noexcept override;
    bool IsCursorDoubleWidth() const override;
    const bool IsGridLineDrawingAllowed() noexcept override;
    const std::wstring_view GetConsoleTitle() const noexcept override;
    const bool IsSelectionActive() const override;
    const bool IsBlockSelection() const noexcept override;
    void ClearSelection() override;
    void SelectNewRegion(const til::point /*coordStart*/, const til::point /*coordEnd*/) override;
    const til::point GetSelectionAnchor() const noexcept;
    const til::point GetSelectionEnd() const noexcept;
    const bool IsUiaDataInitialized() const noexcept;
    const std::wstring GetHyperlinkUri(uint16_t /*id*/) const;
    const std::wstring GetHyperlinkCustomId(uint16_t /*id*/) const;
    const std::vector<size_t> GetPatternId(const til::point /*location*/) const;
    TextBuffer& GetTextBuffer(void) const noexcept override;
    std::span<const til::point_span> GetSearchHighlights() const noexcept override;
    const til::point_span* GetSearchHighlightFocused(void) const noexcept override;

private:
    IRenderData* _pData;
    Microsoft::Console::Types::Viewport _viewPort;
    til::size _size;
    til::CoordType _row;
    til::recursive_ticket_lock _readWriteLock;
    mutable std::vector<til::point_span> _lastSelectionSpans;
};
