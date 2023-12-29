#include "pch.h"

#include "Search2TextSegment.h"
#include "Search2TextSegment.g.cpp"
#include "Search2TextLine.g.cpp"

using namespace winrt::Windows::Foundation;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    Search2TextSegment::Search2TextSegment()
    {
    }

    Search2TextSegment::Search2TextSegment(const winrt::hstring& textSegment, bool isHighlighted) :
        _TextSegment(textSegment),
        _IsHighlighted(isHighlighted)
    {
    }

    Search2TextLine::Search2TextLine(const Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::Search2TextSegment>& segments, int32_t score, int32_t row, int32_t firstPosition, int32_t length) :
        _Segments(segments),
        _Score(score),
        _Row(row),
        _FirstPosition(firstPosition),
        _Length(length)
    {
    }
}
