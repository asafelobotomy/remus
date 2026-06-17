/**
 * @file test_library_service.cpp
 * @brief Unit tests for LibraryService (scanFilesystem + persistScanResults)
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSet>

#include "../src/services/library_service.h"
#include "../src/core/database.h"

using namespace Remus;

namespace {
int scanAndPersist(LibraryService &svc, const QString &path, Database *db,
    LibraryService::ProgressCallback progressCb = nullptr, int existingLibraryId = 0) {
    const auto results = svc.scanFilesystem(path, progressCb);
    if (svc.wasCancelled())
        return 0;
    int libraryId = existingLibraryId > 0 ? existingLibraryId : db->insertLibrary(path);
    if (libraryId == 0)
        return 0;
    return svc.persistScanResults(results, libraryId, db);
}
} // namespace

class TestLibraryService : public QObject {
    Q_OBJECT

private:
    void createStubRoms(const QString &dir) {
        QByteArray nesHeader("NES\x1A");
        nesHeader.append(QByteArray(12, '\x00'));
        nesHeader.append(QByteArray(32, '\xBB'));

        QFile f1(dir + "/TestRom.nes");
        QVERIFY(f1.open(QIODevice::WriteOnly));
        QVERIFY(f1.write(nesHeader) == nesHeader.size());
        f1.close();

        QFile f2(dir + "/Another.nes");
        QVERIFY(f2.open(QIODevice::WriteOnly));
        QVERIFY(f2.write(nesHeader) == nesHeader.size());
        f2.close();

        QFile txt(dir + "/readme.txt");
        QVERIFY(txt.open(QIODevice::WriteOnly));
        QVERIFY(txt.write("this is not a rom") == 17);
        txt.close();
    }

private slots:

    void testScanInsertsFiles() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/lib_svc.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        int inserted = scanAndPersist(svc, tmp.path(), &db);
        QVERIFY2(inserted >= 2, qPrintable(QString("Expected ≥2 inserted, got %1").arg(inserted)));

        auto files = db.getAllFiles();
        QVERIFY(files.size() >= 2);
    }

    void testScanProgressCallback() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/lib_svc_progress.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        int progressCalls = 0;
        LibraryService svc;
        scanAndPersist(svc, tmp.path(), &db, [&](int, int, const QString &) { ++progressCalls; });
        QVERIFY2(progressCalls > 0, "Progress callback was never called");
    }

    void testGetStats() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/lib_svc_stats.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        scanAndPersist(svc, tmp.path(), &db);

        auto files = db.getAllFiles();
        QVERIFY(files.size() >= 2);
    }

    void testGetSystems() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/lib_svc_sys.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        scanAndPersist(svc, tmp.path(), &db);

        auto files = db.getAllFiles();
        QSet<int> systemIds;
        for (const auto &f : files)
            if (f.systemId > 0)
                systemIds.insert(f.systemId);
        QVERIFY2(!systemIds.isEmpty(), "Expected at least one detected system");
    }

    void testScanPreservesCueBinParentLinkage() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString cuePath = tmp.path() + "/disc.cue";
        const QString binPath = tmp.path() + "/disc.bin";

        QFile cue(cuePath);
        QVERIFY(cue.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(cue.write("FILE \"disc.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n") > 0);
        cue.close();

        QFile bin(binPath);
        QVERIFY(bin.open(QIODevice::WriteOnly));
        QVERIFY(bin.write(QByteArray(2352, '\0')) == 2352);
        bin.close();

        QString dbPath = tmp.path() + "/lib_svc_lineage.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        const int inserted = scanAndPersist(svc, tmp.path(), &db);
        QVERIFY(inserted >= 2);

        FileRecord cueRecord;
        FileRecord binRecord;
        for (const FileRecord &file : db.getAllFiles()) {
            if (file.filename == "disc.cue") {
                cueRecord = file;
            } else if (file.filename == "disc.bin") {
                binRecord = file;
            }
        }

        QVERIFY(cueRecord.id > 0);
        QVERIFY(binRecord.id > 0);
        QCOMPARE(cueRecord.parentFileId, binRecord.id);
        QVERIFY(binRecord.isPrimary);
        QVERIFY(!cueRecord.isPrimary);
    }

    void testRescanLinksNewCueToExistingBinParent() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString binPath = tmp.path() + "/disc.bin";
        QFile bin(binPath);
        QVERIFY(bin.open(QIODevice::WriteOnly));
        QVERIFY(bin.write(QByteArray(2352, '\0')) == 2352);
        bin.close();

        QString dbPath = tmp.path() + "/lib_svc_rescan.db";
        Database db;
        QVERIFY(db.initialize(dbPath));
        const int libraryId = db.insertLibrary(tmp.path(), "Test");
        QVERIFY(libraryId > 0);

        LibraryService svc;
        QCOMPARE(scanAndPersist(svc, tmp.path(), &db, nullptr, libraryId), 1);

        const QString cuePath = tmp.path() + "/disc.cue";
        QFile cue(cuePath);
        QVERIFY(cue.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(cue.write("FILE \"disc.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n") > 0);
        cue.close();

        QVERIFY(scanAndPersist(svc, tmp.path(), &db, nullptr, libraryId) >= 1);

        FileRecord cueRecord;
        FileRecord binRecord;
        int cueCount = 0;
        int binCount = 0;
        for (const FileRecord &file : db.getAllFiles()) {
            if (file.filename == "disc.cue") {
                cueRecord = file;
                cueCount++;
            } else if (file.filename == "disc.bin") {
                binRecord = file;
                binCount++;
            }
        }

        QVERIFY(cueRecord.id > 0);
        QVERIFY(binRecord.id > 0);
        QCOMPARE(cueCount, 1);
        QCOMPARE(binCount, 1);
        QCOMPARE(cueRecord.parentFileId, binRecord.id);
    }

    void testScanEmptyDir() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QString dbPath = tmp.path() + "/lib_svc_empty.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        int inserted = scanAndPersist(svc, tmp.path(), &db);
        QCOMPARE(inserted, 0);
    }

    void testGetAllExtensions() {
        LibraryService svc;
        QStringList exts = svc.getAllExtensions();
        QVERIFY2(!exts.isEmpty(), "Scanner should recognize at least some extensions");
        bool hasNes = false;
        for (const auto &e : exts) {
            if (e.contains("nes", Qt::CaseInsensitive))
                hasNes = true;
        }
        QVERIFY2(hasNes, ".nes should be a recognized extension");
    }

    void testRemoveLibrary() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/lib_svc_rm.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        LibraryService svc;
        int inserted = scanAndPersist(svc, tmp.path(), &db);
        QVERIFY(inserted >= 2);

        auto files = db.getAllFiles();
        QVERIFY(!files.isEmpty());
        int libId = files.first().libraryId;
        QVERIFY(libId > 0);

        QVERIFY(svc.removeLibrary(&db, libId));

        auto remaining = db.getAllFiles();
        int remainingForLib = 0;
        for (const auto &f : remaining)
            if (f.libraryId == libId)
                remainingForLib++;
        QCOMPARE(remainingForLib, 0);
    }
};

QTEST_MAIN(TestLibraryService)
#include "test_library_service.moc"
