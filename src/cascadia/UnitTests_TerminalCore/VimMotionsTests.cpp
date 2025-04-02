/*
* Copyright (c) Microsoft Corporation.
* Licensed under the MIT license.
*
* This File was generated using the VisualTAEF C++ Project Wizard.
* Class Name: SelectionTest
*/
#include "pch.h"
#include <WexTestClass.h>

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "../cascadia/TerminalCore/lib/VimMotions.hpp"
#include "../renderer/inc/DummyRenderer.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::Terminal::Core;
using namespace winrt::Microsoft::Terminal::Core;

namespace VimMotionsTests
{
    class VimSelectionTest
    {
        TEST_CLASS(VimSelectionTest);

        // Method Description:
        // - Validate a selection that spans only one row
        // Arguments:
        // - term: the terminal that is contains the selection
        // - expected: the expected value of the selection rect
        // Return Value:
        // - N/A
        void ValidateLinearSelection(Terminal& term, const til::point start, const til::point end)
        {
            // Simulate renderer calling TriggerSelection and acquiring selection area
            auto selectionSpans = term.GetSelectionSpans();

            // Validate selection area
            VERIFY_ARE_EQUAL(selectionSpans.size(), static_cast<size_t>(1));

            auto& sp{ selectionSpans[0] };
            VERIFY_ARE_EQUAL(start, sp.start, L"start");
            VERIFY_ARE_EQUAL(end, sp.end, L"end");
        }

        TextBuffer& GetTextBuffer(Terminal& term)
        {
            return term.GetBufferAndViewport().buffer;
        }

        TEST_METHOD(SelectLastNonSpaceChar)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            auto textLength = static_cast<int>(text.length());

            ValidateLinearSelection(term, { textLength - 1, 0 }, { static_cast<int>(text.length()), 0 });
        }

        TEST_METHOD(MoveLeft)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);

            vim::motions::MoveLeft(term, false);

            ValidateLinearSelection(term, { textLength - 2, 0 }, {textLength - 1, 0});
        }

        TEST_METHOD(MoveRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveLeft(term, false);
            ValidateLinearSelection(term, { textLength - 2, 0 }, {textLength - 1, 0});

            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { textLength - 1, 0 }, {textLength - 0, 0});
        }

        TEST_METHOD(MoveLeft_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveLeft(term, true);

            ValidateLinearSelection(term, { textLength - 2, 0 }, {textLength, 0});
        }

        TEST_METHOD(MoveDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            ValidateLinearSelection(term, { textLength - 1, 1 }, { textLength, 1 });

            vim::motions::MoveUp(term, false);
            ValidateLinearSelection(term, { textLength - 1, 0 }, { textLength, 0 });

            vim::motions::MoveDown(term, false);
            ValidateLinearSelection(term, { textLength - 1, 1 }, { textLength, 1 });
        }

        TEST_METHOD(MoveUp)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            ValidateLinearSelection(term, { textLength - 1, 1 }, { textLength, 1 });

            vim::motions::MoveUp(term, false);
            ValidateLinearSelection(term, { textLength - 1, 0 }, { textLength, 0 });
        }

        TEST_METHOD(MoveToStartOfLine)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
        }

        TEST_METHOD(MoveToStartOfLine_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, true);

            ValidateLinearSelection(term, { 0, 0 }, {textLength, 0});
        }

        TEST_METHOD(MoveToEndOfLine)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveToEndOfLine(term, false);

            ValidateLinearSelection(term, { textLength - 1, 0 }, {textLength, 0});
        }

        TEST_METHOD(MoveToEndOfLine_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveToEndOfLine(term, true);

            ValidateLinearSelection(term, { 0, 0 }, {textLength, 0});
        }

        TEST_METHOD(TilChar)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::TilChar(term, L"i", false);

            ValidateLinearSelection(term, { 6, 0 }, {7, 0});
        }

        TEST_METHOD(TilChar_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::TilChar(term, L"i", true);

            ValidateLinearSelection(term, { 0, 0 }, {7, 0});
        }

        TEST_METHOD(FindChar)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::FindChar(term, L"i", false);

            ValidateLinearSelection(term, { 7, 0 }, {8, 0});
        }

        TEST_METHOD(FindChar_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::FindChar(term, L"i", true);

            ValidateLinearSelection(term, { 0, 0 }, {8, 0});
        }

        TEST_METHOD(FindChar_Backwards)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::FindCharBackwards(term, L"i", false);

            ValidateLinearSelection(term, { 7, 0 }, {8, 0});
        }

        TEST_METHOD(FindChar_Backwards_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::FindCharBackwards(term, L"i", true);

            ValidateLinearSelection(term, { 7, 0 }, {textLength, 0});
        }

        TEST_METHOD(TilChar_Backwards)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::TilCharBackwards(term, L"i", false);

            ValidateLinearSelection(term, { 8, 0 }, {9, 0});
        }

        TEST_METHOD(TilChar_Backwards_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::TilCharBackwards(term, L"i", true);

            ValidateLinearSelection(term, { 8, 0 }, {textLength, 0});
        }

        TEST_METHOD(MoveWordLeft)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveWordLeft(term, false, false);

            ValidateLinearSelection(term, { 3, 0 }, {4, 0});

            vim::motions::MoveWordLeft(term, false, false);

            ValidateLinearSelection(term, { 2, 0 }, {3, 0});

            vim::motions::MoveWordLeft(term, false, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
        }

        TEST_METHOD(MoveWordLeft_RegularWords)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is a test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveWordLeft(term, false, false);

            ValidateLinearSelection(term, { 10, 0 }, {11, 0});

            vim::motions::MoveWordLeft(term, false, false);

            ValidateLinearSelection(term, { 8, 0 }, {9, 0});

            vim::motions::MoveWordLeft(term, false, false);

            ValidateLinearSelection(term, { 5, 0 }, {6, 0});

            vim::motions::MoveWordLeft(term, false, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
        }

        TEST_METHOD(MoveWordLeft_LargeWords)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveWordLeft(term, true, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
        }

        TEST_METHOD(MoveWordRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveWordRight(term, false, false);

            //This should stop on the \ instead of the :
            ValidateLinearSelection(term, { 1, 0 }, {2, 0});

            vim::motions::MoveWordRight(term, false, false);

            ValidateLinearSelection(term, { 10, 0 }, {11, 0});

            vim::motions::MoveWordRight(term, false, false);

            ValidateLinearSelection(term, { 11, 0 }, {12, 0});
        }

        TEST_METHOD(MoveWordRight_LargeWord)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveWordRight(term, true, false);

            ValidateLinearSelection(term, { 11, 0 }, {12, 0});
        }

        TEST_METHOD(MoveWordRight_RegularWords)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is a test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveWordRight(term, false, false);

            //This should stop on the \ instead of the :
            ValidateLinearSelection(term, { 3, 0 }, {4, 0});

            vim::motions::MoveWordRight(term, false, false);

            ValidateLinearSelection(term, { 6, 0 }, {7, 0});

            vim::motions::MoveWordRight(term, false, false);

            ValidateLinearSelection(term, { 8, 0 }, {9, 0});

            vim::motions::MoveWordRight(term, false, false);

            ValidateLinearSelection(term, { 13, 0 }, {14, 0});
        }

        TEST_METHOD(MoveWordStartRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveWordStartRight(term, false, false);

            ValidateLinearSelection(term, { 1, 0 }, {2, 0});

            vim::motions::MoveWordStartRight(term, false, false);

            ValidateLinearSelection(term, { 2, 0 }, {3, 0});

            vim::motions::MoveWordStartRight(term, false, false);

            ValidateLinearSelection(term, { 11, 0 }, {12, 0});
        }

        TEST_METHOD(MoveWordStartRight_LargeWord)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveWordStartRight(term, true, false);

            ValidateLinearSelection(term, { 0, 1 }, {1, 1});
        }

        TEST_METHOD(InWord_EndOfWord)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveLeft(term, false);

            vim::motions::SelectInWord(term, false);

            ValidateLinearSelection(term, { 3, 0 }, {11, 0});
        }

        TEST_METHOD(InWord_InsideOfWord)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);

            vim::motions::SelectInWord(term, false);

            ValidateLinearSelection(term, { 3, 0 }, {11, 0});
        }
    };
}
