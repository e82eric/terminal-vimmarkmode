// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include "FuzzySearchTextControl.h"

#include "FuzzySearchTextControl.g.cpp"

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
    DependencyProperty FuzzySearchTextControl::_textProperty = DependencyProperty::Register(
        L"Text",
        xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>(),
        xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchTextControl>(),
        PropertyMetadata(nullptr, FuzzySearchTextControl::_onTextChanged));

    FuzzySearchTextControl::FuzzySearchTextControl()
    {
        InitializeComponent();
    }

    // Method Description:
    // - Returns the Identifier of the "Text" dependency property
    DependencyProperty FuzzySearchTextControl::TextProperty()
    {
        return _textProperty;
    }

    // Method Description:
    // - Returns the TextBlock view used to render the highlighted text
    // Can be used when the Text property change is triggered by the event system to update the view
    // We need to expose it rather than simply bind a data source because we update the runs in code-behind
    Controls::TextBlock FuzzySearchTextControl::TextView()
    {
        return _textView();
    }

    winrt::Microsoft::Terminal::Control::FuzzySearchTextLine FuzzySearchTextControl::Text()
    {
        return winrt::unbox_value<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>(GetValue(_textProperty));
    }

    void FuzzySearchTextControl::Text(const winrt::Microsoft::Terminal::Control::FuzzySearchTextLine& value)
    {
        SetValue(_textProperty, winrt::box_value(value));
    }

    // Method Description:
    // - This callback is triggered when the Text property is changed. Responsible for updating the view
    // Arguments:
    // - o - dependency object that was modified, expected to be an instance of this control
    // - e - event arguments of the property changed event fired by the event system upon Text property change.
    // The new value is expected to be an instance of HighlightedText
    void FuzzySearchTextControl::_onTextChanged(const DependencyObject& o, const DependencyPropertyChangedEventArgs& e)
    {
        const auto control = o.try_as<winrt::Microsoft::Terminal::Control::FuzzySearchTextControl>();
        const auto highlightedText = e.NewValue().try_as<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>();

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
                    foregroundBrush.Color(Windows::UI::Colors::OrangeRed());
                }
                else
                {
                    // Set the brush color for non-highlighted text
                    foregroundBrush.Color(Windows::UI::Colors::White());
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
