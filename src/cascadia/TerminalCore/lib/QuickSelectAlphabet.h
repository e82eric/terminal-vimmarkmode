#pragma once
namespace Microsoft::Console::Render
{
    struct QuickSelectChar;
    struct QuickSelectSelection;
}

class QuickSelectAlphabet
{
    bool _enabled = false;
    std::vector<wchar_t> _quickSelectAlphabet;
    std::unordered_map<wchar_t, int16_t> _quickSelectAlphabetMap;
    std::wstring _chars;

public:
    QuickSelectAlphabet();
    bool Enabled();
    void Enabled(bool val);
    void AppendChar(wchar_t* ch);
    void RemoveChar();
    void ClearChars();
    std::vector<Microsoft::Console::Render::QuickSelectSelection> GetQuickSelectChars(int32_t number) noexcept;
    bool AllCharsSet(int32_t number);
    int32_t GetIndexForChars();
};
