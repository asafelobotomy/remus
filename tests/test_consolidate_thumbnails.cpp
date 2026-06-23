#include <QtTest/QtTest>
#include "thumbnail_url_helper.h"
#include "../src/cli/compendium_artwork_transcode.h"

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
};

QTEST_MAIN(ConsolidateThumbnailsTest)
#include "test_consolidate_thumbnails.moc"
