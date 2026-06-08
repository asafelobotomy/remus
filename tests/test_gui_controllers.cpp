#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "controllers/app_controller.h"
#include "controllers/conversion_controller.h"
#include "controllers/match_controller.h"
#include "controllers/scan_controller.h"
#include "controllers/verification_controller.h"
#include "models/verification_result_model.h"

using namespace Remus;

class GuiControllersSmokeTest : public QObject {
    Q_OBJECT

private slots:
    void scanController_requiresOpenLibrary();
    void scanController_rejectsMissingDirectory();
    void scanController_persistsLastDirectory();
    void matchController_startsWithZeroUnconfirmed();
    void matchController_refreshModelWithoutLibraryIsNoOp();
    void verificationController_verifySelectedRequiresSelection();
    void verificationController_clearResultsResetsSummary();
    void verificationController_verifyAllOnEmptyLibrary();
    void conversionController_defaultTargetFormat();
    void conversionController_refreshToolStatusPopulatesMap();
};

void GuiControllersSmokeTest::scanController_requiresOpenLibrary() {
    AppController app;
    ScanController scan(&app);

    QSignalSpy errorSpy(&scan, &ScanController::scanError);
    scan.startScan(QStringLiteral("/tmp/roms"));

    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.at(0).at(0).toString().contains(QStringLiteral("Open a library")));
    QVERIFY(!scan.isScanning());
}

void GuiControllersSmokeTest::scanController_rejectsMissingDirectory() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    AppController app;
    QVERIFY(app.openLibrary(tempDir.filePath(QStringLiteral("library.db"))));

    ScanController scan(&app);
    QSignalSpy errorSpy(&scan, &ScanController::scanError);

    scan.startScan(tempDir.filePath(QStringLiteral("does-not-exist")));
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.at(0).at(0).toString().contains(QStringLiteral("does not exist")));
    QVERIFY(!scan.isScanning());
}

void GuiControllersSmokeTest::scanController_persistsLastDirectory() {
    AppController app;
    ScanController scan(&app);

    scan.setLastDirectory(QStringLiteral("/tmp/example-roms"));
    QCOMPARE(scan.lastDirectory(), QStringLiteral("/tmp/example-roms"));
}

void GuiControllersSmokeTest::matchController_startsWithZeroUnconfirmed() {
    AppController app;
    MatchController match(&app);

    QCOMPARE(match.unconfirmedMatchCount(), 0);
    QVERIFY(!match.isMatching());
}

void GuiControllersSmokeTest::matchController_refreshModelWithoutLibraryIsNoOp() {
    AppController app;
    MatchController match(&app);

    // Should not crash when no library is open and no model is attached.
    match.refreshModel();
    QCOMPARE(match.unconfirmedMatchCount(), 0);
}

void GuiControllersSmokeTest::verificationController_verifySelectedRequiresSelection() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    AppController app;
    QVERIFY(app.openLibrary(tempDir.filePath(QStringLiteral("library.db"))));

    VerificationController verification(&app);
    verification.verifySelected();

    QVERIFY(verification.lastError().contains(QStringLiteral("Select a file")));
    QVERIFY(!verification.isVerifying());
}

void GuiControllersSmokeTest::verificationController_clearResultsResetsSummary() {
    AppController app;
    VerificationController verification(&app);
    VerificationResultModel model;
    verification.setModel(&model);

    verification.clearResults();
    QCOMPARE(verification.summary().size(), 0);
    QCOMPARE(model.rowCount(), 0);
}

void GuiControllersSmokeTest::verificationController_verifyAllOnEmptyLibrary() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    AppController app;
    QVERIFY(app.openLibrary(tempDir.filePath(QStringLiteral("library.db"))));

    VerificationController verification(&app);
    verification.verifyAll();

    QVERIFY(!verification.isVerifying());
    QCOMPARE(verification.summary().value(QStringLiteral("totalFiles")).toInt(), 0);
    QVERIFY(verification.lastError().isEmpty());
}

void GuiControllersSmokeTest::conversionController_defaultTargetFormat() {
    AppController app;
    ConversionController conversion(&app);

    QCOMPARE(conversion.targetFormat(), QStringLiteral("CHD"));
    QVERIFY(!conversion.isConverting());

    conversion.setTargetFormat(QStringLiteral("CSO"));
    QCOMPARE(conversion.targetFormat(), QStringLiteral("CSO"));
}

void GuiControllersSmokeTest::conversionController_refreshToolStatusPopulatesMap() {
    AppController app;
    ConversionController conversion(&app);

    conversion.refreshToolStatus();
    QVERIFY(!conversion.toolStatus().isEmpty());
}

QTEST_MAIN(GuiControllersSmokeTest)

#include "test_gui_controllers.moc"
