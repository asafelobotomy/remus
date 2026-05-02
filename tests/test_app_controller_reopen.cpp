#include <QtTest>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "controllers/app_controller.h"

using namespace Remus;

class AppControllerReopenTest : public QObject {
    Q_OBJECT

private slots:
    void reopeningLibraryRebuildsOrchestrator();
    void eraseLibraryDatabaseWipesDataAndReopens();
};

void AppControllerReopenTest::reopeningLibraryRebuildsOrchestrator()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    AppController controller;
    QSignalSpy libraryOpenedSpy(&controller, &AppController::libraryOpened);
    QSignalSpy orchestratorChangedSpy(&controller, &AppController::orchestratorChanged);

    const QString dbPath = tempDir.filePath(QStringLiteral("library.db"));

    QVERIFY(controller.openLibrary(dbPath));
    QVERIFY(controller.isLibraryOpen());

    ProviderOrchestrator *firstOrchestrator = controller.orchestrator();
    QVERIFY(firstOrchestrator != nullptr);

    QVERIFY(controller.openLibrary(dbPath));
    QVERIFY(controller.isLibraryOpen());

    ProviderOrchestrator *secondOrchestrator = controller.orchestrator();
    QVERIFY(secondOrchestrator != nullptr);
    QVERIFY(secondOrchestrator != firstOrchestrator);

    QCOMPARE(libraryOpenedSpy.count(), 2);
    QCOMPARE(orchestratorChangedSpy.count(), 2);
}

void AppControllerReopenTest::eraseLibraryDatabaseWipesDataAndReopens()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.filePath(QStringLiteral("erase-test.db"));

    AppController controller;
    QVERIFY(controller.openLibrary(dbPath));
    QVERIFY(controller.isLibraryOpen());

    // Seed a library record so the database contains at least one row.
    Database *db = controller.database();
    const int libId = db->insertLibrary(QStringLiteral("/roms/seed"), QStringLiteral("seed"));
    QVERIFY(libId > 0);
    QCOMPARE(db->getLibraryCount(), 1);

    QSignalSpy erasedSpy(&controller, &AppController::libraryDatabaseErased);
    QSignalSpy openedSpy(&controller, &AppController::libraryOpened);

    // Erase the database.
    QVERIFY(controller.eraseLibraryDatabase());

    // Controller must still be open on the same path.
    QVERIFY(controller.isLibraryOpen());
    QCOMPARE(controller.libraryPath(), dbPath);
    QVERIFY(QFile::exists(dbPath));

    // The seeded row must be gone.
    QCOMPARE(controller.database()->getLibraryCount(), 0);

    // Signals must have fired.
    QCOMPARE(erasedSpy.count(), 1);
    QCOMPARE(openedSpy.count(), 1); // spy created after initial open; only the erase-reopen fires it
}

QTEST_MAIN(AppControllerReopenTest)
#include "test_app_controller_reopen.moc"