#include "processing_controller.h"
#include "../../core/constants/systems.h"
#include "../../core/match_utils.h"
#include "../../core/constants/match_methods.h"
#include "../../core/constants/hash_algorithms.h"
#include "../../core/constants/settings.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <exception>
#include "../../core/logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logUi)
#define qInfo() qCInfo(logUi)
#define qWarning() qCWarning(logUi)
#define qCritical() qCCritical(logUi)

namespace Remus {

using namespace Constants;

void ProcessingController::stepExtract()
{
    qDebug() << "Extracting:" << m_currentFilePath;
    
    // Check if it's actually an archive
    if (!isArchiveFile(m_currentFilePath)) {
        // Not an archive, working path is the original file's directory
        m_extractedDir = QFileInfo(m_currentFilePath).absolutePath();
        onStepComplete(true, QString());
        return;
    }
    
    // Get archive info
    ArchiveInfo info = m_archiveExtractor->getArchiveInfo(m_currentFilePath);
    if (info.format == ArchiveFormat::Unknown) {
        qWarning() << "Unknown archive format:" << m_currentFilePath;
        m_extractedDir = QFileInfo(m_currentFilePath).absolutePath();
        onStepComplete(true, QString());  // Continue anyway
        return;
    }
    
    // Extract to a permanent folder next to the archive
    // e.g., "Game.zip" -> "Game/"
    QFileInfo archiveInfo(m_currentFilePath);
    QString baseName = archiveInfo.completeBaseName();
    QString extractDir = archiveInfo.absolutePath() + "/" + baseName;
    
    // Create the extraction directory
    QDir dir;
    if (!dir.mkpath(extractDir)) {
        qWarning() << "Failed to create extraction directory:" << extractDir;
        onStepComplete(false, "Failed to create extraction directory");
        return;
    }
    
    ExtractionResult result = m_archiveExtractor->extract(m_currentFilePath, extractDir);
    
    if (result.success && !result.extractedFiles.isEmpty()) {
        m_extractedDir = extractDir;
        m_wasArchive = true;
        
        // Use the first ROM file found for hashing
        for (const QString &extracted : result.extractedFiles) {
            QFileInfo fi(extracted);
            QString ext = fi.suffix().toLower();
            // Check if it's a ROM file (not a readme, etc.)
            if (ext != "txt" && ext != "nfo" && ext != "diz") {
                m_workingFilePath = extracted;
                qDebug() << "Extracted ROM:" << m_workingFilePath;
                
                // Update database to point to extracted file location
                // This prevents duplicates on re-scan
                m_db->updateFileOriginalPath(m_currentFileId, extracted);
                
                break;
            }
        }
        qDebug() << "Extraction complete. Directory:" << m_extractedDir;
        onStepComplete(true, QString());
    } else {
        qWarning() << "Extraction failed:" << result.error;
        onStepComplete(false, result.error.isEmpty() ? "Extraction failed" : result.error);
    }
}

void ProcessingController::stepHash()
{
    qDebug() << "Hashing:" << m_workingFilePath;
    
    // Calculate hashes using display names (hasher expects uppercase: "CRC32", "MD5", "SHA1")
    QString crc32 = m_hasher->calculateHash(m_workingFilePath, HashAlgorithms::CRC32_DISPLAY);
    QString md5 = m_hasher->calculateHash(m_workingFilePath, HashAlgorithms::MD5_DISPLAY);
    QString sha1 = m_hasher->calculateHash(m_workingFilePath, HashAlgorithms::SHA1_DISPLAY);
    
    // Update database
    m_db->updateFileHashes(m_currentFileId, crc32, md5, sha1);
    
    qDebug() << "Hashes calculated - CRC32:" << crc32.left(8) 
             << "MD5:" << md5.left(8) << "SHA1:" << sha1.left(8);
    
    // Emit signal for real-time sidebar update
    emit hashCalculated(m_currentFileId, crc32, md5, sha1);
    
    onStepComplete(true, QString());
}

void ProcessingController::stepMatch()
{
    qDebug() << "Matching:" << m_currentFilename;
    
    // Get file record with hashes
    FileRecord file = m_db->getFileById(m_currentFileId);
    
    // Get system name for provider API calls
    QString systemName = SystemResolver::internalName(m_currentSystemId);
    GameMetadata metadata;
    QString matchMethod = MatchMethods::NONE;
    int confidence = 0;
    
    // Try hash-based matching with all available hashes
    // Prefer MD5/SHA1 (widely supported by metadata providers like Hasheous)
    // Then try CRC32 (supported by some No-Intro databases)
    const QString preferredHash = selectBestMatchHash(file);

    if (!preferredHash.isEmpty()) {
        metadata = m_orchestrator->getByHashWithFallback(
            preferredHash, systemName, file.crc32, file.md5, file.sha1);
        if (!metadata.title.isEmpty()) {
            matchMethod = MatchMethods::HASH;
            confidence = 100;
            qDebug() << "Hash match found:" << metadata.title << "(using" << preferredHash.left(8) << "...)";
        }
    }
    
    // Fall back to name-based matching
    if (metadata.title.isEmpty()) {
        const QString cleanName = deriveMatchingDisplayName(file);
        
        if (!cleanName.isEmpty()) {
            metadata = m_orchestrator->searchWithFallback("", cleanName, systemName);
            if (!metadata.title.isEmpty()) {
                matchMethod = MatchMethods::NAME;
                confidence = 70;  // Name match base confidence
                qDebug() << "Name match found:" << metadata.title;
            }
        }
    }
    
    // Store match in database if found
    if (!metadata.title.isEmpty()) {
        // Get release year from metadata
        int releaseYear = 0;
        if (!metadata.releaseDate.isEmpty()) {
            releaseYear = metadata.releaseDate.left(4).toInt();
        }
        
        // Convert genres list to comma-separated string
        QString genresStr = metadata.genres.join(", ");
        
        // Convert players int to string
        QString playersStr = metadata.players > 0 ? QString::number(metadata.players) : QString();
        
        // Create game record with complete metadata
        int gameId = m_db->insertGame(metadata.title, m_currentSystemId, 
                                       metadata.region, metadata.publisher,
                                       metadata.developer, metadata.releaseDate,
                                       metadata.description, genresStr,
                                       playersStr, metadata.rating);
        
        if (gameId > 0) {
            // Create match record
            m_db->insertMatch(m_currentFileId, gameId, confidence, matchMethod);
            qDebug() << "Match stored: gameId=" << gameId << "confidence=" << confidence;

            // Emit signal for real-time sidebar update
            emit matchFound(m_currentFileId, metadata.title, metadata.publisher,
                           releaseYear, confidence, matchMethod);

            // Fetch artwork URLs from provider and cache for the artwork pipeline step
            ArtworkUrls artwork;
            if (!metadata.providerId.isEmpty() && !metadata.id.isEmpty()) {
                try {
                    artwork = m_orchestrator->getArtworkWithFallback(metadata.id, systemName, metadata.providerId);
                } catch (const std::exception &exception) {
                    qWarning() << "Failed to fetch artwork URLs for provider" << metadata.providerId
                               << "game" << metadata.id << "file" << m_currentFilename
                               << ":" << exception.what();
                } catch (...) {
                    qWarning() << "Failed to fetch artwork URLs for provider" << metadata.providerId
                               << "game" << metadata.id << "file" << m_currentFilename
                               << ": unknown exception";
                }
            }

            // Store for stepArtwork — prefer boxFront, fall back to screenshot
            if (!artwork.boxFront.isEmpty()) {
                m_pendingArtworkUrl    = artwork.boxFront;
                m_pendingArtworkGameId = gameId;
            } else if (!artwork.screenshot.isEmpty()) {
                m_pendingArtworkUrl    = artwork.screenshot;
                m_pendingArtworkGameId = gameId;
            } else {
                m_pendingArtworkUrl    = QUrl();
                m_pendingArtworkGameId = -1;
            }

            // Emit detailed metadata for sidebar display
            emit metadataUpdated(m_currentFileId,
                                metadata.description,
                                artwork.boxFront.toString(),
                                artwork.systemLogo.toString(),
                                artwork.screenshot.toString(),
                                artwork.titleScreen.toString(),
                                metadata.rating,
                                metadata.ratingSource);
        }
    }
    
    onStepComplete(true, QString());
}

void ProcessingController::stepMetadata()
{
    qDebug() << "Fetching metadata:" << m_currentFilename;
    
    // Metadata is already fetched during match step via orchestrator
    // This step could be used for additional enrichment if needed
    
    onStepComplete(true, QString());
}

void ProcessingController::stepArtwork()
{
    qDebug() << "Downloading artwork:" << m_currentFilename;

    // Skip if no valid match was found during the match step
    if (m_pendingArtworkGameId <= 0 || m_pendingArtworkUrl.isEmpty()) {
        qDebug() << "No artwork URL available for:" << m_currentFilename << "— skipping";
        onStepComplete(true, QString());
        return;
    }

    // Skip if the caller has not configured an output path
    if (m_artworkBasePath.isEmpty()) {
        qDebug() << "artworkBasePath not set — skipping artwork download";
        onStepComplete(true, QString());
        return;
    }

    // Build destination: <artworkBasePath>/<gameId>/boxfront.<ext>
    const QString destDir = m_artworkBasePath + "/" + QString::number(m_pendingArtworkGameId);
    if (!QDir().mkpath(destDir)) {
        qWarning() << "Failed to create artwork directory:" << destDir;
        onStepComplete(true, QString());  // Non-fatal — continue pipeline
        return;
    }

    const QString urlPath = m_pendingArtworkUrl.path();
    QString ext = QFileInfo(urlPath).suffix();
    if (ext.isEmpty()) ext = "jpg";

    const QString destPath = destDir + "/boxfront." + ext;

    // Avoid redundant downloads
    if (QFile::exists(destPath)) {
        qDebug() << "Artwork already cached:" << destPath;
        emit artworkDownloaded(m_currentFileId, m_pendingArtworkGameId, destPath);
        onStepComplete(true, QString());
        return;
    }

    if (m_artworkDownloader->download(m_pendingArtworkUrl, destPath)) {
        qDebug() << "Artwork downloaded:" << destPath;
        emit artworkDownloaded(m_currentFileId, m_pendingArtworkGameId, destPath);
    } else {
        qWarning() << "Artwork download failed for gameId" << m_pendingArtworkGameId
                   << "URL:" << m_pendingArtworkUrl.toString();
    }

    // Reset pending state for next file
    m_pendingArtworkUrl    = QUrl();
    m_pendingArtworkGameId = -1;

    onStepComplete(true, QString());
}

void ProcessingController::stepConvert()
{
    qDebug() << "Converting to CHD:" << m_currentFilename;
    
    if (!m_convertToChd) {
        onStepComplete(true, QString());
        return;
    }
    
    // Only convert disc-based games
    if (!isDiscBasedSystem(m_currentSystemId)) {
        qDebug() << "System is not disc-based, skipping CHD conversion";
        onStepComplete(true, QString());
        return;
    }
    
    // Check if already CHD
    if (m_workingFilePath.endsWith(".chd", Qt::CaseInsensitive)) {
        qDebug() << "File is already CHD, skipping";
        onStepComplete(true, QString());
        return;
    }
    
    // Check if chdman is available
    if (!m_chdConverter->isChdmanAvailable()) {
        qWarning() << "chdman not available, skipping CHD conversion";
        onStepComplete(true, QString());
        return;
    }
    
    // Determine output path
    QFileInfo fi(m_workingFilePath);
    QString ext = fi.suffix().toLower();
    QString chdPath = fi.path() + "/" + fi.completeBaseName() + ".chd";
    
    // Convert based on input type
    CHDConversionResult result;
    if (ext == "cue") {
        result = m_chdConverter->convertCueToCHD(m_workingFilePath, chdPath);
    } else if (ext == "iso") {
        result = m_chdConverter->convertIsoToCHD(m_workingFilePath, chdPath);
    } else if (ext == "gdi") {
        result = m_chdConverter->convertGdiToCHD(m_workingFilePath, chdPath);
    } else {
        qDebug() << "Unsupported format for CHD conversion:" << ext;
        onStepComplete(true, QString());
        return;
    }
    
    if (result.success) {
        qDebug() << "CHD conversion successful:" << chdPath 
                 << "Ratio:" << result.compressionRatio;
        m_db->updateFilePath(m_currentFileId, chdPath);
    } else {
        qWarning() << "CHD conversion failed:" << result.error;
    }
    
    onStepComplete(true, QString());
}

// === Helper Methods ===

bool ProcessingController::isDiscBasedSystem(int systemId) const
{
    // Use Constants::Systems to determine if system uses multi-file/disc-based media
    const Constants::Systems::SystemDef* systemDef = Constants::Systems::getSystem(systemId);
    if (systemDef) {
        return systemDef->isMultiFile;
    }
    return false;
}

bool ProcessingController::isArchiveFile(const QString &path) const
{
    QString ext = QFileInfo(path).suffix().toLower();
    return ext == "zip" || ext == "7z" || ext == "rar" || ext == "gz";
}

QString ProcessingController::getSystemPreferredHash(int systemId) const
{
    // Use Constants::Systems to get the preferred hash algorithm
    const Constants::Systems::SystemDef* systemDef = Constants::Systems::getSystem(systemId);
    if (systemDef) {
        return systemDef->preferredHash.toLower();
    }
    return "crc32";  // Fallback
}

void ProcessingController::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

QString ProcessingController::getSystemNameForId(int systemId) const
{
    // Use SystemResolver for consistent name mapping
    return SystemResolver::internalName(systemId);
}

void ProcessingController::createMarkerFile(int fileId)
{
    Q_UNUSED(fileId);
    
    // Use the extracted directory path (set during stepExtract)
    if (m_extractedDir.isEmpty()) {
        qWarning() << "No extracted directory set for marker file creation";
        return;
    }
    
    QString markerPath = m_extractedDir + "/" + Constants::Settings::Files::MARKER_PROCESSED;
    
    // Only create if it doesn't exist
    if (QFile::exists(markerPath)) {
        return;
    }
    
    QFile markerFile(markerPath);
    if (markerFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&markerFile);
        out << "# Remus Processed Marker\n";
        out << "# Created: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        out << "# This file indicates the directory has been processed by Remus\n";
        markerFile.close();
        qDebug() << "Created marker file:" << markerPath;
    } else {
        qWarning() << "Failed to create marker file:" << markerPath;
    }
}

bool ProcessingController::hasMarkerFile(const QString &directoryPath)
{
    QString markerPath = directoryPath + "/" + Constants::Settings::Files::MARKER_PROCESSED;
    return QFile::exists(markerPath);
}

void ProcessingController::moveArchiveToOriginals(const QString &archivePath)
{
    QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists()) {
        qWarning() << "Archive file not found for move:" << archivePath;
        return;
    }
    
    // Create Originals folder in the same directory as the archive
    QString originalsDir = archiveInfo.absolutePath() + "/Originals";
    QDir dir;
    if (!dir.mkpath(originalsDir)) {
        qWarning() << "Failed to create Originals directory:" << originalsDir;
        return;
    }
    
    // Create marker file so scanner will skip this folder
    QString markerPath = originalsDir + "/" + Constants::Settings::Files::MARKER_SKIP_SCAN;
    if (!QFile::exists(markerPath)) {
        QFile markerFile(markerPath);
        if (markerFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&markerFile);
            out << "# Remus Directory Marker\n";
            out << "# Created: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
            out << "# This directory contains original archive files and should be excluded from scanning\n";
            markerFile.close();
            qDebug() << "Created .remusdir marker in Originals folder";
        }
    }
    
    // Move the archive to Originals folder
    QString destPath = originalsDir + "/" + archiveInfo.fileName();
    
    // Handle case where file already exists in Originals
    if (QFile::exists(destPath)) {
        qDebug() << "Archive already exists in Originals, removing duplicate";
        QFile::remove(archivePath);
    } else {
        if (QFile::rename(archivePath, destPath)) {
            qDebug() << "Moved archive to Originals:" << destPath;
        } else {
            qWarning() << "Failed to move archive to Originals:" << archivePath;
        }
    }
}

} // namespace Remus
