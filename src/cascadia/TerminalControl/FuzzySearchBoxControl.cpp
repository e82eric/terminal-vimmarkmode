// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "FuzzySearchBoxControl.h"
#include "FuzzySearchBoxControl.g.cpp"
#include <LibraryResources.h>
#include "../../cascadia/TerminalCore/FuzzySearchRenderData.hpp"
#include "../../renderer/atlas/AtlasEngine.h"
#include "../../renderer/base/renderer.hpp"
using namespace winrt::Windows::UI::Xaml::Media;

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    DependencyProperty FuzzySearchBoxControl::_borderColorProperty =
        DependencyProperty::Register(
            L"BorderColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchBoxControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty FuzzySearchBoxControl::_headerTextColorProperty =
        DependencyProperty::Register(
            L"HeaderTextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchBoxControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty FuzzySearchBoxControl::_BackgroundColorProperty =
        DependencyProperty::Register(
            L"BackgroundColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchBoxControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty FuzzySearchBoxControl::_SelectedItemColorProperty =
        DependencyProperty::Register(
            L"SelectedItemColor",
            xaml_typename<Windows::UI::Color>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchBoxControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty FuzzySearchBoxControl::_InnerBorderThicknessProperty =
        DependencyProperty::Register(
            L"BorderThickness",
            xaml_typename<Thickness>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchBoxControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty FuzzySearchBoxControl::_TextColorProperty =
        DependencyProperty::Register(
            L"TextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchBoxControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty FuzzySearchBoxControl::_HighlightedTextColorProperty =
        DependencyProperty::Register(
            L"HighlightedTextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchBoxControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty FuzzySearchBoxControl::BackgroundColorProperty()
    {
        return _BackgroundColorProperty;
    }

    Brush FuzzySearchBoxControl::BackgroundColor()
    {
        return GetValue(_BackgroundColorProperty).as<Brush>();
    }

    void FuzzySearchBoxControl::BackgroundColor(Brush const& value)
    {
        if (value != BackgroundColor())
        {
            SetValue(_BackgroundColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"BackgroundColor" });
        }
    }

    DependencyProperty FuzzySearchBoxControl::SelectedItemColorProperty()
    {
        return _SelectedItemColorProperty;
    }

    Windows::UI::Color FuzzySearchBoxControl::SelectedItemColor()
    {
        return GetValue(_SelectedItemColorProperty).as<Windows::UI::Color>();
    }

    void FuzzySearchBoxControl::SelectedItemColor(Windows::UI::Color const& value)
    {
        if (value != SelectedItemColor())
        {
            SetValue(_SelectedItemColorProperty, box_value(value));
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"SelectedItemColor" });
        }
    }

    DependencyProperty FuzzySearchBoxControl::BorderColorProperty()
    {
        return _borderColorProperty;
    }

    Brush FuzzySearchBoxControl::BorderColor()
    {
        return GetValue(_borderColorProperty).as<Brush>();
    }

    void FuzzySearchBoxControl::BorderColor(Brush const& value)
    {
        if (value != BorderColor())
        {
            SetValue(_borderColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"BorderColor" });
        }
    }

    DependencyProperty FuzzySearchBoxControl::HeaderTextColorProperty()
    {
        return _headerTextColorProperty;
    }

    Brush FuzzySearchBoxControl::HeaderTextColor()
    {
        return GetValue(_headerTextColorProperty).as<Brush>();
    }

    void FuzzySearchBoxControl::HeaderTextColor(Brush const& value)
    {
        if (value != HeaderTextColor())
        {
            SetValue(_headerTextColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"HeaderTextColor" });
        }
    }

    DependencyProperty FuzzySearchBoxControl::InnerBorderThicknessProperty()
    {
        return _InnerBorderThicknessProperty;
    }

    Thickness FuzzySearchBoxControl::InnerBorderThickness()
    {
        return GetValue(_InnerBorderThicknessProperty).as<Thickness>();
    }

    void FuzzySearchBoxControl::InnerBorderThickness(Thickness const& value)
    {
        if (value != InnerBorderThickness())
        {
            SetValue(_InnerBorderThicknessProperty, box_value(value));
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"InnerBorderThickness" });
        }
    }

    Brush FuzzySearchBoxControl::TextColor()
    {
        return GetValue(_TextColorProperty).as<Brush>();
    }

    void FuzzySearchBoxControl::TextColor(Brush const& value)
    {
        if (value != TextColor())
        {
            SetValue(_TextColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"TextColor" });
        }
    }

    DependencyProperty FuzzySearchBoxControl::TextColorProperty()
    {
        return _TextColorProperty;
    }

    Brush FuzzySearchBoxControl::HighlightedTextColor()
    {
        return GetValue(_HighlightedTextColorProperty).as<Brush>();
    }

    void FuzzySearchBoxControl::HighlightedTextColor(Brush const& value)
    {
        if (value != HighlightedTextColor())
        {
            SetValue(_HighlightedTextColorProperty, value);
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"HighlightedTextColor" });
        }
    }

    DependencyProperty FuzzySearchBoxControl::HighlightedTextColorProperty()
    {
        return _HighlightedTextColorProperty;
    }

    FuzzySearchBoxControl::FuzzySearchBoxControl(Control::IControlSettings settings, Control::IControlAppearance unfocusedAppearance):
        FuzzySearchBoxControl(settings, unfocusedAppearance, std::make_shared<::Microsoft::Terminal::Core::Terminal>())
    {
    }

    FuzzySearchBoxControl::FuzzySearchBoxControl(
        IControlSettings settings,
        IControlAppearance unfocusedAppearance,
        std::shared_ptr<::Microsoft::Terminal::Core::Terminal> terminal) :
        _desiredFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, DEFAULT_FONT_SIZE, CP_UTF8 },
        _actualFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false }
    {
        InitializeComponent();
        _dispatcher = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        _settings = winrt::make_self<implementation::ControlSettings>(settings, unfocusedAppearance);
        _terminal = terminal;
        const auto lock = _terminal->LockForWriting();

        {
            auto renderThread = std::make_unique<::Microsoft::Console::Render::RenderThread>();
            auto* const localPointerToThread = renderThread.get();

            const auto& renderSettings = _terminal->GetRenderSettings();
            _fuzzySearchRenderData = std::make_unique<FuzzySearchRenderData>(_terminal.get());

            _renderer = std::make_unique<::Microsoft::Console::Render::Renderer>(renderSettings, _fuzzySearchRenderData.get(), nullptr, 0, std::move(renderThread));

            //_renderer->SetRendererEnteredErrorStateCallback([this]() { RendererEnteredErrorState.raise(nullptr, nullptr); });
            THROW_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));
        }

        _updateSettings(settings, unfocusedAppearance);

        FuzzySearchTextBox().KeyUp([this](const IInspectable& sender, Input::KeyRoutedEventArgs const& e) {
            auto textBox{ sender.try_as<Controls::TextBox>() };

            if (ListBox() != nullptr)
            {
                if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Down || e.OriginalKey() == winrt::Windows::System::VirtualKey::Up)
                {
                    auto selectedIndex = ListBox().SelectedIndex();

                    if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Down)
                    {
                        selectedIndex++;
                    }
                    else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Up)
                    {
                        selectedIndex--;
                    }

                    if (selectedIndex >= 0 && selectedIndex < static_cast<int32_t>(ListBox().Items().Size()))
                    {
                        ListBox().SelectedIndex(selectedIndex);
                        ListBox().ScrollIntoView(ListBox().SelectedItem());
                    }

                    e.Handled(true);
                }
            }
            else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
            {
                auto selectedItem = ListBox().SelectedItem();
                if (selectedItem)
                {
                    auto castedItem = selectedItem.try_as<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>();
                    if (castedItem)
                    {
                        _OnReturnHandlers(*this, castedItem);
                        e.Handled(true);
                    }
                }
            }
        });
    }

    static ::Microsoft::Console::Render::Atlas::GraphicsAPI parseGraphicsAPI(GraphicsAPI api) noexcept
    {
        using GA = ::Microsoft::Console::Render::Atlas::GraphicsAPI;
        switch (api)
        {
        case GraphicsAPI::Direct2D:
            return GA::Direct2D;
        case GraphicsAPI::Direct3D11:
            return GA::Direct3D11;
        default:
            return GA::Automatic;
        }
    }

    bool FuzzySearchBoxControl::_isBackgroundTransparent()
    {
        // If we're:
        // * Not fully opaque
        // * rendering on top of an image
        //
        // then the renderer should not render "default background" text with a
        // fully opaque background. Doing that would cover up our nice
        // transparency, or our acrylic, or our image.
        return Opacity() < 1.0f || !_settings->BackgroundImage().empty() || _settings->UseBackgroundImageForWindow();
    }

    winrt::fire_and_forget FuzzySearchBoxControl::_renderEngineSwapChainChanged(const HANDLE sourceHandle)
    {
        // `sourceHandle` is a weak ref to a HANDLE that's ultimately owned by the
        // render engine's own unique_handle. We'll add another ref to it here.
        // This will make sure that we always have a valid HANDLE to give to
        // callers of our own SwapChainHandle method, even if the renderer is
        // currently in the process of discarding this value and creating a new
        // one. Callers should have already set up the SwapChainChanged
        // callback, so this all works out.

        winrt::handle duplicatedHandle;
        const auto processHandle = GetCurrentProcess();
        THROW_IF_WIN32_BOOL_FALSE(DuplicateHandle(processHandle, sourceHandle, processHandle, duplicatedHandle.put(), 0, FALSE, DUPLICATE_SAME_ACCESS));

        const auto weakThis{ get_weak() };

        // Concurrent read of _dispatcher is safe, because Detach() calls WaitForPaintCompletionAndDisable()
        // which blocks until this call returns. _dispatcher will only be changed afterwards.
        co_await wil::resume_foreground(_dispatcher);

        if (auto core{ weakThis.get() })
        {
            // `this` is safe to use now

            _lastSwapChainHandle = std::move(duplicatedHandle);
            // Now bubble the event up to the control.
            attach_dxgi_swap_chain_to_xaml(_lastSwapChainHandle.get());
        }
    }

    void FuzzySearchBoxControl::attach_dxgi_swap_chain_to_xaml(HANDLE swapChainHandle)
    {
        auto nativePanel = FuzzySearchSwapChainPanel().as<ISwapChainPanelNative2>();
        nativePanel->SetSwapChainHandle(swapChainHandle);
    }

    void FuzzySearchBoxControl::_SwapChainScaleChanged(const Windows::UI::Xaml::Controls::SwapChainPanel& sender,
                                             const Windows::Foundation::IInspectable& /*args*/)
    {
        const auto scaleX = sender.CompositionScaleX();

        _scaleChanged(scaleX);
    }

    void FuzzySearchBoxControl::_scaleChanged(const float scale)
    {
        _panelWidth = static_cast<float>(FuzzySearchSwapChainPanel().ActualWidth());
        _panelHeight = static_cast<float>(FuzzySearchSwapChainPanel().ActualHeight());
        _compositionScale = FuzzySearchSwapChainPanel().CompositionScaleX();

        if (!_renderEngine)
        {
            return;
        }
        _sizeOrScaleChanged(_panelWidth, _panelHeight, scale);
    }

    void FuzzySearchBoxControl::_SwapChainSizeChanged(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                            const SizeChangedEventArgs& e)
    {
        _panelWidth = e.NewSize().Width;
        _panelHeight = e.NewSize().Height;
        _compositionScale = FuzzySearchSwapChainPanel().CompositionScaleX();

        if (!_initialized)
        {
            _initialize(_panelWidth, _panelHeight, _compositionScale);
        }

        const auto newSize = e.NewSize();
        _sizeOrScaleChanged(newSize.Width, newSize.Height, _compositionScale);
    }

    bool FuzzySearchBoxControl::_initialize(const float actualWidth,
                                 const float actualHeight,
                                 const float compositionScale)
    {
        assert(_settings);

        _panelWidth = actualWidth;
        _panelHeight = actualHeight;
        _compositionScale = compositionScale;

        { // scope for terminalLock
            const auto lock = _terminal->LockForWriting();

            //if (_initializedTerminal.load(std::memory_order_relaxed))
            //{
            //    return false;
            //}

            const auto windowWidth = actualWidth * compositionScale;
            const auto windowHeight = actualHeight * compositionScale;

            if (windowWidth == 0 || windowHeight == 0)
            {
                return false;
            }

            _renderEngine = std::make_unique<::Microsoft::Console::Render::AtlasEngine>();
            _renderer->AddRenderEngine(_renderEngine.get());

            // Hook up the warnings callback as early as possible so that we catch everything.
            //_renderEngine->SetWarningCallback([this](HRESULT hr, wil::zwstring_view parameter) {
            //    _rendererWarning(hr, parameter);
            //});

            // Initialize our font with the renderer
            // We don't have to care about DPI. We'll get a change message immediately if it's not 96
            // and react accordingly.
            _updateFont();

            const til::size windowSize{ til::math::rounding, windowWidth, windowHeight };

            // First set up the dx engine with the window size in pixels.
            // Then, using the font, get the number of characters that can fit.
            // Resize our terminal connection to match that size, and initialize the terminal with that size.
            const auto viewInPixels = ::Microsoft::Console::Types::Viewport::FromDimensions({ 0, 0 }, windowSize);
            LOG_IF_FAILED(_renderEngine->SetWindowSize({ viewInPixels.Width(), viewInPixels.Height() }));

            const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);
            const auto width = vp.Width();
            const auto height = vp.Height();

            // Override the default width and height to match the size of the swapChainPanel
            _settings->InitialCols(width);
            _settings->InitialRows(height);

            // Tell the render engine to notify us when the swap chain changes.
            // We do this after we initially set the swapchain so as to avoid
            // unnecessary callbacks (and locking problems)
            _renderEngine->SetCallback([this](HANDLE handle) {
                _renderEngineSwapChainChanged(handle);
            });

            _renderEngine->SetRetroTerminalEffect(_settings->RetroTerminalEffect());
            _renderEngine->SetPixelShaderPath(_settings->PixelShaderPath());
            _renderEngine->SetPixelShaderImagePath(_settings->PixelShaderImagePath());
            _renderEngine->SetGraphicsAPI(parseGraphicsAPI(_settings->GraphicsAPI()));
            _renderEngine->SetDisablePartialInvalidation(_settings->DisablePartialInvalidation());
            _renderEngine->SetSoftwareRendering(_settings->SoftwareRendering());

            _updateAntiAliasingMode();

            // GH#5098: Inform the engine of the opacity of the default text background.
            // GH#11315: Always do this, even if they don't have acrylic on.
            _renderEngine->EnableTransparentBackground(_isBackgroundTransparent());

            //_initializedTerminal.store(true, std::memory_order_relaxed);

            auto cx = gsl::narrow_cast<til::CoordType>(lrint(_panelWidth * _compositionScale));
            auto cy = gsl::narrow_cast<til::CoordType>(lrint(_panelHeight * _compositionScale));

            cx = std::max(cx, _actualFont.GetSize().width);
            cy = std::max(cy, _actualFont.GetSize().height);

            const auto viewInPixels2 = ::Microsoft::Console::Types::Viewport::FromDimensions({ 0, 0 }, { cx, cy });
            const auto vp2 = _renderEngine->GetViewportInCharacters(viewInPixels2);
            _fuzzySearchRenderData->SetSize(vp2.Dimensions());
            _renderer->EnablePainting();
            _initialized = true;
        } // scope for TerminalLock

        return true;
    }

    void FuzzySearchBoxControl::_sizeOrScaleChanged(const float width,
                                         const float height,
                                         const float scale)
    {
        const auto scaleChanged = _compositionScale != scale;
        // _refreshSizeUnderLock redraws the entire terminal.
        // Don't call it if we don't have to.
        //if (_panelWidth == width && _panelHeight == height && !scaleChanged)
        //{
        //    return;
        //}

        _panelWidth = width;
        _panelHeight = height;
        _compositionScale = scale;

        const auto lock = _terminal->LockForWriting();
        if (scaleChanged)
        {
            // _updateFont relies on the new _compositionScale set above
            _updateFont();
        }
        _refreshSizeUnderLock();

        auto cx = gsl::narrow_cast<til::CoordType>(lrint(_panelWidth * _compositionScale));
        auto cy = gsl::narrow_cast<til::CoordType>(lrint(_panelHeight * _compositionScale));

        cx = std::max(cx, _actualFont.GetSize().width);
        cy = std::max(cy, _actualFont.GetSize().height);

        const auto viewInPixels = ::Microsoft::Console::Types::Viewport::FromDimensions({ 0, 0 }, { cx, cy });
        const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);
        _fuzzySearchRenderData->SetSize(vp.Dimensions());
    }

    void FuzzySearchBoxControl::_updateSettings(const Control::IControlSettings& settings, const IControlAppearance& newAppearance)
    {
        _settings = winrt::make_self<implementation::ControlSettings>(settings, newAppearance);

        const auto lock = _terminal->LockForWriting();

        _builtinGlyphs = _settings->EnableBuiltinGlyphs();
        _colorGlyphs = _settings->EnableColorGlyphs();
        _cellWidth = CSSLengthPercentage::FromString(_settings->CellWidth().c_str());
        _cellHeight = CSSLengthPercentage::FromString(_settings->CellHeight().c_str());
        //_runtimeOpacity = std::nullopt;
        //_runtimeFocusedOpacity = std::nullopt;

        // Manually turn off acrylic if they turn off transparency.
        //_runtimeUseAcrylic = _settings->Opacity() < 1.0 && _settings->UseAcrylic();

        const auto sizeChanged = _setFontSizeUnderLock(_settings->FontSize());

        if (!_initialized)
        {
            // If we haven't initialized, there's no point in continuing.
            // Initialization will handle the renderer settings.
            return;
        }

        _renderEngine->SetGraphicsAPI(parseGraphicsAPI(_settings->GraphicsAPI()));
        _renderEngine->SetDisablePartialInvalidation(_settings->DisablePartialInvalidation());
        _renderEngine->SetSoftwareRendering(_settings->SoftwareRendering());
        // Inform the renderer of our opacity
        _renderEngine->EnableTransparentBackground(_isBackgroundTransparent());

        // Trigger a redraw to repaint the window background and tab colors.
        _renderer->TriggerRedrawAll(true, true);

        _updateAntiAliasingMode();

        if (sizeChanged)
        {
            _refreshSizeUnderLock();
        }
    }

    void FuzzySearchBoxControl::_updateAntiAliasingMode()
    {
        D2D1_TEXT_ANTIALIAS_MODE mode;
        // Update AtlasEngine's AntialiasingMode
        switch (_settings->AntialiasingMode())
        {
        case TextAntialiasingMode::Cleartype:
            mode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE;
            break;
        case TextAntialiasingMode::Aliased:
            mode = D2D1_TEXT_ANTIALIAS_MODE_ALIASED;
            break;
        default:
            mode = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE;
            break;
        }

        _renderEngine->SetAntialiasingMode(mode);
    }

    void FuzzySearchBoxControl::_refreshSizeUnderLock()
    {
        //if (_IsClosing())
        //{
        //    return;
        //}

        auto cx = gsl::narrow_cast<til::CoordType>(lrint(_panelWidth * _compositionScale));
        auto cy = gsl::narrow_cast<til::CoordType>(lrint(_panelHeight * _compositionScale));

        // Don't actually resize so small that a single character wouldn't fit
        // in either dimension. The buffer really doesn't like being size 0.
        cx = std::max(cx, _actualFont.GetSize().width);
        cy = std::max(cy, _actualFont.GetSize().height);

        // Convert our new dimensions to characters
        const auto viewInPixels = ::Microsoft::Console::Types::Viewport::FromDimensions({ 0, 0 }, { cx, cy });
        const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);

        // Tell the dx engine that our window is now the new size.
        THROW_IF_FAILED(_renderEngine->SetWindowSize({ cx, cy }));

        // Invalidate everything
        _renderer->TriggerRedrawAll();
    }

    bool FuzzySearchBoxControl::_setFontSizeUnderLock(float fontSize)
    {
        // Make sure we have a non-zero font size
        const auto newSize = std::max(fontSize, 1.0f);
        const auto fontFace = _settings->FontFace();
        const auto fontWeight = _settings->FontWeight();
        _desiredFont = { fontFace, 0, fontWeight.Weight, newSize, CP_UTF8 };
        _actualFont = { fontFace, 0, fontWeight.Weight, _desiredFont.GetEngineSize(), CP_UTF8, false };

        _desiredFont.SetEnableBuiltinGlyphs(_builtinGlyphs);
        _desiredFont.SetEnableColorGlyphs(_colorGlyphs);
        _desiredFont.SetCellSize(_cellWidth, _cellHeight);

        const auto before = _actualFont.GetSize();
        _updateFont();
        const auto after = _actualFont.GetSize();
        return before != after;
    }

    void FuzzySearchBoxControl::_updateFont()
    {
        const auto newDpi = static_cast<int>(lrint(_compositionScale * USER_DEFAULT_SCREEN_DPI));

        if (_renderEngine)
        {
            std::unordered_map<std::wstring_view, float> featureMap;
            if (const auto fontFeatures = _settings->FontFeatures())
            {
                featureMap.reserve(fontFeatures.Size());

                for (const auto& [tag, param] : fontFeatures)
                {
                    featureMap.emplace(tag, param);
                }
            }
            std::unordered_map<std::wstring_view, float> axesMap;
            if (const auto fontAxes = _settings->FontAxes())
            {
                axesMap.reserve(fontAxes.Size());

                for (const auto& [axis, value] : fontAxes)
                {
                    axesMap.emplace(axis, value);
                }
            }

            // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
            //      actually fail. We need a way to gracefully fallback.
            LOG_IF_FAILED(_renderEngine->UpdateDpi(newDpi));
            LOG_IF_FAILED(_renderEngine->UpdateFont(_desiredFont, _actualFont, featureMap, axesMap));
        }
    }

    void FuzzySearchBoxControl::_setStatus(int32_t totalRowsSearched, int32_t numberOfResults)
    {
        hstring result;
        if (totalRowsSearched == 0)
        {
            result = RS_(L"TermControl_NoMatch");
        }
        else
        {
            result = winrt::hstring{ RS_fmt(L"TermControl_NumResults", numberOfResults, totalRowsSearched) };
        }

        StatusBox().Text(result);
    }

    void FuzzySearchBoxControl::SearchString(const winrt::hstring searchString)
    {
        FuzzySearchTextBox().Text(searchString);
    }

    void FuzzySearchBoxControl::_selectFirstItem()
    {
        if (ListBox().Items().Size() > 0)
        {
            ListBox().SelectedIndex(0);
        }
    }

    void FuzzySearchBoxControl::SetFontSize(int32_t width, int32_t height)
    {
        _fontSize = til::size{width, height};
    }

    void FuzzySearchBoxControl::_setSwapChainHandle(HANDLE handle)
    {
        auto nativePanel = FuzzySearchSwapChainPanel().as<ISwapChainPanelNative2>();
        nativePanel->SetSwapChainHandle(handle);
    }

    void FuzzySearchBoxControl::SetSearchResult(FuzzySearchResult val)
    {
        ListBox().Items().Clear();
        for (auto a : val.Results())
        {
            ListBox().Items().Append(a);
        }
        _setStatus(val.TotalRowsSearched(), val.NumberOfResults());
        _selectFirstItem();
    }

    void FuzzySearchBoxControl::_TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        const auto searchString= FuzzySearchTextBox().Text();
        _SearchHandlers(searchString);
    }

    void FuzzySearchBoxControl::_close()
    {
        ListBox().Items().Clear();
        _ClosedHandlers(*this, RoutedEventArgs{});
    }

    void FuzzySearchBoxControl::_TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Escape)
        {
            _close();
            e.Handled(true);
        }
        else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            auto selectedItem = ListBox().SelectedItem();
            if (selectedItem)
            {
                auto castedItem = selectedItem.try_as<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>();
                if (castedItem)
                {
                    _close();
                    _OnReturnHandlers(*this, castedItem);
                    e.Handled(true);
                }
            }
        }
    }

    void FuzzySearchBoxControl::_OnListBoxSelectionChanged(winrt::Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
    {
        auto selectedItem = ListBox().SelectedItem();
        if (selectedItem)
        {
            auto castedItem = selectedItem.try_as<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>();
            if (castedItem)
            {
                auto lock = _terminal->LockForReading();
                _fuzzySearchRenderData->SetTopRow(castedItem.Row());
                _renderer->TriggerRedrawAll();
            }
        }
    }

    void FuzzySearchBoxControl::Show(std::wstring_view searchString)
    {
        _fuzzySearchRenderData->ResetTopRow();
        _renderer->TriggerRedrawAll();
        if (FuzzySearchTextBox())
        {
            _setStatus(0, 0);
            FuzzySearchTextBox().Text(searchString);
            Input::FocusManager::TryFocusAsync(FuzzySearchTextBox(), FocusState::Keyboard);
            FuzzySearchTextBox().SelectAll();
            if (!searchString.empty())
            {
                _SearchHandlers(searchString);
            }
        }
    }

    void FuzzySearchBoxControl::Detach()
    {
        // Disable the renderer, so that it doesn't try to start any new frames
        // for our engines while we're not attached to anything.
        _renderer->WaitForPaintCompletionAndDisable(INFINITE);
    }

    til::point FuzzySearchBoxControl::_toPosInDips(const Core::Point terminalCellPos)
    {
        const til::point terminalPos{ terminalCellPos };
        const til::size marginsInDips{ til::math::rounding, FuzzySearchSwapChainPanel().Margin().Left, FuzzySearchSwapChainPanel().Margin().Top };
        const til::point posInPixels{ terminalPos * _fontSize };
        const auto scale{ FuzzySearchSwapChainPanel().CompositionScaleX() };
        const til::point posInDIPs{ til::math::flooring, posInPixels.x / scale, posInPixels.y / scale };
        return posInDIPs + marginsInDips;
    }

    FuzzySearchBoxControl::~FuzzySearchBoxControl()
    {
        _renderer.reset();
        _renderEngine.reset();
    }
}
