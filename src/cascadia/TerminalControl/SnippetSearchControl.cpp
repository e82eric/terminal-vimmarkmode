// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "SnippetSearchControl.h"
#include "SnippetSearchControl.g.cpp"
#include "FuzzySearchTextSegment.h"

using namespace winrt::Windows::UI::Xaml::Media;

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    SnippetSearchControl::SnippetSearchControl()
    {
        InitializeComponent();
        _focusableElements.insert(FuzzySearchTextBox());

        FuzzySearchTextBox().KeyUp([this](const IInspectable& sender, Input::KeyRoutedEventArgs const& e) {
            auto textBox{ sender.try_as<Controls::TextBox>() };

            if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
            {
                if (const auto selectedItem = ListBox().SelectedItem())
                {
                    if (const auto listBoxItem = selectedItem.try_as<Controls::ListBoxItem>())
                    {
                        if (const auto fuzzyMatch = listBoxItem.DataContext().try_as<hstring>())
                        {
                            _close();
                            _OnReturnHandlers(*this, fuzzyMatch.value());
                            e.Handled(true);
                        }
                    }
                }
            }
            else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Down || e.OriginalKey() == winrt::Windows::System::VirtualKey::Up)
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
        });
    }

    void SnippetSearchControl::_selectFirstItem()
    {
        if (ListBox().Items().Size() > 0)
        {
            ListBox().SelectedIndex(0);
        }
    }

    void SnippetSearchControl::_populateForEmptySearch()
    {
        ListBox().Items().Clear();
        for (const auto input : _snippets)
        {
            auto row = SnippetSearchResultRow{
                {},
                input
            };
            _appendItem(row);
        }
        ListBox().SelectedIndex(0);
    }

    void SnippetSearchControl::_TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        if (FuzzySearchTextBox().Text().empty())
        {
            _populateForEmptySearch();
            return;
        }

        struct RowResult
        {
            UText *text;
            int score;
            long long length;
            hstring input;
        };

        auto searchResults = std::vector<SnippetSearchResultRow>();
        const UChar* uPattern = reinterpret_cast<const UChar*>(FuzzySearchTextBox().Text().data());
        ufzf_pattern_t* fzfPattern = ufzf_parse_pattern(CaseSmart, false, uPattern, true);
        auto rowResults = std::vector<RowResult>();
        int minScore = 1;
        constexpr int32_t maxResults = 100;

        for (auto input : _snippets)
        {
            if (input.size() > 0)
            {
                const UChar* uInput = reinterpret_cast<const UChar*>(input.c_str());
                UErrorCode status = U_ZERO_ERROR;
                auto utInput = utext_openUChars(nullptr, uInput, input.size(), &status);
                int icuRowScore = ufzf_get_score(utInput, fzfPattern, _fzfSlab);
                if (icuRowScore >= minScore)
                {
                    auto rowResult = RowResult{};
                    //I think this is small enough to copy
                    rowResult.text = utInput;
                    rowResult.score = icuRowScore;
                    rowResult.length = input.size();
                    rowResult.input = input;
                    rowResults.push_back(rowResult);
                }
                else
                {
                    utext_close(utInput);
                }
            }
        }

        //sort so the highest scores and shortest lengths are first
        std::ranges::sort(rowResults, [](const auto& a, const auto& b) {
            if (a.score != b.score)
            {
                return a.score > b.score;
            }
            return a.length < b.length;
        });

        for (size_t rank = 0; rank < rowResults.size(); rank++)
        {
            const auto rowResult = rowResults[rank];
            if (rank <= maxResults)
            {
                fzf_position_t* fzfPositions = ufzf_get_positions(rowResult.text, fzfPattern, _fzfSlab);

                //This is likely the result of an inverse search that didn't have any positions
                //We want to return it in the results.  It just won't have any highlights
                if (!fzfPositions || fzfPositions->size == 0)
                {
                    searchResults.emplace_back(SnippetSearchResultRow{  {}, rowResult.input });
                    fzf_free_positions(fzfPositions);
                }
                else
                {
                    std::vector<int32_t> vec;
                    vec.reserve(fzfPositions->size);

                    for (size_t i = 0; i < fzfPositions->size; ++i)
                    {
                        vec.push_back(static_cast<int32_t>(fzfPositions->data[i]));
                    }

                    searchResults.emplace_back(SnippetSearchResultRow{  vec, rowResult.input });
                    fzf_free_positions(fzfPositions);
                }
            }
        }

        for (const auto rowResult : rowResults)
        {
            utext_close(rowResult.text);
        }

        ufzf_free_pattern(fzfPattern);

        ListBox().Items().Clear();
        for (auto fuzzyMatch : searchResults)
        {
            _appendItem(fuzzyMatch);
        }

        ListBox().SelectedIndex(0);
    }

    void SnippetSearchControl::_close()
    {
        ListBox().Items().Clear();
        _ClosedHandlers(*this, RoutedEventArgs{});
    }

    void SnippetSearchControl::_TextBoxKeyDown(const Windows::Foundation::IInspectable& /*sender*/, const Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == Windows::System::VirtualKey::Escape)
        {
            _close();
            e.Handled(true);
        }
        else if (e.OriginalKey() == Windows::System::VirtualKey::Enter)
        {
            if (const auto selectedItem = ListBox().SelectedItem())
            {
                if (const auto listBoxItem = selectedItem.try_as<Controls::ListBoxItem>())
                {
                    if (const auto fuzzyMatch = listBoxItem.DataContext().try_as<hstring>())
                    {
                        _close();
                        _OnReturnHandlers(*this, fuzzyMatch.value());
                        e.Handled(true);
                    }
                }
            }
        }
    }

    void SnippetSearchControl::_appendItem(SnippetSearchResultRow& fuzzyMatch)
    {
        //sort the positions descending so that it is easier to create text segments from them
        std::ranges::sort(fuzzyMatch.positions, [](int32_t a, int32_t b) {
            return a < b;
        });

        ////Covert row text to text runs
        const auto runs = winrt::single_threaded_observable_vector<Control::FuzzySearchTextSegment>();
        std::wstring currentRun;
        bool isCurrentRunHighlighted = false;
        size_t highlightIndex = 0;

        for (auto i = 0; i < static_cast<int32_t>(fuzzyMatch.input.size()); ++i)
        {
            if (highlightIndex < fuzzyMatch.positions.size() && i == fuzzyMatch.positions[highlightIndex])
            {
                if (!isCurrentRunHighlighted)
                {
                    if (!currentRun.empty())
                    {
                        auto textSegmentHString = hstring(currentRun);
                        auto textSegment = winrt::make<FuzzySearchTextSegment>(textSegmentHString, false);
                        runs.Append(textSegment);
                        currentRun.clear();
                    }
                    isCurrentRunHighlighted = true;
                }
                highlightIndex++;
            }
            else
            {
                if (isCurrentRunHighlighted)
                {
                    if (!currentRun.empty())
                    {
                        hstring textSegmentHString = hstring(currentRun);
                        auto textSegment = winrt::make<FuzzySearchTextSegment>(textSegmentHString, true);
                        runs.Append(textSegment);
                        currentRun.clear();
                    }
                    isCurrentRunHighlighted = false;
                }
            }
            currentRun += fuzzyMatch.input[i];
        }

        if (!currentRun.empty())
        {
            auto textSegmentHString = hstring(currentRun);
            const auto textSegment = winrt::make<FuzzySearchTextSegment>(textSegmentHString, isCurrentRunHighlighted);
            runs.Append(textSegment);
        }

        const auto line = winrt::make<FuzzySearchTextLine>(runs, 0, 0);

        const auto control = Controls::TextBlock{};
        control.TextWrapping(TextWrapping::Wrap);
        const auto inlinesCollection = control.Inlines();
        inlinesCollection.Clear();

        for (const auto& match : line.Segments())
        {
            const auto matchText = match.TextSegment();
            const auto fontWeight = match.IsHighlighted() ? Windows::UI::Text::FontWeights::Bold() : Windows::UI::Text::FontWeights::Normal();

            SolidColorBrush foregroundBrush;

            if (match.IsHighlighted())
            {
                foregroundBrush.Color(Windows::UI::ColorHelper::FromArgb(0xFF, 0xFB, 0x49, 0x34)); // Gruvbox red
            }
            else
            {
                // Set the brush color for non-highlighted text
                foregroundBrush.Color(Windows::UI::ColorHelper::FromArgb(0xFF, 0xf9, 0xf5, 0xd7)); // Gruvbox light beige
            }

            Documents::Run run;
            run.Text(matchText);
            run.FontWeight(fontWeight);
            run.Foreground(foregroundBrush);
            inlinesCollection.Append(run);
        }
        const auto lbi = Controls::ListBoxItem();
        lbi.DataContext(box_value(fuzzyMatch.input));
        lbi.Content(control);
        ListBox().Items().Append(lbi);
    }

    void SnippetSearchControl::Show(Windows::Foundation::Collections::IVector<hstring> snippets)
    {
        _snippets = snippets;
        if (FuzzySearchTextBox())
        {
            Input::FocusManager::TryFocusAsync(FuzzySearchTextBox(), FocusState::Keyboard);
        }
        _populateForEmptySearch();
    }

    bool SnippetSearchControl::ContainsFocus()
    {
        auto focusedElement = Input::FocusManager::GetFocusedElement(this->XamlRoot());
        if (_focusableElements.count(focusedElement) > 0)
        {
            return true;
        }

        return false;
    }

    SnippetSearchControl::~SnippetSearchControl()
    {
        fzf_free_slab(_fzfSlab);
    }
}
