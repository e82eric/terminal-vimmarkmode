#pragma once

#include "SnippetSearchControl.g.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include <ControlSettings.h>
#include "../../buffer/fzf/fzf.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct SnippetSearchControl : SnippetSearchControlT<SnippetSearchControl>
    {
    public:
        SnippetSearchControl();
        ~SnippetSearchControl() override;

        til::property_changed_event PropertyChanged;
        static Windows::UI::Xaml::DependencyProperty BorderColorProperty();
        static Windows::UI::Xaml::DependencyProperty HeaderTextColorProperty();
        static Windows::UI::Xaml::DependencyProperty BackgroundColorProperty();
        static Windows::UI::Xaml::DependencyProperty SelectedItemColorProperty();
        static Windows::UI::Xaml::DependencyProperty InnerBorderThicknessProperty();
        static Windows::UI::Xaml::DependencyProperty TextColorProperty();
        static Windows::UI::Xaml::DependencyProperty HighlightedTextColorProperty();
        static Windows::UI::Xaml::DependencyProperty ResultFontSizeProperty();

        Windows::UI::Xaml::Media::Brush BorderColor();
        void BorderColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush HeaderTextColor();
        void HeaderTextColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush BackgroundColor();
        void BackgroundColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush TextColor();
        void TextColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Xaml::Media::Brush HighlightedTextColor();
        void HighlightedTextColor(Windows::UI::Xaml::Media::Brush const& value);

        Windows::UI::Color SelectedItemColor();
        void SelectedItemColor(Windows::UI::Color const& value);

        Windows::UI::Xaml::Thickness InnerBorderThickness();
        void InnerBorderThickness(Windows::UI::Xaml::Thickness const& value);

        double ResultFontSize();
        void ResultFontSize(double const& value);

        void Show(Windows::Foundation::Collections::IVector<hstring> snippets);
        bool ContainsFocus();

        void _TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        TYPED_EVENT(Closed, Control::SnippetSearchControl, Windows::UI::Xaml::RoutedEventArgs);
        TYPED_EVENT(OnReturn, Control::SnippetSearchControl, hstring);

        private:
        struct SnippetSearchResultRow
        {
            std::vector<int32_t> positions;
            hstring input;
        };

        void _appendItem(SnippetSearchResultRow& fuzzyMatch);
        void _selectFirstItem();
        void _populateForEmptySearch();
        void _close();
        std::unordered_set<winrt::Windows::Foundation::IInspectable> _focusableElements;
        Windows::Foundation::Collections::IVector<hstring> _snippets;

        static Windows::UI::Xaml::DependencyProperty _borderColorProperty;
        static Windows::UI::Xaml::DependencyProperty _headerTextColorProperty;
        static Windows::UI::Xaml::DependencyProperty _BackgroundColorProperty;
        static Windows::UI::Xaml::DependencyProperty _SelectedItemColorProperty;
        static Windows::UI::Xaml::DependencyProperty _InnerBorderThicknessProperty;
        static Windows::UI::Xaml::DependencyProperty _TextColorProperty;
        static Windows::UI::Xaml::DependencyProperty _HighlightedTextColorProperty;
        static Windows::UI::Xaml::DependencyProperty _ResultFontSizeProperty;

        fzf_slab_t* _fzfSlab;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(SnippetSearchControl);
}
