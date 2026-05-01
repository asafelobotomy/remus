#include "artwork_controller.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include "app_controller.h"
#include "settings_controller.h"
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
}

void ArtworkController::refreshSelectedArtwork()
{
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

    if (!refreshArtworkForFile(fileId, true) || !m_previewUrl.isValid() || m_previewUrl.isLocalFile()) {
        setLastError(QStringLiteral("No remote artwork URL is available for the selected file."));
        return false;
    }

    m_downloading = true;
    m_downloadProgress = 0;
    m_downloadTotal = 1;
    emit downloadingChanged();
    emit progressChanged();

    QString savedPath;
    const bool success = m_downloader.download(m_previewUrl, artworkPathForFile(fileId), &savedPath);
    m_downloading = false;
    m_downloadProgress = success ? 1 : 0;
    emit downloadingChanged();
    emit progressChanged();

    if (!success) {
        setLastError(QStringLiteral("Artwork download failed."));
        return false;
    }

    m_localArtworkPath = savedPath.isEmpty() ? artworkPathForFile(fileId) : savedPath;
    m_previewUrl = QUrl::fromLocalFile(m_localArtworkPath);
    emit previewChanged();
    return true;
}

void ArtworkController::downloadAllMatched()
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        return;
    }

    const QList<FileRecord> files = m_appController->database()->getExistingFiles();
    m_downloading = true;
    m_downloadProgress = 0;
    m_downloadTotal = files.size();
    emit downloadingChanged();
    emit progressChanged();

    for (const FileRecord &file : files) {
        if (refreshArtworkForFile(file.id, true) && m_previewUrl.isValid() && !m_previewUrl.isLocalFile()) {
            QString ignoredSavedPath;
            m_downloader.download(m_previewUrl, artworkPathForFile(file.id), &ignoredSavedPath);
        }
        m_downloadProgress++;
        emit progressChanged();
    }

    refreshSelectedArtwork();
    m_downloading = false;
    emit downloadingChanged();
}

bool ArtworkController::refreshArtworkForFile(int fileId, bool requireDownloadableUrl)
{
    if (fileId <= 0 || m_appController == nullptr || !m_appController->isLibraryOpen()) {
        return false;
    }

    const QString localPath = artworkPathForFile(fileId);
    if (QFileInfo::exists(localPath)) {
        m_localArtworkPath = localPath;
        m_previewUrl = QUrl::fromLocalFile(localPath);
        emit previewChanged();
        return true;
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
    GameMetadata metadata = orchestrator->searchWithFallback(
        selectBestMatchHash(file),
        deriveMatchingDisplayName(file),
        systemName,
        file.crc32,
        file.md5,
        file.sha1,
        QString(),
        true);

    QString boxArtUrl = metadata.boxArtUrl;
    if (boxArtUrl.isEmpty() && !metadata.id.isEmpty()) {
        const ArtworkUrls artwork = orchestrator->getArtworkWithFallback(metadata.id, systemName, metadata.providerId);
        boxArtUrl = artwork.boxFront.toString();
    }

    if (boxArtUrl.isEmpty()) {
        if (!requireDownloadableUrl) {
            m_previewUrl = QUrl();
            m_localArtworkPath.clear();
            emit previewChanged();
        }
        return false;
    }

    m_previewUrl = QUrl(boxArtUrl);
    m_localArtworkPath.clear();
    emit previewChanged();
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
    return dir.filePath(QStringLiteral("artwork_%1.png").arg(fileId));
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