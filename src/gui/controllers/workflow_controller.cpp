#include "workflow_controller.h"

#include "app_controller.h"
#include "hash_controller.h"
#include "match_controller.h"
#include "artwork_controller.h"
#include "organize_controller.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>

namespace Remus {

WorkflowController::WorkflowController(AppController   *app,
                                       HashController  *hash,
                                       MatchController *match,
                                       ArtworkController   *artwork,
                                       OrganizeController  *organize,
                                       QObject *parent)
    : QObject(parent)
    , m_appController(app)
    , m_hashController(hash)
    , m_matchController(match)
    , m_artworkController(artwork)
    , m_organizeController(organize)
{
    connect(app, &AppController::libraryOpened,
            this, &WorkflowController::refresh);
    connect(app, &AppController::libraryClosed,
            this, &WorkflowController::refresh);
    connect(app, &AppController::selectedFileChanged,
            this, &WorkflowController::onSelectedFileChanged);
}

// ── Public invokables ─────────────────────────────────────────────────────────

void WorkflowController::refresh()
{
    refreshCounts();
    refreshQueueFiles();
    refreshHint();
}

void WorkflowController::setQueueStage(int stage)
{
    if (m_queueStage == stage) return;
    m_queueStage = stage;
    emit queueStageChanged();
    refreshQueueFiles();
}

void WorkflowController::runAll()
{
    if (!m_appController || !m_appController->isLibraryOpen() || m_running) return;
    m_running  = true;
    m_runStep  = 0;
    emit runningChanged();
    advanceRunAll();
}

void WorkflowController::cancel()
{
    if (m_running) cancelRunAll();
}

bool WorkflowController::artworkExistsForFile(int fileId) const
{
    if (fileId <= 0) return false;
    const QString path =
        QDir(artworkDirPath()).filePath(QStringLiteral("artwork_%1.png").arg(fileId));
    return QFileInfo::exists(path);
}

// ── Private helpers ───────────────────────────────────────────────────────────

QString WorkflowController::artworkDirPath() const
{
    QSettings s;
    const QString cfg = s.value(QStringLiteral("gui/artwork_cache_dir")).toString().trimmed();
    if (!cfg.isEmpty()) return cfg;
    return QDir(QStandardPaths::writableLocation(
                    QStandardPaths::AppDataLocation)).filePath(QStringLiteral("artwork"));
}

void WorkflowController::refreshCounts()
{
    if (!m_appController || !m_appController->isLibraryOpen()) {
        m_identityCount = m_enrichCount = m_doneCount = 0;
        emit stageCountsChanged();
        return;
    }

    QSqlDatabase db = m_appController->database()->database();

    // Identity: no hash OR no confirmed (non-rejected) match
    {
        QSqlQuery q(db);
        const bool ok = q.exec(QStringLiteral(
            "SELECT COUNT(DISTINCT f.id) FROM files f "
            "WHERE (f.md5 IS NULL OR f.md5 = '') "
            "   OR NOT EXISTS ("
            "     SELECT 1 FROM matches m "
            "     WHERE m.file_id = f.id "
            "       AND m.is_confirmed = 1 AND m.is_rejected = 0)"));
        m_identityCount = (ok && q.next()) ? q.value(0).toInt() : 0;
    }

    // Enrich / Done: confirmed files split by artwork presence
    int enriched = 0;
    int done     = 0;
    {
        QSqlQuery q(db);
        const bool ok = q.exec(QStringLiteral(
            "SELECT DISTINCT f.id FROM files f "
            "WHERE EXISTS ("
            "  SELECT 1 FROM matches m "
            "  WHERE m.file_id = f.id "
            "    AND m.is_confirmed = 1 AND m.is_rejected = 0)"));
        if (ok) {
            while (q.next()) {
                if (artworkExistsForFile(q.value(0).toInt())) ++done;
                else ++enriched;
            }
        }
    }
    m_enrichCount = enriched;
    m_doneCount   = done;

    emit stageCountsChanged();
}

void WorkflowController::refreshQueueFiles()
{
    m_queueFiles.clear();

    if (!m_appController || !m_appController->isLibraryOpen()) {
        emit queueFilesChanged();
        return;
    }

    QSqlDatabase db = m_appController->database()->database();
    QSqlQuery q(db);

    // Build SQL depending on filter
    QString sql;
    switch (m_queueStage) {
    case Identity:
        sql = QStringLiteral(
            "SELECT DISTINCT f.id, f.filename, f.current_path, f.md5 FROM files f "
            "WHERE (f.md5 IS NULL OR f.md5 = '') "
            "   OR NOT EXISTS ("
            "     SELECT 1 FROM matches m "
            "     WHERE m.file_id = f.id "
            "       AND m.is_confirmed = 1 AND m.is_rejected = 0) "
            "ORDER BY f.filename LIMIT 500");
        break;
    case Enrich:
    case Done:
        sql = QStringLiteral(
            "SELECT DISTINCT f.id, f.filename, f.current_path, f.md5 FROM files f "
            "WHERE EXISTS ("
            "  SELECT 1 FROM matches m "
            "  WHERE m.file_id = f.id "
            "    AND m.is_confirmed = 1 AND m.is_rejected = 0) "
            "ORDER BY f.filename LIMIT 500");
        break;
    default: // AllFiles
        sql = QStringLiteral(
            "SELECT f.id, f.filename, f.current_path, f.md5 FROM files f "
            "ORDER BY f.filename LIMIT 500");
        break;
    }

    if (!q.exec(sql)) {
        emit queueFilesChanged();
        return;
    }

    while (q.next()) {
        const int    id         = q.value(0).toInt();
        const bool   hasArtwork = artworkExistsForFile(id);

        if (m_queueStage == Enrich && hasArtwork)  continue;
        if (m_queueStage == Done   && !hasArtwork) continue;

        QVariantMap item;
        item[QStringLiteral("fileId")]     = id;
        item[QStringLiteral("filename")]   = q.value(1).toString();
        item[QStringLiteral("path")]       = q.value(2).toString();
        item[QStringLiteral("hasHash")]    = !q.value(3).toString().isEmpty();
        item[QStringLiteral("hasArtwork")] = hasArtwork;
        m_queueFiles.append(item);
    }

    emit queueFilesChanged();
}

void WorkflowController::refreshHint()
{
    if (!m_appController || !m_appController->isLibraryOpen()) {
        if (!m_hint.isEmpty()) { m_hint.clear(); emit hintChanged(); }
        return;
    }

    const int fid = m_appController->selectedFileId();
    QString   hint;

    if (fid <= 0) {
        hint = QStringLiteral("Select a file from the queue to continue.");
    } else {
        const QVariantMap file  = m_appController->selectedFile();
        const QVariantMap match = m_appController->selectedMatch();

        if (file.value(QStringLiteral("md5")).toString().isEmpty()) {
            hint = QStringLiteral("Hash this file to identify it.");
        } else if (match.isEmpty() || !match.value(QStringLiteral("confirmed")).toBool()) {
            hint = QStringLiteral("Run matching to identify this file.");
        } else if (!artworkExistsForFile(fid)) {
            hint = QStringLiteral("Download artwork for this file.");
        } else {
            hint = QStringLiteral("Ready to package and organize.");
        }
    }

    if (m_hint != hint) {
        m_hint = hint;
        emit hintChanged();
    }
}

void WorkflowController::onSelectedFileChanged()
{
    refreshHint();
}

void WorkflowController::advanceRunAll()
{
    if (!m_running) return;

    switch (m_runStep++) {

    case 0: {
        // Hash all files
        connect(m_hashController, &HashController::hashCompleted,
                this, [this](int) { advanceRunAll(); }, Qt::SingleShotConnection);
        connect(m_hashController, &HashController::hashError,
                this, [this](const QString &) { cancelRunAll(); }, Qt::SingleShotConnection);
        m_hashController->startHashAll();
        break;
    }

    case 1: {
        // Match all files — guard QObject auto-disconnects on delete
        auto *guard = new QObject(this);
        connect(m_matchController, &MatchController::libraryChanged, guard, [this, guard]() {
            delete guard;
            advanceRunAll();
        });
        m_matchController->matchAll();
        break;
    }

    case 2:
        // Artwork (synchronous download loop)
        m_artworkController->downloadAllMatched();
        advanceRunAll();
        break;

    default:
        cancelRunAll();
        break;
    }
}

void WorkflowController::cancelRunAll()
{
    m_running = false;
    m_runStep = 0;
    emit runningChanged();
    refresh();
}

} // namespace Remus
