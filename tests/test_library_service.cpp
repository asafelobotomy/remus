/**
 * @file test_library_service.cpp
 * @brief Unit tests for LibraryService (scan, stats, systems)
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "../src/services/library_service.h"
#include "../src/core/database.h"

using namespace Remus;

class TestLibraryService : public QObject
{
    Q_OBJECT

private:
    void createStubRoms(const QString &dir)
    {
        // NES stub with iNES header
        QByteArray nesHeader("NES\x1A");
        nesHeader.append(QByteArray(12, '\x00'));
        nesHeader.append(QByteArray(32, '\xBB'));

        QFile f1(dir + "/TestRom.nes");
        QVERIFY(f1.open(QIODevice::WriteOnly));
        f1.write(nesHeader);
        f1.close();

        QFile f2(dir + "/Another.nes");
        QVERIFY(f2.open(QIODevice::WriteOnly));
        f2.write(nesHeader);
        f2.close();

        // Non-ROM file should be ignored
        QFile txt(dir + "/readme.txt");
        QVERIFY(txt.open(QIODevice::WriteOnly));
        txt.write("this is not a rom");
        txt.close();
    }

private slots:

    void testScanInsertsFiles()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/lib_svc.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        int inserted = svc.scan(tmp.path(), &db);
        QVERIFY2(inserted >= 2,
                 qPrintable(QString("Expected ≥2 inserted, got %1").arg(inserted)));

        auto files = db.getAllFiles();
        QVERIFY(files.size() >= 2);
    }

    void testScanProgressCallback()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/lib_svc_progress.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        int progressCalls = 0;
        LibraryService svc;
        svc.scan(tmp.path(), &db,
                 [&](int, int, const QString &) { ++progressCalls; });
        QVERIFY2(progressCalls > 0, "Progress callback was never called");
    }

    void testGetStats()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/lib_svc_stats.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        svc.scan(tmp.path(), &db);

        QVariantMap stats = svc.getStats(&db);
        QVERIFY(stats.contains("totalFiles"));
        QVERIFY(stats.value("totalFiles").toInt() >= 2);
    }

    void testGetSystems()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/lib_svc_sys.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        svc.scan(tmp.path(), &db);

        QVariantList systems = svc.getSystems(&db);
        QVERIFY2(!systems.isEmpty(), "Expected at least one detected system");
    }

    void testScanEmptyDir()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // No files created — directory is empty

        QString dbPath = tmp.path() + "/lib_svc_empty.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        int inserted = svc.scan(tmp.path(), &db);
        QCOMPARE(inserted, 0);
    }

    void testGetAllExtensions()
    {
        LibraryService svc;
        QStringList exts = svc.getAllExtensions();
        QVERIFY2(!exts.isEmpty(), "Scanner should recognize at least some extensions");
        // Spot-check some well-known extensions
        bool hasNes = false;
        for (const auto &e : exts) {
            if (e.contains("nes", Qt::CaseInsensitive)) hasNes = true;
        }
        QVERIFY2(hasNes, ".nes should be a recognized extension");
    }

    void testRemoveLibrary()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/lib_svc_rm.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        int inserted = svc.scan(tmp.path(), &db);
        QVERIFY(inserted >= 2);

        // Get library ID (scan creates one automatically)
        auto files = db.getAllFiles();
        QVERIFY(!files.isEmpty());
        int libId = files.first().libraryId;
        QVERIFY(libId > 0);

        // Remove library
        QVERIFY(svc.removeLibrary(&db, libId));

        // Files should be gone
        auto remaining = db.getAllFiles();
        // After removal, files for that library are deleted
        int remainingForLib = 0;
        for (const auto &f : remaining)
            if (f.libraryId == libId) remainingForLib++;
        QCOMPARE(remainingForLib, 0);
    }
};

QTEST_MAIN(TestLibraryService)
#include "test_library_service.moc"
