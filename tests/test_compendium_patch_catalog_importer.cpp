#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "../src/metadata/compendium_patch_catalog_importer.h"
#include "../src/metadata/clrmamepro_parser.h"

using namespace Remus;

class CompendiumPatchCatalogImporterTest : public QObject {
    Q_OBJECT

private:
    bool createSchema(QSqlDatabase &db) {
        QSqlQuery q(db);
        return q.exec(QStringLiteral(
                   "CREATE TABLE systems ("
                   "  system_id INTEGER PRIMARY KEY,"
                   "  internal_name TEXT NOT NULL,"
                   "  libretro_name TEXT NOT NULL"
                   ")"))
            && q.exec(QStringLiteral(
                   "INSERT INTO systems VALUES (1, 'NES', 'Nintendo - Nintendo Entertainment System')"))
            && q.exec(QStringLiteral(
                   "CREATE TABLE patch_catalog_sources ("
                   "  source_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "  system_name TEXT NOT NULL,"
                   "  catalog_name TEXT NOT NULL,"
                   "  catalog_version TEXT,"
                   "  catalog_source TEXT,"
                   "  catalog_description TEXT,"
                   "  entry_count INTEGER NOT NULL DEFAULT 0,"
                   "  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                   "  UNIQUE (system_name, catalog_name)"
                   ")"))
            && q.exec(QStringLiteral(
                   "CREATE TABLE patch_entries ("
                   "  entry_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "  source_id INTEGER NOT NULL,"
                   "  game_name TEXT NOT NULL,"
                   "  rom_name TEXT NOT NULL,"
                   "  rom_size INTEGER,"
                   "  crc32 TEXT, md5 TEXT, sha1 TEXT,"
                   "  description TEXT, status TEXT,"
                   "  base_title TEXT, patch_name TEXT, file_type TEXT,"
                   "  FOREIGN KEY (source_id) REFERENCES patch_catalog_sources(source_id) ON DELETE CASCADE"
                   ")"));
    }

private slots:
    void importMapsLibretroNameAndPatchUrl();
};

void CompendiumPatchCatalogImporterTest::importMapsLibretroNameAndPatchUrl() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dbPath = tempDir.filePath(QStringLiteral("patch.db"));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("patch_import"));
        db.setDatabaseName(dbPath);
        QVERIFY(db.open());
        QVERIFY(createSchema(db));
        db.close();
        QSqlDatabase::removeDatabase(QStringLiteral("patch_import"));
    }

    const QString datContent = QStringLiteral(
        "clrmamepro (\n"
        "    name \"NES hacks\"\n"
        "    description \"Test hacks\"\n"
        ")\n"
        "game (\n"
        "    name \"Dragon Quest III (English) [T-En by Foo]\"\n"
        "    rom ( name \"dq3en.nes\" size 262144 crc AABBCCDD )\n"
        "    patch \"http://www.romhacking.net/translations/1234/\"\n"
        ")\n");
    const QString datPath = tempDir.filePath(
        QStringLiteral("Nintendo - Nintendo Entertainment System.dat"));
    {
        QFile datFile(datPath);
        QVERIFY(datFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QVERIFY(datFile.write(datContent.toUtf8()) > 0);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("patch_import_run"));
    db.setDatabaseName(dbPath);
    QVERIFY(db.open());

    CompendiumPatchCatalog::ImportStats stats;
    QString error;
    QVERIFY2(CompendiumPatchCatalog::importDirectory(db, tempDir.path(), stats, error), qPrintable(error));
    QCOMPARE(stats.sourcesImported, 1);
    QCOMPARE(stats.entriesImported, 1);

    QSqlQuery q(db);
    QVERIFY(q.exec(QStringLiteral("SELECT system_name, catalog_name, entry_count FROM patch_catalog_sources")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("NES"));
    QCOMPARE(q.value(1).toString(), QStringLiteral("hacks"));
    QCOMPARE(q.value(2).toInt(), 1);

    QVERIFY(q.exec(QStringLiteral("SELECT patch_name, file_type FROM patch_entries")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("http://www.romhacking.net/translations/1234/"));
    QVERIFY(!q.value(1).toString().isEmpty());

    db.close();
    QSqlDatabase::removeDatabase(QStringLiteral("patch_import_run"));
}

QTEST_MAIN(CompendiumPatchCatalogImporterTest)
#include "test_compendium_patch_catalog_importer.moc"
