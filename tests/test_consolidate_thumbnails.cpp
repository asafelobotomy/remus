#include <QtTest/QtTest>
#include "thumbnail_url_helper.h"
#include "../src/cli/compendium_artwork_transcode.h"
#include "../src/cli/compendium_consolidate_thumbnails.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

using namespace Remus::Metadata;

class ConsolidateThumbnailsTest : public QObject {
    Q_OBJECT

private slots:
    void testLibretroFolderMapping() {
        QCOMPARE(
            ThumbnailUrlHelper::libretroFolderForAssetType(QStringLiteral("box")), QStringLiteral("Named_Boxarts"));
        QCOMPARE(ThumbnailUrlHelper::libretroFolderForAssetType(QStringLiteral("logo")), QStringLiteral("Named_Logos"));
    }

    void testResolveStoragePath() {
        const QString repo = QStringLiteral("/tmp/remus-repo");
        QCOMPARE(
            ThumbnailUrlHelper::resolveStoragePath(repo, QStringLiteral("data/remus-thumbnails/blobs/ab/cd/ef.webp")),
            QDir(repo).filePath(QStringLiteral("data/remus-thumbnails/blobs/ab/cd/ef.webp")));
    }

    void testRepoRootFromCompendiumDb() {
        const QString root = ThumbnailUrlHelper::repoRootFromCompendiumDb(
            QStringLiteral("/tmp/remus-repo/data/compendium/remus_compendium.db"));
        QCOMPARE(root, QStringLiteral("/tmp/remus-repo"));
    }

    void testLookupGameAssetPath() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString dbPath = tempDir.filePath(QStringLiteral("test.db"));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("asset_test"));
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());

            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE game_assets ("
                                          "game_id TEXT, asset_type TEXT, storage_path TEXT, "
                                          "PRIMARY KEY (game_id, asset_type))")));
            q.prepare(QStringLiteral("INSERT INTO game_assets VALUES (?, ?, ?)"));
            q.addBindValue(QStringLiteral("g1"));
            q.addBindValue(QStringLiteral("box"));
            q.addBindValue(QStringLiteral("data/remus-thumbnails/blobs/aa/bb/cc.webp"));
            QVERIFY(q.exec());

            const QString path
                = ThumbnailUrlHelper::lookupGameAssetPath(db, QStringLiteral("g1"), QStringLiteral("box"));
            QCOMPARE(path, QStringLiteral("data/remus-thumbnails/blobs/aa/bb/cc.webp"));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("asset_test"));
    }
    void testStoredAssetNeedsUpgrade() {
        QVERIFY(!CompendiumArtworkTranscode::storedAssetNeedsUpgrade(
            QStringLiteral("image/webp"), QStringLiteral("image/webp")));
        QVERIFY(CompendiumArtworkTranscode::storedAssetNeedsUpgrade(
            QStringLiteral("image/png"), QStringLiteral("image/webp")));
        QVERIFY(!CompendiumArtworkTranscode::storedAssetNeedsUpgrade(
            QStringLiteral("image/png"), QStringLiteral("image/png")));
    }

    void testCorruptAcquisitionPngIsMissNotFatal() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString repoRoot = tempDir.path();
        const QString acquisitionDir = QDir(repoRoot).filePath(QStringLiteral("data/acquisition/libretro-thumbnails"));
        const QString outputDir = QDir(repoRoot).filePath(QStringLiteral("data/remus-thumbnails"));
        const QString boxDir = QDir(acquisitionDir).filePath(QStringLiteral("Nintendo - GameCube/Named_Boxarts"));
        QVERIFY(QDir().mkpath(boxDir));

        const QString corruptPath = QDir(boxDir).filePath(QStringLiteral("Bad Game.png"));
        {
            QFile corrupt(corruptPath);
            QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
            corrupt.write("<html><body>upstream error</body></html>");
        }

        const QString dbPath = tempDir.filePath(QStringLiteral("test.db"));
        const QString conn = QStringLiteral("consolidate_corrupt_test");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());

            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE systems ("
                                          "system_id INTEGER PRIMARY KEY, libretro_name TEXT)")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE games ("
                                          "game_id TEXT PRIMARY KEY, system_id INTEGER, canonical_title TEXT, "
                                          "cover_url TEXT)")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE sources (source_id TEXT PRIMARY KEY)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO sources VALUES ('remus-thumbnails')")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE game_assets ("
                                          "game_id TEXT, asset_type TEXT, storage_path TEXT, content_sha256 TEXT, "
                                          "byte_size INTEGER, width INTEGER, height INTEGER, mime_type TEXT, "
                                          "source_id TEXT, source_path TEXT, "
                                          "PRIMARY KEY (game_id, asset_type))")));
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE blob_inventory ("
                                          "content_sha256 TEXT PRIMARY KEY, storage_path TEXT UNIQUE, mime_type TEXT, "
                                          "byte_size INTEGER, ref_count INTEGER DEFAULT 0)")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO systems VALUES (1, 'Nintendo - GameCube')")));
            QVERIFY(q.exec(QStringLiteral("INSERT INTO games VALUES ('g1', 1, 'Bad Game', NULL)")));

            ConsolidateThumbnailsOptions opts;
            opts.acquisitionDir = acquisitionDir;
            opts.outputDir = outputDir;
            opts.format = QStringLiteral("png");

            ConsolidateThumbnailsStats stats;
            QString error;
            QVERIFY2(CompendiumThumbnails::consolidateThumbnails(db, opts, stats, error),
                qPrintable(error));
            QVERIFY(stats.misses >= 1);
            QCOMPARE(stats.assetsWritten, 0);

            db.close();
        }
        QSqlDatabase::removeDatabase(conn);
    }
};

QTEST_MAIN(ConsolidateThumbnailsTest)
#include "test_consolidate_thumbnails.moc"
