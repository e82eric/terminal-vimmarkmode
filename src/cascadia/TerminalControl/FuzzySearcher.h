#pragma once
#include "FuzzySearchBoxControl.h"
#include "../../buffer/fzf/fzf.h"
#include "../renderer/inc/IRenderData.hpp"

namespace Microsoft::Console::Render
{
    class IRenderData;
}

class FuzzySearcher
{
public:
    FuzzySearcher();
    ~FuzzySearcher();

    //void Search(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle) const;
    winrt::Microsoft::Terminal::Control::FuzzySearchResult Search(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle);
private:

    struct FuzzySearchResultRow
    {
        til::CoordType startRowNumber;
        std::vector<int32_t> positions;
    };

    static std::wstring _getRowFullText(FuzzySearcher::FuzzySearchResultRow& fuzzySearchResult2, TextBuffer& textBuffer);

    fzf_slab_t *_fzfSlab;
    std::vector<FuzzySearchResultRow> _search(Microsoft::Console::Render::IRenderData& renderData, const std::wstring_view& needle) const;
};
