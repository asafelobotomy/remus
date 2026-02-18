/**
 * @file test_tui_widgets.cpp
 * @brief Unit tests for TUI widgets: TextInput, SelectableList, ProgressBarWidget,
 *        SplitPane, Toast, HelpOverlay
 *
 * These widgets manage state independently of notcurses rendering,
 * so we validate state transitions only — no ncplane needed.
 */

#include <QtTest/QtTest>
#include <notcurses/notcurses.h>

#include "../src/tui/widgets/text_input.h"
#include "../src/tui/widgets/selectable_list.h"
#include "../src/tui/widgets/progress_bar.h"
#include "../src/tui/widgets/split_pane.h"
#include "../src/tui/widgets/toast.h"
#include "../src/tui/widgets/help_overlay.h"

// ════════════════════════════════════════════════════════════
// TextInput Tests
// ════════════════════════════════════════════════════════════

class TestTextInput : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        TextInput input("Label: ", "placeholder");
        QVERIFY(input.empty());
        QCOMPARE(input.value(), std::string());
        QCOMPARE(input.label(), std::string("Label: "));
    }

    void testCharInsertion()
    {
        TextInput input;
        QVERIFY(input.handleInput('a'));
        QVERIFY(input.handleInput('b'));
        QVERIFY(input.handleInput('c'));
        QCOMPARE(input.value(), std::string("abc"));
        QVERIFY(!input.empty());
    }

    void testBackspace()
    {
        TextInput input;
        input.setValue("hello");
        QVERIFY(input.handleInput(127)); // backspace
        QCOMPARE(input.value(), std::string("hell"));
        QVERIFY(input.handleInput(NCKEY_BACKSPACE));
        QCOMPARE(input.value(), std::string("hel"));
    }

    void testBackspaceOnEmpty()
    {
        TextInput input;
        QVERIFY(!input.handleInput(127)); // returns false — nothing to delete
    }

    void testNonPrintableIgnored()
    {
        TextInput input;
        QVERIFY(!input.handleInput(1));   // Ctrl+A
        QVERIFY(!input.handleInput(27));  // ESC char
        QVERIFY(input.empty());
    }

    void testSetValue()
    {
        TextInput input;
        input.setValue("test");
        QCOMPARE(input.value(), std::string("test"));
    }

    void testClear()
    {
        TextInput input;
        input.setValue("content");
        input.clear();
        QVERIFY(input.empty());
    }

    void testIsSubmit()
    {
        QVERIFY(TextInput::isSubmit(NCKEY_ENTER));
        QVERIFY(TextInput::isSubmit('\n'));
        QVERIFY(TextInput::isSubmit('\r'));
        QVERIFY(!TextInput::isSubmit('a'));
        QVERIFY(!TextInput::isSubmit(NCKEY_ESC));
    }

    void testMaskedMode()
    {
        TextInput input;
        input.setMasked(true);
        input.handleInput('s');
        input.handleInput('e');
        input.handleInput('c');
        QCOMPARE(input.value(), std::string("sec")); // value stored unmasked
    }

    void testLabelAndPlaceholder()
    {
        TextInput input;
        input.setLabel("Path: ");
        input.setPlaceholder("/enter/path");
        QCOMPARE(input.label(), std::string("Path: "));
    }

    void testPrintableRange()
    {
        TextInput input;
        // Space (32) should be accepted
        QVERIFY(input.handleInput(' '));
        QCOMPARE(input.value(), std::string(" "));
        // Tilde (126) should be accepted
        QVERIFY(input.handleInput('~'));
        QCOMPARE(input.value(), std::string(" ~"));
        // DEL (127) is backspace, not printable
        // Char 127 should trigger backspace
        QVERIFY(input.handleInput(127));
        QCOMPARE(input.value(), std::string(" "));
    }
};

// ════════════════════════════════════════════════════════════
// SelectableList Tests
// ════════════════════════════════════════════════════════════

class TestSelectableList : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        SelectableList list;
        QCOMPARE(list.count(), 0);
        QCOMPARE(list.selected(), -1);
        QVERIFY(!list.hasSelection());
    }

    void testSetCount()
    {
        SelectableList list;
        list.setCount(10);
        QCOMPARE(list.count(), 10);
        QCOMPARE(list.selected(), 0); // clamped from -1 to 0
        QVERIFY(list.hasSelection());
    }

    void testNavigationDown()
    {
        SelectableList list;
        list.setCount(5);
        list.setSelected(0);

        auto act = list.handleInput('j');
        QCOMPARE(act, SelectableList::Action::SelectionChanged);
        QCOMPARE(list.selected(), 1);

        act = list.handleInput(NCKEY_DOWN);
        QCOMPARE(act, SelectableList::Action::SelectionChanged);
        QCOMPARE(list.selected(), 2);
    }

    void testNavigationUp()
    {
        SelectableList list;
        list.setCount(5);
        list.setSelected(3);

        auto act = list.handleInput('k');
        QCOMPARE(act, SelectableList::Action::SelectionChanged);
        QCOMPARE(list.selected(), 2);

        act = list.handleInput(NCKEY_UP);
        QCOMPARE(act, SelectableList::Action::SelectionChanged);
        QCOMPARE(list.selected(), 1);
    }

    void testBoundsClamping()
    {
        SelectableList list;
        list.setCount(3);
        list.setSelected(0);

        // Can't go above 0
        auto act = list.handleInput('k');
        QCOMPARE(act, SelectableList::Action::None);
        QCOMPARE(list.selected(), 0);

        // Navigate to end
        list.setSelected(2);
        act = list.handleInput('j');
        QCOMPARE(act, SelectableList::Action::None);
        QCOMPARE(list.selected(), 2);
    }

    void testGoToFirstLast()
    {
        SelectableList list;
        list.setCount(10);
        list.setSelected(5);

        list.handleInput('g'); // go to first
        QCOMPARE(list.selected(), 0);
        QCOMPARE(list.scroll(), 0);

        list.handleInput('G'); // go to last
        QCOMPARE(list.selected(), 9);
    }

    void testScrollOffset()
    {
        SelectableList list;
        list.setCount(100);
        list.setSelected(0);

        // Move down many times
        for (int i = 0; i < 50; ++i)
            list.handleInput('j');
        QCOMPARE(list.selected(), 50);

        // Ensure visible with small viewport
        list.ensureVisible(10);
        QVERIFY(list.scroll() <= 50);
        QVERIFY(list.scroll() >= 50 - 10 + 1);
    }

    void testCheckboxToggle()
    {
        SelectableList list;
        list.setCount(5);
        list.setCheckboxes(true);
        list.setSelected(0);

        auto act = list.handleInput(' ');
        QCOMPARE(act, SelectableList::Action::ToggleCheck);

        act = list.handleInput('a');
        QCOMPARE(act, SelectableList::Action::ToggleAll);
    }

    void testCheckboxDisabled()
    {
        SelectableList list;
        list.setCount(5);
        list.setCheckboxes(false);

        auto act = list.handleInput(' ');
        QCOMPARE(act, SelectableList::Action::None); // not ToggleCheck

        act = list.handleInput('a');
        QCOMPARE(act, SelectableList::Action::None); // not ToggleAll
    }

    void testSubmitAction()
    {
        SelectableList list;
        list.setCount(5);
        list.setSelected(2);

        auto act = list.handleInput(NCKEY_ENTER);
        QCOMPARE(act, SelectableList::Action::Submit);
    }

    void testEmptyList()
    {
        SelectableList list;
        list.setCount(0);
        QCOMPARE(list.selected(), -1);
        QVERIFY(!list.hasSelection());

        auto act = list.handleInput('j');
        QCOMPARE(act, SelectableList::Action::None);
    }

    void testSetCountClampsSelection()
    {
        SelectableList list;
        list.setCount(10);
        list.setSelected(9);
        QCOMPARE(list.selected(), 9);

        // Shrink count — selection should clamp
        list.setCount(5);
        QCOMPARE(list.selected(), 4); // clamped to count-1
    }

    void testHandleClick()
    {
        SelectableList list;
        list.setCount(10);
        list.setSelected(0);

        int result = list.handleClick(5, 0, 20); // click at row 5, list starts at 0
        QCOMPARE(result, 5);
        QCOMPARE(list.selected(), 5);

        // Click outside
        result = list.handleClick(25, 0, 20);
        QCOMPARE(result, -1);
    }

    void testRowsPerItem()
    {
        SelectableList list;
        list.setRowsPerItem(2);
        QCOMPARE(list.rowsPerItem(), 2);

        list.setCount(10);
        list.setSelected(5);
        list.ensureVisible(8); // 8 rows = 4 items visible
        QVERIFY(list.scroll() >= 5 - 4 + 1);
    }
};

// ════════════════════════════════════════════════════════════
// ProgressBarWidget Tests
// ════════════════════════════════════════════════════════════

class TestProgressBar : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        ProgressBarWidget bar;
        QCOMPARE(bar.done(), 0);
        QCOMPARE(bar.total(), 0);
        QCOMPARE(bar.label(), std::string());
    }

    void testSetProgress()
    {
        ProgressBarWidget bar;
        bar.set(5, 10, "hashing", "/path/file.nes");
        QCOMPARE(bar.done(), 5);
        QCOMPARE(bar.total(), 10);
        QCOMPARE(bar.label(), std::string("hashing"));
    }

    void testReset()
    {
        ProgressBarWidget bar;
        bar.set(3, 7, "scanning");
        bar.reset();
        QCOMPARE(bar.done(), 0);
        QCOMPARE(bar.total(), 0);
        QCOMPARE(bar.label(), std::string());
    }

    void testZeroPercent()
    {
        ProgressBarWidget bar;
        bar.set(0, 100, "start");
        QCOMPARE(bar.done(), 0);
    }

    void testHundredPercent()
    {
        ProgressBarWidget bar;
        bar.set(100, 100, "done");
        QCOMPARE(bar.done(), 100);
        QCOMPARE(bar.total(), 100);
    }

    void testFiftyPercent()
    {
        ProgressBarWidget bar;
        bar.set(50, 100, "midway");
        QCOMPARE(bar.done(), 50);
    }

    void testZeroTotal()
    {
        // Guard against div-by-zero in render — state should still be valid
        ProgressBarWidget bar;
        bar.set(0, 0, "empty");
        QCOMPARE(bar.done(), 0);
        QCOMPARE(bar.total(), 0);
    }
};

// ════════════════════════════════════════════════════════════
// SplitPane Tests
// ════════════════════════════════════════════════════════════

class TestSplitPane : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultLeftPercent()
    {
        SplitPane pane;
        auto layout = pane.compute(120, 40, /*headerH=*/3, /*footerH=*/1);
        // Default 55% of 120 = 66
        QCOMPARE(layout.leftW, 66);
        QCOMPARE(layout.rightX, 67); // leftW + 1
        QCOMPARE(layout.bodyY, 3);
    }

    void testCustomLeftPercent()
    {
        SplitPane pane;
        pane.setLeftPercent(30);
        auto layout = pane.compute(100, 30, 2, 1);
        QCOMPARE(layout.leftW, 30);
        QCOMPARE(layout.rightX, 31);
    }

    void testBodyHeightCalculation()
    {
        SplitPane pane;
        // rows=40, headerH=3, footerH=1, progressH=2 (default)
        auto layout = pane.compute(80, 40, 3, 1);
        QCOMPARE(layout.bodyH, 34); // 40 - 3 - 1 - 2
        QCOMPARE(layout.progressY, 37); // bodyY(3) + bodyH(34)
    }

    void testMinimumBodyHeight()
    {
        SplitPane pane;
        // Very small terminal — bodyH should be clamped to 3
        auto layout = pane.compute(80, 8, 3, 3, 2);
        QCOMPARE(layout.bodyH, 3);
    }

    void testMinimumLeftWidth()
    {
        SplitPane pane;
        // Very narrow terminal — leftW should be clamped to 20
        auto layout = pane.compute(30, 20, 2, 1);
        QCOMPARE(layout.leftW, 20);
    }

    void testProgressYPosition()
    {
        SplitPane pane;
        auto layout = pane.compute(100, 30, 2, 1, 3);
        // bodyH = 30 - 2 - 1 - 3 = 24
        QCOMPARE(layout.progressY, 26); // bodyY(2) + bodyH(24)
    }
};

// ════════════════════════════════════════════════════════════
// Toast Tests
// ════════════════════════════════════════════════════════════

class TestToast : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        Toast toast;
        QVERIFY(!toast.visible());
        QVERIFY(!toast.tick()); // no state change
    }

    void testShowMakesVisible()
    {
        Toast toast;
        toast.show("Hello", Toast::Level::Info, 5000);
        QVERIFY(toast.visible());
    }

    void testDismiss()
    {
        Toast toast;
        toast.show("Hello");
        QVERIFY(toast.visible());
        toast.dismiss();
        QVERIFY(!toast.visible());
    }

    void testTickExpiresMessage()
    {
        Toast toast;
        // Show with 0ms duration — should expire immediately
        toast.show("Expires now", Toast::Level::Warning, 0);
        QVERIFY(toast.visible());
        // tick should expire it and return true (state changed)
        bool changed = toast.tick();
        QVERIFY(changed);
        QVERIFY(!toast.visible());
    }

    void testTickNoChangeWhenNotVisible()
    {
        Toast toast;
        QVERIFY(!toast.tick());
    }

    void testShowReplacesPrevious()
    {
        Toast toast;
        toast.show("First");
        toast.show("Second", Toast::Level::Error, 10000);
        QVERIFY(toast.visible());
        // After replacing, tick shouldn't expire immediately (10s timeout)
        QVERIFY(!toast.tick());
        QVERIFY(toast.visible());
    }
};

// ════════════════════════════════════════════════════════════
// HelpOverlay Tests
// ════════════════════════════════════════════════════════════

class TestHelpOverlay : public QObject
{
    Q_OBJECT

private slots:
    void testInitialState()
    {
        HelpOverlay overlay;
        QVERIFY(!overlay.visible());
    }

    void testShowMakesVisible()
    {
        HelpOverlay overlay;
        overlay.show("Library", {{"j/k", "Navigate"}, {"Enter", "Select"}});
        QVERIFY(overlay.visible());
    }

    void testDismiss()
    {
        HelpOverlay overlay;
        overlay.show("Test", {});
        overlay.dismiss();
        QVERIFY(!overlay.visible());
    }

    void testHandleInputQuestionMark()
    {
        HelpOverlay overlay;
        overlay.show("Test", {{"?", "Help"}});
        QVERIFY(overlay.handleInput('?'));
        QVERIFY(!overlay.visible());
    }

    void testHandleInputEsc()
    {
        HelpOverlay overlay;
        overlay.show("Test", {});
        QVERIFY(overlay.handleInput(NCKEY_ESC));
        QVERIFY(!overlay.visible());
    }

    void testHandleInputQ()
    {
        HelpOverlay overlay;
        overlay.show("Test", {});
        QVERIFY(overlay.handleInput('q'));
        QVERIFY(!overlay.visible());
    }

    void testHandleInputConsumesAll()
    {
        HelpOverlay overlay;
        overlay.show("Test", {});
        // Any input should be consumed when visible (modal)
        QVERIFY(overlay.handleInput('x'));
        // But 'x' doesn't dismiss it — overlay is still visible
        // Actually let's re-check: handleInput for 'x' returns true (consumed)
        // but only ?, Esc, q dismiss it
        overlay.show("Test", {});
        QVERIFY(overlay.handleInput('j'));
        QVERIFY(overlay.visible()); // still visible, just consumed
    }

    void testHandleInputWhenHidden()
    {
        HelpOverlay overlay;
        // When not visible, should not consume input
        QVERIFY(!overlay.handleInput('j'));
    }
};

// ════════════════════════════════════════════════════════════
// Main
// ════════════════════════════════════════════════════════════

int main(int argc, char *argv[])
{
    int status = 0;

    {
        TestTextInput t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestSelectableList t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestProgressBar t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestSplitPane t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestToast t;
        status |= QTest::qExec(&t, argc, argv);
    }
    {
        TestHelpOverlay t;
        status |= QTest::qExec(&t, argc, argv);
    }

    return status;
}

#include "test_tui_widgets.moc"
