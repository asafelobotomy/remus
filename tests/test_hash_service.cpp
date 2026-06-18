/**
 * @file test_hash_service.cpp
 * @brief Unit tests for HashService — single-file hashing with DB persistence.
 *
 * Creates known-content files in a temporary directory, inserts them into
 * a test database, then verifies that HashService correctly computes
 * CRC32/MD5/SHA1 and persists them via the database.
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSqlQuery>
#include <QSqlError>

#include "../src/services/hash_service.h"
#include "../src/core/archive_creator.h"
#include "../src/core/archive_extractor.h"
#include "../src/core/database.h"
#include "../src/core/hasher.h"

using namespace Remus;

class TestHashService : public QObject {
    Q_OBJECT

private:
    /// Write known bytes to a file and return the path.
    QString writeTestFile(const QString &dir, const QString &name, const QByteArray &data) {
        if (!QDir().mkpath(dir)) {
            return { };
        }
        QString path = dir + "/" + name;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return { };
        if (f.write(data) != data.size())
            return { };
        f.close();
        return path;
    }

    /// Insert a FileRecord into the DB for the given file.
    int insertTestFile(
        Database &db, int libId, const QString &path, const QString &filename, const QString &ext, int sysId) {
        FileRecord fr;
        fr.libraryId = libId;
        fr.filename = filename;
        fr.originalPath = path;
        fr.currentPath = path;
        fr.extension = ext;
        fr.systemId = sysId;
        int id = db.insertFile(fr);
        if (id <= 0)
            qFatal("insertTestFile: insertFile returned %d", id);
        return id;
    }

private slots:

    // ── hashRecord (no DB) ────────────────────────────────

    void testHashRecordKnownContent() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Write 4 zero bytes: CRC32 = 2144DF1C (for "\0\0\0\0")
        QByteArray data(4, '\0');
        QString path = writeTestFile(tmp.path(), "zeros.bin", data);

        FileRecord fr;
        fr.currentPath = path;
        fr.extension = ".bin";

        HashService svc;
        HashResult res = svc.hashRecord(fr);

        QVERIFY(res.success);
        QVERIFY(!res.crc32.isEmpty());
        QVERIFY(!res.md5.isEmpty());
        QVERIFY(!res.sha1.isEmpty());
    }

    void testHashRecordNonexistentFile() {
        FileRecord fr;
        fr.currentPath = "/tmp/nonexistent_file_remus_test_12345.bin";
        fr.extension = ".bin";

        HashService svc;
        HashResult res = svc.hashRecord(fr);

        QVERIFY(!res.success);
    }

    // ── hashFile (with DB) ────────────────────────────────

    void testHashRecordCompressedArchiveUsesRequestedMember() {
        ArchiveCreator creator;
        ArchiveExtractor extractor;
        if (!creator.canCompress(ArchiveFormat::ZIP) || !extractor.canExtract(ArchiveFormat::ZIP)) {
            QSKIP("zip/unzip tools not available");
        }

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString sourceDir = tmp.path() + "/source";
        QVERIFY(QDir().mkpath(sourceDir + "/nested"));

        const QString targetPath = sourceDir + "/nested/target.nes";
        const QString otherPath = sourceDir + "/other.nes";
        QVERIFY(!writeTestFile(sourceDir + "/nested", "target.nes", QByteArrayLiteral("TARGET_DATA")).isEmpty());
        QVERIFY(!writeTestFile(sourceDir, "other.nes", QByteArrayLiteral("OTHER_DATA")).isEmpty());

        const QString archivePath = tmp.path() + "/games.zip";
        const CompressionResult compressed
            = creator.compressDirectoryContents(sourceDir, archivePath, ArchiveFormat::ZIP);
        QVERIFY2(compressed.success, qPrintable(compressed.error));

        Hasher hasher;
        const HashResult expected = hasher.calculateHashes(targetPath);
        QVERIFY(expected.success);

        FileRecord fr;
        fr.currentPath = archivePath;
        fr.archivePath = archivePath;
        fr.archiveInternalPath = "nested/target.nes";
        fr.filename = "target.nes";
        fr.extension = ".nes";
        fr.isCompressed = true;

        HashService svc;
        HashResult res = svc.hashRecord(fr);

        QVERIFY(res.success);
        QCOMPARE(res.crc32, expected.crc32);
        QCOMPARE(res.md5, expected.md5);
        QCOMPARE(res.sha1, expected.sha1);
        QVERIFY(res.md5 != hasher.calculateHashes(otherPath).md5);
    }

    void testHashFilePersistsToDatabase() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/hash_svc.db"));

        int libId = db.insertLibrary(tmp.path(), "Hash Test");
        QVERIFY(libId > 0);

        int sysId = db.getSystemId("NES");
        QVERIFY(sysId > 0);

        // Create a small test file
        QByteArray content("Hello Remus Hash");
        QString path = writeTestFile(tmp.path() + "/roms", "test.nes", content);
        int fileId = insertTestFile(db, libId, path, "test.nes", ".nes", sysId);

        // Verify no hash yet
        FileRecord before = db.getFileById(fileId);
        QVERIFY(!before.hashCalculated);

        // Run hashFile
        HashService svc;
        bool ok = svc.hashFile(&db, fileId);
        QVERIFY(ok);

        // Verify hashes persisted
        FileRecord after = db.getFileById(fileId);
        QVERIFY(after.hashCalculated);
        QVERIFY(!after.crc32.isEmpty());
        QVERIFY(!after.md5.isEmpty());
        QVERIFY(!after.sha1.isEmpty());
    }

    void testHashFileInvalidId() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/hash_invalid.db"));

        HashService svc;
        bool ok = svc.hashFile(&db, 999999);
        QVERIFY(!ok);
    }

    void testHashFileNullDb() {
        HashService svc;
        bool ok = svc.hashFile(nullptr, 1);
        QVERIFY(!ok);
    }

    // ── hashAll ───────────────────────────────────────────

    void testHashAllProcessesUnhashedFiles() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/hash_all.db"));

        int libId = db.insertLibrary(tmp.path(), "Hash All Test");
        int sysId = db.getSystemId("NES");

        // Create 3 files
        for (int i = 0; i < 3; ++i) {
            QString name = QString("rom%1.nes").arg(i);
            QByteArray data = QString("content_%1").arg(i).toUtf8();
            QString path = writeTestFile(tmp.path() + "/roms", name, data);
            insertTestFile(db, libId, path, name, ".nes", sysId);
        }

        // Hash all
        int progressCalls = 0;
        HashService svc;
        int hashed = svc.hashAll(&db, [&](int, int, const QString &) { progressCalls++; });

        QCOMPARE(hashed, 3);
        QVERIFY(progressCalls > 0); // at least some progress callbacks

        // Verify all have hashes
        auto files = db.getAllFiles();
        for (const auto &f : files) {
            QVERIFY2(f.hashCalculated, qPrintable("File " + f.filename + " not hashed"));
        }
    }

    void testHashAllWithCancellation() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/hash_cancel.db"));

        int libId = db.insertLibrary(tmp.path(), "Cancel Test");
        int sysId = db.getSystemId("NES");

        // Create 5 files
        for (int i = 0; i < 5; ++i) {
            QString name = QString("rom%1.nes").arg(i);
            QByteArray data = QString("data_%1").arg(i).toUtf8();
            QString path = writeTestFile(tmp.path() + "/roms", name, data);
            insertTestFile(db, libId, path, name, ".nes", sysId);
        }

        // Cancel immediately
        std::atomic<bool> cancelled { true };
        HashService svc;
        int hashed = svc.hashAll(&db, nullptr, nullptr, &cancelled);

        QCOMPARE(hashed, 0);
    }

    void testHashAllEmptyDatabase() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/hash_empty.db"));

        HashService svc;
        int hashed = svc.hashAll(&db);
        QCOMPARE(hashed, 0);
    }

    void testHashAllNullDb() {
        HashService svc;
        int hashed = svc.hashAll(nullptr);
        QCOMPARE(hashed, 0);
    }

    // H1: failures must be counted as skipped and surfaced in the log
    void testHashAllReportsSkippedFilesWithReason() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/skip_test.db"));

        int libId = db.insertLibrary(tmp.path(), "Skip Test");
        int sysId = db.getSystemId("NES");

        // Insert a record whose currentPath does not exist — hash will fail.
        const QString missingPath = tmp.path() + "/missing.nes";
        insertTestFile(db, libId, missingPath, "missing.nes", ".nes", sysId);

        QStringList logLines;
        HashService svc;
        int hashed = svc.hashAll(&db, nullptr, [&](const QString &msg) { logLines << msg; });

        QCOMPARE(hashed, 0);

        bool hasSkipLine = false;
        for (const QString &line : logLines) {
            if (line.contains("missing.nes") || line.contains("skipped", Qt::CaseInsensitive)) {
                hasSkipLine = true;
                break;
            }
        }
        QVERIFY2(hasSkipLine, qPrintable("Expected a skip message in log but got: " + logLines.join("; ")));
    }

    // P4/Finding #3 — Progress must be fired once per file, during hashing (not in a post-hoc burst)
    void testComputeHashesProgressFiredDuringWork() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        constexpr int FILE_COUNT = 4;
        QList<FileRecord> records;
        for (int i = 0; i < FILE_COUNT; ++i) {
            QByteArray data = QString("progress_test_%1").arg(i).toUtf8();
            const QString name = QString("prog%1.nes").arg(i);
            QString path = writeTestFile(tmp.path(), name, data);
            QVERIFY(!path.isEmpty());
            FileRecord fr;
            fr.id = i + 1;
            fr.currentPath = path;
            fr.filename = name;
            fr.extension = QStringLiteral(".nes");
            records.append(fr);
        }

        QMutex mu;
        QList<int> doneValues;

        HashService svc;
        svc.computeHashes(records, [&mu, &doneValues](int done, int /*total*/, const QString &) {
            QMutexLocker lock(&mu);
            doneValues.append(done);
        });

        // Exactly one callback per file, each with a unique done value 1..N
        QCOMPARE(doneValues.size(), FILE_COUNT);
        std::sort(doneValues.begin(), doneValues.end());
        for (int i = 0; i < FILE_COUNT; ++i)
            QCOMPARE(doneValues[i], i + 1);
    }

    // P4 — Batch API must produce the same hashes as single-file hashRecord()
    void testComputeHashesMatchesSingleFileResults() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        constexpr int FILE_COUNT = 4;
        QList<FileRecord> records;
        QList<QString> expectedCrc32s;

        HashService refSvc;
        for (int i = 0; i < FILE_COUNT; ++i) {
            QByteArray data = QString("batch_hash_test_data_%1").arg(i).toUtf8();
            const QString name = QString("rom%1.nes").arg(i);
            QString path = writeTestFile(tmp.path(), name, data);
            QVERIFY(!path.isEmpty());

            FileRecord fr;
            fr.id = i + 1; // synthetic id for comparison
            fr.currentPath = path;
            fr.filename = name;
            fr.extension = QStringLiteral(".nes");
            records.append(fr);

            // Reference: single-file result
            HashResult ref = refSvc.hashRecord(fr);
            QVERIFY(ref.success);
            expectedCrc32s.append(ref.crc32);
        }

        // Batch via computeHashes()
        HashService batchSvc;
        const QList<HashService::HashBatchResult> results = batchSvc.computeHashes(records);

        QCOMPARE(results.size(), FILE_COUNT);
        for (int i = 0; i < FILE_COUNT; ++i) {
            const HashService::HashBatchResult &task = results[i];
            QVERIFY2(
                !task.skipped, qPrintable(QString("File %1 unexpectedly skipped: %2").arg(i).arg(task.skipReason)));
            QVERIFY(task.result.success);
            QCOMPARE(task.result.crc32, expectedCrc32s[i]);
        }
    }

    // C2 — regression: when updateFileHashes returns false (DB write rejected),
    // the file must NOT be counted as hashed and the method returns 0.
    void testHashAllDbWriteFailure_returnsZeroHashed() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/hash_fail.db"));

        int libId = db.insertLibrary(tmp.path(), "DB Fail Test");
        QVERIFY(libId > 0);
        int sysId = db.getSystemId("NES");
        QVERIFY(sysId > 0);

        QByteArray content("Hello Remus Fault Injection");
        QString path = writeTestFile(tmp.path() + "/roms", "test.nes", content);
        QVERIFY(!path.isEmpty());
        insertTestFile(db, libId, path, "test.nes", ".nes", sysId);

        // Switch to read-only mode so all DB writes (updateFileHashes) fail.
        QSqlQuery pragma(db.database());
        QVERIFY2(pragma.exec("PRAGMA query_only = 1"), qPrintable(pragma.lastError().text()));

        HashService svc;
        int hashed = svc.hashAll(&db);

        // Nothing should be counted as hashed when DB writes are rejected.
        QCOMPARE(hashed, 0);
    }

    void testBackfillChdSha1ReturnsZeroWhenNothingPending() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/backfill_empty.db"));

        HashService svc;
        QCOMPARE(svc.backfillChdSha1(&db), 0);
    }

    void testBackfillRvzSha1ReturnsZeroWhenNothingPending() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/backfill_rvz_empty.db"));

        HashService svc;
        QCOMPARE(svc.backfillRvzSha1(&db), 0);
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication coreApp(argc, argv);
    TestHashService t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_hash_service.moc"
