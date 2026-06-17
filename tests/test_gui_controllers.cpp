#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFile>
#include <QCoreApplication>

#include "controllers/app_controller.h"
#include "controllers/conversion_controller.h"
#include "controllers/match_controller.h"
#include "controllers/scan_controller.h"
#include "controllers/verification_controller.h"
#include "models/verification_result_model.h"
#include "../src/core/database.h"
#include "../src/core/database_types.h"

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
    void scanController_scanDirectoryCompletesWithInsertedFiles();
    void matchController_refreshModelReflectsUnconfirmedMatch();
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

void GuiControllersSmokeTest::scanController_scanDirectoryCompletesWithInsertedFiles() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString scanDir = tempDir.filePath(QStringLiteral("roms"));
    QVERIFY(QDir().mkpath(scanDir));

    QFile romFile(scanDir + QStringLiteral("/sample.bin"));
    QVERIFY(romFile.open(QIODevice::WriteOnly));
    QVERIFY(romFile.write(QByteArrayLiteral("rom-bytes")) == 9);
    romFile.close();

    AppController app;
    QVERIFY(app.openLibrary(tempDir.filePath(QStringLiteral("library.db"))));

    ScanController scan(&app);
    QSignalSpy completedSpy(&scan, &ScanController::scanCompleted);

    scan.startScan(scanDir);

    QTRY_COMPARE(completedSpy.count(), 1);
    QVERIFY(completedSpy.at(0).at(0).toInt() >= 1);
    QVERIFY(!scan.isScanning());
}

void GuiControllersSmokeTest::matchController_refreshModelReflectsUnconfirmedMatch() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    AppController app;
    QVERIFY(app.openLibrary(tempDir.filePath(QStringLiteral("library.db"))));

    Database *db = app.database();
    const int libraryId = db->insertLibrary(tempDir.path(), QStringLiteral("roms"));
    QVERIFY(libraryId > 0);

    const QString romDir = tempDir.filePath(QStringLiteral("roms"));
    QVERIFY(QDir().mkpath(romDir));
    const QString romPath = QDir(romDir).filePath(QStringLiteral("game.bin"));
    {
        QFile romFile(romPath);
        QVERIFY(romFile.open(QIODevice::WriteOnly));
        romFile.write("fake rom");
    }

    FileRecord file;
    file.libraryId = libraryId;
    file.filename = QStringLiteral("game.bin");
    file.originalPath = romPath;
    file.currentPath = romPath;
    file.extension = QStringLiteral(".bin");
    file.fileSize = 9;
    file.md5 = QStringLiteral("0123456789abcdef0123456789abcdef");
    const int fileId = db->insertFile(file);
    QVERIFY(fileId > 0);

    QSqlQuery gameQuery(db->database());
    QVERIFY(gameQuery.exec(QStringLiteral("INSERT INTO games (title) VALUES ('Matched Game')")));
    const int gameId = gameQuery.lastInsertId().toInt();
    QVERIFY(gameId > 0);

    QSqlQuery matchQuery(db->database());
    matchQuery.prepare(
        QStringLiteral("INSERT INTO matches (file_id, game_id, confidence, match_method, is_confirmed, is_rejected) "
                       "VALUES (?, ?, ?, 'test', 0, 0)"));
    matchQuery.addBindValue(fileId);
    matchQuery.addBindValue(gameId);
    matchQuery.addBindValue(0.85);
    QVERIFY2(matchQuery.exec(), qPrintable(matchQuery.lastError().text()));

    MatchController match(&app);
    match.refreshModel();

    QCOMPARE(match.unconfirmedMatchCount(), 1);
}

QTEST_MAIN(GuiControllersSmokeTest)

#include "test_gui_controllers.moc"
