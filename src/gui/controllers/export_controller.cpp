#include "export_controller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>

#include "app_controller.h"
#include "settings_controller.h"
#include "../../core/archive_creator.h"
#include "../../core/constants/constants.h"
#include "../../core/library_exporter.h"
#include "../../core/m3u_generator.h"
#include <QHash>
#include <QSet>

namespace Remus {

namespace {

    /// Returns the path where ArtworkController stores downloaded artwork for a file,
    /// probing common image extensions because the downloader may rename the file.
    QString artworkPathForFile(int fileId) {
        QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
            QString::fromLatin1(Constants::SETTINGS_APPLICATION));
        const QString configured = settings.value(QLatin1String(GuiSettings::ARTWORK_CACHE_DIR)).toString().trimmed();
        const QString artDir = configured.isEmpty()
            ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                  .filePath(QStringLiteral("artwork"))
            : configured;
        const QString base = QDir(artDir).filePath(QStringLiteral("artwork_%1").arg(fileId));
        for (const char *ext : { ".png", ".jpg", ".jpeg", ".webp" }) {
            const QString candidate = base + QLatin1String(ext);
            if (QFileInfo::exists(candidate))
                return candidate;
        }
        return QString(); // not found
    }

    bool trashOriginalEnabled() {
        QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
            QString::fromLatin1(Constants::SETTINGS_APPLICATION));
        return settings.value(QStringLiteral("gui/trash_original_after_bundle"), false).toBool();
    }

    /// Prefer ZIP; fall back to 7z. Returns Unknown if neither tool is available.
    ArchiveFormat pickBestFormat() {
        ArchiveCreator probe;
        if (probe.canCompress(ArchiveFormat::ZIP))
            return ArchiveFormat::ZIP;
        if (probe.canCompress(ArchiveFormat::SevenZip))
            return ArchiveFormat::SevenZip;
        return ArchiveFormat::Unknown;
    }

    /// Move @p filePath to {scanDir}/original_roms/ before bundling.
    /// Returns the new path on success, or an empty string on failure.
    QString moveToOriginalRoms(const QString &filePath, const QString &scanDir) {
        const QString origRomsDir = QDir(scanDir).filePath(QStringLiteral("original_roms"));
        if (!QDir().mkpath(origRomsDir))
            return QString();

        // Write the .remusdir marker so the scanner automatically skips this
        // directory on future scans — original ROMs must not be re-imported.
        const QString markerPath
            = QDir(origRomsDir).filePath(QString::fromLatin1(Constants::Settings::Files::MARKER_SKIP_SCAN));
        if (!QFileInfo::exists(markerPath)) {
            QFile marker(markerPath);
            if (!marker.open(QIODevice::WriteOnly)) {
                qWarning() << "export_controller: failed to write scan-skip marker at" << markerPath << "-"
                           << marker.errorString();
            }
        }

        const QString destPath = QDir(origRomsDir).filePath(QFileInfo(filePath).fileName());
        if (QFileInfo::exists(destPath)) {
            // Already moved in a previous run — just acknowledge it.
            if (!QFileInfo::exists(filePath))
                return destPath;
            // Both exist; rename with a numeric suffix to avoid clobbering.
            int n = 1;
            QString candidate;
            do {
                candidate = QDir(origRomsDir)
                                .filePath(QFileInfo(filePath).completeBaseName() + QStringLiteral("_%1.").arg(n++)
                                    + QFileInfo(filePath).suffix());
            } while (QFileInfo::exists(candidate));
            if (!QFile::rename(filePath, candidate))
                return QString();
            return candidate;
        }
        if (!QFile::rename(filePath, destPath))
            return QString();
        return destPath;
    }

} // anonymous namespace

ExportController::ExportController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
    , m_bundler(std::make_unique<RomBundler>(*appController->database(), this)) {
    connect(this, &ExportController::libraryChanged, m_appController, &AppController::refreshSelectedFile);
}

void ExportController::bundleSelected(const QString &scanDir, const QString &namingTemplate) {
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastMessage(QStringLiteral("Open a library before bundling files."));
        return;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        setLastMessage(QStringLiteral("Select a matched file first."));
        return;
    }

    FileRecord file = m_appController->database()->getFileById(fileId);
    const Database::MatchResult match = m_appController->database()->getMatchForFile(fileId);
    if (file.id <= 0 || match.matchId <= 0) {
        setLastMessage(QStringLiteral("Bundling requires a selected file with a metadata match."));
        return;
    }
    if (!match.isConfirmed) {
        setLastMessage(QStringLiteral("Bundling requires a confirmed match. Please confirm the match first."));
        return;
    }

    // Bundle goes into the ROM's own directory.
    const QString romDir = QFileInfo(file.currentPath).absolutePath();

    m_exporting = true;
    m_bundledFiles = 0;
    m_totalBundleFiles = 1;
    m_progressMessage = QStringLiteral("Bundling \"%1\"\u2026").arg(QFileInfo(file.currentPath).fileName());
    emit exportingChanged();
    emit bundleProgressChanged();
    emit progressMessageChanged();

    const bool trashOriginal = trashOriginalEnabled();

    const ArchiveFormat fmt = pickBestFormat();
    if (fmt == ArchiveFormat::Unknown) {
        m_exporting = false;
        emit exportingChanged();
        setLastMessage(QStringLiteral("No archive tool found — install 'zip' or '7z' to create bundles."));
        return;
    }

    if (!trashOriginal) {
        // Move original to original_roms/ before placing the bundle so it can
        // take its canonical name.  Fall back to the ROM's own directory when
        // no scan directory is recorded (e.g. library re-opened across sessions).
        const QString baseDir = scanDir.isEmpty() ? romDir : scanDir;
        const QString newPath = moveToOriginalRoms(file.currentPath, baseDir);
        if (newPath.isEmpty()) {
            m_exporting = false;
            emit exportingChanged();
            setLastMessage(QStringLiteral("Failed to move original ROM to original_roms/."));
            return;
        }
        m_appController->database()->updateFilePath(fileId, newPath);
        file = m_appController->database()->getFileById(fileId);
    }

    RomBundler::BundleConfig config;
    config.outputFormat = fmt;
    config.namingTemplate = namingTemplate;
    const QString cachedArt = artworkPathForFile(fileId);
    if (QFileInfo::exists(cachedArt))
        config.artworkPath = cachedArt;

    const RomBundler::BundleResult result = m_bundler->bundle(file, match, metadataForMatch(match), romDir, config);

    m_exporting = false;
    m_bundledFiles = 1;
    emit exportingChanged();
    emit bundleProgressChanged();

    if (!result.success) {
        m_progressMessage = result.error.isEmpty() ? QStringLiteral("Bundle export failed.") : result.error;
        emit progressMessageChanged();
        setLastMessage(result.error.isEmpty() ? QStringLiteral("Bundle export failed.") : result.error);
        return;
    }

    // Trash original if requested (bundle was created successfully).
    if (trashOriginal && QFileInfo::exists(file.currentPath)) {
        QFile::moveToTrash(file.currentPath);
    }

    m_lastOutputPath = result.outputPath;
    {
        QSqlQuery upd(m_appController->database()->database());
        upd.prepare(
            QStringLiteral("UPDATE files SET is_bundled = 1, bundle_output_path = ?, base_title = ? WHERE id = ?"));
        upd.addBindValue(result.outputPath);
        // Store the bundle display name (filename without archive extension) as base_title
        // so the queue sidebar reflects the bundled & renamed title.
        upd.addBindValue(QFileInfo(result.outputPath).completeBaseName());
        upd.addBindValue(fileId);
        upd.exec();
    }
    m_progressMessage = QStringLiteral("Bundle created: %1").arg(QFileInfo(result.outputPath).fileName());
    emit progressMessageChanged();
    setLastMessage(QStringLiteral("Bundle created: %1").arg(result.outputPath));
    emit exportFinished();
    emit libraryChanged();
}

void ExportController::bundleAll(const QString &scanDir, const QString &namingTemplate) {
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastMessage(QStringLiteral("Open a library before bundling files."));
        return;
    }

    const auto allMatches = m_appController->database()->getAllMatches();
    if (allMatches.isEmpty()) {
        setLastMessage(QStringLiteral("No matched files to bundle."));
        return;
    }

    m_exporting = true;
    m_bundledFiles = 0;
    m_totalBundleFiles = allMatches.size();
    m_progressMessage = QStringLiteral("Bundling files\u2026");
    emit exportingChanged();
    emit bundleProgressChanged();
    emit progressMessageChanged();
    // Yield so the progress bar becomes visible before the loop starts.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const bool trashOriginal = trashOriginalEnabled();

    const ArchiveFormat fmt = pickBestFormat();
    if (fmt == ArchiveFormat::Unknown) {
        m_exporting = false;
        emit exportingChanged();
        setLastMessage(QStringLiteral("No archive tool found — install 'zip' or '7z' to create bundles."));
        return;
    }

    // Prefetch is_bundled for all file IDs in a single query to avoid N+1.
    QSet<int> alreadyBundledIds;
    {
        QSet<int> allFileIds;
        for (auto it = allMatches.constBegin(); it != allMatches.constEnd(); ++it)
            allFileIds.insert(it.key());
        if (!allFileIds.isEmpty()) {
            QStringList ph;
            for (int i = 0; i < allFileIds.size(); ++i)
                ph.append(QStringLiteral("?"));
            QSqlQuery checkQ(m_appController->database()->database());
            checkQ.prepare(QStringLiteral("SELECT id FROM files WHERE is_bundled = 1 AND id IN (%1)")
                    .arg(ph.join(QStringLiteral(","))));
            for (int id : allFileIds)
                checkQ.addBindValue(id);
            if (checkQ.exec()) {
                while (checkQ.next())
                    alreadyBundledIds.insert(checkQ.value(0).toInt());
            }
        }
    }

    // Prefetch all file records in a single batch query.
    QHash<int, FileRecord> fileRecordCache;
    {
        QSet<int> allFileIds;
        for (auto it = allMatches.constBegin(); it != allMatches.constEnd(); ++it)
            allFileIds.insert(it.key());
        const QList<FileRecord> prefetched = m_appController->database()->getFilesByIds(allFileIds);
        for (const FileRecord &fr : prefetched)
            fileRecordCache.insert(fr.id, fr);
    }

    int bundled = 0;
    int failed = 0;

    for (auto it = allMatches.constBegin(); it != allMatches.constEnd(); ++it) {
        FileRecord file = fileRecordCache.value(it.key());
        const Database::MatchResult &match = it.value();
        if (match.isRejected || !match.isConfirmed) {
            ++m_bundledFiles;
            emit bundleProgressChanged();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            continue;
        }
        // Skip files that were already bundled in a previous run.
        if (alreadyBundledIds.contains(it.key())) {
            ++m_bundledFiles;
            emit bundleProgressChanged();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            continue;
        }
        if (file.id <= 0) {
            ++failed;
            ++m_bundledFiles;
            emit bundleProgressChanged();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            continue;
        }

        m_progressMessage = QStringLiteral("Bundling %1 / %2\u2026").arg(m_bundledFiles + 1).arg(m_totalBundleFiles);
        emit progressMessageChanged();

        const QString romDir = QFileInfo(file.currentPath).absolutePath();

        if (!trashOriginal) {
            // Move original to original_roms/ before bundling.  Fall back to
            // the ROM's own directory when no scan directory is available.
            const QString baseDir = scanDir.isEmpty() ? romDir : scanDir;
            const QString newPath = moveToOriginalRoms(file.currentPath, baseDir);
            if (!newPath.isEmpty()) {
                m_appController->database()->updateFilePath(it.key(), newPath);
                file.currentPath = newPath;
                file.filename = QFileInfo(newPath).fileName();
            }
        }

        RomBundler::BundleConfig config;
        config.outputFormat = fmt;
        config.namingTemplate = namingTemplate;
        const QString cachedArt = artworkPathForFile(it.key());
        if (QFileInfo::exists(cachedArt))
            config.artworkPath = cachedArt;

        const RomBundler::BundleResult result = m_bundler->bundle(file, match, metadataForMatch(match), romDir, config);

        if (result.success) {
            if (trashOriginal && QFileInfo::exists(file.currentPath))
                QFile::moveToTrash(file.currentPath);
            ++bundled;
            {
                QSqlQuery upd(m_appController->database()->database());
                upd.prepare(QStringLiteral(
                    "UPDATE files SET is_bundled = 1, bundle_output_path = ?, base_title = ? WHERE id = ?"));
                upd.addBindValue(result.outputPath);
                upd.addBindValue(QFileInfo(result.outputPath).completeBaseName());
                upd.addBindValue(it.key());
                if (!upd.exec())
                    qWarning() << "bundleAll: failed to set is_bundled=1 for file" << it.key()
                               << upd.lastError().text();
            }
        } else {
            ++failed;
        }
        ++m_bundledFiles;
        emit bundleProgressChanged();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    m_exporting = false;
    m_progressMessage = QStringLiteral("Bundled %1 | Failed %2").arg(bundled).arg(failed);
    emit exportingChanged();
    emit bundleProgressChanged();
    emit progressMessageChanged();

    m_lastOutputPath = scanDir;
    setLastMessage(QStringLiteral("Bundled %1 | Failed %2").arg(bundled).arg(failed));
    // Always refresh the library so badge colours update even when all files were
    // already bundled in a previous session (bundled == 0 but is_bundled = 1 in DB).
    emit libraryChanged();
    if (bundled > 0)
        emit exportFinished();
}

int ExportController::generateM3uPlaylists(const QString &outputDir, const QString &systemsCsv) {
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastMessage(QStringLiteral("Open a library before generating playlists."));
        return 0;
    }

    const QString cleanedDir = outputDir.trimmed();
    if (cleanedDir.isEmpty()) {
        setLastMessage(QStringLiteral("Choose an output directory for M3U playlists."));
        return 0;
    }

    Database *db = m_appController->database();
    M3UGenerator generator(*db, this);
    const QStringList systems = parseSystemsFilter(systemsCsv);
    int generated = 0;
    if (systems.isEmpty()) {
        generated = generator.generateAll(QString(), cleanedDir);
    } else {
        const QList<LibraryExportRow> rows = LibraryExporter::buildRows(*db, systems);
        QSet<int> fileIds;
        fileIds.reserve(rows.size());
        for (const LibraryExportRow &row : rows) {
            fileIds.insert(row.file.id);
        }
        generated = generator.generateAll(fileIds, cleanedDir);
    }

    m_lastOutputPath = cleanedDir;
    setLastMessage(QStringLiteral("Generated %1 M3U playlist(s) in %2").arg(generated).arg(cleanedDir));
    emit exportFinished();
    return generated;
}

GameMetadata ExportController::metadataForMatch(const Database::MatchResult &match) const {
    GameMetadata metadata;
    metadata.title = match.gameTitle;
    metadata.region = match.region;
    metadata.publisher = match.publisher;
    metadata.developer = match.developer;
    metadata.description = match.description;
    metadata.releaseDate = match.releaseYear > 0 ? QString::number(match.releaseYear) : QString();
    metadata.genres = match.genre.split(',', Qt::SkipEmptyParts);
    metadata.players = match.players.toInt();
    metadata.rating = match.rating;
    return metadata;
}

void ExportController::setLastMessage(const QString &message) {
    if (m_lastMessage == message) {
        return;
    }

    m_lastMessage = message;
    emit lastMessageChanged();
}

QStringList ExportController::parseSystemsFilter(const QString &systemsCsv) const {
    if (systemsCsv.trimmed().isEmpty())
        return { };
    return systemsCsv.split(',', Qt::SkipEmptyParts);
}

QVariantMap ExportController::exportPreview(const QString &systemsCsv) {
    QVariantMap preview;
    preview.insert(QStringLiteral("totalGames"), 0);
    preview.insert(QStringLiteral("systems"), QVariantList());

    if (m_appController == nullptr || !m_appController->isLibraryOpen())
        return preview;

    const QStringList filters = parseSystemsFilter(systemsCsv);
    const QList<LibraryExportRow> rows = LibraryExporter::buildRows(*m_appController->database(), filters);

    QMap<QString, int> counts;
    for (const auto &row : rows) {
        const QString systemName = m_appController->database()->getSystemDisplayName(row.file.systemId);
        counts[systemName] = counts.value(systemName) + 1;
    }

    QVariantList systemRows;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), it.key());
        item.insert(QStringLiteral("count"), it.value());
        systemRows.append(item);
    }

    preview.insert(QStringLiteral("totalGames"), rows.size());
    preview.insert(QStringLiteral("systems"), systemRows);
    return preview;
}

bool ExportController::exportFrontend(const QString &format, const QString &outputPath, const QString &systemsCsv) {
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastMessage(QStringLiteral("Open a library before exporting."));
        return false;
    }
    if (m_exporting) {
        setLastMessage(QStringLiteral("Another export is already running."));
        return false;
    }

    const QStringList filters = parseSystemsFilter(systemsCsv);
    const QList<LibraryExportRow> rows = LibraryExporter::buildRows(*m_appController->database(), filters);
    if (rows.isEmpty()) {
        setLastMessage(QStringLiteral("No matched files to export."));
        return false;
    }

    m_exporting = true;
    m_exportProgress = 0;
    m_exportTotal = rows.size();
    m_progressMessage = QStringLiteral("Exporting %1…").arg(format);
    emit exportingChanged();
    emit exportProgressChanged();
    emit exportTotalChanged();
    emit progressMessageChanged();

    QString error;
    const bool ok = LibraryExporter::exportToFile(*m_appController->database(), format, outputPath, filters, &error);

    m_exporting = false;
    m_exportProgress = m_exportTotal;
    emit exportingChanged();
    emit exportProgressChanged();

    if (!ok) {
        m_progressMessage.clear();
        emit progressMessageChanged();
        setLastMessage(error.isEmpty() ? QStringLiteral("Export failed.") : error);
        return false;
    }

    m_lastOutputPath = LibraryExporter::resolveOutputPath(format, outputPath);
    m_progressMessage = QStringLiteral("Export complete: %1").arg(m_lastOutputPath);
    emit progressMessageChanged();
    setLastMessage(m_progressMessage);
    emit exportFinished();
    return true;
}

} // namespace Remus