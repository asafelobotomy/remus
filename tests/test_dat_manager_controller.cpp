#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include "ui/controllers/dat_manager_controller.h"
#include "core/database.h"

using namespace Remus;

class DatManagerControllerTest : public QObject {
    Q_OBJECT

private slots:
    void handlesMissingProvider();
    void loadsAndRemovesPatchCatalog();
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

void DatManagerControllerTest::loadsAndRemovesPatchCatalog()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.filePath("remus.db");
    Database db;
    QVERIFY(db.initialize(dbPath));

    const QString datPath = tempDir.filePath("patches.dat");
    QFile datFile(datPath);
    QVERIFY(datFile.open(QIODevice::WriteOnly | QIODevice::Text));

    QTextStream stream(&datFile);
    stream << "<?xml version=\"1.0\"?>\n"
           << "<datafile>\n"
           << "  <header><name>NES Patch Catalog</name><version>2026-03</version><description>Known translated outputs</description></header>\n"
           << "  <game name=\"Dragon Quest III\" base_title=\"Dragon Quest III\" patch_name=\"English v2.0\">\n"
           << "    <rom name=\"Dragon Quest III (English v2.0).nes\" size=\"1\" crc=\"1234ABCD\" md5=\"0123456789abcdef0123456789abcdef\" sha1=\"0123456789abcdef0123456789abcdef01234567\"/>\n"
           << "  </game>\n"
           << "</datafile>\n";
    datFile.close();

    DatManagerController controller(nullptr, &db);
    QSignalSpy patchLoadedSpy(&controller, &DatManagerController::patchDatLoaded);

    QVERIFY(controller.loadPatchDat(datPath, "NES"));
    QCOMPARE(patchLoadedSpy.count(), 1);
    QVERIFY(controller.hasPatchDat("NES"));

    const QVariantList loaded = controller.loadedPatchDats();
    QCOMPARE(loaded.size(), 1);
    QCOMPARE(loaded.first().toMap().value("system").toString(), QStringLiteral("NES"));
    QCOMPARE(loaded.first().toMap().value("name").toString(), QStringLiteral("NES Patch Catalog"));

    QVERIFY(controller.removePatchDat("NES"));
    QVERIFY(!controller.hasPatchDat("NES"));
    QVERIFY(controller.loadedPatchDats().isEmpty());
}

QTEST_MAIN(DatManagerControllerTest)
#include "test_dat_manager_controller.moc"
