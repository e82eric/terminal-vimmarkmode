// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "search.h"

#include "textBuffer.hpp"
#include "UTextAdapter.h"

using namespace Microsoft::Console::Types;

bool Search::ResetIfStale(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool reverse, bool caseInsensitive)
{
    const auto& textBuffer = renderData.GetTextBuffer();
    const auto lastMutationId = textBuffer.GetLastMutationId();

    if (_needle == needle &&
        _caseInsensitive == caseInsensitive &&
        _lastMutationId == lastMutationId)
    {
        _step = reverse ? -1 : 1;
        return false;
    }

    _renderData = &renderData;
    _needle = needle;
    _caseInsensitive = caseInsensitive;
    _lastMutationId = lastMutationId;

    _results = textBuffer.SearchText(needle, caseInsensitive);
    _index = reverse ? gsl::narrow_cast<ptrdiff_t>(_results.size()) - 1 : 0;
    _step = reverse ? -1 : 1;

    return true;
}

bool Search::ResetIfStaleRegex(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool reverse, bool caseInsensitive)
{
    const auto& textBuffer = renderData.GetTextBuffer();
    const auto lastMutationId = textBuffer.GetLastMutationId();

    if (_needle == needle &&
        _caseInsensitive == caseInsensitive &&
        _lastMutationId == lastMutationId)
    {
        _step = reverse ? -1 : 1;
        return false;
    }

    _renderData = &renderData;
    _needle = needle;
    _caseInsensitive = caseInsensitive;
    _lastMutationId = lastMutationId;

    _results = textBuffer.SearchTextRegex(needle, caseInsensitive);
    _index = reverse ? gsl::narrow_cast<ptrdiff_t>(_results.size()) - 1 : 0;
    _step = reverse ? -1 : 1;

    return true;
}

void Search::QuickSelectRegex(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool caseInsensitive)
{
    const auto& textBuffer = renderData.GetTextBuffer();

    _renderData = &renderData;
    _needle = needle;
    _caseInsensitive = caseInsensitive;

    _results = textBuffer.SearchTextRegex(needle, caseInsensitive);
    _index =  0;
    _step = 1;
}


std::vector<FuzzySearchResultRow> Search::FuzzySearch(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle) const
{
    struct RowResult
    {
        UText text;
        int icuScore;
        til::CoordType startRowNumber;
        long long length;
    };

    auto searchResults = std::vector<FuzzySearchResultRow>();
    const auto& textBuffer = renderData.GetTextBuffer();

    auto searchTextNotBlank = std::ranges::any_of(needle, [](wchar_t ch) {
        return !std::iswspace(ch);
    });

    if (!searchTextNotBlank)
    {
        return {};
    }

    const UChar* uPattern = reinterpret_cast<const UChar*>(needle.data());
    ufzf_pattern_t* fzfPattern = ufzf_parse_pattern(CaseSmart, false, uPattern, true);

    auto rowResults = std::vector<RowResult>();
    auto rowCount = textBuffer.GetLastNonSpaceCharacter().y + 1;
    int minScore = 1;

    for (int rowNumber = 0; rowNumber < rowCount; rowNumber++)
    {
        //Row number is going to get incremented by this function if there is a row wrap
        auto uRowText = Microsoft::Console::ICU::UTextForLogicalRow(textBuffer, rowNumber);
        auto length = utext_nativeLength(&uRowText);

        if (length > 0)
        {
            int icuRowScore = ufzf_get_score(&uRowText, fzfPattern, _fzf_slab);
            if (icuRowScore >= minScore)
            {
                auto rowResult = RowResult{};
                //I think this is small enough to copy
                rowResult.text = uRowText;
                rowResult.startRowNumber = rowNumber;
                rowResult.icuScore = icuRowScore;
                rowResult.length = length;
                rowResults.push_back(rowResult);

                //sort so the highest scores and shortest lengths are first
                std::ranges::sort(rowResults, [](const auto& a, const auto& b) {
                    if (a.icuScore != b.icuScore)
                    {
                        return a.icuScore > b.icuScore;
                    }
                    return a.length < b.length;
                });

                //remove any results after 100 to save from having to get positions and render in xaml
                if (rowResults.size() > 100)
                {
                    utext_close(&rowResults[100].text);
                    rowResults.pop_back();
                    minScore = rowResults[99].icuScore;
                }
            }
        }
    }

    for (auto p : rowResults)
    {
        fzf_position_t* icuPos = ufzf_get_positions(&p.text, fzfPattern, _fzf_slab);

        if (!icuPos || icuPos->size == 0)
        {
            searchResults.emplace_back(FuzzySearchResultRow{ p.startRowNumber, p.text.b, {} });
            fzf_free_positions(icuPos);
        }
        else
        {
            std::vector<int32_t> vec;
            vec.reserve(icuPos->size);

            for (size_t i = 0; i < icuPos->size; ++i)
            {
                vec.push_back(static_cast<int32_t>(icuPos->data[i]));
            }

            searchResults.emplace_back(FuzzySearchResultRow{ p.startRowNumber, p.text.b, vec });
            fzf_free_positions(icuPos);
            utext_close(&p.text);
        }
    }

    ufzf_free_pattern(fzfPattern);

    return searchResults;
}

void Search::MoveToCurrentSelection()
{
    if (_renderData->IsSelectionActive())
    {
        MoveToPoint(_renderData->GetTextBuffer().ScreenToBufferPosition(_renderData->GetSelectionAnchor()));
    }
}

void Search::MoveToPoint(const til::point anchor) noexcept
{
    if (_results.empty())
    {
        return;
    }

    const auto count = gsl::narrow_cast<ptrdiff_t>(_results.size());
    ptrdiff_t index = 0;

    if (_step < 0)
    {
        for (index = count - 1; index >= 0 && til::at(_results, index).start > anchor; --index)
        {
        }
    }
    else
    {
        for (index = 0; index < count && til::at(_results, index).start < anchor; ++index)
        {
        }
    }

    _index = (index + count) % count;
}

void Search::MovePastPoint(const til::point anchor) noexcept
{
    if (_results.empty())
    {
        return;
    }

    const auto count = gsl::narrow_cast<ptrdiff_t>(_results.size());
    ptrdiff_t index = 0;

    if (_step < 0)
    {
        for (index = count - 1; index >= 0 && til::at(_results, index).start >= anchor; --index)
        {
        }
    }
    else
    {
        for (index = 0; index < count && til::at(_results, index).start <= anchor; ++index)
        {
        }
    }

    _index = (index + count) % count;
}

void Search::FindNext() noexcept
{
    if (const auto count{ gsl::narrow_cast<ptrdiff_t>(_results.size()) })
    {
        _index = (_index + _step + count) % count;
    }
}

const til::point_span* Search::GetCurrent() const noexcept
{
    const auto index = gsl::narrow_cast<size_t>(_index);
    if (index < _results.size())
    {
        return &til::at(_results, index);
    }
    return nullptr;
}

void Search::HighlightResults() const
{
    std::vector<til::inclusive_rect> toSelect;
    const auto& textBuffer = _renderData->GetTextBuffer();

    for (const auto& r : _results)
    {
        const auto rbStart = textBuffer.BufferToScreenPosition(r.start);
        const auto rbEnd = textBuffer.BufferToScreenPosition(r.end);

        til::inclusive_rect re;
        re.top = rbStart.y;
        re.bottom = rbEnd.y;
        re.left = rbStart.x;
        re.right = rbEnd.x;

        toSelect.emplace_back(re);
    }

    _renderData->SelectSearchRegions(std::move(toSelect));
}

// Routine Description:
// - Takes the found word and selects it in the screen buffer

bool Search::SelectCurrent() const
{
    if (const auto s = GetCurrent())
    {
        // Convert buffer selection offsets into the equivalent screen coordinates
        // required by SelectNewRegion, taking line renditions into account.
        const auto& textBuffer = _renderData->GetTextBuffer();
        const auto selStart = textBuffer.BufferToScreenPosition(s->start);
        const auto selEnd = textBuffer.BufferToScreenPosition(s->end);
        _renderData->SelectNewRegion(selStart, selEnd);
        return true;
    }

    _renderData->ClearSelection();
    return false;
}

const std::vector<til::point_span>& Search::Results() const noexcept
{
    return _results;
}

ptrdiff_t Search::CurrentMatch() const noexcept
{
    return _index;
}
