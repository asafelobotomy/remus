#include "cli_helpers.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include "../core/archive_extractor.h"
#include "../core/constants/files.h"
#include "../core/space_calculator.h"
#include "../core/system_resolver.h"
#include "../metadata/filename_normalizer.h"
#include "../metadata/local_database_provider.h"
#include "../metadata/metadata_cache.h"
#include "../metadata/screenscraper_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/gametdb_provider.h"
#include "../metadata/retroachievements_provider.h"
#include "../metadata/wikidata_provider.h"
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

QString findDataSubdir(const QString &subdir)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QString seg = QStringLiteral("data/") + subdir;
    const QStringList candidates = {
        cwd + "/" + seg,
        appDir + "/" + seg,
        appDir + "/../" + seg,
        appDir + "/../../" + seg,
        appDir + "/../../../" + seg,
        cwd + "/../" + seg,
        cwd + "/../../" + seg
    };
    for (const QString &dir : candidates) {
        if (QDir(dir).exists()) {
            return QDir::cleanPath(dir);
        }
    }
    return QString();
}

std::unique_ptr<ProviderOrchestrator> buildOrchestrator(const QCommandLineParser &parser,
                                                         Database *db)
{
    auto orchestrator = std::make_unique<ProviderOrchestrator>();

    // Wire metadata cache if a database is available
    if (db) {
        auto *cache = new MetadataCache(db->database(), orchestrator.get());
        orchestrator->setCache(cache);
    }

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
                                      localInfo ? localInfo->priority : 200);
        } else {
            delete localDbProvider;
        }
    }

    auto hasheousProvider = new HasheousProvider();
    const auto hasheousInfo = Providers::getProviderInfo(Providers::HASHEOUS);
    orchestrator->addProvider(Providers::HASHEOUS, hasheousProvider,
                              hasheousInfo ? hasheousInfo->priority : 80);

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
                                      gametdbInfo ? gametdbInfo->priority : 150);
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
                                  igdbInfo ? igdbInfo->priority : 70);
    }

    // RetroAchievements — hash-based, free API key required
    const QString raUsername = parserOrSetting(parser,
                                              QStringLiteral("ra-user"),
                                              Settings::Providers::RETROACHIEVEMENTS_USERNAME);
    const QString raApiKey = parserOrSetting(parser,
                                            QStringLiteral("ra-api-key"),
                                            Settings::Providers::RETROACHIEVEMENTS_API_KEY);
    if (!raUsername.isEmpty() && !raApiKey.isEmpty()) {
        auto raProvider = new RetroAchievementsProvider();
        raProvider->setCredentials(raUsername, raApiKey);
        const auto raInfo = Providers::getProviderInfo(Providers::RETROACHIEVEMENTS);
        orchestrator->addProvider(Providers::RETROACHIEVEMENTS, raProvider,
                                  raInfo ? raInfo->priority : 60);
    }

    // Wikidata — SPARQL, no auth, CC0 licensed, lowest priority
    auto wikidataProvider = new WikidataProvider();
    const auto wdInfo = Providers::getProviderInfo(Providers::WIKIDATA);
    orchestrator->addProvider(Providers::WIKIDATA, wikidataProvider,
                              wdInfo ? wdInfo->priority : 40);

    return orchestrator;
}

QString resolveCliOptionValue(const QCommandLineParser &parser,
                              const QString &optionName,
                              const QString &presetValue)
{
    if (parser.isSet(optionName)) {
        return parser.value(optionName).trimmed();
    }

    if (!presetValue.trimmed().isEmpty()) {
        return presetValue.trimmed();
    }

    return parser.value(optionName).trimmed();
}

QList<FileRecord> getHashedFiles(Database &db)
{
    return getHashedFiles(db, {});
}

QList<FileRecord> getHashedFiles(Database &db, const QSet<int> &fileScopeIds)
{
    const QList<FileRecord> files = db.getExistingFiles();
    QList<FileRecord> filtered;
    for (const FileRecord &f : files) {
        if (!fileMatchesProcessScope(f, fileScopeIds)) {
            continue;
        }
        if (f.hashCalculated && (!f.crc32.isEmpty() || !f.md5.isEmpty() || !f.sha1.isEmpty()))
            filtered.append(f);
    }
    return filtered;
}

bool fileMatchesProcessScope(const FileRecord &file, const QSet<int> &fileScopeIds)
{
    return fileScopeIds.isEmpty() || fileScopeIds.contains(file.id);
}

int resolveMatchedSystemId(const FileRecord &file,
                           const Database::MatchResult *match)
{
    if (match && match->systemId > 0) {
        return match->systemId;
    }

    return file.systemId;
}

bool fileMatchesSystemFilter(const FileRecord &file,
                             int systemId,
                             const Database::MatchResult *match)
{
    if (systemId < 0) {
        return true;
    }

    return resolveMatchedSystemId(file, match) == systemId;
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

QString buildOutputPath(const QString &inputPath, const QString &outputDir, const QString &targetExt)
{
    QFileInfo info(inputPath);
    const QString filename = info.completeBaseName() + "." + targetExt;
    if (outputDir.isEmpty()) {
        return QDir(info.absolutePath()).filePath(filename);
    }
    QDir().mkpath(outputDir);
    return QDir(outputDir).filePath(filename);
}

bool printConversionResult(const ConversionResult &result, const QString &formatName)
{
    if (result.success) {
        qInfo() << "✓ Conversion successful!";
        qInfo().noquote() << "  Original size:" << SpaceCalculator::formatBytes(result.inputSize);
        qInfo().noquote() << "  " + formatName + " size:" << SpaceCalculator::formatBytes(result.outputSize);
        qInfo().noquote() << "  Saved:" << SpaceCalculator::formatBytes(result.inputSize - result.outputSize);
        qInfo() << "  Compression:"
                << QString::number((1.0 - result.compressionRatio) * 100, 'f', 1) << "%";
        return true;
    }
    qCritical() << "✗ Conversion failed:" << result.error;
    return false;
}
