/**
 * @file test_options_screen.cpp
 * @brief Unit tests for OptionsScreen field structure and initial state.
 *
 * Verifies that the constructor correctly populates settings fields from
 * constants, with proper section headers, field types, and default values.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "../src/tui/app.h"
#include "../src/tui/options_screen.h"
#include "../src/core/database.h"
#include "../src/core/constants/constants.h"

class TestOptionsScreen : public QObject
{
    Q_OBJECT

private slots:

    // ── Field population ──────────────────────────────────

    void testFieldsNotEmpty()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/opts.db"));

        OptionsScreen screen(app);

        QVERIFY(screen.fieldCount() > 0);
    }

    void testSectionHeaders()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/opts.db"));

        OptionsScreen screen(app);

        // Should have at least 3 sections: Metadata Providers, Organize, Performance
        int sectionCount = 0;
        for (int i = 0; i < screen.fieldCount(); ++i) {
            if (screen.fieldAt(i).isSection)
                sectionCount++;
        }
        QVERIFY(sectionCount >= 3);

        // First field should be "METADATA PROVIDERS" section
        QVERIFY(screen.fieldAt(0).isSection);
        QCOMPARE(screen.fieldAt(0).label, std::string("METADATA PROVIDERS"));
    }

    void testProviderFieldsPresent()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/opts.db"));

        OptionsScreen screen(app);

        // Verify that provider fields from constants are present
        size_t providerFieldCount = Remus::Constants::ALL_PROVIDER_FIELDS.size();
        QVERIFY(providerFieldCount > 0);

        // Count non-section fields before the ORGANIZE section
        int providerFields = 0;
        for (int i = 1; i < screen.fieldCount(); ++i) {
            if (screen.fieldAt(i).isSection) break;  // hit next section
            providerFields++;
        }
        QCOMPARE(providerFields, static_cast<int>(providerFieldCount));
    }

    void testOrganizeFieldsPresent()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/opts.db"));

        OptionsScreen screen(app);

        // Find the ORGANIZE section
        bool foundOrganize = false;
        for (int i = 0; i < screen.fieldCount(); ++i) {
            if (screen.fieldAt(i).isSection && screen.fieldAt(i).label == "ORGANIZE") {
                foundOrganize = true;
                // After the header, expect: Naming Template, Organize by System, Preserve Originals
                QVERIFY(i + 3 < screen.fieldCount());
                QCOMPARE(screen.fieldAt(i + 1).label, std::string("Naming Template"));
                QCOMPARE(screen.fieldAt(i + 2).label, std::string("Organize by System"));
                QCOMPARE(screen.fieldAt(i + 3).label, std::string("Preserve Originals"));
                break;
            }
        }
        QVERIFY(foundOrganize);
    }

    void testToggleFieldTypes()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/opts.db"));

        OptionsScreen screen(app);

        // Organize by System and Preserve Originals should be Toggle type
        bool foundToggle = false;
        for (int i = 0; i < screen.fieldCount(); ++i) {
            if (screen.fieldAt(i).label == "Organize by System") {
                QCOMPARE(screen.fieldAt(i).type, OptionsScreen::FieldType::Toggle);
                QCOMPARE(screen.fieldAt(i).value, std::string("true"));
                foundToggle = true;
            }
            if (screen.fieldAt(i).label == "Parallel Hashing") {
                QCOMPARE(screen.fieldAt(i).type, OptionsScreen::FieldType::Toggle);
            }
        }
        QVERIFY(foundToggle);
    }

    void testPasswordFieldTypes()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/opts.db"));

        OptionsScreen screen(app);

        // At least one provider field should be Password type
        bool hasPassword = false;
        for (int i = 0; i < screen.fieldCount(); ++i) {
            if (screen.fieldAt(i).type == OptionsScreen::FieldType::Password) {
                hasPassword = true;
                break;
            }
        }
        QVERIFY(hasPassword);
    }

    // ── Initial state ─────────────────────────────────────

    void testInitialStateClean()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        TuiApp app;
        QVERIFY(app.db().initialize(tmp.path() + "/opts.db"));

        OptionsScreen screen(app);

        QVERIFY(!screen.isDirty());
        QVERIFY(!screen.isEditing());
        // Selected index should be 1 (first non-header field)
        QCOMPARE(screen.selectedIndex(), 1);
    }

    // ── SettingField struct defaults ──────────────────────

    void testSettingFieldDefaults()
    {
        OptionsScreen::SettingField sf;
        QVERIFY(sf.label.empty());
        QVERIFY(sf.key.empty());
        QVERIFY(sf.value.empty());
        QCOMPARE(sf.type, OptionsScreen::FieldType::Text);
        QVERIFY(!sf.isSection);
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication coreApp(argc, argv);
    TestOptionsScreen t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_options_screen.moc"
