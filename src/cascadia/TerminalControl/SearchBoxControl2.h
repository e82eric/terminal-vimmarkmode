

#pragma once

#include "SearchBoxControl2.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct SearchBoxControl2 : SearchBoxControl2T<SearchBoxControl2>
    {
        SearchBoxControl2();   

        static winrt::Windows::UI::Xaml::DependencyProperty ItemsSourceProperty();
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::Search2TextLine> ItemsSource();
        void ItemsSource(winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::Search2TextLine> const& value);

        void SetFocusOnTextbox();
        bool ContainsFocus();

        void OnListBoxSelectionChanged(winrt::Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& e);

        void TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        WINRT_CALLBACK(Search, SearchHandler2);
        TYPED_EVENT(Closed, Control::SearchBoxControl2, Windows::UI::Xaml::RoutedEventArgs);
        TYPED_EVENT(SelectionChanged, Control::SearchBoxControl2, winrt::Microsoft::Terminal::Control::Search2TextLine);
        TYPED_EVENT(OnReturn, Control::SearchBoxControl2, winrt::Microsoft::Terminal::Control::Search2TextLine);

        private:
        std::unordered_set<winrt::Windows::Foundation::IInspectable> _focusableElements;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(SearchBoxControl2);
}
