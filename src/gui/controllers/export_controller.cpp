#include "export_controller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>

#include "app_controller.h"
#include "settings_controller.h"
#include "../../core/constants/constants.h"

namespace Remus {

namespace {

/// Returns the path where ArtworkController stores downloaded artwork for a file,
/// probing common image extensions because the downloader may rename the file.
QString artworkPathForFile(int fileId)
{
    QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                       QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    const QString configured = settings.value(QLatin1String(GuiSettings::ARTWORK_CACHE_DIR)).toString().trimmed();
    const QString artDir = configured.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("artwork"))
        : configured;
    const QString base = QDir(artDir).filePath(QStringLiteral("artwork_%1").arg(fileId));
    for (const char *ext : {".png", ".jpg", ".jpeg", ".webp"}) {
        const QString candidate = base + QLatin1String(ext);
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString(); // not found
}

bool trashOriginalEnabled()
{
    QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                       QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    return settings.value(QStringLiteral("gui/trash_original_after_bundle"), false).toBool();
}

/// Move @p filePath to {scanDir}/original_roms/ before bundling.
/// Returns the new path on success, or an empty string on failure.
QString moveToOriginalRoms(const QString &filePath, const QString &scanDir)
{
    const QString origRomsDir = QDir(scanDir).filePath(QStringLiteral("original_roms"));
    if (!QDir().mkpath(origRomsDir))
        return QString();

    // Write the .remusdir marker so the scanner automatically skips this
    // directory on future scans — original ROMs must not be re-imported.
    const QString markerPath = QDir(origRomsDir).filePath(
        QString::fromLatin1(Constants::Settings::Files::MARKER_SKIP_SCAN));
    if (!QFileInfo::exists(markerPath)) {
        QFile marker(markerPath);
        marker.open(QIODevice::WriteOnly);
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
            candidate = QDir(origRomsDir).filePath(
                QFileInfo(filePath).completeBaseName()
                + QStringLiteral("_%1.").arg(n++)
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
    , m_bundler(std::make_unique<RomBundler>(*appController->database(), this))
{
    connect(this, &ExportController::libraryChanged,
            m_appController, &AppController::refreshSelectedFile);
}

void ExportController::bundleSelected(const QString &scanDir, const QString &namingTemplate)
{
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
        upd.prepare(QStringLiteral("UPDATE files SET is_bundled = 1, bundle_output_path = ? WHERE id = ?"));
        upd.addBindValue(result.outputPath);
        upd.addBindValue(fileId);
        upd.exec();
    }
    m_progressMessage = QStringLiteral("Bundle created: %1").arg(QFileInfo(result.outputPath).fileName());
    emit progressMessageChanged();
    setLastMessage(QStringLiteral("Bundle created: %1").arg(result.outputPath));
    emit exportFinished();
    emit libraryChanged();
}

void ExportController::bundleAll(const QString &scanDir, const QString &namingTemplate)
{
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

    const bool trashOriginal = trashOriginalEnabled();
    int bundled = 0;
    int failed  = 0;

    for (auto it = allMatches.constBegin(); it != allMatches.constEnd(); ++it) {
        FileRecord file = m_appController->database()->getFileById(it.key());
        const Database::MatchResult &match = it.value();
        if (match.isRejected || !match.isConfirmed) {
            ++m_bundledFiles;
            emit bundleProgressChanged();
            continue;
        }
        // Skip files that were already bundled in a previous run.
        {
            QSqlQuery checkQ(m_appController->database()->database());
            checkQ.prepare(QStringLiteral("SELECT is_bundled FROM files WHERE id = ?"));
            checkQ.addBindValue(it.key());
            if (checkQ.exec() && checkQ.next() && checkQ.value(0).toBool()) {
                ++m_bundledFiles;
                emit bundleProgressChanged();
                continue;
            }
        }
        if (file.id <= 0) {
            ++failed;
            ++m_bundledFiles;
            emit bundleProgressChanged();
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
                file = m_appController->database()->getFileById(it.key());
            }
        }

        RomBundler::BundleConfig config;
        config.namingTemplate = namingTemplate;
        const QString cachedArt = artworkPathForFile(it.key());
        if (QFileInfo::exists(cachedArt))
            config.artworkPath = cachedArt;

        const RomBundler::BundleResult result =
            m_bundler->bundle(file, match, metadataForMatch(match), romDir, config);

        if (result.success) {
            if (trashOriginal && QFileInfo::exists(file.currentPath))
                QFile::moveToTrash(file.currentPath);
            ++bundled;
            {
                QSqlQuery upd(m_appController->database()->database());
                upd.prepare(QStringLiteral("UPDATE files SET is_bundled = 1, bundle_output_path = ? WHERE id = ?"));
                upd.addBindValue(result.outputPath);
                upd.addBindValue(it.key());
                upd.exec();
            }
        } else {
            ++failed;
        }
        ++m_bundledFiles;
        emit bundleProgressChanged();
    }

    m_exporting = false;
    m_progressMessage = QStringLiteral("Bundled %1 | Failed %2").arg(bundled).arg(failed);
    emit exportingChanged();
    emit bundleProgressChanged();
    emit progressMessageChanged();

    m_lastOutputPath = scanDir;
    setLastMessage(QStringLiteral("Bundled %1 | Failed %2").arg(bundled).arg(failed));
    if (bundled > 0) {
        emit exportFinished();
        emit libraryChanged();
    }
}

bool ExportController::exportM3u(const QString &outputPath)
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastMessage(QStringLiteral("Open a library before exporting playlists."));
        return false;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        setLastMessage(QStringLiteral("Select a file first."));
        return false;
    }

    const FileRecord file = m_appController->database()->getFileById(fileId);
    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setLastMessage(QStringLiteral("Failed to create playlist: %1").arg(outputPath));
        return false;
    }

    QTextStream stream(&outFile);
    stream << file.currentPath << '\n';
    // Include child files (multi-disc/multi-track groups)
    const QList<FileRecord> children = m_appController->database()->getFilesByParent(fileId);
    for (const FileRecord &child : children) {
        if (!child.currentPath.isEmpty())
            stream << child.currentPath << '\n';
    }
    outFile.close();

    m_lastOutputPath = outputPath;
    setLastMessage(QStringLiteral("Playlist exported: %1").arg(outputPath));
    emit exportFinished();
    return true;
}

GameMetadata ExportController::metadataForMatch(const Database::MatchResult &match) const
{
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

void ExportController::setLastMessage(const QString &message)
{
    if (m_lastMessage == message) {
        return;
    }

    m_lastMessage = message;
    emit lastMessageChanged();
}

} // namespace Remus