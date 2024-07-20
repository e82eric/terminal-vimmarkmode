#pragma once

#include "SnippetSearchControl.g.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include <ControlSettings.h>
#include "../../buffer/fzf/fzf.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct SnippetSearchControl : SnippetSearchControlT<SnippetSearchControl>
    {
        SnippetSearchControl();
        ~SnippetSearchControl() override;

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

        fzf_slab_t* _fzfSlab;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(SnippetSearchControl);
}
