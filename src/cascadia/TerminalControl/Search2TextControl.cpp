// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include "Search2TextControl.h"

#include "Search2TextControl.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    // Our control exposes a "Text" property to be used with Data Binding
    // To allow this we need to register a Dependency Property Identifier to be used by the property system
    // (https://docs.microsoft.com/en-us/windows/uwp/xaml-platform/custom-dependency-properties)
    DependencyProperty Search2TextControl::_textProperty = DependencyProperty::Register(
        L"Text",
        xaml_typename<winrt::Microsoft::Terminal::Control::Search2TextLine>(),
        xaml_typename<winrt::Microsoft::Terminal::Control::Search2TextControl>(),
        PropertyMetadata(nullptr, Search2TextControl::_onTextChanged));

    Search2TextControl::Search2TextControl()
    {
        InitializeComponent();
    }

    // Method Description:
    // - Returns the Identifier of the "Text" dependency property
    DependencyProperty Search2TextControl::TextProperty()
    {
        return _textProperty;
    }

    // Method Description:
    // - Returns the TextBlock view used to render the highlighted text
    // Can be used when the Text property change is triggered by the event system to update the view
    // We need to expose it rather than simply bind a data source because we update the runs in code-behind
    Controls::TextBlock Search2TextControl::TextView()
    {
        return _textView();
    }

    winrt::Microsoft::Terminal::Control::Search2TextLine Search2TextControl::Text()
    {
        return winrt::unbox_value<winrt::Microsoft::Terminal::Control::Search2TextLine>(GetValue(_textProperty));
    }

    void Search2TextControl::Text(const winrt::Microsoft::Terminal::Control::Search2TextLine& value)
    {
        SetValue(_textProperty, winrt::box_value(value));
    }

    // Method Description:
    // - This callback is triggered when the Text property is changed. Responsible for updating the view
    // Arguments:
    // - o - dependency object that was modified, expected to be an instance of this control
    // - e - event arguments of the property changed event fired by the event system upon Text property change.
    // The new value is expected to be an instance of HighlightedText
    void Search2TextControl::_onTextChanged(const DependencyObject& o, const DependencyPropertyChangedEventArgs& e)
    {
        const auto control = o.try_as<winrt::Microsoft::Terminal::Control::Search2TextControl>();
        const auto highlightedText = e.NewValue().try_as<winrt::Microsoft::Terminal::Control::Search2TextLine>();

        if (control && highlightedText)
        {
            // Replace all the runs on the TextBlock
            // Use IsHighlighted to decide if the run should be highlighted.
            // To do - export the highlighting style into XAML
            const auto inlinesCollection = control.TextView().Inlines();
            inlinesCollection.Clear();

            for (const auto& match : highlightedText.Segments())
            {
                const auto matchText = match.TextSegment();
                const auto fontWeight = match.IsHighlighted() ? FontWeights::Bold() : FontWeights::Normal();

                Windows::UI::Xaml::Media::SolidColorBrush foregroundBrush;

                if (match.IsHighlighted())
                {
                    // Set the brush color for highlighted text
                    foregroundBrush.Color(Windows::UI::Colors::OrangeRed()); // Example: Red color for highlighted text
                }
                else
                {
                    // Set the brush color for non-highlighted text
                    foregroundBrush.Color(Windows::UI::Colors::White()); // Example: Black color for regular text
                }

                Documents::Run run;
                run.Text(matchText);
                run.FontWeight(fontWeight);
                run.Foreground(foregroundBrush);
                inlinesCollection.Append(run);
            }
        }
    }
}
