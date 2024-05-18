// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "search.h"

#include "textBuffer.hpp"
#include "UTextAdapter.h"

using namespace Microsoft::Console::Types;

bool Search::IsStale(const Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool caseInsensitive) const noexcept
{
    return _renderData != &renderData ||
           _needle != needle ||
           _caseInsensitive != caseInsensitive ||
           _lastMutationId != renderData.GetTextBuffer().GetLastMutationId();
}

bool Search::Reset(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool caseInsensitive, bool reverse)
{
    const auto& textBuffer = renderData.GetTextBuffer();

    _renderData = &renderData;
    _needle = needle;
    _caseInsensitive = caseInsensitive;
    _lastMutationId = textBuffer.GetLastMutationId();

    _results = textBuffer.SearchText(needle, caseInsensitive);
    _index = reverse ? gsl::narrow_cast<ptrdiff_t>(_results.size()) - 1 : 0;
    _step = reverse ? -1 : 1;
    return true;
}


bool Search::ResetIfStaleRegex(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool reverse, bool caseInsensitive, std::vector<til::point_span>* prevResults)
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

    if (prevResults)
    {
        *prevResults = std::move(_results);
    }

    _renderData = &renderData;
    _needle = needle;
    _caseInsensitive = caseInsensitive;
    _lastMutationId = lastMutationId;

    _results = _regexSearch(renderData, needle, caseInsensitive);
    _index = reverse ? gsl::narrow_cast<ptrdiff_t>(_results.size()) - 1 : 0;
    _step = reverse ? -1 : 1;

    return true;
}


std::vector<til::point_span> Search::_regexSearch(const Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool caseInsensitive)
{
    std::vector<til::point_span> results;
    UErrorCode status = U_ZERO_ERROR;
    uint32_t flags = 0;
    WI_SetFlagIf(flags, UREGEX_CASE_INSENSITIVE, caseInsensitive);
    const auto& textBuffer = renderData.GetTextBuffer();

    const auto rowCount = textBuffer.GetLastNonSpaceCharacter().y + 1;

    const auto regex = Microsoft::Console::ICU::CreateRegex(needle, flags, &status);
    if (U_FAILURE(status))
    {
        return results;
    }

    const auto viewPortWidth = textBuffer.GetSize().Width();

    for (int32_t i = 0; i < rowCount; i++)
    {
        auto startRow = i;
        auto uText = Microsoft::Console::ICU::UTextForWrappableRow(textBuffer, i);

        uregex_setUText(regex.get(), &uText, &status);
        if (U_FAILURE(status))
        {
            return results;
        }

        if (uregex_find(regex.get(), -1, &status))
        {
            do
            {
                const int32_t icuStart = uregex_start(regex.get(), 0, &status);
                int32_t icuEnd = uregex_end(regex.get(), 0, &status);
                icuEnd--;

                //Start of line is 0,0 and should be skipped (^)
                if (icuEnd >= 0)
                {
                    const auto matchLength = utext_nativeLength(&uText);
                    auto adjustedMatchStart = icuStart - 1 == matchLength ? icuStart - 1 : icuStart;
                    auto adjustedMatchEnd = std::min(static_cast<int32_t>(matchLength), icuEnd);

                    const size_t matchStartLine = (adjustedMatchStart / viewPortWidth) + startRow;
                    const size_t matchEndLine = (adjustedMatchEnd / viewPortWidth) + startRow;

                    if (matchStartLine > startRow)
                    {
                        adjustedMatchStart %= (matchStartLine - startRow) * viewPortWidth;
                    }

                    if (matchEndLine > startRow)
                    {
                        adjustedMatchEnd %= (matchEndLine - startRow) * viewPortWidth;
                    }

                    auto ps = til::point_span{};
                    ps.start = til::point{ adjustedMatchStart, static_cast<int32_t>(matchStartLine) };
                    ps.end = til::point{ adjustedMatchEnd, static_cast<int32_t>(matchEndLine) };
                    results.emplace_back(ps);
                }
            } while (uregex_findNext(regex.get(), &status));
        }
    }
    return results;
}

void Search::QuickSelectRegex(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle, bool caseInsensitive)
{
    _renderData = &renderData;
    _needle = needle;
    _caseInsensitive = caseInsensitive;

    _results = _regexSearch(renderData, needle, caseInsensitive);
    _index = 0;
    _step = 1;
}


std::vector<FuzzySearchResultRow> Search::FuzzySearch(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle) const
{
    struct RowResult
    {
        UText text;
        int score;
        til::CoordType startRowNumber;
        long long length;
    };

    auto searchResults = std::vector<FuzzySearchResultRow>();
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

    constexpr int32_t maxResults = 100;
    auto rowResults = std::vector<RowResult>();
    auto rowCount = textBuffer.GetLastNonSpaceCharacter().y + 1;
    //overkill?
    rowResults.reserve(rowCount);
    searchResults.reserve(maxResults);
    int minScore = 1;

    for (int rowNumber = 0; rowNumber < rowCount; rowNumber++)
    {
        const auto startRowNumber = rowNumber;
        //Warn: row number will be incremented by this function if there is a row wrap.  Is there a better way
        auto uRowText = Microsoft::Console::ICU::UTextForWrappableRow(textBuffer, rowNumber);
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

void Search::FindNext(bool reverse) noexcept
{
    _step = reverse ? -1 : 1;
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

std::vector<til::point_span>&& Search::ExtractResults() noexcept
{
    return std::move(_results);
}

ptrdiff_t Search::CurrentMatch() const noexcept
{
    return _index;
}
