#include "artwork_controller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include "app_controller.h"
#include "settings_controller.h"
#include "../../core/constants/provider_fields.h"
#include "../../core/constants/constants.h"
#include "../../core/match_utils.h"
#include "../../metadata/metadata_provider.h"
#include "../../metadata/provider_orchestrator.h"

namespace Remus {

ArtworkController::ArtworkController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
{
    connect(m_appController, &AppController::selectedFileChanged, this, &ArtworkController::refreshSelectedArtwork);
    connect(m_appController, &AppController::artworkCacheEraseRequested, this, &ArtworkController::clearArtworkCache);
}

void ArtworkController::refreshSelectedArtwork()
{
    // Clear stale error and phase message when the selected file changes.
    if (!m_lastError.isEmpty()) {
        m_lastError.clear();
        emit lastErrorChanged();
    }
    if (!m_progressMessage.isEmpty()) {
        m_progressMessage.clear();
        emit progressMessageChanged();
    }

    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        m_previewUrl = QUrl();
        m_localArtworkPath.clear();
        emit previewChanged();
        return;
    }

    refreshArtworkForFile(m_appController->selectedFileId(), false);
}

bool ArtworkController::downloadSelected()
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastError(QStringLiteral("Open a library before downloading artwork."));
        return false;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        setLastError(QStringLiteral("Select a file first."));
        return false;
    }

    // Phase 1: enriching metadata (indeterminate bar).
    m_downloadProgress = 0;
    m_downloadTotal    = 0;
    m_progressMessage  = QStringLiteral("Enriching metadata\u2026");
    emit progressChanged();
    emit progressMessageChanged();

    // Explicit enrichment: don't require a confirmed match.
    const bool artworkReady = refreshArtworkForFile(fileId, true, false);
    // Refresh match panel in case metadata fields were enriched even if no artwork URL was found.
    m_appController->refreshSelectedMatch();

    if (!artworkReady) {
        const Database::MatchResult mr = m_appController->database()->getMatchForFile(fileId);
        const QString errorMsg = mr.matchId <= 0
            ? QStringLiteral("No match found for the selected file \u2014 run Hash & Match first.")
            : QStringLiteral("No remote artwork URL is available for the selected file.");
        m_progressMessage = errorMsg;
        emit progressMessageChanged();
        setLastError(errorMsg);
        return false;
    }

    // Artwork is already downloaded locally \u2014 nothing to re-download.
    if (m_previewUrl.isLocalFile()) {
        m_downloadProgress = 1;
        m_downloadTotal    = 1;
        m_progressMessage  = QStringLiteral("Artwork already saved locally.");
        emit progressChanged();
        emit progressMessageChanged();
        setLastError(QString());
        return true;
    }

    if (!m_previewUrl.isValid()) {
        const QString errorMsg = QStringLiteral("No remote artwork URL is available for the selected file.");
        m_progressMessage = errorMsg;
        emit progressMessageChanged();
        setLastError(errorMsg);
        return false;
    }

    // Phase 2: downloading artwork (determinate 0 → 1).
    m_downloading      = true;
    m_downloadProgress = 0;
    m_downloadTotal    = 1;
    m_progressMessage  = QStringLiteral("Downloading artwork\u2026");
    emit downloadingChanged();
    emit progressChanged();
    emit progressMessageChanged();

    QString savedPath;
    const bool success = m_downloader.download(m_previewUrl, artworkPathForFile(fileId), &savedPath);
    m_downloading      = false;
    m_downloadProgress = success ? 1 : 0;
    emit downloadingChanged();
    emit progressChanged();

    if (!success) {
        const QString errorMsg = QStringLiteral("Artwork download failed.");
        m_progressMessage = errorMsg;
        emit progressMessageChanged();
        setLastError(errorMsg);
        return false;
    }

    m_localArtworkPath = savedPath.isEmpty() ? artworkPathForFile(fileId) : savedPath;
    m_previewUrl       = QUrl::fromLocalFile(m_localArtworkPath);
    m_progressMessage  = QStringLiteral("Artwork saved.");
    emit progressMessageChanged();
    setLastError(QString());
    emit previewChanged();
    emit artworkDownloaded();
    return true;
}

void ArtworkController::downloadAllMatched()
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        return;
    }

    const QList<FileRecord> files = m_appController->database()->getFilesWithConfirmedMatch();
    m_downloading      = true;
    m_downloadProgress = 0;
    m_downloadTotal    = files.size();
    m_progressMessage  = QStringLiteral("Downloading artwork for all confirmed-match ROMs\u2026");
    emit downloadingChanged();
    emit progressChanged();
    emit progressMessageChanged();

    int done = 0;
    int downloadSucceeded = 0;
    int downloadFailed = 0;
    m_batchDownloading = true;
    for (const FileRecord &file : files) {
        m_progressMessage = QStringLiteral("Processing %1 / %2").arg(done + 1).arg(files.size());
        emit progressMessageChanged();
        if (refreshArtworkForFile(file.id, true) && m_previewUrl.isValid() && !m_previewUrl.isLocalFile()) {
            QString savedPath;
            if (m_downloader.download(m_previewUrl, artworkPathForFile(file.id), &savedPath))
                ++downloadSucceeded;
            else
                ++downloadFailed;
        }
        m_downloadProgress = ++done;
        emit progressChanged();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    m_batchDownloading = false;
    refreshSelectedArtwork();
    m_downloading     = false;
    m_progressMessage = QStringLiteral("Completed: %1 downloaded, %2 failed, %3 skipped.")
                            .arg(downloadSucceeded).arg(downloadFailed)
                            .arg(done - downloadSucceeded - downloadFailed);
    emit downloadingChanged();
    emit progressMessageChanged();
    emit artworkDownloaded();
}

void ArtworkController::clearArtworkCache()
{
    const QString dir = defaultArtworkDir();
    const QStringList files = QDir(dir).entryList(
        QStringList() << QStringLiteral("*.png") << QStringLiteral("*.jpg")
                      << QStringLiteral("*.jpeg") << QStringLiteral("*.webp"),
        QDir::Files);
    for (const QString &filename : files) {
        QFile::remove(QDir(dir).filePath(filename));
    }
    m_previewUrl = QUrl();
    m_localArtworkPath.clear();
    emit previewChanged();
}

bool ArtworkController::refreshArtworkForFile(int fileId, bool requireDownloadableUrl, bool requireConfirmed)
{
    if (fileId <= 0 || m_appController == nullptr || !m_appController->isLibraryOpen()) {
        return false;
    }

    const Database::MatchResult matchResult = m_appController->database()->getMatchForFile(fileId);
    if (matchResult.matchId <= 0) {
        m_previewUrl = QUrl();
        m_localArtworkPath.clear();
        if (!m_batchDownloading) emit previewChanged();
        return false;
    }

    // AUTO-PREVIEW PATH (file selection): never query providers.
    // Show local artwork if present; otherwise clear the preview.
    if (!requireDownloadableUrl) {
        const QString localPath = artworkPathForFile(fileId);
        if (QFileInfo::exists(localPath)) {
            m_localArtworkPath = localPath;
            m_previewUrl = QUrl::fromLocalFile(localPath);
            if (!m_batchDownloading) emit previewChanged();
            return true;
        }
        m_previewUrl = QUrl();
        m_localArtworkPath.clear();
        if (!m_batchDownloading) emit previewChanged();
        return false;
    }

    // EXPLICIT ENRICHMENT PATH: query providers when required.
    if (requireConfirmed && !matchResult.isConfirmed) {
        m_previewUrl = QUrl();
        m_localArtworkPath.clear();
        if (!m_batchDownloading) emit previewChanged();
        return false;
    }

    ProviderOrchestrator *orchestrator = m_appController->orchestrator();
    if (orchestrator == nullptr) {
        return false;
    }

    const FileRecord file = m_appController->database()->getFileById(fileId);
    if (file.id <= 0) {
        return false;
    }

    const QString systemName = m_appController->systemName(file.systemId);

    // Build baseline from the stored match so identity fields aren't re-fetched.
    GameMetadata existing;
    existing.title     = matchResult.gameTitle;
    existing.publisher = matchResult.publisher;
    existing.developer = matchResult.developer;
    if (matchResult.releaseYear > 0)
        existing.releaseDate = QString::number(matchResult.releaseYear);
    existing.description = matchResult.description;
    if (!matchResult.genre.isEmpty())
        existing.genres = matchResult.genre.split(QStringLiteral(", "));
    if (!matchResult.players.isEmpty()) {
        bool ok = false;
        const int p = matchResult.players.toInt(&ok);
        if (ok) existing.players = p;
    }
    existing.rating = matchResult.rating;
    existing.region = matchResult.region;

    // Target only enrichment fields — identity is already resolved from match.
    using namespace Constants::ProviderFields;
    ProviderOrchestrator::FieldSet missing;
    if (existing.releaseDate.isEmpty()) missing.insert(RELEASE_DATE);
    if (existing.description.isEmpty()) missing.insert(DESCRIPTION);
    if (existing.genres.isEmpty())      missing.insert(GENRES);
    if (existing.players == 0)          missing.insert(PLAYERS);
    if (existing.rating == 0.0f)        missing.insert(RATING);
    missing.insert(BOX_ART_URL);

    // Always run enrichment — even if local artwork already exists.
    // This ensures description/rating/release are populated on every Enrich click
    // until the DB record is fully filled, not just on the first one.
    const GameMetadata enriched = orchestrator->enrichMissingFields(
        missing, existing,
        selectBestMatchHash(file),
        deriveMatchingDisplayName(file),
        systemName,
        file.crc32, file.md5, file.sha1);

    // Persist newly enriched fields to the stored game record.
    if (matchResult.gameId > 0 && !enriched.title.isEmpty()) {
        m_appController->database()->updateGame(
            matchResult.gameId,
            QString(),
            QString(),
            existing.releaseDate.isEmpty() ? enriched.releaseDate                                                        : QString(),
            existing.description.isEmpty() ? enriched.description                                                        : QString(),
            existing.genres.isEmpty()      ? enriched.genres.join(QStringLiteral(", "))                                  : QString(),
            existing.players == 0          ? (enriched.players > 0 ? QString::number(enriched.players) : QString())      : QString(),
            (existing.rating == 0.0f && enriched.rating > 0.0f) ? enriched.rating : -1.0f);
    }

    // If artwork is already downloaded locally, show it and return.
    const QString localPath = artworkPathForFile(fileId);
    if (QFileInfo::exists(localPath)) {
        m_localArtworkPath = localPath;
        m_previewUrl = QUrl::fromLocalFile(localPath);
        if (!m_batchDownloading) emit previewChanged();
        return true;
    }

    // No local artwork yet — use the remote URL from enrichment for the download step.
    if (enriched.boxArtUrl.isEmpty()) {
        return false;
    }

    m_previewUrl = QUrl(enriched.boxArtUrl);
    m_localArtworkPath.clear();
    if (!m_batchDownloading) emit previewChanged();
    return true;
}

QString ArtworkController::defaultArtworkDir() const
{
    QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                       QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    const QString configured = settings.value(QString::fromLatin1(GuiSettings::ARTWORK_CACHE_DIR)).toString().trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }

    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("artwork"));
}

QString ArtworkController::artworkPathForFile(int fileId) const
{
    QDir dir(defaultArtworkDir());
    dir.mkpath(QStringLiteral("."));
    // The downloader may rename the file to match the actual image format (.jpg, .webp …).
    // Probe known extensions before falling back to the default .png destination path.
    const QString base = dir.filePath(QStringLiteral("artwork_%1").arg(fileId));
    for (const char *ext : {".png", ".jpg", ".jpeg", ".webp"}) {
        const QString candidate = base + QLatin1String(ext);
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return base + QStringLiteral(".png"); // default destination for new downloads
}

void ArtworkController::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

} // namespace Remus