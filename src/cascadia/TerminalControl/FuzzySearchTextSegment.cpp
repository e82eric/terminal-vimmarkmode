#include "pch.h"

#include "FuzzySearchTextSegment.h"
#include "FuzzySearchTextSegment.g.cpp"
#include "FuzzySearchTextLine.g.cpp"

using namespace winrt::Windows::Foundation;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    FuzzySearchTextSegment::FuzzySearchTextSegment()
    {
    }

    FuzzySearchTextSegment::FuzzySearchTextSegment(const winrt::hstring& textSegment, bool isHighlighted) :
        _TextSegment(textSegment),
        _IsHighlighted(isHighlighted)
    {
    }

    FuzzySearchTextLine::FuzzySearchTextLine(const Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::FuzzySearchTextSegment>& segments, int32_t score, int32_t row, int32_t firstPosition, int32_t length) :
        _Segments(segments),
        _Score(score),
        _Row(row),
        _FirstPosition(firstPosition),
        _Length(length)
    {
    }
}
