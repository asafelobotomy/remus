#include "cli_helpers.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include "../core/archive_extractor.h"
#include "../core/constants/files.h"
#include "../core/system_resolver.h"
#include "../metadata/filename_normalizer.h"
#include "../metadata/local_database_provider.h"
#include "../metadata/screenscraper_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/gametdb_provider.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

namespace {

QSettings remusSettings()
{
    return QSettings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                     QString::fromLatin1(Constants::SETTINGS_APPLICATION));
}

QString parserOrSetting(const QCommandLineParser &parser,
                        const QString &optionName,
                        const char *settingKey)
{
    if (parser.isSet(optionName)) {
        return parser.value(optionName).trimmed();
    }

    QSettings settings = remusSettings();
    return settings.value(QString::fromLatin1(settingKey)).toString().trimmed();
}

}

/**
 * @brief Select the best hash for matching based on system's preferred algorithm.
 *
 * Disc-based systems (PlayStation, Saturn, etc.) prefer MD5/SHA1.
 * Cartridge-based systems (NES, SNES, GBA, etc.) prefer CRC32.
 */
QString selectBestHash(const FileRecord &file)
{
    return selectBestMatchHash(file);
}

static bool isArchivePath(const QString &path)
{
    const QString lower = path.toLower();
    for (const QString &extension : Files::ARCHIVE_EXTENSIONS) {
        if (lower.endsWith(extension)) {
            return true;
        }
    }
    return false;
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

QString findDatabaseDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QStringList candidates = {
        cwd + "/data/databases",
        appDir + "/data/databases",
        appDir + "/../data/databases",
        appDir + "/../../data/databases",
        appDir + "/../../../data/databases",
        cwd + "/../data/databases",
        cwd + "/../../data/databases"
    };
    for (const QString &dir : candidates) {
        if (QDir(dir).exists()) {
            return QDir::cleanPath(dir);
        }
    }
    return QString();
}

QString findMetadataDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QStringList candidates = {
        cwd + "/data/metadata",
        appDir + "/data/metadata",
        appDir + "/../data/metadata",
        appDir + "/../../data/metadata",
        appDir + "/../../../data/metadata",
        cwd + "/../data/metadata",
        cwd + "/../../data/metadata"
    };
    for (const QString &dir : candidates) {
        if (QDir(dir).exists()) {
            return QDir::cleanPath(dir);
        }
    }
    return QString();
}

QString findGameTDBDir()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QStringList candidates = {
        cwd + "/data/gametdb",
        appDir + "/data/gametdb",
        appDir + "/../data/gametdb",
        appDir + "/../../data/gametdb",
        appDir + "/../../../data/gametdb",
        cwd + "/../data/gametdb",
        cwd + "/../../data/gametdb"
    };
    for (const QString &dir : candidates) {
        if (QDir(dir).exists()) {
            return QDir::cleanPath(dir);
        }
    }
    return QString();
}

std::unique_ptr<ProviderOrchestrator> buildOrchestrator(const QCommandLineParser &parser)
{
    auto orchestrator = std::make_unique<ProviderOrchestrator>();

    // Local DAT database — offline, no auth, highest priority
    const QString dbDir = findDatabaseDir();
    if (!dbDir.isEmpty()) {
        auto localDbProvider = new LocalDatabaseProvider();
        int loaded = localDbProvider->loadDatabases(dbDir);
        if (loaded > 0) {
            // Load enrichment metadata (genre, developer, publisher, etc.)
            const QString metaDir = findMetadataDir();
            if (!metaDir.isEmpty()) {
                localDbProvider->loadMetadata(metaDir);
            }
            const auto localInfo = Providers::getProviderInfo(Providers::LOCAL_DATABASE);
            orchestrator->addProvider(Providers::LOCAL_DATABASE, localDbProvider,
                                      localInfo ? localInfo->priority : 110);
        } else {
            delete localDbProvider;
        }
    }

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

    // GameTDB — offline XML databases for Nintendo/PS3, no auth
    const QString gametdbDir = findGameTDBDir();
    if (!gametdbDir.isEmpty()) {
        auto gametdbProvider = new GameTDBProvider();
        int gametdbLoaded = gametdbProvider->loadDatabases(gametdbDir);
        if (gametdbLoaded > 0) {
            const auto gametdbInfo = Providers::getProviderInfo(Providers::GAMETDB);
            orchestrator->addProvider(Providers::GAMETDB, gametdbProvider,
                                      gametdbInfo ? gametdbInfo->priority : 60);
        } else {
            delete gametdbProvider;
        }
    }

    auto tgdbProvider = new TheGamesDBProvider();
    const QString tgdbApiKey = parserOrSetting(parser,
                                               QStringLiteral("tgdb-api-key"),
                                               Settings::Providers::THEGAMESDB_API_KEY);
    if (!tgdbApiKey.isEmpty()) {
        tgdbProvider->setApiKey(tgdbApiKey);
    }
    const auto tgdbInfo = Providers::getProviderInfo(Providers::THEGAMESDB);
    orchestrator->addProvider(Providers::THEGAMESDB, tgdbProvider,
                              tgdbInfo ? tgdbInfo->priority : 50);

    const QString igdbClientId = parserOrSetting(parser,
                                                 QStringLiteral("igdb-client-id"),
                                                 Settings::Providers::IGDB_CLIENT_ID);
    const QString igdbClientSecret = parserOrSetting(parser,
                                                     QStringLiteral("igdb-client-secret"),
                                                     Settings::Providers::IGDB_CLIENT_SECRET);

    if (!igdbClientId.isEmpty() && !igdbClientSecret.isEmpty()) {
        auto igdbProvider = new IGDBProvider();
        igdbProvider->setCredentials(igdbClientId, igdbClientSecret);
        const auto igdbInfo = Providers::getProviderInfo(Providers::IGDB);
        orchestrator->addProvider(Providers::IGDB, igdbProvider,
                                  igdbInfo ? igdbInfo->priority : 40);
    }

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

QString getMatchingDisplayName(const FileRecord &file)
{
    return Remus::deriveMatchingDisplayName(file);
}

QString getMatchingSystemName(const FileRecord &file)
{
    if (file.systemId <= 0) {
        return QString();
    }

    const QString systemName = SystemResolver::internalName(file.systemId);
    return systemName == QStringLiteral("Unknown") ? QString() : systemName;
}

int persistMetadata(Database &db, const FileRecord &file, const GameMetadata &metadata)
{
    int systemId = db.getSystemId(metadata.system);
    if (systemId == 0) systemId = file.systemId;

    const QString region = metadata.region.isEmpty()
        ? Metadata::FilenameNormalizer::extractRegion(file.filename)
        : metadata.region;
    const QString genres = metadata.genres.join(", ");
    const QString players = metadata.players > 0 ? QString::number(metadata.players) : QString();
    int gameId = db.insertGame(metadata.title, systemId, region, metadata.publisher,
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
    if (file.isCompressed) {
        qInfo() << "Container Path:" << file.currentPath;
        qInfo() << "Archive Path:" << (file.archivePath.isEmpty() ? file.currentPath : file.archivePath);
        qInfo() << "Archive Entry:" << (file.archiveInternalPath.isEmpty() ? file.filename : file.archiveInternalPath);
        qInfo() << "Container Filename:" << QFileInfo(file.currentPath).fileName();
        qInfo() << "Entry Filename:" << file.filename;
        qInfo() << "Entry Extension:" << file.extension;
    } else {
        qInfo() << "Path:" << file.currentPath;
        qInfo() << "Filename:" << file.filename;
        qInfo() << "Extension:" << file.extension;
    }
    qInfo() << "Original Path:" << file.originalPath;
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
    qInfo() << "File Type:" << file.fileType;
    qInfo() << "Patched:" << file.isPatched;
    qInfo() << "Patch Name:" << file.patchName;
    qInfo() << "Processed:" << file.isProcessed << "Status:" << file.processingStatus;
}
