#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "Search2TextSegment.g.h"
#include "Search2TextControl.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct Search2TextControl : Search2TextControlT<Search2TextControl>
    {
        Search2TextControl();

        static Windows::UI::Xaml::DependencyProperty TextProperty();

        winrt::Microsoft::Terminal::Control::Search2TextLine Text();
        void Text(const winrt::Microsoft::Terminal::Control::Search2TextLine& value);

        Windows::UI::Xaml::Controls::TextBlock TextView();

    private:
        static Windows::UI::Xaml::DependencyProperty _textProperty;
        static void _onTextChanged(const Windows::UI::Xaml::DependencyObject& o, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(Search2TextControl);
}
