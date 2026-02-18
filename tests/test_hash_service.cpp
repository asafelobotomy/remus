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

#include "../src/services/hash_service.h"
#include "../src/core/database.h"
#include "../src/core/hasher.h"

using namespace Remus;

class TestHashService : public QObject
{
    Q_OBJECT

private:
    /// Write known bytes to a file and return the path.
    QString writeTestFile(const QString &dir, const QString &name, const QByteArray &data)
    {
        QDir().mkpath(dir);
        QString path = dir + "/" + name;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return {};
        f.write(data);
        f.close();
        return path;
    }

    /// Insert a FileRecord into the DB for the given file.
    int insertTestFile(Database &db, int libId, const QString &path,
                       const QString &filename, const QString &ext, int sysId)
    {
        FileRecord fr;
        fr.libraryId = libId;
        fr.filename = filename;
        fr.originalPath = path;
        fr.currentPath = path;
        fr.extension = ext;
        fr.systemId = sysId;
        int id = db.insertFile(fr);
        Q_ASSERT(id > 0);
        return id;
    }

private slots:

    // ── hashRecord (no DB) ────────────────────────────────

    void testHashRecordKnownContent()
    {
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

    void testHashRecordNonexistentFile()
    {
        FileRecord fr;
        fr.currentPath = "/tmp/nonexistent_file_remus_test_12345.bin";
        fr.extension = ".bin";

        HashService svc;
        HashResult res = svc.hashRecord(fr);

        QVERIFY(!res.success);
    }

    // ── hashFile (with DB) ────────────────────────────────

    void testHashFilePersistsToDatabase()
    {
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

    void testHashFileInvalidId()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/hash_invalid.db"));

        HashService svc;
        bool ok = svc.hashFile(&db, 999999);
        QVERIFY(!ok);
    }

    void testHashFileNullDb()
    {
        HashService svc;
        bool ok = svc.hashFile(nullptr, 1);
        QVERIFY(!ok);
    }

    // ── hashAll ───────────────────────────────────────────

    void testHashAllProcessesUnhashedFiles()
    {
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
        int hashed = svc.hashAll(&db, [&](int, int, const QString &) {
            progressCalls++;
        });

        QCOMPARE(hashed, 3);
        QVERIFY(progressCalls > 0); // at least some progress callbacks

        // Verify all have hashes
        auto files = db.getAllFiles();
        for (const auto &f : files) {
            QVERIFY2(f.hashCalculated,
                     qPrintable("File " + f.filename + " not hashed"));
        }
    }

    void testHashAllWithCancellation()
    {
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
        std::atomic<bool> cancelled{true};
        HashService svc;
        int hashed = svc.hashAll(&db, nullptr, nullptr, &cancelled);

        QCOMPARE(hashed, 0);
    }

    void testHashAllEmptyDatabase()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Database db;
        QVERIFY(db.initialize(tmp.path() + "/hash_empty.db"));

        HashService svc;
        int hashed = svc.hashAll(&db);
        QCOMPARE(hashed, 0);
    }

    void testHashAllNullDb()
    {
        HashService svc;
        int hashed = svc.hashAll(nullptr);
        QCOMPARE(hashed, 0);
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication coreApp(argc, argv);
    TestHashService t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_hash_service.moc"
