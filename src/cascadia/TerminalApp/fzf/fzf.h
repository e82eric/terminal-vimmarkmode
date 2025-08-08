#pragma once

#include <vector>
#include <icu.h>

namespace fzf::matcher
{
    enum class MatchType : uint8_t
    {
        Fuzzy = 0,
        Suffix = 1,
        NotContains = 2,
        Prefix = 3
    };

    enum class Location : uint8_t
    {
        Context = 0,
        Token = 1
    };

    struct TextRun
    {
        size_t Start;
        size_t End;
    };

    struct MatchResult
    {
        int32_t Score = 0;
        std::vector<TextRun> Runs;
    };

    struct TokenMatchResult
    {
        bool ExpectsMatchOnToken = false;
        bool ExpectsMatchOnContext = false;
        std::optional<MatchResult> TokenResult;
        std::optional<MatchResult> ContextResult;
    };

    struct Term
    {
        std::vector<UChar32> codePoints;
        MatchType type = MatchType::Fuzzy;
        Location location = Location::Token;
    };

    struct Pattern
    {
        std::vector<std::vector<UChar32>> terms;
        std::vector<Term> typedTerms;
    };

    Pattern ParsePattern(std::wstring_view patternStr);
    Pattern ParsePatternWithTypes(std::wstring_view patternStr);
    std::optional<MatchResult> Match(std::wstring_view text, const Pattern& pattern);
    std::optional<TokenMatchResult> MatchToken(std::wstring_view token, std::wstring_view context, const Pattern& pattern);
}
