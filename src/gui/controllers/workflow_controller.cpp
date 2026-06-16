#include "workflow_controller.h"

#include "app_controller.h"
#include "conversion_controller.h"
#include "hash_controller.h"
#include "match_controller.h"
#include "artwork_controller.h"
#include "organize_controller.h"
#include "export_controller.h"
#include "settings_controller.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace Remus {

WorkflowController::WorkflowController(AppController *app, HashController *hash, MatchController *match,
    ArtworkController *artwork, ConversionController *conversion, OrganizeController *organize,
    ExportController *export_ctl, QObject *parent)
    : QObject(parent)
    , m_appController(app)
    , m_hashController(hash)
    , m_matchController(match)
    , m_artworkController(artwork)
    , m_conversionController(conversion)
    , m_organizeController(organize)
    , m_exportController(export_ctl) {
    // Debounce refresh() calls so that multiple rapid signals (e.g. from several
    // controllers finishing in quick succession) trigger only one filesystem scan.
    // openStage 1–6 (QML StageCards) != Stage enum 0–3 (pipeline queue buckets).
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(0); // coalesces back-to-back signals; fires on next event loop tick
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        refreshCounts();
        refreshQueueFiles();
    });
    connect(app, &AppController::libraryOpened, this, &WorkflowController::refresh);
    connect(app, &AppController::libraryClosed, this, &WorkflowController::refresh);
    connect(hash, &HashController::libraryChanged, this, &WorkflowController::refresh);
    connect(match, &MatchController::libraryChanged, this, &WorkflowController::refresh);
    connect(organize, &OrganizeController::libraryChanged, this, &WorkflowController::refresh);
    connect(export_ctl, &ExportController::libraryChanged, this, &WorkflowController::refresh);
    connect(conversion, &ConversionController::libraryChanged, this, &WorkflowController::refresh);
    connect(artwork, &ArtworkController::artworkDownloaded, this, &WorkflowController::refresh);
}

// ── Public invokables ─────────────────────────────────────────────────────────

void WorkflowController::refresh() {
    if (m_refreshTimer)
        m_refreshTimer->start(); // coalesce rapid-fire signals into one refresh
}

void WorkflowController::setQueueStage(int stage) {
    if (m_queueStage == stage)
        return;
    m_queueStage = stage;
    emit queueStageChanged();
    refreshQueueFiles();
}

void WorkflowController::runAll(const QString &scanDir, const QString &destDir, const QString &namingTemplate) {
    if (!m_appController || !m_appController->isLibraryOpen() || m_running)
        return;
    m_scanDir = scanDir;
    m_destDir = destDir;
    m_namingTemplate = namingTemplate;
    m_running = true;
    m_runStep = 0;
    emit runningChanged();
    advanceRunAll();
}

void WorkflowController::cancel() {
    if (m_running)
        cancelRunAll();
}

void WorkflowController::hashAndMatchAll() {
    if (!m_appController || !m_appController->isLibraryOpen())
        return;
    connect(
        m_hashController, &HashController::hashCompleted, this, [this](int) { m_matchController->matchAll(); },
        Qt::SingleShotConnection);
    m_hashController->startHashAll();
}

void WorkflowController::hashAndMatchSelected() {
    if (!m_appController || !m_appController->isLibraryOpen())
        return;
    connect(
        m_hashController, &HashController::hashCompleted, this, [this](int) { m_matchController->matchSelected(); },
        Qt::SingleShotConnection);
    m_hashController->hashSelected();
}

bool WorkflowController::artworkExistsForFile(int fileId) const {
    if (fileId <= 0)
        return false;
    const QString base = QDir(artworkDirPath()).filePath(QStringLiteral("artwork_%1").arg(fileId));
    for (const char *ext : { ".png", ".jpg", ".jpeg", ".webp" }) {
        if (QFileInfo::exists(base + QLatin1String(ext)))
            return true;
    }
    return false;
}

// ── Private helpers ───────────────────────────────────────────────────────────

QString WorkflowController::artworkDirPath() const {
    QSettings s;
    const QString cfg = s.value(QLatin1String(GuiSettings::ARTWORK_CACHE_DIR)).toString().trimmed();
    if (!cfg.isEmpty())
        return cfg;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("artwork"));
}

void WorkflowController::refreshCounts() {
    if (!m_appController || !m_appController->isLibraryOpen()) {
        m_identityCount = m_enrichCount = m_doneCount = 0;
        emit stageCountsChanged();
        return;
    }

    QSqlDatabase db = m_appController->database()->database();

    // Identity: no confirmed (non-rejected) match
    {
        QSqlQuery q(db);
        const bool ok = q.exec(QStringLiteral("SELECT COUNT(DISTINCT f.id) FROM files f "
                                              "WHERE NOT EXISTS ("
                                              "  SELECT 1 FROM matches m "
                                              "  WHERE m.file_id = f.id "
                                              "    AND m.is_confirmed = 1 AND m.is_rejected = 0)"));
        m_identityCount = (ok && q.next()) ? q.value(0).toInt() : 0;
    }

    // Enrich / Done: confirmed files split by artwork presence
    int enriched = 0;
    int done = 0;
    {
        QSqlQuery q(db);
        const bool ok = q.exec(QStringLiteral("SELECT DISTINCT f.id FROM files f "
                                              "WHERE EXISTS ("
                                              "  SELECT 1 FROM matches m "
                                              "  WHERE m.file_id = f.id "
                                              "    AND m.is_confirmed = 1 AND m.is_rejected = 0)"));
        if (ok) {
            while (q.next()) {
                if (artworkExistsForFile(q.value(0).toInt()))
                    ++done;
                else
                    ++enriched;
            }
        }
    }
    m_enrichCount = enriched;
    m_doneCount = done;

    emit stageCountsChanged();
}

void WorkflowController::refreshQueueFiles() {
    m_queueFiles.clear();

    if (!m_appController || !m_appController->isLibraryOpen()) {
        emit queueFilesChanged();
        return;
    }

    QSqlDatabase db = m_appController->database()->database();
    QSqlQuery q(db);

    // Columns: 0=id, 1=filename, 2=current_path, 3=md5, 4=base_title, 5=extension,
    //           6=child_exts, 7=has_match (confirmed), 8=has_any_match (any non-rejected),
    //           9=is_organized, 10=is_converted, 11=is_bundled,
    //           12=system_name, 13=matched_title, 14=match_confidence, 15=release_date
    static const QLatin1String kFromJoin("FROM files f "
                                         "LEFT JOIN systems sys ON f.system_id = sys.id ");
    static const QLatin1String kMatchMeta(
        "sys.display_name AS system_name, "
        "(SELECT g.title FROM matches m "
        " LEFT JOIN games g ON m.game_id = g.id "
        " WHERE m.file_id = f.id AND m.is_rejected = 0 "
        " ORDER BY m.is_confirmed DESC, m.confidence DESC LIMIT 1) AS matched_title, "
        "(SELECT m.confidence FROM matches m "
        " WHERE m.file_id = f.id AND m.is_rejected = 0 "
        " ORDER BY m.is_confirmed DESC, m.confidence DESC LIMIT 1) AS match_confidence, "
        "(SELECT g.release_date FROM matches m "
        " LEFT JOIN games g ON m.game_id = g.id "
        " WHERE m.file_id = f.id AND m.is_rejected = 0 "
        " ORDER BY m.is_confirmed DESC, m.confidence DESC LIMIT 1) AS release_date");
    static const QLatin1String kChildExts(
        "(SELECT GROUP_CONCAT(f2.extension, ',') FROM files f2 "
        " WHERE f2.parent_file_id = f.id) AS child_exts,"
        " EXISTS(SELECT 1 FROM matches m2"
        "  WHERE m2.file_id = f.id AND m2.is_confirmed = 1 AND m2.is_rejected = 0) AS has_match,"
        " EXISTS(SELECT 1 FROM matches m3"
        "  WHERE m3.file_id = f.id AND m3.is_rejected = 0) AS has_any_match,"
        " EXISTS(SELECT 1 FROM undo_queue u"
        "  WHERE u.file_id = f.id AND u.undone = 0) AS is_organized,"
        " f.is_converted,"
        " f.is_bundled,"
        "%1");

    QString sql;
    switch (m_queueStage) {
    case Identity:
        sql = QStringLiteral("SELECT f.id, f.filename, f.current_path, f.md5, f.base_title, f.extension, "
                             "%1 "
                             "%2 "
                             "WHERE f.is_primary = 1 "
                             "  AND ((f.md5 IS NULL OR f.md5 = '') "
                             "       OR NOT EXISTS ("
                             "         SELECT 1 FROM matches m "
                             "         WHERE m.file_id = f.id "
                             "           AND m.is_confirmed = 1 AND m.is_rejected = 0)) "
                             "ORDER BY COALESCE(f.base_title, f.filename) LIMIT 500")
                  .arg(kChildExts.arg(kMatchMeta), kFromJoin);
        break;
    case Enrich:
    case Done:
        sql = QStringLiteral("SELECT f.id, f.filename, f.current_path, f.md5, f.base_title, f.extension, "
                             "%1 "
                             "%2 "
                             "WHERE f.is_primary = 1 "
                             "  AND EXISTS ("
                             "    SELECT 1 FROM matches m "
                             "    WHERE m.file_id = f.id "
                             "      AND m.is_confirmed = 1 AND m.is_rejected = 0) "
                             "ORDER BY COALESCE(f.base_title, f.filename) LIMIT 500")
                  .arg(kChildExts.arg(kMatchMeta), kFromJoin);
        break;
    default: // AllFiles
        sql = QStringLiteral("SELECT f.id, f.filename, f.current_path, f.md5, f.base_title, f.extension, "
                             "%1 "
                             "%2 "
                             "WHERE f.is_primary = 1 "
                             "ORDER BY COALESCE(f.base_title, f.filename) LIMIT 500")
                  .arg(kChildExts.arg(kMatchMeta), kFromJoin);
        break;
    }

    if (!q.exec(sql)) {
        qWarning() << "WorkflowController::reloadQueue query failed:" << q.lastError().text();
        emit queueFilesChanged();
        return;
    }

    while (q.next()) {
        const int id = q.value(0).toInt();
        const bool hasArtwork = artworkExistsForFile(id);
        const QString baseTitle = q.value(4).toString();
        const QString matchedTitle = q.value(13).toString();
        const QString displayName = !matchedTitle.isEmpty() ? matchedTitle
            : baseTitle.isEmpty()                           ? q.value(1).toString()
                                                            : baseTitle;
        const QString releaseDate = q.value(15).toString();
        int releaseYear = 0;
        if (!releaseDate.isEmpty())
            releaseYear = releaseDate.left(4).toInt();

        if (m_queueStage == Enrich && hasArtwork)
            continue;
        if (m_queueStage == Done && !hasArtwork)
            continue;

        QVariantMap item;
        item[QStringLiteral("fileId")] = id;
        item[QStringLiteral("filename")] = displayName;
        item[QStringLiteral("rawFilename")] = q.value(1).toString();
        item[QStringLiteral("path")] = q.value(2).toString();
        item[QStringLiteral("systemName")] = q.value(12).toString();
        item[QStringLiteral("matchedTitle")] = matchedTitle;
        item[QStringLiteral("confidence")] = q.value(14).toFloat();
        item[QStringLiteral("releaseYear")] = releaseYear;
        static const QStringList kConvertibleExts
            = { QStringLiteral(".cue"), QStringLiteral(".gdi"), QStringLiteral(".iso"), QStringLiteral(".bin"),
                  QStringLiteral(".img"), QStringLiteral(".mdf"), QStringLiteral(".nrg"), QStringLiteral(".gcm") };
        const QString rawExt = q.value(5).toString().toLower(); // already stored with leading dot
        item[QStringLiteral("hasHash")] = !q.value(3).toString().isEmpty();
        item[QStringLiteral("hasArtwork")] = hasArtwork;
        item[QStringLiteral("extension")] = q.value(5).toString();
        item[QStringLiteral("childExtensions")] = q.value(6).toString();
        item[QStringLiteral("hasMatch")] = q.value(7).toBool();
        item[QStringLiteral("hasAnyMatch")] = q.value(8).toBool();
        item[QStringLiteral("isOrganized")] = q.value(9).toBool();
        item[QStringLiteral("isConverted")] = q.value(10).toBool();
        item[QStringLiteral("isBundled")] = q.value(11).toBool();
        item[QStringLiteral("isConvertible")] = kConvertibleExts.contains(rawExt);
        m_queueFiles.append(item);
    }

    emit queueFilesChanged();
}

} // namespace Remus
