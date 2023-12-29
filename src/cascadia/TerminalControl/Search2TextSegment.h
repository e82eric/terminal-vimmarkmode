#pragma once

#include <winrt/Windows.Foundation.h>
#include "Search2TextSegment.g.h"
#include "Search2TextLine.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct Search2TextSegment : Search2TextSegmentT<Search2TextSegment>
    {
        Search2TextSegment();
        Search2TextSegment(const winrt::hstring& textSegment, bool isHighlighted);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, TextSegment, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(bool, IsHighlighted, _PropertyChangedHandlers);
    };

    struct Search2TextLine : Search2TextLineT<Search2TextLine>
    {
        Search2TextLine() = default;
        Search2TextLine(const Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::Search2TextSegment>& segments, int32_t score, int32_t row, int32_t firstPosition, int32_t length);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Control::Search2TextSegment>, Segments, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(int32_t, Score, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(int32_t, Row, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(int32_t, FirstPosition, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(int32_t, Length, _PropertyChangedHandlers);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(Search2TextSegment);
    BASIC_FACTORY(Search2TextLine);
}
