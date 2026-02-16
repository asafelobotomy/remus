#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include "../src/ui/controllers/library_controller.h"
#include "../src/core/database.h"

using namespace Remus;

class LibraryControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void testScanInvalidPath();
    void testScanAndHash();
};

void LibraryControllerTest::testScanInvalidPath()
{
    Database db;
    QVERIFY(db.initialize(":memory:"));

    LibraryController controller(&db);
    QSignalSpy errorSpy(&controller, &LibraryController::scanError);

    controller.scanDirectory("/path/does/not/exist");
    QCOMPARE(errorSpy.count(), 1);
}

void LibraryControllerTest::testScanAndHash()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString romPath = dir.path() + "/test.nes";
    QByteArray header("NES\x1A");
    header.append(QByteArray(12, '\x00'));
    QByteArray body("rom-test-data");

    QFile file(romPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(header) == header.size());
    QVERIFY(file.write(body) == body.size());
    file.close();

    const QString dbPath = dir.path() + "/test.db";
    Database db;
    QVERIFY(db.initialize(dbPath));

    LibraryController controller(&db);
    QSignalSpy scanSpy(&controller, &LibraryController::scanCompleted);
    QSignalSpy hashSpy(&controller, &LibraryController::hashingCompleted);

    controller.scanDirectory(dir.path());
    QVERIFY(scanSpy.wait(5000));
    QCOMPARE(scanSpy.count(), 1);

    controller.hashFiles();
    QVERIFY(hashSpy.wait(5000));

    QList<FileRecord> files = db.getAllFiles();
    QCOMPARE(files.size(), 1);
    QVERIFY(!files.first().crc32.isEmpty());
}

QTEST_MAIN(LibraryControllerTest)
#include "test_library_controller.moc"
