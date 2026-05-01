#include <QtTest/QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>

#include "controllers/workflow_controller.h"
#include "controllers/app_controller.h"
#include "controllers/hash_controller.h"
#include "controllers/match_controller.h"
#include "controllers/artwork_controller.h"
#include "controllers/organize_controller.h"
#include "../src/core/database_types.h"

namespace Remus {

class WorkflowControllerTest : public QObject {
    Q_OBJECT

private:
    // Open a fresh temp library (unique file per call) and return the db path.
    // A library row is inserted and m_libraryId is cached for insertFile().
    QString setupTempLibrary(AppController *app)
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/remus_wf_test_")
                             + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
                             + QStringLiteral(".db");
        QFile::remove(path); // ensure no stale file
        const bool ok = app->openLibrary(path);
        Q_ASSERT_X(ok, "setupTempLibrary", qPrintable(path));
        m_dbPaths.append(path);

        // Insert a library row so files can reference a valid library_id.
        m_libraryId = app->database()->insertLibrary(dir + QStringLiteral("/roms"));
        Q_ASSERT(m_libraryId > 0);

        return path;
    }

    // Insert a minimal file row using the Database API (provides all NOT NULL fields).
    int insertFile(AppController *app,
                   const QString &filename,
                   const QString &md5 = QString())
    {
        FileRecord rec;
        rec.libraryId    = m_libraryId;
        rec.filename     = filename;
        rec.originalPath = QStringLiteral("/tmp/") + filename;
        rec.currentPath  = rec.originalPath;
        rec.extension    = filename.contains('.') ? filename.mid(filename.lastIndexOf('.')) : QString();
        rec.fileSize     = 0;
        rec.md5          = md5;
        const int id = app->database()->insertFile(rec);
        Q_ASSERT_X(id > 0, "insertFile", qPrintable("Failed to insert " + filename));
        return id;
    }

    // Insert a confirmed match for a file
    void insertConfirmedMatch(AppController *app, int fileId, int gameId = 1)
    {
        QSqlQuery q(app->database()->database());
        q.prepare(QStringLiteral(
            "INSERT INTO matches (file_id, game_id, confidence, is_confirmed, is_rejected) "
            "VALUES (?, ?, ?, 1, 0)"));
        q.addBindValue(fileId);
        q.addBindValue(gameId);
        q.addBindValue(0.9);
        const bool ok = q.exec();
        Q_ASSERT_X(ok, "insertConfirmedMatch", qPrintable(q.lastError().text()));
    }

    QStringList m_dbPaths;   // for cleanup
    int         m_libraryId = 0;

private slots:

    void test_refreshCounts_emptyLibrary()
    {
        AppController    app;
        HashController   hash(&app);
        MatchController  match(&app);
        ArtworkController art(&app);
        OrganizeController org(&app);
        WorkflowController wf(&app, &hash, &match, &art, &org);

        setupTempLibrary(&app);
        wf.refresh();

        QCOMPARE(wf.identityCount(), 0);
        QCOMPARE(wf.enrichCount(),   0);
        QCOMPARE(wf.doneCount(),     0);
    }

    void test_identityCount_noHash()
    {
        AppController    app;
        HashController   hash(&app);
        MatchController  match(&app);
        ArtworkController art(&app);
        OrganizeController org(&app);
        WorkflowController wf(&app, &hash, &match, &art, &org);

        setupTempLibrary(&app);
        insertFile(&app, QStringLiteral("game1.iso")); // no hash
        insertFile(&app, QStringLiteral("game2.iso")); // no hash

        wf.refresh();
        QCOMPARE(wf.identityCount(), 2);
    }

    void test_identityCount_hashedButNoMatch()
    {
        AppController    app;
        HashController   hash(&app);
        MatchController  match(&app);
        ArtworkController art(&app);
        OrganizeController org(&app);
        WorkflowController wf(&app, &hash, &match, &art, &org);

        setupTempLibrary(&app);
        insertFile(&app, QStringLiteral("game1.iso"), QStringLiteral("abc123")); // hashed, no match

        wf.refresh();
        QCOMPARE(wf.identityCount(), 1);
    }

    void test_enrichCount_confirmedNoArtwork()
    {
        AppController    app;
        HashController   hash(&app);
        MatchController  match(&app);
        ArtworkController art(&app);
        OrganizeController org(&app);
        WorkflowController wf(&app, &hash, &match, &art, &org);

        setupTempLibrary(&app);
        const int fid = insertFile(&app, QStringLiteral("game1.iso"), QStringLiteral("abc123"));
        insertConfirmedMatch(&app, fid);

        wf.refresh();
        QCOMPARE(wf.identityCount(), 0); // matched → not identity
        QCOMPARE(wf.enrichCount(),   1); // confirmed but no artwork
    }

    void test_queueStage_all_returnsFiles()
    {
        AppController    app;
        HashController   hash(&app);
        MatchController  match(&app);
        ArtworkController art(&app);
        OrganizeController org(&app);
        WorkflowController wf(&app, &hash, &match, &art, &org);

        setupTempLibrary(&app);
        insertFile(&app, QStringLiteral("a.iso"));
        insertFile(&app, QStringLiteral("b.iso"));

        wf.setQueueStage(WorkflowController::AllFiles);
        QCOMPARE(wf.queueFiles().count(), 2);
    }

    void test_hint_noFileSelected()
    {
        AppController    app;
        HashController   hash(&app);
        MatchController  match(&app);
        ArtworkController art(&app);
        OrganizeController org(&app);
        WorkflowController wf(&app, &hash, &match, &art, &org);

        setupTempLibrary(&app);
        wf.refresh();

        QVERIFY(!wf.hint().isEmpty());
        QVERIFY(wf.hint().contains("Select", Qt::CaseInsensitive));
    }

    void test_artworkExistsForFile_noFile()
    {
        AppController    app;
        HashController   hash(&app);
        MatchController  match(&app);
        ArtworkController art(&app);
        OrganizeController org(&app);
        WorkflowController wf(&app, &hash, &match, &art, &org);

        QVERIFY(!wf.artworkExistsForFile(9999));
    }
};

} // namespace Remus

QTEST_MAIN(Remus::WorkflowControllerTest)
#include "test_workflow_controller.moc"
