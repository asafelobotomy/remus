#include "workflow_controller.h"

#include "app_controller.h"
#include "conversion_controller.h"
#include "hash_controller.h"
#include "match_controller.h"
#include "artwork_controller.h"
#include "organize_controller.h"
#include "export_controller.h"
#include "settings_controller.h"

#include "../../core/disc_set_utils.h"
#include "../../core/compendium_disc_bridge.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QDateTime>
#include <algorithm>

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

void WorkflowController::toggleDiscGroupExpanded(const QString &groupKey) {
    if (groupKey.isEmpty())
        return;
    const bool expanded = m_discGroupExpanded.value(groupKey, true);
    m_discGroupExpanded.insert(groupKey, !expanded);
    refreshQueueFiles();
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

    // Enrich / Done: confirmed files split by has_local_artwork flag
    {
        QSqlQuery q(db);
        const bool ok = q.exec(QStringLiteral(
            "SELECT "
            "  SUM(CASE WHEN f.has_local_artwork = 1 THEN 1 ELSE 0 END), "
            "  SUM(CASE WHEN f.has_local_artwork = 0 THEN 1 ELSE 0 END) "
            "FROM files f "
            "WHERE EXISTS ("
            "  SELECT 1 FROM matches m "
            "  WHERE m.file_id = f.id "
            "    AND m.is_confirmed = 1 AND m.is_rejected = 0)"));
        if (ok && q.next()) {
            m_doneCount = q.value(0).toInt();
            m_enrichCount = q.value(1).toInt();
        } else {
            m_enrichCount = m_doneCount = 0;
        }
    }

    emit stageCountsChanged();
}

void WorkflowController::refreshQueueFiles() {
    m_queueFiles.clear();

    if (!m_appController || !m_appController->isLibraryOpen()) {
        emit queueFilesChanged();
        return;
    }

    QSqlDatabase db = m_appController->database()->database();
    QString catalogConn;
    QSqlDatabase catalogDb;
    const QString compendiumPath = m_appController->database()->compendiumDbPath();
    if (!compendiumPath.isEmpty() && QFileInfo::exists(compendiumPath)) {
        catalogConn = QStringLiteral("workflow_catalog_%1").arg(QDateTime::currentMSecsSinceEpoch());
        catalogDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), catalogConn);
        catalogDb.setDatabaseName(compendiumPath);
        if (!catalogDb.open()) {
            QSqlDatabase::removeDatabase(catalogConn);
            catalogConn.clear();
        }
    }
    const auto closeCatalog = [&]() {
        if (!catalogConn.isEmpty()) {
            if (catalogDb.isOpen())
                catalogDb.close();
            QSqlDatabase::removeDatabase(catalogConn);
            catalogConn.clear();
        }
    };

    QSqlQuery q(db);

    // Columns: 0=id, 1=filename, 2=current_path, 3=md5, 4=base_title, 5=extension,
    //           6=child_exts, 7=has_match (confirmed), 8=has_any_match (any non-rejected),
    //           9=is_organized, 10=is_converted, 11=is_bundled,
    //           12=system_name, 13=matched_title, 14=match_confidence, 15=release_date,
    //           16=archive_path, 17=archive_internal_path, 18=disc_set_key, 19=disc_number,
    //           20=has_local_artwork
    //
    // Best-match CTE (bm): collapses the old three correlated subqueries into a single
    // LEFT JOIN so matched_title / match_confidence / release_date are fetched in one pass.
    static const QLatin1String kFromJoin(
        "FROM files f "
        "LEFT JOIN systems sys ON f.system_id = sys.id "
        "LEFT JOIN ("
        "  SELECT m.file_id,"
        "         g.title        AS matched_title,"
        "         m.confidence   AS match_confidence,"
        "         g.release_date AS release_date,"
        "         ROW_NUMBER() OVER ("
        "           PARTITION BY m.file_id"
        "           ORDER BY m.is_confirmed DESC, m.confidence DESC"
        "         ) AS rn"
        "  FROM matches m"
        "  LEFT JOIN games g ON m.game_id = g.id"
        "  WHERE m.is_rejected = 0"
        ") bm ON bm.file_id = f.id AND bm.rn = 1 ");
    static const QLatin1String kMatchMeta(
        "sys.display_name AS system_name, "
        "bm.matched_title, "
        "bm.match_confidence, "
        "bm.release_date");
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
                             "%1, f.archive_path, f.archive_internal_path, f.disc_set_key, f.disc_number, "
                             "f.has_local_artwork "
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
                             "%1, f.archive_path, f.archive_internal_path, f.disc_set_key, f.disc_number, "
                             "f.has_local_artwork "
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
                             "%1, f.archive_path, f.archive_internal_path, f.disc_set_key, f.disc_number, "
                             "f.has_local_artwork "
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

    QList<QVariantMap> flatItems;
    flatItems.reserve(500);

    while (q.next()) {
        const int id = q.value(0).toInt();
        const bool hasArtwork = q.value(20).toBool();
        const QString baseTitle = q.value(4).toString();
        const QString matchedTitle = q.value(13).toString();
        const QString rawFilename = q.value(1).toString();
        const QString currentPath = q.value(2).toString();
        const QString archivePath = q.value(16).toString();
        const QString archiveInternalPath = q.value(17).toString();
        const QString discSetKey = q.value(18).toString();
        const int discNumber = q.value(19).toInt();
        const QString systemName = q.value(12).toString();
        const QString labelPath = DiscSetUtils::labelPath(currentPath, archivePath, archiveInternalPath, rawFilename);
        const QString displayName = !matchedTitle.isEmpty() ? matchedTitle
            : baseTitle.isEmpty()                           ? rawFilename
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
        item[QStringLiteral("rowType")] = QStringLiteral("file");
        item[QStringLiteral("fileId")] = id;
        item[QStringLiteral("filename")] = displayName;
        item[QStringLiteral("rawFilename")] = rawFilename;
        item[QStringLiteral("path")] = currentPath;
        item[QStringLiteral("labelPath")] = labelPath;
        item[QStringLiteral("systemName")] = systemName;
        item[QStringLiteral("matchedTitle")] = matchedTitle;
        item[QStringLiteral("confidence")] = q.value(14).toFloat();
        item[QStringLiteral("releaseYear")] = releaseYear;
        item[QStringLiteral("discNumber")] = discNumber;
        item[QStringLiteral("discSetKey")] = discSetKey;
        item[QStringLiteral("groupKey")] = discSetKey;
        static const QStringList kConvertibleExts
            = { QStringLiteral(".cue"), QStringLiteral(".gdi"), QStringLiteral(".iso"), QStringLiteral(".bin"),
                  QStringLiteral(".img"), QStringLiteral(".mdf"), QStringLiteral(".nrg"), QStringLiteral(".gcm") };
        const QString rawExt = q.value(5).toString().toLower();
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
        flatItems.append(item);
    }

    QHash<QString, QList<int>> multiDiscGroups;
    for (int i = 0; i < flatItems.size(); ++i) {
        const QString groupKey = flatItems[i].value(QStringLiteral("discSetKey")).toString();
        if (!groupKey.isEmpty())
            multiDiscGroups[groupKey].append(i);
    }

    QSet<QString> emittedGroups;
    for (int i = 0; i < flatItems.size(); ++i) {
        const QVariantMap &source = flatItems.at(i);
        const QString groupKey = source.value(QStringLiteral("discSetKey")).toString();
        const bool inMultiDiscSet = !groupKey.isEmpty() && multiDiscGroups.value(groupKey).size() >= 2;

        if (inMultiDiscSet) {
            if (emittedGroups.contains(groupKey))
                continue;
            emittedGroups.insert(groupKey);

            QList<int> memberIndices = multiDiscGroups.value(groupKey);
            std::sort(memberIndices.begin(), memberIndices.end(), [&flatItems](int a, int b) {
                const int discA = flatItems.at(a).value(QStringLiteral("discNumber")).toInt();
                const int discB = flatItems.at(b).value(QStringLiteral("discNumber")).toInt();
                if (discA != discB)
                    return discA < discB;
                return flatItems.at(a).value(QStringLiteral("rawFilename")).toString()
                    < flatItems.at(b).value(QStringLiteral("rawFilename")).toString();
            });

            const QVariantMap &first = flatItems.at(memberIndices.first());
            const QString groupTitle = !first.value(QStringLiteral("matchedTitle")).toString().isEmpty()
                ? first.value(QStringLiteral("matchedTitle")).toString()
                : !first.value(QStringLiteral("filename")).toString().isEmpty()
                ? first.value(QStringLiteral("filename")).toString()
                : first.value(QStringLiteral("rawFilename")).toString();
            const bool expanded = m_discGroupExpanded.value(groupKey, true);

            int matchedCount = 0;
            int anyMatchCount = 0;
            int hashedCount = 0;
            int artworkCount = 0;
            int convertedCount = 0;
            int bundledCount = 0;
            int organizedCount = 0;
            for (int memberIndex : memberIndices) {
                const QVariantMap &member = flatItems.at(memberIndex);
                if (member.value(QStringLiteral("hasMatch")).toBool())
                    ++matchedCount;
                if (member.value(QStringLiteral("hasAnyMatch")).toBool())
                    ++anyMatchCount;
                if (member.value(QStringLiteral("hasHash")).toBool())
                    ++hashedCount;
                if (member.value(QStringLiteral("hasArtwork")).toBool())
                    ++artworkCount;
                if (member.value(QStringLiteral("isConverted")).toBool())
                    ++convertedCount;
                if (member.value(QStringLiteral("isBundled")).toBool())
                    ++bundledCount;
                if (member.value(QStringLiteral("isOrganized")).toBool())
                    ++organizedCount;
            }
            const int memberTotal = memberIndices.size();

            int catalogDiscCount = 0;
            if (catalogDb.isOpen()) {
                CatalogDiscSetSummary summary;
                if (lookupCatalogDiscSetSummary(catalogDb, groupKey, summary) && summary.found)
                    catalogDiscCount = summary.catalogDiscCount;
            }

            QVariantMap header;
            header[QStringLiteral("rowType")] = QStringLiteral("group");
            header[QStringLiteral("groupKey")] = groupKey;
            header[QStringLiteral("filename")] = groupTitle;
            header[QStringLiteral("systemName")] = first.value(QStringLiteral("systemName"));
            header[QStringLiteral("discCount")] = memberTotal;
            header[QStringLiteral("catalogDiscCount")] = catalogDiscCount;
            header[QStringLiteral("discProgress")]
                = catalogDiscCount > 0 ? QStringLiteral("%1/%2").arg(memberTotal).arg(catalogDiscCount)
                                     : QStringLiteral("%1").arg(memberTotal);
            header[QStringLiteral("discSetComplete")] = catalogDiscCount <= 0 || memberTotal >= catalogDiscCount;
            header[QStringLiteral("expanded")] = expanded;
            header[QStringLiteral("releaseYear")] = first.value(QStringLiteral("releaseYear"));
            header[QStringLiteral("hasMatch")] = matchedCount == memberTotal;
            header[QStringLiteral("hasAnyMatch")] = anyMatchCount > 0;
            header[QStringLiteral("hasHash")] = hashedCount > 0;
            header[QStringLiteral("hasArtwork")] = artworkCount == memberTotal;
            header[QStringLiteral("isConverted")] = convertedCount == memberTotal;
            header[QStringLiteral("isBundled")] = bundledCount == memberTotal;
            header[QStringLiteral("isOrganized")] = organizedCount == memberTotal;
            header[QStringLiteral("isConvertible")] = first.value(QStringLiteral("isConvertible"));
            header[QStringLiteral("matchProgress")] = QStringLiteral("%1/%2").arg(matchedCount).arg(memberTotal);
            header[QStringLiteral("artworkProgress")] = QStringLiteral("%1/%2").arg(artworkCount).arg(memberTotal);

            QStringList searchParts;
            searchParts << header.value(QStringLiteral("filename")).toString();
            for (int memberIndex : memberIndices) {
                const QVariantMap &member = flatItems.at(memberIndex);
                searchParts << member.value(QStringLiteral("rawFilename")).toString();
                searchParts << member.value(QStringLiteral("path")).toString();
                searchParts << member.value(QStringLiteral("matchedTitle")).toString();
                searchParts << DiscSetUtils::discRowLabel(member.value(QStringLiteral("labelPath")).toString(),
                    member.value(QStringLiteral("discNumber")).toInt());
            }
            header[QStringLiteral("memberSearchText")] = searchParts.join(QChar(' '));

            m_queueFiles.append(header);

            if (!expanded)
                continue;

            for (int memberIndex : memberIndices) {
                QVariantMap disc = flatItems.at(memberIndex);
                disc[QStringLiteral("rowType")] = QStringLiteral("disc");
                const int discNumber = disc.value(QStringLiteral("discNumber")).toInt();
                disc[QStringLiteral("discLabel")]
                    = DiscSetUtils::discRowLabel(disc.value(QStringLiteral("labelPath")).toString(), discNumber);
                disc[QStringLiteral("filename")] = disc.value(QStringLiteral("discLabel"));
                m_queueFiles.append(disc);
            }
            continue;
        }

        m_queueFiles.append(source);
    }

    closeCatalog();
    emit queueFilesChanged();
}

} // namespace Remus
