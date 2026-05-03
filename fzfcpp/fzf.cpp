#include "pch.h"
#include "fzf.h"

#include <algorithm>
#include <span>
#include <winerror.h>

#undef CharLower
#undef CharUpper

using namespace fzfcpp::matcher;

constexpr int16_t ScoreMatch = 16;
constexpr int16_t ScoreGapStart = -3;
constexpr int16_t ScoreGapExtension = -1;
constexpr int16_t BoundaryBonus = 0;
constexpr int16_t NonWordBonus = ScoreMatch / 2;
constexpr int16_t CamelCaseBonus = BoundaryBonus + ScoreGapExtension;
constexpr int16_t BonusConsecutive = -(ScoreGapStart + ScoreGapExtension);
constexpr int16_t BonusFirstCharMultiplier = 0;
constexpr size_t npos = std::numeric_limits<size_t>::max();

struct PreparedText
{
    std::vector<UChar32> codePoints;
    std::vector<UChar32> foldedCodePoints;
};

struct MatchScratch
{
    std::vector<int16_t> initialScores;
    std::vector<int16_t> consecutiveScores;
    std::vector<size_t> firstOccurrenceOfEachChar;
    std::vector<int16_t> bonuses;
    std::vector<int16_t> scoreMatrix;
    std::vector<int16_t> consecutiveCharMatrix;
    std::vector<size_t> allUtf32Pos;
    std::vector<size_t> termUtf32Pos;
};

struct MatcherContext
{
    PreparedText preparedText;
    MatchScratch scratch;
};

enum class CharClass : uint8_t
{
    NonWord = 0,
    CharLower = 1,
    CharUpper = 2,
    Digit = 3,
};

static MatcherContext& matcherContext()
{
    thread_local MatcherContext context;
    return context;
}

static void utf16ToUtf32(std::wstring_view text, std::vector<UChar32>& out)
{
    const auto* data = reinterpret_cast<const UChar*>(text.data());
    const auto dataLen = static_cast<int32_t>(text.size());
    const auto cpCount = u_countChar32(data, dataLen);

    out.resize(cpCount);
    if (cpCount == 0)
    {
        return;
    }

    UErrorCode status = U_ZERO_ERROR;
    u_strToUTF32(out.data(), static_cast<int32_t>(out.size()), nullptr, data, dataLen, &status);
    //THROW_HR_IF(E_UNEXPECTED, status > U_ZERO_ERROR);
}

static void foldStringUtf32(std::span<const UChar32> source, std::vector<UChar32>& out)
{
    out.resize(source.size());
    for (size_t i = 0; i < source.size(); ++i)
    {
        out[i] = u_foldCase(source[i], U_FOLD_CASE_DEFAULT);
    }
}

static void foldStringUtf32(std::vector<UChar32>& str)
{
    for (auto& cp : str)
    {
        cp = u_foldCase(cp, U_FOLD_CASE_DEFAULT);
    }
}

static void prepareText(std::wstring_view text, PreparedText& preparedText)
{
    utf16ToUtf32(text, preparedText.codePoints);
    foldStringUtf32(preparedText.codePoints, preparedText.foldedCodePoints);
}

static size_t trySkip(std::span<const UChar32> input, const UChar32 searchChar, size_t startIndex)
{
    for (size_t i = startIndex; i < input.size(); ++i)
    {
        if (input[i] == searchChar)
        {
            return i;
        }
    }
    return npos;
}

// Unlike the equivalent in fzf, this one does more than Unicode.
static size_t asciiFuzzyIndex(std::span<const UChar32> input, std::span<const UChar32> pattern)
{
    size_t idx = 0;
    size_t firstIdx = 0;
    for (size_t pi = 0; pi < pattern.size(); ++pi)
    {
        idx = trySkip(input, pattern[pi], idx);
        if (idx == npos)
        {
            return npos;
        }

        if (pi == 0 && idx > 0)
        {
            firstIdx = idx - 1;
        }

        idx++;
    }
    return firstIdx;
}

static int16_t calculateBonus(CharClass prevClass, CharClass currentClass)
{
    if (prevClass == CharClass::NonWord && currentClass != CharClass::NonWord)
    {
        return BoundaryBonus;
    }
    if ((prevClass == CharClass::CharLower && currentClass == CharClass::CharUpper) ||
        (prevClass != CharClass::Digit && currentClass == CharClass::Digit))
    {
        return CamelCaseBonus;
    }
    if (currentClass == CharClass::NonWord)
    {
        return NonWordBonus;
    }
    return 0;
}

static constexpr auto s_charClassLut = []() {
    std::array<CharClass, U_CHAR_CATEGORY_COUNT> lut{};
    lut.fill(CharClass::NonWord);
    lut[U_UPPERCASE_LETTER] = CharClass::CharUpper;
    lut[U_LOWERCASE_LETTER] = CharClass::CharLower;
    lut[U_MODIFIER_LETTER] = CharClass::CharLower;
    lut[U_OTHER_LETTER] = CharClass::CharLower;
    lut[U_DECIMAL_DIGIT_NUMBER] = CharClass::Digit;
    return lut;
}();

static CharClass classOf(UChar32 /*ch*/)
{
    return CharClass::CharLower;
}

static int32_t fzfFuzzyMatchV2(const PreparedText& text, const std::vector<UChar32>& pattern, MatchScratch& scratch, std::vector<size_t>* pos)
{
    if (pattern.size() == 0)
    {
        return 0;
    }

    const auto textSize = text.codePoints.size();
    size_t firstIndexOf = asciiFuzzyIndex(text.foldedCodePoints, pattern);
    if (firstIndexOf == npos)
    {
        return 0;
    }

    scratch.initialScores.resize(textSize);
    scratch.consecutiveScores.resize(textSize);
    scratch.firstOccurrenceOfEachChar.resize(pattern.size());
    scratch.bonuses.resize(textSize);

    int16_t maxScore = 0;
    size_t maxScorePos = 0;
    size_t patternIndex = 0;
    size_t lastIndex = 0;
    UChar32 firstPatternChar = pattern[0];
    UChar32 currentPatternChar = pattern[0];
    int16_t previousInitialScore = 0;
    CharClass previousClass = CharClass::NonWord;
    bool inGap = false;

    std::span<const UChar32> lowerText(text.foldedCodePoints);
    auto lowerTextSlice = lowerText.subspan(firstIndexOf);
    auto initialScoresSlice = std::span(scratch.initialScores).subspan(firstIndexOf);
    auto consecutiveScoresSlice = std::span(scratch.consecutiveScores).subspan(firstIndexOf);
    auto bonusesSlice = std::span(scratch.bonuses).subspan(firstIndexOf, textSize - firstIndexOf);

    for (size_t i = 0; i < lowerTextSlice.size(); i++)
    {
        const auto currentChar = lowerTextSlice[i];
        const auto currentClass = classOf(text.codePoints[i + firstIndexOf]);
        const auto bonus = calculateBonus(previousClass, currentClass);
        bonusesSlice[i] = bonus;
        previousClass = currentClass;

        //currentPatternChar was already folded in ParsePattern
        if (currentChar == currentPatternChar)
        {
            if (patternIndex < pattern.size())
            {
                scratch.firstOccurrenceOfEachChar[patternIndex] = firstIndexOf + i;
                patternIndex++;
                if (patternIndex < pattern.size())
                {
                    currentPatternChar = pattern[patternIndex];
                }
            }
            lastIndex = firstIndexOf + i;
        }
        if (currentChar == firstPatternChar)
        {
            int16_t score = ScoreMatch + bonus * BonusFirstCharMultiplier;
            initialScoresSlice[i] = score;
            consecutiveScoresSlice[i] = 1;
            if (pattern.size() == 1 && (score > maxScore))
            {
                maxScore = score;
                maxScorePos = firstIndexOf + i;
                if (bonus == BoundaryBonus)
                {
                    break;
                }
            }
            inGap = false;
        }
        else
        {
            initialScoresSlice[i] = std::max<int16_t>(previousInitialScore + (inGap ? ScoreGapExtension : ScoreGapStart), 0);
            consecutiveScoresSlice[i] = 0;
            inGap = true;
        }
        previousInitialScore = initialScoresSlice[i];
    }

    if (patternIndex != pattern.size())
    {
        return 0;
    }

    if (pattern.size() == 1)
    {
        if (pos)
        {
            pos->push_back(maxScorePos);
        }
        return maxScore;
    }

    const auto firstOccurrenceOfFirstChar = scratch.firstOccurrenceOfEachChar[0];
    const auto width = lastIndex - firstOccurrenceOfFirstChar + 1;
    const auto rows = pattern.size();
    const auto consecutiveCharMatrixSize = width * pattern.size();

    scratch.scoreMatrix.resize(width * rows);
    std::copy_n(scratch.initialScores.begin() + static_cast<std::ptrdiff_t>(firstOccurrenceOfFirstChar), width, scratch.scoreMatrix.begin());
    std::span scoreSpan(scratch.scoreMatrix);

    scratch.consecutiveCharMatrix.resize(width * rows);
    std::copy_n(scratch.consecutiveScores.begin() + static_cast<std::ptrdiff_t>(firstOccurrenceOfFirstChar), width, scratch.consecutiveCharMatrix.begin());
    std::span consecutiveCharMatrixSpan(scratch.consecutiveCharMatrix);

    auto patternSliceStr = std::span(pattern).subspan(1);

    for (size_t off = 0; off < pattern.size() - 1; off++)
    {
        auto patternCharOffset = scratch.firstOccurrenceOfEachChar[off + 1];
        auto sliceLen = lastIndex - patternCharOffset + 1;
        currentPatternChar = patternSliceStr[off];
        patternIndex = off + 1;
        auto row = patternIndex * width;
        inGap = false;
        std::span<const UChar32> textSlice = lowerText.subspan(patternCharOffset, sliceLen);
        std::span bonusSlice(scratch.bonuses.begin() + static_cast<std::ptrdiff_t>(patternCharOffset), textSlice.size());
        std::span<int16_t> consecutiveCharMatrixSlice = consecutiveCharMatrixSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar, textSlice.size());
        std::span<int16_t> consecutiveCharMatrixDiagonalSlice = consecutiveCharMatrixSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar - 1 - width, textSlice.size());
        std::span<int16_t> scoreMatrixSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar, textSlice.size());
        std::span<int16_t> scoreMatrixDiagonalSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar - 1 - width, textSlice.size());
        std::span<int16_t> scoreMatrixLeftSlice = scoreSpan.subspan(row + patternCharOffset - firstOccurrenceOfFirstChar - 1, textSlice.size());

        if (!scoreMatrixLeftSlice.empty())
        {
            scoreMatrixLeftSlice[0] = 0;
        }

        for (size_t j = 0; j < textSlice.size(); j++)
        {
            const auto currentChar = textSlice[j];
            const auto column = patternCharOffset + j;
            const int16_t score = inGap ? scoreMatrixLeftSlice[j] + ScoreGapExtension : scoreMatrixLeftSlice[j] + ScoreGapStart;
            int16_t diagonalScore = 0;
            int16_t consecutive = 0;
            if (currentChar == currentPatternChar)
            {
                diagonalScore = scoreMatrixDiagonalSlice[j] + ScoreMatch;
                int16_t bonus = bonusSlice[j];
                consecutive = consecutiveCharMatrixDiagonalSlice[j] + 1;
                if (bonus == BoundaryBonus)
                {
                    consecutive = 1;
                }
                else if (consecutive > 1)
                {
                    bonus = std::max({ bonus, BonusConsecutive, scratch.bonuses[column - consecutive + 1] });
                }
                if (diagonalScore + bonus < score)
                {
                    diagonalScore += bonusSlice[j];
                    consecutive = 0;
                }
                else
                {
                    diagonalScore += bonus;
                }
            }
            consecutiveCharMatrixSlice[j] = consecutive;
            inGap = (diagonalScore < score);
            int16_t cellScore = std::max(int16_t{ 0 }, std::max(diagonalScore, score));
            if (off + 2 == pattern.size() && cellScore > maxScore)
            {
                maxScore = cellScore;
                maxScorePos = column;
            }
            scoreMatrixSlice[j] = cellScore;
        }
    }

    size_t currentColIndex = maxScorePos;
    if (pos)
    {
        patternIndex = pattern.size() - 1;
        bool preferCurrentMatch = true;
        while (true)
        {
            const auto rowStartIndex = patternIndex * width;
            const auto colOffset = currentColIndex - firstOccurrenceOfFirstChar;
            const auto cellScore = scratch.scoreMatrix[rowStartIndex + colOffset];
            int32_t diagonalCellScore = 0;
            int32_t leftCellScore = 0;

            if (patternIndex > 0 && currentColIndex >= scratch.firstOccurrenceOfEachChar[patternIndex])
            {
                diagonalCellScore = scratch.scoreMatrix[rowStartIndex - width + colOffset - 1];
            }
            if (currentColIndex > scratch.firstOccurrenceOfEachChar[patternIndex])
            {
                leftCellScore = scratch.scoreMatrix[rowStartIndex + colOffset - 1];
            }

            if (cellScore > diagonalCellScore &&
                (cellScore > leftCellScore || (cellScore == leftCellScore && preferCurrentMatch)))
            {
                pos->push_back(currentColIndex);
                if (patternIndex == 0)
                {
                    break;
                }
                patternIndex--;
            }

            currentColIndex--;
            if (rowStartIndex + colOffset >= consecutiveCharMatrixSize)
            {
                break;
            }

            preferCurrentMatch = (scratch.consecutiveCharMatrix[rowStartIndex + colOffset] > 1) ||
                                 ((rowStartIndex + width + colOffset + 1 <
                                    consecutiveCharMatrixSize) &&
                                  (scratch.consecutiveCharMatrix[rowStartIndex + width + colOffset + 1] > 0));
        }
    }
    return maxScore;
}

static int32_t suffixMatch(const PreparedText& text, const std::vector<UChar32>& pattern, std::vector<size_t>* pos)
{
    if (pattern.size() == 0)
    {
        return 0;
    }

    auto textLen = text.foldedCodePoints.size();
    while (textLen > 0 && text.foldedCodePoints[textLen - 1] == U' ')
    {
        --textLen;
    }

    if (pattern.size() > textLen)
    {
        return 0;
    }

    const size_t startPos = textLen - pattern.size();
    bool matches = true;

    for (size_t i = 0; i < pattern.size(); ++i)
    {
        if (text.foldedCodePoints[startPos + i] != pattern[i])
        {
            matches = false;
            break;
        }
    }

    if (!matches)
    {
        return 0;
    }

    int32_t score = ScoreMatch * static_cast<int32_t>(pattern.size());
    
    if (pos)
    {
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pos->push_back(startPos + i);
        }
    }

    return score;
}

static bool containsFolded(const PreparedText& text, const std::vector<UChar32>& pattern)
{
    //This should never happen.  The pattern parser would not create a empty term
    if (pattern.empty())
    {
        return true;
    }

    if (pattern.size() > text.foldedCodePoints.size())
    {
        return false;
    }

    auto it = std::ranges::search(text.foldedCodePoints, pattern).begin();

    return it != text.foldedCodePoints.end();
}

// Fast-path substring match with case folding. Unlike containsFolded this
// returns a score and the positions of the matched code points so the caller
// can build highlight runs. Mirrors prefixMatch/suffixMatch.
static int32_t containsMatch(const PreparedText& text, const std::vector<UChar32>& pattern, std::vector<size_t>* pos)
{
    if (pattern.size() == 0)
    {
        return 0;
    }

    if (pattern.size() > text.foldedCodePoints.size())
    {
        return 0;
    }

    const auto matchRange = std::ranges::search(text.foldedCodePoints, pattern);
    const auto it = matchRange.begin();
    if (it == text.foldedCodePoints.end())
    {
        return 0;
    }

    const size_t startPos = static_cast<size_t>(std::distance(text.foldedCodePoints.begin(), it));

    int32_t score = ScoreMatch * static_cast<int32_t>(pattern.size());

    if (pos)
    {
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pos->push_back(startPos + i);
        }
    }

    return score;
}

static int32_t prefixMatch(const PreparedText& text, const std::vector<UChar32>& pattern, std::vector<size_t>* pos)
{
    if (pattern.size() == 0)
    {
        return 0;
    }

    if (pattern.size() > text.foldedCodePoints.size())
    {
        return 0;
    }

    bool matches = true;
    for (size_t i = 0; i < pattern.size(); ++i)
    {
        if (text.foldedCodePoints[i] != pattern[i])
        {
            matches = false;
            break;
        }
    }

    if (!matches)
    {
        return 0;
    }

    int32_t score = ScoreMatch * static_cast<int32_t>(pattern.size());

    if (pos)
    {
        for (size_t i = 0; i < pattern.size(); ++i)
        {
            pos->push_back(i);
        }
    }

    return score;
}

Pattern fzfcpp::matcher::ParsePattern(const std::wstring_view patternStr)
{
    Pattern patObj;
    size_t pos = 0;

    while (true)
    {
        const auto beg = patternStr.find_first_not_of(L' ', pos);
        if (beg == std::wstring_view::npos)
        {
            break;
        }

        const auto end = std::min(patternStr.size(), patternStr.find_first_of(L' ', beg));
        const auto word = patternStr.substr(beg, end - beg);
        std::vector<UChar32> codePoints;
        utf16ToUtf32(word, codePoints);
        foldStringUtf32(codePoints);
        patObj.terms.push_back(std::move(codePoints));
        pos = end;
    }

    return patObj;
}

Pattern fzfcpp::matcher::ParsePatternWithTypes(const std::wstring_view patternStr)
{
    Pattern patObj;
    size_t pos = 0;

    while (true)
    {
        const auto beg = patternStr.find_first_not_of(L' ', pos);
        if (beg == std::wstring_view::npos)
        {
            break;
        }

        const auto end = std::min(patternStr.size(), patternStr.find_first_of(L' ', beg));
        auto word = patternStr.substr(beg, end - beg);

        Term term;
        term.type = MatchType::Fuzzy;

        if (!word.empty() && word[0] == L'@')
        {
            term.location = Location::Context;
            word = word.substr(1);
        }
        
        if (true == false && !word.empty() && word[0] == L'!')
        {
            term.type = MatchType::NotContains;
            word = word.substr(1);
        }
        else if (!word.empty() && word[0] == L'^')
        {
            term.type = MatchType::Prefix;
            word = word.substr(1);
        }
        else if (!word.empty() && word.back() == L'$')
        {
            term.type = MatchType::Suffix;
            word = word.substr(0, word.size() - 1);
        }

        if (!word.empty())
        {
            utf16ToUtf32(word, term.codePoints);
            foldStringUtf32(term.codePoints);
            patObj.typedTerms.push_back(std::move(term));
        }

        pos = end;
    }

    return patObj;
}

// Like ParsePatternWithTypes, but defaults to MatchType::Contains instead of
// Fuzzy. Supports !, ^, and $ sigils for negation, prefix, and suffix matching.
// Callers that only need plain substring matching can use this to skip the
// expensive fuzzy matcher entirely.
Pattern fzfcpp::matcher::ParsePatternContainsOnly(const std::wstring_view patternStr)
{
    Pattern patObj;
    size_t pos = 0;

    while (true)
    {
        const auto beg = patternStr.find_first_not_of(L' ', pos);
        if (beg == std::wstring_view::npos)
        {
            break;
        }

        const auto end = std::min(patternStr.size(), patternStr.find_first_of(L' ', beg));
        auto word = patternStr.substr(beg, end - beg);

        Term term;
        term.type = MatchType::Contains;

        if (!word.empty() && word[0] == L'!')
        {
            term.type = MatchType::NotContains;
            word = word.substr(1);
        }
        else if (!word.empty() && word[0] == L'^')
        {
            term.type = MatchType::Prefix;
            word = word.substr(1);
        }
        else if (!word.empty() && word.back() == L'$')
        {
            term.type = MatchType::Suffix;
            word = word.substr(0, word.size() - 1);
        }

        if (!word.empty())
        {
            utf16ToUtf32(word, term.codePoints);
            foldStringUtf32(term.codePoints);
            patObj.typedTerms.push_back(std::move(term));
        }

        pos = end;
    }

    return patObj;
}

static std::optional<int32_t> matchPrepared(const PreparedText& preparedText, const Pattern& pattern, MatchScratch& scratch, std::vector<size_t>* allUtf32Pos)
{
    if (allUtf32Pos)
    {
        allUtf32Pos->clear();
    }

    int32_t totalScore = 0;

    const auto scoreAndCollect = [&](const auto& term, const MatchType type) -> std::optional<int32_t> {
        auto* termPos = allUtf32Pos ? &scratch.termUtf32Pos : nullptr;
        if (termPos)
        {
            termPos->clear();
        }

        int32_t score = 0;
        if (type == MatchType::Suffix)
        {
            score = suffixMatch(preparedText, term, termPos);
        }
        else if (type == MatchType::Prefix)
        {
            score = prefixMatch(preparedText, term, termPos);
        }
        else if (type == MatchType::NotContains)
        {
            score = containsFolded(preparedText, term) ? 0 : 1;
        }
        else if (type == MatchType::Contains)
        {
            score = containsMatch(preparedText, term, termPos);
        }
        else
        {
            score = fzfFuzzyMatchV2(preparedText, term, scratch, termPos);
        }

        if (score <= 0)
        {
            return std::nullopt;
        }

        if (allUtf32Pos && type != MatchType::NotContains)
        {
            allUtf32Pos->insert(allUtf32Pos->end(), termPos->begin(), termPos->end());
        }

        return score;
    };

    if (!pattern.typedTerms.empty())
    {
        for (const auto& term : pattern.typedTerms)
        {
            const auto score = scoreAndCollect(term.codePoints, term.type);
            if (!score)
            {
                return std::nullopt;
            }

            if (term.type != MatchType::NotContains)
            {
                totalScore += *score;
            }
        }
    }
    else
    {
        for (const auto& term : pattern.terms)
        {
            const auto score = scoreAndCollect(term, MatchType::Fuzzy);
            if (!score)
            {
                return std::nullopt;
            }

            totalScore += *score;
        }
    }

    return totalScore;
}

std::optional<MatchResult> fzfcpp::matcher::Match(std::wstring_view text, const Pattern& pattern)
{
    if (pattern.typedTerms.empty() && pattern.terms.empty())
    {
        return MatchResult{};
    }

    auto& context = matcherContext();
    prepareText(text, context.preparedText);

    auto totalScore = matchPrepared(context.preparedText, pattern, context.scratch, &context.scratch.allUtf32Pos);
    if (!totalScore)
    {
        return std::nullopt;
    }

    auto& allUtf32Pos = context.scratch.allUtf32Pos;
    std::ranges::sort(allUtf32Pos);
    allUtf32Pos.erase(std::ranges::unique(allUtf32Pos).begin(), allUtf32Pos.end());

    std::vector<TextRun> runs;
    runs.reserve(allUtf32Pos.size());
    std::size_t nextCodePointPos = 0;
    size_t utf16Offset = 0;

    bool inRun = false;
    size_t runStart = 0;

    for (size_t cpIndex = 0; cpIndex < context.preparedText.codePoints.size(); cpIndex++)
    {
        const auto cp = context.preparedText.codePoints[cpIndex];
        const size_t cpWidth = U16_LENGTH(cp);

        const bool isMatch = (nextCodePointPos < allUtf32Pos.size() && allUtf32Pos[nextCodePointPos] == cpIndex);
        if (isMatch)
        {
            if (!inRun)
            {
                runStart = utf16Offset;
                inRun = true;
            }
            nextCodePointPos++;
        }
        else if (inRun)
        {
            runs.push_back({ runStart, utf16Offset - 1 });
            inRun = false;
        }

        utf16Offset += cpWidth;
    }

    if (inRun)
    {
        runs.push_back({ runStart, utf16Offset - 1 });
    }

    return MatchResult{ *totalScore, std::move(runs) };
}

std::optional<int32_t> fzfcpp::matcher::Score(std::wstring_view text, const Pattern& pattern)
{
    if (pattern.typedTerms.empty() && pattern.terms.empty())
    {
        return 0;
    }

    auto& context = matcherContext();
    prepareText(text, context.preparedText);
    return matchPrepared(context.preparedText, pattern, context.scratch, nullptr);
}

std::optional<TokenMatchResult> fzfcpp::matcher::MatchToken(std::wstring_view token, std::wstring_view context, const Pattern& pattern)
{
    if (pattern.typedTerms.empty() && pattern.terms.empty())
    {
        return TokenMatchResult{};
    }

    std::vector<Term> contextTerms;
    std::vector<Term> tokenTerms;

    for (auto term : pattern.typedTerms)
    {
        if (term.location == Location::Token)
        {
            tokenTerms.emplace_back(term);
        }
        else
        {
            contextTerms.emplace_back(term);
        }
    }

    auto expectsMatchOnToken = tokenTerms.size() > 0;
    auto expectsMatchOnContext = contextTerms.size() > 0;

    auto contextResult = Match(context, Pattern{ {}, std::move(contextTerms) });
    auto tokenResult = Match(token, Pattern{ {}, std::move(tokenTerms) });

    return TokenMatchResult
    {
        expectsMatchOnToken,
        expectsMatchOnContext,
        tokenResult,
        contextResult
    };
}
