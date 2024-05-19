

#pragma once

#include "FuzzySearchBoxControl.g.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include <ControlSettings.h>
#include "../../renderer/inc/FontInfoDesired.hpp"

namespace Microsoft::Console::Render::Atlas
{
    class AtlasEngine;
}

class FuzzySearchRenderData;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct FuzzySearchBoxControl : FuzzySearchBoxControlT<FuzzySearchBoxControl>
    {
        FuzzySearchBoxControl(Control::IControlSettings settings, Control::IControlAppearance unfocusedAppearance );
        FuzzySearchBoxControl(
            Control::IControlSettings settings,
            Control::IControlAppearance unfocusedAppearance,
            std::shared_ptr<::Microsoft::Terminal::Core::Terminal> terminal);
        ~FuzzySearchBoxControl() override;

        void SearchString(const winrt::hstring searchString);
        void SetFontSize(int32_t width, int32_t height);
        void SetSearchResult(FuzzySearchResult val);
        void Show(std::wstring_view searchString);

        void _OnListBoxSelectionChanged(winrt::Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);
        void _TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _SwapChainSizeChanged(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::SizeChangedEventArgs& e);
        void _SwapChainScaleChanged(const Windows::UI::Xaml::Controls::SwapChainPanel& sender, const Windows::Foundation::IInspectable& /*args*/);
        WINRT_CALLBACK(Search, FuzzySearchHandler);
        TYPED_EVENT(Closed, Control::FuzzySearchBoxControl, Windows::UI::Xaml::RoutedEventArgs);
        TYPED_EVENT(OnReturn, Control::FuzzySearchBoxControl, winrt::Microsoft::Terminal::Control::FuzzySearchTextLine);

        private:
        bool _initialize(const float actualWidth, const float actualHeight, const float compositionScale);
        void _updateSettings(const Control::IControlSettings& settings, const IControlAppearance& newAppearance);
        void _sizeOrScaleChanged(const float width, const float height, const float scale);
        void _scaleChanged(const float scale);
        void _setSwapChainHandle(HANDLE swapChainHandle);
        void _selectFirstItem();
        void _setStatus(int32_t totalRowsSearched, int32_t numberOfResults);
        void _close();
        void _updateFont();
        bool _isBackgroundTransparent();
        void _updateAntiAliasingMode();
        bool _setFontSizeUnderLock(float fontSize);
        void _refreshSizeUnderLock();
        void attach_dxgi_swap_chain_to_xaml(HANDLE swapChainHandle);
        winrt::fire_and_forget _renderEngineSwapChainChanged(const HANDLE sourceHandle);
        winrt::handle _lastSwapChainHandle{ nullptr };
        til::point _toPosInDips(const Core::Point terminalCellPos);
        std::unordered_set<winrt::Windows::Foundation::IInspectable> _focusableElements;
        til::size _fontSize;
        winrt::Windows::System::DispatcherQueue _dispatcher{ nullptr };
        winrt::com_ptr<ControlSettings> _settings{ nullptr };
        std::shared_ptr<::Microsoft::Terminal::Core::Terminal> _terminal;
        std::shared_ptr<FuzzySearchRenderData> _fuzzySearchRenderData{ nullptr };
        std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer{ nullptr };
        bool _builtinGlyphs = true;
        bool _colorGlyphs = true;
        CSSLengthPercentage _cellWidth;
        CSSLengthPercentage _cellHeight;
        FontInfoDesired _desiredFont;
        FontInfo _actualFont;
        bool _initialized = false;
        std::unique_ptr<::Microsoft::Console::Render::Atlas::AtlasEngine> _renderEngine{ nullptr };
        float _panelWidth{ 0 };
        float _panelHeight{ 0 };
        float _compositionScale{ 0 };
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(FuzzySearchBoxControl);
}
