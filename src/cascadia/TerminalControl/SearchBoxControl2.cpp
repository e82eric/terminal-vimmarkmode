// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "SearchBoxControl2.h"
#include "SearchBoxControl2.g.cpp"
#include <LibraryResources.h>
using namespace winrt::Windows::UI::Xaml::Media;

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    SearchBoxControl2::SearchBoxControl2()
    {
        InitializeComponent();

        _focusableElements.insert(TextBoxZ());

        TextBoxZ().KeyUp([this](const IInspectable& sender, Input::KeyRoutedEventArgs const& e) {
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
                    auto castedItem = selectedItem.try_as<winrt::Microsoft::Terminal::Control::Search2TextLine>();
                    if (castedItem)
                    {
                        _OnReturnHandlers(*this, castedItem);
                        e.Handled(true);
                    }
                }
            }
        });
        this->FuzzySearchSwapChainPanel().SizeChanged({ this, &SearchBoxControl2::OnSwapChainPanelSizeChanged });
    }

    bool SearchBoxControl2::ContainsFocus()
    {
        auto focusedElement = Input::FocusManager::GetFocusedElement(this->XamlRoot());
        if (_focusableElements.count(focusedElement) > 0)
        {
            return true;
        }

        return false;
    }

    double SearchBoxControl2::PreviewActualHeight()
    {
        return FuzzySearchSwapChainPanel().ActualHeight();
    }
    double SearchBoxControl2::PreviewActualWidth()
    {
        return FuzzySearchSwapChainPanel().ActualWidth();
    }
    float SearchBoxControl2::PreviewCompositionScaleX()
    {
        return FuzzySearchSwapChainPanel().CompositionScaleX();
    }
    float SearchBoxControl2::PreviewCompositionScaleY()
    {
        return FuzzySearchSwapChainPanel().CompositionScaleY();
    }

    DependencyProperty SearchBoxControl2::ItemsSourceProperty()
    {
        static DependencyProperty dp = DependencyProperty::Register(
            L"ItemsSource",
            xaml_typename<Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::Search2TextLine>>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::SearchBoxControl2>(),
            PropertyMetadata{ nullptr });

        return dp;
    }

    Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::Search2TextLine> SearchBoxControl2::ItemsSource()
    {
        return GetValue(ItemsSourceProperty()).as<Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::Search2TextLine>>();
    }

    void SearchBoxControl2::ItemsSource(Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::Search2TextLine> const& value)
    {
        SetValue(ItemsSourceProperty(), value);
    }

    void SearchBoxControl2::SearchString(const winrt::hstring searchString)
    {
        TextBoxZ().Text(searchString);
    }

    void SearchBoxControl2::SelectFirstItem()
    {
        if (ItemsSource().Size() > 0)
        {
            ListBox().SelectedIndex(0);
        }
    }

    void SearchBoxControl2::SetFontSize(til::size fontSize)
    {
        _fontSize = fontSize;
    }

    void SearchBoxControl2::SetSwapChainHandle(HANDLE handle)
    {
        auto nativePanel = FuzzySearchSwapChainPanel().as<ISwapChainPanelNative2>();
        nativePanel->SetSwapChainHandle(handle);
    }

    void SearchBoxControl2::TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        auto a = TextBoxZ().Text();
        _SearchHandlers(a, false, true);
    }

    void SearchBoxControl2::TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Escape)
        {
            _ClosedHandlers(*this, e);
            e.Handled(true);
        }
        else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            auto selectedItem = ListBox().SelectedItem();
            if (selectedItem)
            {
                auto castedItem = selectedItem.try_as<winrt::Microsoft::Terminal::Control::Search2TextLine>();
                if (castedItem)
                {
                    _OnReturnHandlers(*this, castedItem);
                    e.Handled(true);
                }
            }
        }
    }

    void SearchBoxControl2::OnListBoxSelectionChanged(winrt::Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& /*e*/)
    {
        auto selectedItem = ListBox().SelectedItem();
        if (selectedItem)
        {
            auto castedItem = selectedItem.try_as<winrt::Microsoft::Terminal::Control::Search2TextLine>();
            if (castedItem)
            {
                _SelectionChangedHandlers(*this, castedItem);
            }
        }
    }

    void SearchBoxControl2::OnSwapChainPanelSizeChanged(winrt::Windows::Foundation::IInspectable const&, winrt::Windows::UI::Xaml::SizeChangedEventArgs const& e)
    {
        _PreviewSwapChainPanelSizeChangedHandlers(*this, e);
    }

    void SearchBoxControl2::SetFocusOnTextbox()
    {
        if (TextBoxZ())
        {
            Input::FocusManager::TryFocusAsync(TextBoxZ(), FocusState::Keyboard);
            TextBoxZ().SelectAll();
        }
    }

    til::point SearchBoxControl2::_toPosInDips(const Core::Point terminalCellPos)
    {
        const til::point terminalPos{ terminalCellPos };
        const til::size marginsInDips{ til::math::rounding, FuzzySearchSwapChainPanel().Margin().Left, FuzzySearchSwapChainPanel().Margin().Top };
        const til::point posInPixels{ terminalPos * _fontSize };
        const auto scale{ FuzzySearchSwapChainPanel().CompositionScaleX() };
        const til::point posInDIPs{ til::math::flooring, posInPixels.x / scale, posInPixels.y / scale };
        return posInDIPs + marginsInDips;
    }
}
