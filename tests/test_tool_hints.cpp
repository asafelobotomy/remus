/**
 * @file test_tool_hints.cpp
 * @brief Unit tests for ToolHints lookup table and query functions.
 *
 * Validates the static tool registry, install hint lookups, and
 * the getInstallHint/allTools API.
 */

#include <QtTest/QtTest>

#include "../src/tui/tool_hints.h"

class TestToolHints : public QObject
{
    Q_OBJECT

private slots:

    void testAllToolsNonEmpty()
    {
        const auto &tools = ToolHints::allTools();
        QVERIFY(tools.size() >= 4);  // at least chdman, xdelta3, flips, 7z
    }

    void testAllToolsHaveRequiredFields()
    {
        for (const auto &t : ToolHints::allTools()) {
            QVERIFY2(!t.name.empty(),
                     ("Tool missing name: binary=" + t.binary).c_str());
            QVERIFY2(!t.binary.empty(),
                     ("Tool missing binary: name=" + t.name).c_str());
            QVERIFY2(!t.installHint.empty(),
                     ("Tool missing installHint: " + t.name).c_str());
            QVERIFY2(!t.description.empty(),
                     ("Tool missing description: " + t.name).c_str());
        }
    }

    void testGetInstallHintKnownTool()
    {
        std::string hint = ToolHints::getInstallHint("chdman");
        QVERIFY(!hint.empty());
        QVERIFY(hint.find("mame-tools") != std::string::npos);
    }

    void testGetInstallHintXdelta3()
    {
        std::string hint = ToolHints::getInstallHint("xdelta3");
        QVERIFY(!hint.empty());
        QVERIFY(hint.find("xdelta3") != std::string::npos);
    }

    void testGetInstallHintUnknownTool()
    {
        std::string hint = ToolHints::getInstallHint("nonexistenttool12345");
        QVERIFY(hint.empty());
    }

    void testChdmanEntry()
    {
        bool found = false;
        for (const auto &t : ToolHints::allTools()) {
            if (t.binary == "chdman") {
                QCOMPARE(t.name, std::string("chdman"));
                QVERIFY(t.description.find("CHD") != std::string::npos);
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testFlipsEntry()
    {
        bool found = false;
        for (const auto &t : ToolHints::allTools()) {
            if (t.binary == "flips") {
                QVERIFY(t.installHint.find("Flips") != std::string::npos
                        || t.installHint.find("flips") != std::string::npos);
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testToolInfoStructDefaults()
    {
        ToolHints::ToolInfo info;
        QVERIFY(info.name.empty());
        QVERIFY(info.binary.empty());
        QVERIFY(info.installHint.empty());
        QVERIFY(info.description.empty());
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication coreApp(argc, argv);
    TestToolHints t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_tool_hints.moc"
