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

        void ValidateLinearSelection(Terminal& term, const til::point start, const til::point end, const til::point pivot)
        {
            auto selectionSpans = term.GetSelectionAnchors();
            VERIFY_ARE_EQUAL(start, selectionSpans->start, L"start");
            VERIFY_ARE_EQUAL(end, selectionSpans->end, L"end");
            VERIFY_ARE_EQUAL(pivot, selectionSpans->pivot, L"pivot");
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

        TEST_METHOD(MoveLeft_Wrap)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 5, 100 }, 0, renderer);

            const std::wstring_view text = L"thisisatestline";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveDown(term, false);
            ValidateLinearSelection(term, { 0, 1 }, {1, 1});

            vim::motions::MoveLeft(term, false);
            ValidateLinearSelection(term, { 4, 0 }, {5, 0});
        }

        TEST_METHOD(MoveLeft_Wrap_Visual_SingleCell)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 5, 100 }, 0, renderer);

            const std::wstring_view text = L"thisisatestline";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveDown(term, false);
            ValidateLinearSelection(term, { 0, 1 }, {1, 1});

            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 4, 0 }, {1, 1});
        }

        TEST_METHOD(MoveLeft_Wrap_Visual_MultipleCells)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 5, 100 }, 0, renderer);

            const std::wstring_view text = L"thisisatestline";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveDown(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 1, 1 }, {2, 1});

            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 4, 0 }, {2, 1});
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

        TEST_METHOD(MoveRight_Wrap)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 5, 100 }, 0, renderer);

            const std::wstring_view text = L"thisisatestline";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 4, 0 }, {5, 0});

            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 0, 1 }, { 1, 1 });
        }

        TEST_METHOD(MoveRight_Wrap_Visual_SingleCell)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 5, 100 }, 0, renderer);

            const std::wstring_view text = L"thisisatestline";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 4, 0 }, {5, 0});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 4, 0 }, { 1, 1 });
        }

        TEST_METHOD(MoveRight_Wrap_Visual_MultipleCells)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 5, 100 }, 0, renderer);

            const std::wstring_view text = L"thisisatestline";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 3, 0 }, {5, 0});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 3, 0 }, { 1, 1 });
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
        }

        TEST_METHOD(MoveRight_ThenLeft_Visual)
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
            ValidateLinearSelection(term, { 0, 0 }, {width - 1, 0});
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

        TEST_METHOD(MoveLeft_BlockVisual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 0 }, { 3, 0 }, {2,0});

            term.SetBlockSelection(true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 0 }, { 3, 0 }, {3,0});

            //vim::motions::MoveLeft(term, true);
            //ValidateLinearSelection(term, { 1, 0 }, { 3, 2 }, {3,0});
        }

        TEST_METHOD(MoveLeft_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 3, 2 }, {3,0});

            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 0 }, { 3, 2 }, {3,0});
        }

        TEST_METHOD(MoveLeft_BlockVisual_AfterMovingUp)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveUp(term, true);
            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 2, 1 }, { 3, 3 }, {3,3});

            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 1 }, { 3, 3 }, {3,3});
        }

        TEST_METHOD(MoveLeft_BlockVisual_AfterMovingUpAndRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveUp(term, true);
            vim::motions::MoveUp(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 1 }, { 4, 3 }, {2,3});

            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 2, 1 }, { 3, 3 }, {2,3});

            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 1 }, { 3, 3 }, {3,3});

            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 0, 1 }, { 3, 3 }, {3,3});
        }

        TEST_METHOD(MoveLeft_BlockVisual_AfterMovingDownAndRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 4, 2 }, {2,0});

            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 3, 2 }, {2,0});

            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 0 }, { 3, 2 }, {3,0});

            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 0, 0 }, { 3, 2 }, {3,0});
        }

        TEST_METHOD(MoveRight_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 3, 2 }, {3,0});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 4, 2 }, {2,0});
        }

        TEST_METHOD(MoveRight_BlockVisual_AfterMovingUp)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveUp(term, true);
            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 2, 1 }, { 3, 3 }, {3,3});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 1 }, { 4, 3 }, {2,3});
        }

        TEST_METHOD(MoveRight_BlockVisual_AfterMovingUpAndLeft)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveUp(term, true);
            vim::motions::MoveUp(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 1 }, { 3, 3 }, {3,3});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 1 }, { 3, 3 }, {3,3});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 1 }, { 4, 3 }, {2,3});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 1 }, { 5, 3 }, {2,3});
        }

        TEST_METHOD(MoveRight_BlockVisual_AfterMovingDownAndLeft)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 0 }, { 3, 2 }, {3,0});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 3, 2 }, {3,0});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 4, 2 }, {2,0});

            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 5, 2 }, {2,0});
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

        TEST_METHOD(MoveDown_Visual)
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
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });

            vim::motions::MoveToEndOfLine(term, false);
            ValidateLinearSelection(term, { textLength - 1, 0 }, { textLength, 0 });

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { textLength - 1, 0 }, { textLength, 1 });

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { textLength - 1, 0 }, { textLength, 2 });
        }

        TEST_METHOD(MoveDown_BlockVisual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 0 }, { 3, 0 }, { 2, 0});

            term.SetBlockSelection(true);

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 3, 1 }, { 3, 0});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 3, 2 }, { 3, 0});
        }

        TEST_METHOD(MoveDown_BlockVisual_AfterMovingRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 5, 0 }, { 2, 0});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 5, 1 }, { 2, 0});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 5, 2 }, { 2, 0});
        }

        TEST_METHOD(MoveDown_BlockVisual_AfterMovingLeft)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 0 }, { 3, 0 }, { 3, 0});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 1, 0 }, { 3, 1 }, { 3, 0});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 1, 0 }, { 3, 2 }, { 3, 0});
        }

        TEST_METHOD(MoveDown_BlockVisual_AfterMovingLeftAndUp)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 4 });
            term.Write(text);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveUp(term, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveWordRight(term, false, false);
            term.SetBlockSelection(true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveUp(term, false);
            vim::motions::MoveUp(term, false);
            ValidateLinearSelection(term, { 1, 1 }, { 4, 3 }, { 4, 3});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 1, 2 }, { 4, 3 }, { 4, 3});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 1, 3 }, { 4, 3 }, { 4, 3});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 1, 3 }, { 4, 4 }, { 4, 3});
        }

        TEST_METHOD(MoveDown_BlockVisual_AfterMovingRightAndUp)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 4 });
            term.Write(text);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveUp(term, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveUp(term, false);
            vim::motions::MoveUp(term, false);
            ValidateLinearSelection(term, { 1, 1 }, { 4, 3 }, { 1, 3});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 1, 2 }, { 4, 3 }, { 1, 3});

            //vim::motions::MoveDown(term, true);
            //ValidateLinearSelection(term, { 1, 3 }, { 4, 3 }, { 4, 3});

            //vim::motions::MoveDown(term, true);
            //ValidateLinearSelection(term, { 1, 3 }, { 4, 4 }, { 4, 3});
        }

        TEST_METHOD(MoveDown_Across_Pivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 3, 0 }, { 4, 0 }, {3,0});

            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 0 }, { 4, 0 }, {4,0});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 3, 0 }, { 2, 1 }, {3, 0});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 3, 0 }, { 2, 2 }, {3, 0});
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

        TEST_METHOD(MoveUp_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            ValidateLinearSelection(term, { textLength - 1, 1 }, { textLength, 1 }, {textLength - 1, 1});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { textLength - 1, 0 }, { textLength, 1 }, {textLength, 1});
        }

        TEST_METHOD(MoveUp_BlockVisual_AfterMovingRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 1, 3 }, { 4, 3 }, {1, 3});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, {1, 2}, {4, 3}, {1,3});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, {1, 1}, {4, 3}, {1,3});
        }

        TEST_METHOD(MoveUp_BlockVisual_AfterMovingRightAndDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveUp(term, false);
            vim::motions::MoveUp(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 1, 1 }, { 4, 3 }, {1, 1});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, {1, 1}, {4, 2}, {1,1});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, {1, 1}, {4, 1}, {1,1});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, {1, 0}, {4, 1}, {1,1});
        }

        TEST_METHOD(MoveUp_BlockVisual_AfterMovingLeft)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveWordRight(term, false, false);
            term.SetBlockSelection(true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 3 }, { 4, 3 }, {4, 3});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, {1, 2}, {4, 3}, {4,3});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, {1, 1}, {4, 3}, {4,3});
        }

        TEST_METHOD(MoveUp_BlockVisual_AfterMovingLeftAndDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveWordRight(term, false, false);
            vim::motions::MoveUp(term, false);
            vim::motions::MoveUp(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 1, 1 }, { 4, 3 }, {4, 1});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, {1, 1}, {4, 2}, {4,1});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, {1, 1}, {4, 1}, {4,1});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, {1, 0}, {4, 1}, {4,1});
        }

        TEST_METHOD(MoveUp_BlankLine_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"";
            const std::wstring_view text3 = L"test line 2";
            auto textLength = static_cast<int>(text1.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectLastNonSpaceChar(term);
            ValidateLinearSelection(term, { textLength - 1, 2 }, { textLength, 2 }, {textLength - 1, 2});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { textLength - 1, 1 }, { textLength, 2 }, {textLength, 2});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { textLength - 1, 0 }, { textLength, 2 }, {textLength, 2});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { textLength - 1, 0 }, { textLength, 2 }, {textLength, 2});
        }

        TEST_METHOD(MoveUp_Across_Pivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 3, 1 }, { 4, 1 }, {3,1});

            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 3, 1 }, { 6, 1 }, {3,1});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 5, 0 }, { 4, 1 });
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

        TEST_METHOD(MoveToStartOfLine_Wrap)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 5, 5 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            ValidateLinearSelection(term, { 0, 2 }, {1, 2});

            vim::motions::MoveToStartOfLine(term, false);
            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
        }

        TEST_METHOD(MoveDown_Across_Pivot_ThenMoveUp_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 3, 0 }, { 4, 0 }, {3,0});

            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 0 }, { 4, 0 }, {4,0});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 3, 0 }, { 2, 1 }, {3, 0});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 3, 0 }, { 2, 2 }, {3, 0});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 3, 0 }, { 2, 1 }, {3, 0});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 1, 0 }, { 4, 0 }, {4, 0});
        }

        TEST_METHOD(MoveUp_Across_Pivot_ThenMoveDown_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"C:\\Terminal>";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 3, 2 }, { 4, 2 }, {3,2});

            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 3, 2 }, { 6, 2 }, {3,2});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 5, 1 }, { 4, 2 }, {4, 2});

            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 5, 0 }, { 4, 2 }, {4, 2});

            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 5, 1 }, { 4, 2 }, {4,2});
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

        TEST_METHOD(MoveToStartOfLine_FromEndOfLine_Visual)
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

            ValidateLinearSelection(term, { 0, 0 }, {textLength, 0}, {textLength, 0});
        }

        TEST_METHOD(MoveToStartOfLine_FromEndOfLine_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 10, 0 }, {11, 1}, {11, 0});

            vim::motions::MoveToStartOfLine(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {11, 1}, {11, 0});
        }

        TEST_METHOD(MoveToStartOfLine_AcrossPivot_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 8, 0 }, {11, 1}, {8, 0});

            vim::motions::MoveToStartOfLine(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {9, 1}, {9, 0});
        }

        TEST_METHOD(MoveToStartOfLine_AcrossPivot_BlockVisual_AfterMovingUp)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveUp(term, true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 8, 1 }, {11, 2}, {8, 2});

            vim::motions::MoveToStartOfLine(term, true);
            ValidateLinearSelection(term, { 0, 1 }, {9, 2}, {9, 2});
        }

        TEST_METHOD(MoveToStartOfLine_AfterMovingUpOneLine_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {3, 1}, {3, 1});

            vim::motions::MoveToStartOfLine(term, true);
            ValidateLinearSelection(term, { 0, 0 }, { 3, 1 }, { 3, 1 });
        }

        TEST_METHOD(MoveToStartOfLine_AfterMovingDownOneLine_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {3, 1}, {2, 0});

            vim::motions::MoveToStartOfLine(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 1, 1 }, { 2, 0 });
        }

        TEST_METHOD(MoveToStartOfLine_AcrossPivot_Visual)
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
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveToStartOfLine(term, true);

            ValidateLinearSelection(term, { 0, 0 }, {10, 0}, {10, 0});
        }

        TEST_METHOD(MoveToStartOfLine_AfterSelectingMultipleLines_Visual)
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

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, true);
            vim::motions::SelectBottom(term, true, false);
            vim::motions::MoveToEndOfLine(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {11, 2}, {0, 0});

            vim::motions::MoveToStartOfLine(term, true);
            ValidateLinearSelection(term, { 0, 0 }, { 1, 2 }, { 0, 0 });
        }

        TEST_METHOD(MoveToFirstNonBlankChar_AfterMovingUpOneLine_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {3, 1}, {3, 1});

            vim::motions::MoveToFirstNonBlankChar(term, true);
            ValidateLinearSelection(term, { 0, 0 }, { 3, 1 }, { 3, 1 });
        }

        TEST_METHOD(MoveToFirstNonBlankChar_AfterMovingDownOneLine_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {3, 1}, {2, 0});

            vim::motions::MoveToFirstNonBlankChar(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 1, 1 }, { 2, 0 });
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

        TEST_METHOD(MoveToFirstNonBlankChar_BlanksAtStart)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"  test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToFirstNonBlankChar(term, false);

            ValidateLinearSelection(term, { 2, 0 }, {3, 0});
        }

        TEST_METHOD(MoveToFirstNonBlankChar_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"  test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveToFirstNonBlankChar(term, true);

            ValidateLinearSelection(term, { 2, 0 }, {11, 0}, {11, 0});
        }

        TEST_METHOD(MoveToFirstNonBlankChar_FromEndOfLine_BlockVisual_AfterMovingDown)
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

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 8, 0 }, {9, 1}, {9, 0});

            vim::motions::MoveToFirstNonBlankChar(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {9, 1}, {9, 0});
        }

        TEST_METHOD(MoveToFirstNonBlankChar_FromEndOfLine_BlockVisual_AfterMovingUp)
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

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 8, 1 }, {9, 2}, {9, 2});

            vim::motions::MoveToFirstNonBlankChar(term, true);
            ValidateLinearSelection(term, { 0, 1 }, {9, 2}, {9, 2});
        }

        TEST_METHOD(MoveToFirstNonBlankChar_AcrossPivot_BlockVisual_AfterMovingUp)
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

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveUp(term, true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 8, 1 }, {11, 2}, {8, 2});

            vim::motions::MoveToFirstNonBlankChar(term, true);
            ValidateLinearSelection(term, { 0, 1 }, {9, 2}, {9, 2});
        }

        TEST_METHOD(MoveToFirstNonBlankChar_AcrossPivot_BlockVisual_AfterMovingDown)
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

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 8, 0 }, {11, 1}, {8, 0});

            vim::motions::MoveToFirstNonBlankChar(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {9, 1}, {9, 0});
        }

        TEST_METHOD(MoveToFirstNonBlankChar_AcrossPivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"  test line 1";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 8, 0 }, {11, 0}, {8, 0});

            vim::motions::MoveToFirstNonBlankChar(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {9, 0}, {9, 0});
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

        TEST_METHOD(MoveToEndOfLine_Wrap)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 5, 100 }, 0, renderer);

            const std::wstring_view text = L"this is a test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0});

            vim::motions::MoveToEndOfLine(term, false);

            ValidateLinearSelection(term, { 3, 3 }, {4, 3});
        }

        TEST_METHOD(MoveToEndOfLine_VisualBlock)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test longer line 3";
            const std::wstring_view text4 = L"test line 4";
            const std::wstring_view text5 = L"test line 5";
            auto textLength = static_cast<int>(text3.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveDown(term, false); 
            ValidateLinearSelection(term, { 0, 1 }, {1, 1}, {0, 1});

            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true); 
            vim::motions::MoveDown(term, true); 
            ValidateLinearSelection(term, { 0, 1 }, {1, 3}, {1, 1});

            vim::motions::MoveToEndOfLine(term, true);
            ValidateLinearSelection(term, { 0, 1 }, {textLength, 3}, {0, 1});
        }

        TEST_METHOD(MoveToEndOfLine_AcrossPivot_VisualBlock_AfterMovingDownAndLeft)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test longer line 3";
            const std::wstring_view text4 = L"test line 4";
            const std::wstring_view text5 = L"test line 5";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveDown(term, false); 
            vim::motions::MoveRight(term, false); 
            vim::motions::MoveRight(term, false); 
            vim::motions::MoveRight(term, false); 
            ValidateLinearSelection(term, { 3, 1 }, {4, 1}, {3, 1});

            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true); 
            vim::motions::MoveDown(term, true); 
            vim::motions::MoveLeft(term, true); 
            vim::motions::MoveLeft(term, true); 
            ValidateLinearSelection(term, { 1, 1 }, {4, 3}, {4, 1});

            vim::motions::MoveToEndOfLine(term, true);
            ValidateLinearSelection(term, { 3, 1 }, {18, 3}, {3, 1});
        }

        TEST_METHOD(MoveToEndOfLine_AcrossPivot_VisualBlock_AfterMovingDownAndRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test longer line 3";
            const std::wstring_view text4 = L"test line 4";
            const std::wstring_view text5 = L"test line 5";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveDown(term, false); 
            vim::motions::MoveRight(term, false); 
            vim::motions::MoveRight(term, false); 
            vim::motions::MoveRight(term, false); 
            ValidateLinearSelection(term, { 3, 1 }, {4, 1}, {3, 1});

            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true); 
            vim::motions::MoveDown(term, true); 
            vim::motions::MoveRight(term, true); 
            vim::motions::MoveRight(term, true); 
            ValidateLinearSelection(term, { 3, 1 }, {6, 3}, {3, 1});

            vim::motions::MoveToEndOfLine(term, true);
            ValidateLinearSelection(term, { 3, 1 }, {18, 3}, {3, 1});
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

            ValidateLinearSelection(term, { 0, 0 }, {1, 0}, {0, 0});

            vim::motions::MoveToEndOfLine(term, true);

            ValidateLinearSelection(term, { 0, 0 }, {textLength, 0}, {0,0 });
        }

        TEST_METHOD(MoveToEndOfLine_Visual_FromMiddleOfLine)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is a test";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            vim::motions::MoveWordRight(term, false, false);
            vim::motions::MoveWordRight(term, false, false);
            ValidateLinearSelection(term, { 6, 0 }, {7, 0}, {6, 0});

            vim::motions::MoveToEndOfLine(term, true);
            ValidateLinearSelection(term, { 6, 0 }, {textLength, 0}, {6, 0});
        }

        TEST_METHOD(MoveToEndOfLine_Visual_AcrossPivot)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is a test";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            vim::motions::MoveWordRight(term, false, false);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 1, 0 }, {4, 0}, {4, 0});

            vim::motions::MoveToEndOfLine(term, true);

            ValidateLinearSelection(term, { 3, 0 }, {textLength, 0}, {3, 0});
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

        TEST_METHOD(TilChar_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line1";
            const std::wstring_view text2 = L"another test line";
            const std::wstring_view text3 = L"third test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {1, 1}, {1, 0});

            vim::motions::TilChar(term, L"i", true);
            ValidateLinearSelection(term, { 0, 0 }, {14, 1}, {0, 0});
        }

        TEST_METHOD(TilChar_AcrossPivot_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line1";
            const std::wstring_view text2 = L"another test line";
            const std::wstring_view text3 = L"third test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {3, 1}, {3, 0});

            vim::motions::TilChar(term, L"i", true);
            ValidateLinearSelection(term, { 2, 0 }, {14, 1}, {2, 0});
        }

        TEST_METHOD(FindChar_AcrossPivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this pivot test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::FindCharBackwards(term, L"v", false);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 5, 0 }, {8, 0}, {8, 0});

            vim::motions::FindChar(term, L"t", true);
            ValidateLinearSelection(term, { 7, 0 }, {10, 0}, {7, 0});
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

        TEST_METHOD(FindChar_MultipleTimes)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"25814522 2425";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);

            vim::motions::FindChar(term, L"2", false);
            ValidateLinearSelection(term, { 6, 0 }, {7, 0});

            vim::motions::FindChar(term, L"2", false);
            ValidateLinearSelection(term, { 7, 0 }, {8, 0});

            vim::motions::FindChar(term, L"2", false);
            ValidateLinearSelection(term, { 9, 0 }, {10, 0});

            vim::motions::FindChar(term, L"2", false);
            ValidateLinearSelection(term, { 11, 0 }, {12, 0});
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

        TEST_METHOD(FindChar_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"another test line";
            const std::wstring_view text3 = L"third test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {1, 1}, {1, 0});
            vim::motions::FindChar(term, L"r", true);
            ValidateLinearSelection(term, { 0, 0 }, {7, 1}, {0, 0});
        }

        TEST_METHOD(FindChar_AcrossPivot_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"another test line";
            const std::wstring_view text3 = L"third test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {3, 1}, {3, 0});
            vim::motions::FindChar(term, L"r", true);
            ValidateLinearSelection(term, { 2, 0 }, {7, 1}, {2, 0});
        }

        TEST_METHOD(FindChar_AfterMovingDown_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {3, 1}, {2, 0});

            vim::motions::FindChar(term, L"l", true);
            ValidateLinearSelection(term, { 2, 0 }, {6, 1}, {2, 0});
        }

        TEST_METHOD(FindChar_AfterMovingUp_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {3, 1}, {3, 1});

            vim::motions::FindChar(term, L"l", true);
            ValidateLinearSelection(term, { 5, 0 }, {3, 1}, {3, 1});
        }

        TEST_METHOD(TilChar_AcrossPivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this pivot test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::FindCharBackwards(term, L"v", false);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 5, 0 }, {8, 0}, {8, 0});

            vim::motions::TilChar(term, L"t", true);
            ValidateLinearSelection(term, { 7, 0 }, {9, 0}, {7, 0});
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

        TEST_METHOD(FindChar_Backwards_Multiple)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"25814522";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            ValidateLinearSelection(term, { 7, 0 }, {8, 0});

            vim::motions::FindCharBackwards(term, L"2", false);
            ValidateLinearSelection(term, { 6, 0 }, {7, 0});

            vim::motions::FindCharBackwards(term, L"2", false);
            ValidateLinearSelection(term, { 0, 0 }, {1, 0});
        }

        TEST_METHOD(FindChar_Backwards_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this pivot test";
            auto textLength = static_cast<int>(text.length());
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::FindCharBackwards(term, L"i", true);

            ValidateLinearSelection(term, { 6, 0 }, {textLength, 0});
        }

        TEST_METHOD(FindChar_Backwards_BlockVisual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"another test line";
            const std::wstring_view text3 = L"third test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 10, 0 }, {11, 1}, {11, 0} );

            vim::motions::FindCharBackwards(term, L"r", true);

            ValidateLinearSelection(term, { 6, 0 }, { 11, 1 }, { 11, 0 });
        }

        TEST_METHOD(FindChar_AcrossPivot_Backwards_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"another test line";
            const std::wstring_view text3 = L"third test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 8, 0 }, {11, 1}, {8, 0} );

            vim::motions::FindCharBackwards(term, L"r", true);
            ValidateLinearSelection(term, { 6, 0 }, { 9, 1 }, { 9, 0 });
        }

        TEST_METHOD(FindCharBackwards_AfterMovingDown_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 3, 0 }, {4, 1}, {3, 0});

            vim::motions::FindCharBackwards(term, L"e", true);
            ValidateLinearSelection(term, { 3, 0 }, {2, 1}, {3, 0});
        }

        TEST_METHOD(FindChar_Backwards_AcrossPivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this pivot test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::FindCharBackwards(term, L"v", false);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 7, 0 }, {10, 0});

            vim::motions::FindCharBackwards(term, L"p", true);
            ValidateLinearSelection(term, {5, 0}, {8, 0}, {8, 0});
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

        TEST_METHOD(TilChar_Backwards_AcrossPivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this pivot test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::FindCharBackwards(term, L"v", false);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 7, 0 }, {10, 0});

            vim::motions::TilCharBackwards(term, L"p", true);
            ValidateLinearSelection(term, {6, 0}, {8, 0}, {8, 0});
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

        TEST_METHOD(TilChar_Backwards_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"another test line";
            const std::wstring_view text3 = L"third test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 10, 0 }, {11, 1}, { 11, 0});

            vim::motions::TilCharBackwards(term, L"r", true);
            ValidateLinearSelection(term, { 7, 0 }, {11, 1}, { 11, 0});
        }

        TEST_METHOD(TilChar_Backwards_AcrossPivot_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"another test line";
            const std::wstring_view text3 = L"third test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            vim::motions::MoveLeft(term, false);
            vim::motions::MoveLeft(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 8, 0 }, {11, 1}, { 8, 0});

            vim::motions::TilCharBackwards(term, L"r", true);
            ValidateLinearSelection(term, { 7, 0 }, {9, 1}, { 9, 0});
        }

        TEST_METHOD(TilCharBackwards_AfterMovingDown_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 3, 0 }, {4, 1}, {3, 0});

            vim::motions::TilCharBackwards(term, L"e", true);
            ValidateLinearSelection(term, { 3, 0 }, {3, 1}, {3, 0});
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

        TEST_METHOD(MoveWordRight_Visual_AcrossPivot)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is another test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveWordLeft(term, false, false);
            vim::motions::MoveWordLeft(term, false, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 11, 0 }, {12, 0}, {11, 0});

            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 9, 0 }, {12, 0}, {12, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 11, 0 }, {15, 0}, {11, 0});
        }

        TEST_METHOD(MoveWordLeft_Visual_AcrossPivot)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is another test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveWordLeft(term, false, false);
            vim::motions::MoveWordLeft(term, false, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 11, 0 }, {12, 0}, {11, 0});

            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 11, 0 }, {14, 0}, {11, 0});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 8, 0 }, {12, 0}, {12, 0});
        }

        TEST_METHOD(MoveWordLeft_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveRight(term, false);
            vim ::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveDown(term, true);
            vim ::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {3, 2}, {3, 0});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {3, 2}, {3,0});
        }

        TEST_METHOD(MoveWordLeft_BlockVisual_AfterMovingDownAndWordRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveRight(term, false);
            vim ::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveDown(term, true);
            vim ::motions::MoveDown(term, true);
            vim::motions::MoveWordRight(term, false, true);
            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 2, 0 }, {9, 2}, {2, 0});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 2, 0 }, {6, 2}, {2, 0});
        }

        TEST_METHOD(MoveWordLeft_BlockVisual_AfterMovingUpAndWordRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveRight(term, false);
            vim ::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveUp(term, true);
            vim ::motions::MoveUp(term, true);
            vim::motions::MoveWordRight(term, false, true);
            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 2, 1 }, {9, 3}, {2, 3});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 2, 1 }, {6, 3}, {2, 3});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 0, 1 }, {3, 3}, {3, 3});
        }

        TEST_METHOD(MoveWordLeft_BlockVisual_AfterMovingUp)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveRight(term, false);
            vim ::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveUp(term, true);
            vim ::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 2, 1 }, {3, 3}, {3, 3});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, {0, 1}, {3, 3}, {3,3});
        }

        TEST_METHOD(MoveWordLeft_AcrossPivot_BlockVisual_AfterMovingUpAndRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveRight(term, false);
            vim ::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveUp(term, true);
            vim ::motions::MoveUp(term, true);
            vim ::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 1 }, {4, 3}, {2, 3});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, {0, 1}, {3, 3}, {3,3});
        }

        TEST_METHOD(MoveWordLeft_AcrossPivotSingleColumn_BlockVisual_AfterMovingUpAndWordRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveWordStartRight(term, false, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveUp(term, true);
            vim ::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 5, 1 }, {6, 3}, {6, 3});

            vim ::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, { 5, 1 }, {9, 3}, {5, 3});

            vim ::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 5, 1 }, {6, 3}, {5, 3});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, {0, 1}, {6, 3}, {6,3});
        }

        TEST_METHOD(MoveWordLeft_AcrossPivot_BlockVisual_AfterMovingDownAndRight)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveRight(term, false);
            vim ::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveDown(term, true);
            vim ::motions::MoveDown(term, true);
            vim ::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {4, 2}, {2, 0});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {3, 2}, {3,0});
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

        TEST_METHOD(MoveWordLeft_AfterSelecting_MultipleLines_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"another test line";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::SelectBottom(term, true, false);
            vim::motions::MoveToEndOfLine(term, true);
            ValidateLinearSelection(term, { 0, 0 }, { 17, 2 }, {0, 0});

            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 0, 0 }, { 14, 2 }, {0, 0});
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

            ValidateLinearSelection(term, { 0, 0 }, {1, 0}, {0, 0});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 1, 0 }, {2, 0}, {1,0});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 3, 0 }, {4, 0}, {3,0});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 11, 0 }, {12, 0}, {11,0});
        }

        TEST_METHOD(MoveWordStartRight_MultipleLines)
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
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0}, {0, 0});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 5, 0 }, {6, 0}, {5,0});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 10, 0 }, {11, 0}, {10,0});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 0, 1 }, {1, 1}, {0,1});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 0, 2 }, {1, 2}, {0,2});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 8, 2 }, {9, 2}, {8,2});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 13, 2 }, {14, 2}, {13,2});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 16, 2 }, {17, 2}, {16,2});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 16, 2 }, {17, 2}, {16,2});
        }

        TEST_METHOD(MoveWordStartRight_RegularWords)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is a test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0}, {0, 0});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 5, 0 }, {6, 0}, {5,0});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 8, 0 }, {9, 0}, {8,0});

            vim::motions::MoveWordStartRight(term, false, false);
            ValidateLinearSelection(term, { 10, 0 }, {11, 0}, {10,0});
        }

        TEST_METHOD(MoveWordStartRight_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this is a test";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0}, {0, 0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {6, 0}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {9, 0}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {11, 0}, {0,0});
        }

        TEST_METHOD(MoveWordStartRight_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"this is a test";
            const std::wstring_view text2 = L"another test line";
            const std::wstring_view text3 = L"test line3";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {1, 1}, {1, 0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {9, 1}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {14, 1}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {1, 2}, {0,0});
        }

        TEST_METHOD(MoveWordStartRight_AcrossPivot_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"this is a test";
            const std::wstring_view text2 = L"another test line";
            const std::wstring_view text3 = L"test line3";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveWordRight(term, false, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveToStartOfLine(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {4, 1}, {4, 0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {3, 0}, {9, 1}, {3,0});

            //vim::motions::MoveWordStartRight(term, false, true);
            //ValidateLinearSelection(term, {0, 0}, {14, 1}, {0,0});

            //vim::motions::MoveWordStartRight(term, false, true);
            //ValidateLinearSelection(term, {0, 0}, {1, 2}, {0,0});
        }

        TEST_METHOD(MoveWordStartRight_CrossPivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"this tests pivot";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveWordLeft(term, false, false);
            vim::motions::MoveWordLeft(term, false, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 7, 0 }, {8, 0}, {7, 0});

            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 5, 0 }, {8, 0}, {8, 0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {7, 0}, {12, 0}, {7,0});
        }

        TEST_METHOD(MoveWordStartRight_Visual_MultipleLines)
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
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);

            ValidateLinearSelection(term, { 0, 0 }, {1, 0}, {0, 0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {6, 0}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {11, 0}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {1, 1}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {1, 2}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {9, 2}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {14, 2}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {17, 2}, {0,0});

            vim::motions::MoveWordStartRight(term, false, true);
            ValidateLinearSelection(term, {0, 0}, {17, 2}, {0,0});
        }

        TEST_METHOD(MoveWordRight_BlockVisual_MultipleLines)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveRight(term, false);
            vim ::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveUp(term, true);
            vim ::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 2, 1 }, {3, 3}, {3, 3});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, {2, 1}, {4, 3}, {2,3});
        }

        TEST_METHOD(MoveWordRight_AcrossPivot_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveRight(term, false);
            vim ::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveDown(term, true);
            vim ::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {3, 2}, {3, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, {2, 0}, {4, 2}, {2,0});
        }

        TEST_METHOD(MoveWordRight_BlockVisual_AfterMovingDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveWordRight(term, false, false);
            vim ::motions::MoveWordRight(term, false, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveDown(term, true);
            vim ::motions::MoveDown(term, true);
            vim::motions::MoveWordLeft(term, false, true);
            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 0, 0 }, {9, 2}, {9, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, {3, 0}, {9, 2}, {9,0});
        }

        TEST_METHOD(MoveWordRight_BlockVisual_AfterMovingUp)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveWordRight(term, false, false);
            vim ::motions::MoveWordRight(term, false, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveUp(term, true);
            vim ::motions::MoveUp(term, true);
            vim::motions::MoveWordLeft(term, false, true);
            vim::motions::MoveWordLeft(term, false, true);
            ValidateLinearSelection(term, { 0, 1 }, {9, 3}, {9, 3});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, {3, 1}, {9, 3}, {9,3});
        }

        TEST_METHOD(MoveWordRight_BlockVisual_AfterMovingDown_AcrossPivot)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveRight(term, false);
            vim ::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveDown(term, true);
            vim ::motions::MoveDown(term, true);
            vim ::motions::MoveLeft(term, true);
            vim ::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {3, 2}, {3, 0});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, {2, 0}, {4, 2}, {2,0});
        }

        TEST_METHOD(MoveWordRight_AcrossPivot_BlockVisual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text4);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim ::motions::MoveRight(term, false);
            vim ::motions::MoveRight(term, false);
            term.SetBlockSelection(true);
            vim ::motions::MoveUp(term, true);
            vim ::motions::MoveUp(term, true);
            vim ::motions::MoveLeft(term, true);
            vim ::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 0, 1 }, {3, 3}, {3, 3});

            vim::motions::MoveWordRight(term, false, true);
            ValidateLinearSelection(term, {2, 1}, {4, 3}, {2,3});
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
            ValidateLinearSelection(term, { 11, 0 }, {12, 0});
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

        TEST_METHOD(InSingleQuotes_Inside)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a 'test' a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);

            vim::motions::InDelimiterSameLine(term, L"'", false);
            ValidateLinearSelection(term, { 3, 0 }, {7, 0});
        }

        TEST_METHOD(InBraces_Inside)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a {test} a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);

            vim::motions::InDelimiter(term, L"{", L"}", false);
            ValidateLinearSelection(term, { 3, 0 }, {7, 0});
        }

        TEST_METHOD(AroundSingleQuotes_Inside)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a 'test' a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);

            vim::motions::InDelimiterSameLine(term, L"'", true);
            ValidateLinearSelection(term, { 2, 0 }, {8, 0});
        }

        TEST_METHOD(AroundBraces_Inside)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a {test} a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);

            vim::motions::InDelimiter(term, L"{", L"}", true);
            ValidateLinearSelection(term, { 2, 0 }, {8, 0});
        }

        TEST_METHOD(AroundSingleQuotes_FromStartDelimiter)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a 'test' a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::FindChar(term, L"'", false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 0});

            vim::motions::InDelimiterSameLine(term, L"'", true);
            ValidateLinearSelection(term, { 2, 0 }, {8, 0});
        }

        TEST_METHOD(AroundSingleQuotes_FromEndDelimiter)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a 'test' a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::FindChar(term, L"'", false);
            vim::motions::FindChar(term, L"'", false);
            ValidateLinearSelection(term, { 7, 0 }, {8, 0});

            vim::motions::InDelimiterSameLine(term, L"'", true);
            ValidateLinearSelection(term, { 2, 0 }, {8, 0});
        }

        TEST_METHOD(AroundBraces_FromEndDelimiter)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a {test} a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::FindChar(term, L"{", false);
            vim::motions::FindChar(term, L"}", false);
            ValidateLinearSelection(term, { 7, 0 }, {8, 0});

            vim::motions::InDelimiter(term, L"{", L"}", true);
            ValidateLinearSelection(term, { 2, 0 }, {8, 0});
        }

        TEST_METHOD(InSingleQuotes_SingleLine_Inside)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a 'test' a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);

            vim::motions::InDelimiterSameLine(term, L"'", false);
            ValidateLinearSelection(term, { 3, 0 }, {7, 0});
        }

        TEST_METHOD(InBraces_SingleLine_Inside)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a {test} a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);

            vim::motions::InDelimiter(term, L"{", L"}", false);
            ValidateLinearSelection(term, { 3, 0 }, {7, 0});
        }

        TEST_METHOD(InSingleQuotes_SingleLine_StartDelimiter)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a 'test' a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);

            vim::motions::InDelimiterSameLine(term, L"'", false);
            ValidateLinearSelection(term, { 3, 0 }, {7, 0});
        }

        TEST_METHOD(InBraces_SingleLine_StartDelimiter)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a {test} a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);

            vim::motions::InDelimiter(term, L"{", L"}", false);
            ValidateLinearSelection(term, { 3, 0 }, {7, 0});
        }

        TEST_METHOD(InSingleQuotes_SingleLine_EndDelimiter)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a 'test' a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);

            vim::motions::InDelimiterSameLine(term, L"'", false);
            ValidateLinearSelection(term, { 3, 0 }, {7, 0});
        }

        TEST_METHOD(InBraces_SingleLine_EndDelimiter)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text = L"a {test} a";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text);

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);

            vim::motions::InDelimiter(term, L"{", L"}", false);
            ValidateLinearSelection(term, { 3, 0 }, {7, 0});
        }

        TEST_METHOD(SelectLineDown)
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
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });

            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveToEndOfLine(term, true);
            ValidateLinearSelection(term, { 0, 0 }, {11, 0}, {0,0});

            vim::motions::SelectLineDown(term);
            ValidateLinearSelection(term, { 0, 0 }, {11, 1},{0,0});

            vim::motions::SelectLineDown(term);
            ValidateLinearSelection(term, { 0, 0 }, {11, 2},{0,0});
        }

        TEST_METHOD(SelectLineDown_ThenSelectUp_AcrossPivot)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectLastNonSpaceChar(term);
            vim::motions::MoveUp(term, false);
            vim::motions::MoveUp(term, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveToEndOfLine(term, true);
            ValidateLinearSelection(term, { 0, 1 }, {11, 1}, {0,1});

            vim::motions::SelectLineDown(term);
            ValidateLinearSelection(term, { 0, 1 }, {11, 2},{0,1});

            vim::motions::SelectLineDown(term);
            ValidateLinearSelection(term, { 0, 1 }, {11, 3},{0,1});

            vim::motions::SelectLineUp(term);
            ValidateLinearSelection(term, { 0, 1 }, {11, 2},{0,1});

            vim::motions::SelectLineUp(term);
            ValidateLinearSelection(term, { 0, 1 }, {11, 1},{0,1});

            vim::motions::SelectLineUp(term);
            ValidateLinearSelection(term, { 0, 0 }, {11, 1},{0,1});

            vim::motions::SelectLineDown(term);
            ValidateLinearSelection(term, { 0, 1 }, {11, 1},{0,1});

            vim::motions::SelectLineDown(term);
            ValidateLinearSelection(term, { 0, 1 }, {11, 2},{0,1});
        }

        TEST_METHOD(HalfPageDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 0}, {2,0});

            vim::motions::SelectHalfPageDown(term, false, false);
            ValidateLinearSelection(term, { 2, 3 }, {3, 3}, {2,3});
        }

        TEST_METHOD(HalfPageDown_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 0}, {2,0});

            vim::motions::SelectHalfPageDown(term, true, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 3}, {2,0});
        }

        TEST_METHOD(HalfPageDown_BlockVisual_AfterMovingLeft)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 8, 0 }, {11, 0}, {11,0});

            vim::motions::SelectHalfPageDown(term, true, false);
            ValidateLinearSelection(term, { 8, 0 }, {11, 3}, {11,0});
        }

        TEST_METHOD(HalfPageDown_AcrossPivot_BlockVisual_AfterMovingLeft)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToEndOfLine(term, false);
            vim::motions::MoveDown(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveUp(term, true);
            vim::motions::MoveLeft(term, true);
            vim::motions::MoveLeft(term, true);
            ValidateLinearSelection(term, { 8, 0 }, {11, 1}, {11,1});

            vim::motions::SelectHalfPageDown(term, true, false);
            ValidateLinearSelection(term, { 8, 1 }, {11, 3}, {11,1});
        }

        TEST_METHOD(HalfPageDown_AcrossPivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveDown(term, false);
            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 2, 0 }, { 3, 1 }, { 3, 1 });

            vim::motions::SelectHalfPageDown(term, true, false);
            ValidateLinearSelection(term, { 2, 1 }, {3, 3}, {2,1});
        }

        TEST_METHOD(HalfPageUp)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 3 }, {3, 3}, {2,3});

            vim::motions::SelectHalfPageUp(term, false, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 0}, {2,0});
        }

        TEST_METHOD(HalfPageUp_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 3 }, {3, 3}, {2,3});

            vim::motions::SelectHalfPageUp(term, true, false);
            ValidateLinearSelection(term, { 1, 0 }, {3, 3}, {3,3});
        }

        TEST_METHOD(HalfPageUp_BlockVisual_AfterMovingRightAndDown)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveUp(term, false);
            term.SetBlockSelection(true);
            vim::motions::MoveDown(term, true);
            vim::motions::MoveRight(term, true);
            vim::motions::MoveRight(term, true);
            ValidateLinearSelection(term, { 0, 2 }, {3, 3}, {0,2});

            vim::motions::SelectHalfPageUp(term, true, false);
            ValidateLinearSelection(term, { 0, 0 }, {3, 2}, {0,2});
        }

        TEST_METHOD(SelectTop)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 3 }, {3, 3}, {2,3});

            vim::motions::SelectTop(term, false, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 0}, {2,0});
        }

        TEST_METHOD(SelectTop_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 3 }, {3, 3}, {2,3});

            vim::motions::SelectTop(term, true, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 3}, {3,3});
        }

        TEST_METHOD(SelectTop_EntireLine_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveUp(term, false);
            ValidateLinearSelection(term, { 0, 2 }, {1, 2}, {0,2});

            vim::motions::SelectTop(term, true, true);
            ValidateLinearSelection(term, { 0, 0 }, {11, 2}, {11,2});
        }

        TEST_METHOD(SelectTop_EntireLine_AcrossPivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveUp(term, false);
            vim::motions::MoveUp(term, false);
            vim::motions::SelectBottom(term, false, true);
            ValidateLinearSelection(term, { 0, 1 }, {11, 3}, {0,1});

            vim::motions::SelectTop(term, true, true);
            ValidateLinearSelection(term, { 0, 0 }, {11, 1}, {11,1});
        }

        TEST_METHOD(SelectTop_AcrossPivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectBottom(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveUp(term, false);
            vim::motions::MoveDown(term, true);
            ValidateLinearSelection(term, { 2, 2 }, {3, 3}, {2,2});

            vim::motions::SelectTop(term, true, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 2}, {3,2});
        }

        TEST_METHOD(SelectBottom)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 0}, {2,0});

            vim::motions::SelectBottom(term, false, false);
            ValidateLinearSelection(term, { 2, 3 }, {3, 3}, {2,3});
        }

        TEST_METHOD(SelectBottom_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 0}, {2,0});

            vim::motions::SelectBottom(term, true, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 3}, {2,0});
        }

        TEST_METHOD(SelectBottom_EntireLine_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            ValidateLinearSelection(term, { 2, 0 }, {3, 0}, {2,0});

            vim::motions::SelectBottom(term, true, true);
            ValidateLinearSelection(term, { 0, 0 }, {11, 3}, {0,0});
        }

        TEST_METHOD(SelectBottom_AcrossPivot_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveToStartOfLine(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveRight(term, false);
            vim::motions::MoveDown(term, false);
            vim::motions::MoveUp(term, true);
            ValidateLinearSelection(term, { 2, 0 }, {3, 1}, {3,1});

            vim::motions::SelectBottom(term, true, false);
            ValidateLinearSelection(term, { 2, 1 }, {3, 3}, {2,1});
        }

        TEST_METHOD(SelectBottom_AcrossPivot_EntireLine_Visual)
        {
            Terminal term{ Terminal::TestDummyMarker{} };
            DummyRenderer renderer{ &term };
            term.Create({ 100, 100 }, 0, renderer);

            const std::wstring_view text1 = L"test line 1";
            const std::wstring_view text2 = L"test line 2";
            const std::wstring_view text3 = L"test line 3";
            const std::wstring_view text4 = L"test line 4";
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 0 });
            term.Write(text1);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 1 });
            term.Write(text2);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 2 });
            term.Write(text3);
            GetTextBuffer(term).GetCursor().SetPosition({ 0, 3 });
            term.Write(text3);

            vim::motions::SelectTop(term, false, false);
            vim::motions::MoveDown(term, false);
            vim::motions::MoveDown(term, false);
            vim::motions::SelectTop(term, false, true);
            ValidateLinearSelection(term, { 0, 0 }, {11, 2}, {11,2});

            vim::motions::SelectBottom(term, true, true);
            ValidateLinearSelection(term, { 0, 2 }, {11, 3}, {0,2});
        }
    };
}
