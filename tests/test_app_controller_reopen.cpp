#include <QtTest>

#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "controllers/app_controller.h"
#include "metadata/provider_orchestrator.h"
#include "services/secret_store.h"

using namespace Remus;

class AppControllerReopenTest : public QObject {
    Q_OBJECT

private slots:
    void reopeningLibraryRebuildsOrchestrator();
    void eraseLibraryDatabaseWipesDataAndReopens();
    void orchestratorSkipsCredentialsOnBackendError();
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

/// Verifies that when the OS keychain returns BackendError, rebuildOrchestrator()
/// does NOT fall back to plaintext QSettings for provider credentials.
/// When the keychain is available and returns NotFound, the QSettings fallback
/// IS expected (correct legacy migration path).
void AppControllerReopenTest::orchestratorSkipsCredentialsOnBackendError()
{
    // Seed QSettings with IGDB credentials so the old insecure code path
    // would have populated the IGDB provider.
    QSettings s(QStringLiteral("Remus"), QStringLiteral("Remus"));
    s.setValue(QStringLiteral("igdb/client_id"),     QStringLiteral("qs_test_id"));
    s.setValue(QStringLiteral("igdb/client_secret"),  QStringLiteral("qs_test_secret"));
    s.sync();

    // Probe actual keychain state so the assertion is meaningful in all envs.
    const SecretStore::ReadResult kr =
        SecretStore::readWithStatus(QStringLiteral("igdb/client_id"));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    AppController controller;
    QVERIFY(controller.openLibrary(tempDir.filePath(QStringLiteral("bke-test.db"))));

    const QStringList enabled = controller.orchestrator()->getEnabledProviders();

    if (kr.status == SecretStore::ReadResult::Status::BackendError) {
        // Guard must prevent IGDB from silently loading plaintext QSettings creds.
        QVERIFY2(!enabled.contains(QStringLiteral("igdb")),
                 "IGDB must not be added when keychain returns BackendError");
    } else if (kr.status == SecretStore::ReadResult::Status::NotFound) {
        // Keychain available, key absent — QSettings fallback is the correct path.
        QVERIFY2(enabled.contains(QStringLiteral("igdb")),
                 "IGDB must be added via QSettings fallback when keychain returns NotFound");
    } else {
        QSKIP("igdb/client_id found in keychain; cannot test plaintext fallback path");
    }

    s.remove(QStringLiteral("igdb/client_id"));
    s.remove(QStringLiteral("igdb/client_secret"));
    s.sync();
}

QTEST_MAIN(AppControllerReopenTest)
#include "test_app_controller_reopen.moc"