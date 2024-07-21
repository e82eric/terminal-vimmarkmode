// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include "FuzzySearchTextControl.h"

#include "FuzzySearchTextControl.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    DependencyProperty FuzzySearchTextControl::_TextColorProperty =
        DependencyProperty::Register(
            L"TextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchTextControl>(),
            PropertyMetadata{ nullptr });

    DependencyProperty FuzzySearchTextControl::_HighlightedTextColorProperty =
        DependencyProperty::Register(
            L"HighlightedTextColor",
            xaml_typename<Brush>(),
            xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchTextControl>(),
            PropertyMetadata{ nullptr });

    // Our control exposes a "Text" property to be used with Data Binding
    // To allow this we need to register a Dependency Property Identifier to be used by the property system
    // (https://docs.microsoft.com/en-us/windows/uwp/xaml-platform/custom-dependency-properties)
    DependencyProperty FuzzySearchTextControl::_textProperty = DependencyProperty::Register(
        L"Text",
        xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchTextLine>(),
        xaml_typename<winrt::Microsoft::Terminal::Control::FuzzySearchTextControl>(),
        PropertyMetadata(nullptr, FuzzySearchTextControl::_onTextChanged));

    Brush FuzzySearchTextControl::TextColor()
    {
        return GetValue(_TextColorProperty).as<Brush>();
    }

    void FuzzySearchTextControl::TextColor(Brush const& value)
    {
        SetValue(_TextColorProperty, value);
    }

    DependencyProperty FuzzySearchTextControl::TextColorProperty()
    {
        return _TextColorProperty;
    }

    Brush FuzzySearchTextControl::HighlightedTextColor()
    {
        return GetValue(_HighlightedTextColorProperty).as<Brush>();
    }

    void FuzzySearchTextControl::HighlightedTextColor(Brush const& value)
    {
        SetValue(_HighlightedTextColorProperty, value);
    }

    DependencyProperty FuzzySearchTextControl::HighlightedTextColorProperty()
    {
        return _HighlightedTextColorProperty;
    }

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

                Documents::Run run;
                if (match.IsHighlighted())
                {
                    run.Foreground(control.HighlightedTextColor());
                }
                else
                {
                    run.Foreground(control.TextColor());
                }

                run.Text(matchText);
                run.FontWeight(fontWeight);
                inlinesCollection.Append(run);
            }
        }
    }
}
