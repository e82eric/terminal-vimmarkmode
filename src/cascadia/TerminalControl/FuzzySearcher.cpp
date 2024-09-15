#include "pch.h"
#include "FuzzySearcher.h"

#include "FuzzySearchTextSegment.h"
#include "..\buffer\out\textBuffer.hpp"
#include "..\buffer\out\UTextAdapter.h"

struct FuzzySearchResultRow;
using namespace winrt::Microsoft::Terminal::Control;

FuzzySearcher::FuzzySearcher()
{
    _fzfSlab = fzf_make_default_slab();
}

FuzzySearcher::~FuzzySearcher()
{
    fzf_free_slab(_fzfSlab);
}

FuzzySearchResult FuzzySearcher::Search(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& text)
{
    const auto fuzzySearchResultRows = _search(renderData, text);

    auto searchResults = winrt::single_threaded_observable_vector<FuzzySearchTextLine>();

    for (auto p : fuzzySearchResultRows)
    {
        auto rowFullText = _getRowFullText(p, renderData.GetTextBuffer());

        //sort the positions descending so that it is easier to create text segments from them
        std::ranges::sort(p.positions, [](int32_t a, int32_t b) {
            return a < b;
        });

        //Covert row text to text runs
        auto runs = winrt::single_threaded_observable_vector<FuzzySearchTextSegment>();
        std::wstring currentRun;
        bool isCurrentRunHighlighted = false;
        size_t highlightIndex = 0;

        for (int32_t i = 0; i < rowFullText.length(); ++i)
        {
            if (highlightIndex < p.positions.size() && i == p.positions[highlightIndex])
            {
                if (!isCurrentRunHighlighted)
                {
                    if (!currentRun.empty())
                    {
                        auto textSegmentHString = winrt::hstring(currentRun);
                        auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(textSegmentHString, false);
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
                        winrt::hstring textSegmentHString = winrt::hstring(currentRun);
                        auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(textSegmentHString, true);
                        runs.Append(textSegment);
                        currentRun.clear();
                    }
                    isCurrentRunHighlighted = false;
                }
            }
            currentRun += rowFullText[i];
        }

        if (!currentRun.empty())
        {
            auto textSegmentHString = winrt::hstring(currentRun);
            auto textSegment = winrt::make<implementation::FuzzySearchTextSegment>(textSegmentHString, isCurrentRunHighlighted);
            runs.Append(textSegment);
        }

        auto firstPosition = 0;
        if (p.positions.size() > 0)
        {
            firstPosition = p.positions[0];
        }
        auto line = winrt::make<implementation::FuzzySearchTextLine>(runs, p.startRowNumber, firstPosition);

        searchResults.Append(line);
    }

    auto fuzzySearchResult = winrt::make<implementation::FuzzySearchResult>(searchResults, static_cast<int32_t>(fuzzySearchResultRows.size()), static_cast<int32_t>(searchResults.Size()));
    return fuzzySearchResult;
}

//void FuzzySearcher::Search(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle) const
std::vector<FuzzySearcher::FuzzySearchResultRow> FuzzySearcher::_search(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle) const
{
    constexpr int32_t maxResults = 100;
    auto searchResults = std::vector<FuzzySearcher::FuzzySearchResultRow>();
    searchResults.reserve(maxResults);

    struct RowResult
    {
        UText text;
        int score;
        til::CoordType startRowNumber;
        long long length;
    };

    const auto& textBuffer = renderData.GetTextBuffer();

    auto searchTextNotBlank = std::ranges::any_of(needle, [](const wchar_t ch) {
        return !std::iswspace(ch);
    });

    if (!searchTextNotBlank)
    {
        return {};
    }

    const UChar* uPattern = reinterpret_cast<const UChar*>(needle.data());
    ufzf_pattern_t* fzfPattern = ufzf_parse_pattern(CaseSmart, false, uPattern, true);

    auto rowCount = textBuffer.GetLastNonSpaceCharacter().y + 1;
    //overkill?
    auto rowResults = std::vector<RowResult>();
    rowResults.reserve(rowCount);
    //searchResults.reserve(maxResults);
    int minScore = 1;

    for (int rowNumber = 0; rowNumber < rowCount; rowNumber++)
    {
        const auto startRowNumber = rowNumber;
        //Warn: row number will be incremented by this function if there is a row wrap.  Is there a better way
        auto uRowText = Microsoft::Console::ICU::UTextForWrappableRow(textBuffer, rowNumber, rowNumber == rowCount - 1);
        auto length = utext_nativeLength(&uRowText);

        if (length > 0)
        {
            int icuRowScore = ufzf_get_score(&uRowText, fzfPattern, _fzfSlab);
            if (icuRowScore >= minScore)
            {
                auto rowResult = RowResult{};
                //I think this is small enough to copy
                rowResult.text = uRowText;
                rowResult.startRowNumber = startRowNumber;
                rowResult.score = icuRowScore;
                rowResult.length = length;
                rowResults.push_back(rowResult);
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
        auto rowResult = rowResults[rank];
        if (rank <= maxResults)
        {
            fzf_position_t* fzfPositions = ufzf_get_positions(&rowResult.text, fzfPattern, _fzfSlab);

            //This is likely the result of an inverse search that didn't have any positions
            //We want to return it in the results.  It just won't have any highlights
            if (!fzfPositions || fzfPositions->size == 0)
            {
                searchResults.emplace_back(FuzzySearchResultRow{ rowResult.startRowNumber, {} });
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

                searchResults.emplace_back(FuzzySearchResultRow{ rowResult.startRowNumber, vec });
                fzf_free_positions(fzfPositions);
            }
        }
        utext_close(&rowResult.text);
    }

    ufzf_free_pattern(fzfPattern);

    return searchResults;
}

std::wstring FuzzySearcher::_getRowFullText(FuzzySearcher::FuzzySearchResultRow& fuzzySearchResult2, TextBuffer& textBuffer)
{
    std::wstring result;

    auto i = fuzzySearchResult2.startRowNumber;
    while (textBuffer.GetRowByOffset(i).WasWrapForced())
    {
        result += textBuffer.GetRowByOffset(i).GetText();
        i++;
    }
    result += textBuffer.GetRowByOffset(i).GetText();

    return result;
}
