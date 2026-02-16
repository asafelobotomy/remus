#include <QtTest>
#include <QSignalSpy>
#include "ui/controllers/dat_manager_controller.h"

using namespace Remus;

class DatManagerControllerTest : public QObject {
    Q_OBJECT

private slots:
    void handlesMissingProvider();
};

void DatManagerControllerTest::handlesMissingProvider()
{
    DatManagerController controller(nullptr);
    QSignalSpy errSpy(&controller, &DatManagerController::error);

    QVERIFY(controller.loadedDats().isEmpty());
    QVERIFY(!controller.checkForUpdate("/tmp/file.dat"));
    QVERIFY(!controller.loadDat("/tmp/file.dat"));
    QVERIFY(!controller.reloadDat("/tmp/file.dat"));
    QVERIFY(!errSpy.isEmpty());
}

QTEST_MAIN(DatManagerControllerTest)
#include "test_dat_manager_controller.moc"
