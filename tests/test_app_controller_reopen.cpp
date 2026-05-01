#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "controllers/app_controller.h"

using namespace Remus;

class AppControllerReopenTest : public QObject {
    Q_OBJECT

private slots:
    void reopeningLibraryRebuildsOrchestrator();
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

QTEST_MAIN(AppControllerReopenTest)
#include "test_app_controller_reopen.moc"