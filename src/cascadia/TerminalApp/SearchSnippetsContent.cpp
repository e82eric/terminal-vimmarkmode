// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SearchSnippetsContent.h"
#include "HighlightedText.h"

#include "../../buffer/fzf/fzf.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    SearchSnippetsContent::SearchSnippetsContent(const winrt::Microsoft::Terminal::Control::TermControl& control, const Microsoft::Terminal::Settings::Model::CascadiaSettings& settings)
    {
        _control = control;
        _root = winrt::Windows::UI::Xaml::Controls::Grid{};
        _tasks = settings.GlobalSettings().ActionMap().FilterToSendInput(winrt::hstring{});

        auto res = Windows::UI::Xaml::Application::Current().Resources();
        auto bg = res.Lookup(winrt::box_value(L"UnfocusedBorderBrush"));
        _root.Background(bg.try_as<Media::Brush>());

        _box = winrt::Windows::UI::Xaml::Controls::TextBox{};
        _box.Margin({ 10, 10, 10, 10 });
        _box.AcceptsReturn(true);
        _box.TextWrapping(TextWrapping::Wrap);
        _root.Children().Append(_box);
        const Controls::RowDefinition row1;
        row1.Height(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        _root.RowDefinitions().Append(row1);

        const Controls::RowDefinition row2;
        row2.Height(GridLengthHelper::Auto());
        _root.RowDefinitions().Append(row2);

        _listBox = Controls::ListBox{};
        _listBox.Margin({ 10, 10, 10, 10 });
        _root.Children().Append(_listBox);

        _searchBox = Controls::TextBox{};
        _root.Children().Append(_searchBox);

        _searchBox.TextChanged({ this, &SearchSnippetsContent::OnTextChanged });
        _searchBox.KeyDown({ this, &SearchSnippetsContent::OnKeyUp });

        Controls::Grid::SetRow(_listBox, 0);
        Controls::Grid::SetRow(_searchBox, 1);

        _fzfSlab = fzf_make_default_slab();
        _populateForEmptySearch();
    }

    void SearchSnippetsContent::UpdateSettings(const CascadiaSettings& settings)
    {
        _tasks = settings.GlobalSettings().ActionMap().FilterToSendInput(winrt::hstring{});
    }

    winrt::Windows::UI::Xaml::FrameworkElement SearchSnippetsContent::GetRoot()
    {
        return _root;
    }
    winrt::Windows::Foundation::Size SearchSnippetsContent::MinimumSize()
    {
        return { 1, 1 };
    }
    void SearchSnippetsContent::Focus(winrt::Windows::UI::Xaml::FocusState reason)
    {
        _searchBox.Focus(reason);
    }
    void SearchSnippetsContent::Close()
    {
        CloseRequested.raise(*this, nullptr);
    }

    INewContentArgs SearchSnippetsContent::GetNewTerminalArgs(const BuildStartupKind /* kind */) const
    {
        return BaseContentArgs(L"scratchpad");
    }

    winrt::hstring SearchSnippetsContent::Icon() const
    {
        static constexpr std::wstring_view glyph{ L"\xe70b" }; // QuickNote
        return winrt::hstring{ glyph };
    }

    winrt::Windows::UI::Xaml::Media::Brush SearchSnippetsContent::BackgroundBrush()
    {
        return _root.Background();
    }

    void SearchSnippetsContent::OnTextChanged(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::TextChangedEventArgs const&)
    {
        if (_searchBox.Text().empty())
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

        auto searchResults = std::vector<FuzzySearchResultRow>();
        const UChar* uPattern = reinterpret_cast<const UChar*>(_searchBox.Text().data());
        ufzf_pattern_t* fzfPattern = ufzf_parse_pattern(CaseSmart, false, uPattern, true);
        auto rowResults = std::vector<RowResult>();
        int minScore = 1;
        constexpr int32_t maxResults = 100;

        for (auto task : _tasks)
        {
            auto sendInputArgs = task.ActionAndArgs().Args().try_as<SendInputArgs>();
            auto input = sendInputArgs.Input();

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
                    searchResults.emplace_back(FuzzySearchResultRow{  {}, rowResult.input });
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

                    searchResults.emplace_back(FuzzySearchResultRow{  vec, rowResult.input });
                    fzf_free_positions(fzfPositions);
                }
            }
        }

        for (const auto rowResult : rowResults)
        {
            utext_close(rowResult.text);
        }

        ufzf_free_pattern(fzfPattern);

        const auto searchResultsRuns = winrt::single_threaded_observable_vector<winrt::TerminalApp::HighlightedText>();
        _listBox.Items().Clear();
        for (auto fuzzyMatch : searchResults)
        {
            _appendItem(fuzzyMatch);
        }

        _listBox.SelectedIndex(0);
    }

    void SearchSnippetsContent::OnKeyUp(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Down || e.OriginalKey() == winrt::Windows::System::VirtualKey::Up)
        {
            auto selectedIndex = _listBox.SelectedIndex();

            if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Down)
            {
                selectedIndex++;
            }
            else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Up)
            {
                selectedIndex--;
            }

            if (selectedIndex >= 0 && selectedIndex < static_cast<int32_t>(_listBox.Items().Size()))
            {
                _listBox.SelectedIndex(selectedIndex);
                _listBox.ScrollIntoView(Controls::ListBox().SelectedItem());
            }

            e.Handled(true);
        }
        else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            if (const auto selectedItem = _listBox.SelectedItem())
            {
                if (const auto listBoxItem = selectedItem.try_as<Controls::ListBoxItem>())
                {
                    if (const auto fuzzyMatch = listBoxItem.DataContext().try_as<hstring>())
                    {
                        _control.SendInput(fuzzyMatch.value());
                        e.Handled(true);
                        Close();
                    }
                }
            }
        }
        else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Escape)
        {
            Close();
            e.Handled(true);
        }
    }

    void SearchSnippetsContent::_populateForEmptySearch() const
    {
        _listBox.Items().Clear();
        for (auto task : _tasks)
        {
            auto sendInputArgs = task.ActionAndArgs().Args().try_as<SendInputArgs>();
            auto input = sendInputArgs.Input();
            auto row = FuzzySearchResultRow{
                {},
                input
            };
            _appendItem(row);
        }
        _listBox.SelectedIndex(0);
    }

    void SearchSnippetsContent::_appendItem(FuzzySearchResultRow& fuzzyMatch) const
    {
        //sort the positions descending so that it is easier to create text segments from them
        std::ranges::sort(fuzzyMatch.positions, [](int32_t a, int32_t b) {
            return a < b;
        });

        ////Covert row text to text runs
        const auto runs = winrt::single_threaded_observable_vector<winrt::TerminalApp::HighlightedTextSegment>();
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
                        auto textSegment = winrt::make<implementation::HighlightedTextSegment>(textSegmentHString, false);
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
                        auto textSegment = winrt::make<implementation::HighlightedTextSegment>(textSegmentHString, true);
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
            const auto textSegment = winrt::make<implementation::HighlightedTextSegment>(textSegmentHString, isCurrentRunHighlighted);
            runs.Append(textSegment);
        }

        const auto line = winrt::make<implementation::HighlightedText>(runs);

        const auto control = Controls::TextBlock{};
        control.TextWrapping(TextWrapping::Wrap);
        const auto inlinesCollection = control.Inlines();
        inlinesCollection.Clear();

        for (const auto& match : line.Segments())
        {
            const auto matchText = match.TextSegment();
            const auto fontWeight = match.IsHighlighted() ? Windows::UI::Text::FontWeights::Bold() : Windows::UI::Text::FontWeights::Normal();

            Media::SolidColorBrush foregroundBrush;

            if (match.IsHighlighted())
            {
                foregroundBrush.Color(Windows::UI::Colors::OrangeRed());
            }
            else
            {
                foregroundBrush.Color(Windows::UI::Colors::White());
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
        _listBox.Items().Append(lbi);
    }
}
