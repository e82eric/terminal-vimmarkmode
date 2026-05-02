// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "SnippetSearchItem.h"
#include "SnippetSearchItem.g.cpp"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    SnippetSearchItem::SnippetSearchItem(const winrt::hstring& input, const winrt::hstring& description, const winrt::hstring& escapedInput) :
        _Input(input),
        _Description(description),
        _EscapedInput(escapedInput)
    {
    }
}
