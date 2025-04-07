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

            for (auto i = textLength - 1; i >= 1; i--)
            {
                vim::motions::MoveLeft(term, false);
                ValidateLinearSelection(term, { i - 1, 0 }, { i, 0 });
            }

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
            vim::motions::MoveLeft(term, false);
            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
        }

        TEST_METHOD(MoveLeft_VisualExtension)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);

            for (auto i = textLength - 1; i >= 1; i--)
            {
                vim::motions::MoveLeft(term, true);
                ValidateLinearSelection(term, { i - 1, 0 }, { textLength, 0 });
            }

            ValidateLinearSelection(term, { 0, 0 }, {textLength, 0});
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {textLength, 0});
        }

        TEST_METHOD(MoveRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            auto width = 100;
            term.Create({ width, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            for (auto i = 1; i < width; i++)
            {
                vim::motions::MoveRight(term, false);
                ValidateLinearSelection(term, { i, 0 }, { i + 1, 0 });
            }

            ValidateLinearSelection(term, { width - 1, 0 }, {width, 0});
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { width - 1, 0 }, { width, 0 });
        }

        TEST_METHOD(MoveRight_VisualExtension)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            auto width = 100;
            term.Create({ width, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);

            for (auto i = 1; i < width; i++)
            {
                vim::motions::MoveRight(term, true);
                ValidateLinearSelection(term, { 0, 0 }, { i + 1, 0 });
            }

            ValidateLinearSelection(term, { 0, 0 }, {width, 0});
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {width, 0});
        }

        TEST_METHOD(MoveRight_ThenMoveLeftCrossingPivot_Visually)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            auto width = 100;
            term.Create({ width, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);

            for (auto i = 0; i < 5; i++)
            {
                vim::motions::MoveRight(term, true);
                ValidateLinearSelection(term, { textLength - 1, 0 }, { textLength + 1 + i, 0 });
            }

            for (auto i = 0; i < 5; i++)
            {
                vim::motions::MoveLeft(term, true);
                ValidateLinearSelection(term, { textLength - 1, 0 }, { textLength + 1 + 3 - i, 0 });
            }

            for (auto i = 0; i < 5; i++)
            {
                vim::motions::MoveLeft(term, true);
                ValidateLinearSelection(term, { textLength - 2 - i, 0 }, { textLength, 0 });
            }
        }

        TEST_METHOD(MoveLeft_ThenMoveRightCrossingPivot_Visually)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            auto width = 100;
            term.Create({ width, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);

            for (auto i = 1; i < 5; i++)
            {
                vim::motions::MoveLeft(term, true);
                ValidateLinearSelection(term, { textLength - 1 - i, 0 }, { textLength, 0 });
            }

            for (auto i = 1; i < 5; i++)
            {
                vim::motions::MoveRight(term, true);
                ValidateLinearSelection(term, { textLength - 5 + i, 0 }, { textLength, 0 });
            }

            for (auto i = 1; i < 5; i++)
            {
                vim::motions::MoveLeft(term, true);
                ValidateLinearSelection(term, { textLength - 1 - i, 0 }, { textLength, 0 });
            }
        }

        TEST_METHOD(MoveRight_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {2, 0});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {3, 0});
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

        TEST_METHOD(MoveToStartOfLine2)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 2 }, {1, 2});
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

        TEST_METHOD(MoveToFirstNonBlankChar)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToFirstNonBlankChar(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
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
            ValidateLinearSelection(term, { 1, 0 }, {2, 0});

            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
        }

        TEST_METHOD(MoveWordLeft2)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is a test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            ValidateLinearSelection(term, { 13, 0 }, {14, 0});

            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 10, 0 }, {11, 0});

            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 8, 0 }, {9, 0});

            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 5, 0 }, {6, 0});

            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
        }

        TEST_METHOD(MoveWordRight_ThenMoveLeft_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is a test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveWordLeft(term, false, false);
            vim::motions::MoveWordLeft(term, false, false);
            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 5, 0 }, {6, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 5, 0 }, {7, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 5, 0 }, {9, 0});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 5, 0 }, {6, 0});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 0, 0 }, {6, 0});
        }

        TEST_METHOD(MoveWordLeft_MultipleLines)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"";
            const std::wstring_view text3 = L"another test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectLastNonSpaceChar(term);
            ValidateLinearSelection(term, { 16, 2 }, { 17, 2 });

            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 13, 2 }, { 14, 2 });

            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 8, 2 }, { 9, 2 });

            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 0, 2 }, { 1, 2 });

            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 0, 1 }, { 1, 1 });

            vim::motions::MoveWordLeft(term, false, false);
            ValidateLinearSelection(term, { 10, 0 }, { 11, 0 });
        }

        TEST_METHOD(MoveWordLeft_MultipleLines_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"";
            const std::wstring_view text3 = L"another test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectLastNonSpaceChar(term);
            ValidateLinearSelection(term, { 16, 2 }, { 17, 2 });

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 13, 2 }, { 17, 2 });

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 8, 2 }, { 17, 2 });

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 0, 2 }, { 17, 2 });

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 0, 1 }, { 17, 2 });

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 10, 0 }, { 17, 2 });

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 5, 0 }, { 17, 2 });

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 0, 0 }, { 17, 2 });

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 0, 0 }, { 17, 2 });
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

            ValidateLinearSelection(term, { 2, 0 }, {3, 0});

            vim::motions::MoveWordRight(term, false, false);

            ValidateLinearSelection(term, { 10, 0 }, {11, 0});

            vim::motions::MoveWordRight(term, false, false);

            ValidateLinearSelection(term, { 11, 0 }, {12, 0});
        }

        TEST_METHOD(MoveWordRight_Visual)
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

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 0, 0 }, {3, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 0, 0 }, {11, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 0, 0 }, {12, 0});
        }

        TEST_METHOD(MoveWordRight_Visual_CursorAtEnd)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is a test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 6, 0 }, {7, 0});

            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {7, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 3, 0 }, {7, 0});

            //vim::motions::MoveWordRight(term, false, true);
            //ValidateLinearSelection(term, { 0, 0 }, {11, 0});

            //vim::motions::MoveWordRight(term, false, true);
            //ValidateLinearSelection(term, { 0, 0 }, {12, 0});
        }

        TEST_METHOD(MoveWordRight_MultipleLines)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"";
            const std::wstring_view text3 = L"another test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });

            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 3, 0 }, {4, 0});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 8, 0 }, {9, 0});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 10, 0 }, {11, 0});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 6, 2 }, {7, 2});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 11, 2 }, {12, 2});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 16, 2 }, {17, 2});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 16, 2 }, {17, 2});
        }

        TEST_METHOD(MoveWordRight_MultipleLines_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"";
            const std::wstring_view text3 = L"another test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });

            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 0, 0}, {4, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 0, 0}, {9, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 0, 0}, {11, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 0, 0}, {7, 2});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 0, 0}, {12, 2});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 0, 0}, {17, 2});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 0, 0}, {17, 2});
        }

        TEST_METHOD(MoveWordRight_MultipleLines_Spaces)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test   line 1";
            const std::wstring_view text2 = L"  ";
            const std::wstring_view text3 = L"  another test line";
            const std::wstring_view text4 = L"  ";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });

            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 3, 0 }, {4, 0});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 10, 0 }, {11, 0});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 12, 0 }, {13, 0});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 8, 2 }, {9, 2});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 13, 2 }, {14, 2});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 18, 2 }, {19, 2});

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 18, 2 }, {19, 2});
        }

        TEST_METHOD(MoveWordRight_Delimiters)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"-a---           1/15/2025 10:27 PM";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });

            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, { 1, 0 });

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 1, 0 }, { 2, 0 });

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 4, 0 }, { 5, 0 });

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 16, 0 }, { 17, 0 });

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 17, 0 }, { 18, 0 });

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 19, 0 }, { 20, 0 });

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 20, 0 }, { 21, 0 });

            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 24, 0 }, { 25, 0 });
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
