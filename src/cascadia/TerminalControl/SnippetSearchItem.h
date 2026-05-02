// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "SnippetSearchItem.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct SnippetSearchItem : SnippetSearchItemT<SnippetSearchItem>
    {
        SnippetSearchItem() = default;
        SnippetSearchItem(const winrt::hstring& input, const winrt::hstring& description, const winrt::hstring& escapedInput);

        WINRT_PROPERTY(winrt::hstring, Input);
        WINRT_PROPERTY(winrt::hstring, Description);
        WINRT_PROPERTY(winrt::hstring, EscapedInput);
        WINRT_PROPERTY(Microsoft::Terminal::Control::SuggestionInvokedHandler, OnInvoked, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(SnippetSearchItem);
}
