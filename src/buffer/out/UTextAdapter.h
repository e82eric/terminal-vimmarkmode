// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <icu.h>
#include <til/regex.h>

class TextBuffer;

namespace Microsoft::Console::ICU
{
    using unique_utext = wil::unique_struct<UText, decltype(&utext_close), &utext_close>;

    UText UTextForWrappableRow(const TextBuffer& textBuffer, til::CoordType& row, bool isLastRow) noexcept;
    unique_utext UTextFromTextBuffer(const TextBuffer& textBuffer, til::CoordType rowBeg, til::CoordType rowEnd) noexcept;
    til::ICU::unique_uregex CreateRegex(const std::wstring_view& pattern, uint32_t flags, UErrorCode* status) noexcept;
    std::vector<til::point_span> SearchBuffer(const std::wstring_view& pattern, const TextBuffer& textBuffer, uint32_t flags, UErrorCode* status) noexcept;
    til::point_span BufferRangeFromMatch(UText* ut, URegularExpression* re);
}
