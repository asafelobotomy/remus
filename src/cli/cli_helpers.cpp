#include "cli_helpers.h"
#include <QFileInfo>
#include <QTemporaryDir>
#include "../core/archive_extractor.h"
#include "../metadata/screenscraper_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/hasheous_provider.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

/**
 * @brief Select the best hash for matching based on system's preferred algorithm.
 *
 * Disc-based systems (PlayStation, Saturn, etc.) prefer MD5/SHA1.
 * Cartridge-based systems (NES, SNES, GBA, etc.) prefer CRC32.
 */
QString selectBestHash(const FileRecord &file)
{
    if (Systems::SYSTEMS.contains(file.systemId)) {
        const Systems::SystemDef &sysDef = Systems::SYSTEMS[file.systemId];
        const QString preferred = sysDef.preferredHash.toLower();

        if (preferred == "md5" && !file.md5.isEmpty()) return file.md5;
        if (preferred == "sha1" && !file.sha1.isEmpty()) return file.sha1;
        if (preferred == "crc32" && !file.crc32.isEmpty()) return file.crc32;
    }

    // Fallback: CRC32 → SHA1 → MD5
    if (!file.crc32.isEmpty()) return file.crc32;
    if (!file.sha1.isEmpty()) return file.sha1;
    if (!file.md5.isEmpty()) return file.md5;
    return QString();
}

static bool isArchivePath(const QString &path)
{
    const QString lower = path.toLower();
    return lower.endsWith(".zip") || lower.endsWith(".7z") || lower.endsWith(".rar") ||
           lower.endsWith(".tar") || lower.endsWith(".tar.gz") || lower.endsWith(".tgz") ||
           lower.endsWith(".tar.bz2") || lower.endsWith(".tbz2");
}

HashResult hashFileRecord(const FileRecord &file, Hasher &hasher)
{
    const QString archivePath = file.archivePath.isEmpty() ? file.currentPath : file.archivePath;
    const bool treatAsArchive = file.isCompressed || isArchivePath(archivePath);

    if (!treatAsArchive) {
        int headerSize = Hasher::detectHeaderSize(file.currentPath, file.extension);
        return hasher.calculateHashes(file.currentPath, headerSize > 0, headerSize);
    }

    HashResult result;
    if (!QFileInfo::exists(archivePath)) {
        result.error = "Archive file not found";
        return result;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        result.error = "Failed to create temporary directory";
        return result;
    }

    ArchiveExtractor extractor;
    const QString internalPath = file.archiveInternalPath.isEmpty() ? file.filename : file.archiveInternalPath;
    ExtractionResult extraction = extractor.extractFile(archivePath, internalPath, tempDir.path());
    if (!extraction.success || extraction.extractedFiles.isEmpty()) {
        extraction = extractor.extract(archivePath, tempDir.path(), false);
        if (!extraction.success || extraction.extractedFiles.isEmpty()) {
            result.error = extraction.error.isEmpty()
                ? QString("Failed to extract %1 from archive").arg(internalPath)
                : extraction.error;
            return result;
        }

        QString picked;
        for (const QString &path : extraction.extractedFiles) {
            if (path.endsWith(file.extension, Qt::CaseInsensitive)) { picked = path; break; }
        }
        if (picked.isEmpty()) picked = extraction.extractedFiles.first();
        int headerSize = Hasher::detectHeaderSize(picked, file.extension);
        return hasher.calculateHashes(picked, headerSize > 0, headerSize);
    }

    const QString extractedPath = extraction.extractedFiles.first();
    int headerSize = Hasher::detectHeaderSize(extractedPath, file.extension);
    return hasher.calculateHashes(extractedPath, headerSize > 0, headerSize);
}

std::unique_ptr<ProviderOrchestrator> buildOrchestrator(const QCommandLineParser &parser)
{
    auto orchestrator = std::make_unique<ProviderOrchestrator>();

    auto hasheousProvider = new HasheousProvider();
    const auto hasheousInfo = Providers::getProviderInfo(Providers::HASHEOUS);
    orchestrator->addProvider(Providers::HASHEOUS, hasheousProvider,
                              hasheousInfo ? hasheousInfo->priority : 100);

    if (parser.isSet("ss-user") && parser.isSet("ss-pass")) {
        auto ssProvider = new ScreenScraperProvider();
        ssProvider->setCredentials(parser.value("ss-user"), parser.value("ss-pass"));
        if (parser.isSet("ss-devid") && parser.isSet("ss-devpass"))
            ssProvider->setDeveloperCredentials(parser.value("ss-devid"), parser.value("ss-devpass"));
        const auto ssInfo = Providers::getProviderInfo(Providers::SCREENSCRAPER);
        orchestrator->addProvider(Providers::SCREENSCRAPER, ssProvider,
                                  ssInfo ? ssInfo->priority : 90);
    }

    auto tgdbProvider = new TheGamesDBProvider();
    const auto tgdbInfo = Providers::getProviderInfo(Providers::THEGAMESDB);
    orchestrator->addProvider(Providers::THEGAMESDB, tgdbProvider,
                              tgdbInfo ? tgdbInfo->priority : 50);

    auto igdbProvider = new IGDBProvider();
    const auto igdbInfo = Providers::getProviderInfo(Providers::IGDB);
    orchestrator->addProvider(Providers::IGDB, igdbProvider,
                              igdbInfo ? igdbInfo->priority : 40);

    return orchestrator;
}

QList<FileRecord> getHashedFiles(Database &db)
{
    const QList<FileRecord> files = db.getExistingFiles();
    QList<FileRecord> filtered;
    for (const FileRecord &f : files) {
        if (f.hashCalculated && (!f.crc32.isEmpty() || !f.md5.isEmpty() || !f.sha1.isEmpty()))
            filtered.append(f);
    }
    return filtered;
}

int persistMetadata(Database &db, const FileRecord &file, const GameMetadata &metadata)
{
    int systemId = db.getSystemId(metadata.system);
    if (systemId == 0) systemId = file.systemId;

    const QString genres = metadata.genres.join(", ");
    const QString players = metadata.players > 0 ? QString::number(metadata.players) : QString();
    int gameId = db.insertGame(metadata.title, systemId, metadata.region, metadata.publisher,
                               metadata.developer, metadata.releaseDate, metadata.description,
                               genres, players, metadata.rating);
    if (gameId == 0) return 0;

    const int confidence = metadata.matchScore > 0 ? static_cast<int>(metadata.matchScore * 100) : 0;
    const QString method = metadata.matchMethod.isEmpty() ? QStringLiteral("auto") : metadata.matchMethod;
    db.insertMatch(file.id, gameId, confidence, method);
    return gameId;
}

void printFileInfo(const FileRecord &file)
{
    qInfo() << "File ID:" << file.id;
    qInfo() << "Library ID:" << file.libraryId;
    qInfo() << "Path:" << file.currentPath;
    qInfo() << "Original Path:" << file.originalPath;
    qInfo() << "Filename:" << file.filename;
    qInfo() << "Extension:" << file.extension;
    qInfo() << "Size:" << file.fileSize;
    qInfo() << "System ID:" << file.systemId;
    qInfo() << "Hash calculated:" << file.hashCalculated;
    if (file.hashCalculated) {
        qInfo() << "CRC32:" << file.crc32;
        qInfo() << "MD5:" << file.md5;
        qInfo() << "SHA1:" << file.sha1;
    }
    qInfo() << "Primary:" << file.isPrimary;
    qInfo() << "Parent ID:" << file.parentFileId;
    qInfo() << "Processed:" << file.isProcessed << "Status:" << file.processingStatus;
}
