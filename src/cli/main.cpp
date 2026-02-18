#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <memory>
#include "../core/scanner.h"
#include "../core/system_detector.h"
#include "../core/hasher.h"
#include "../core/database.h"
#include "../core/matching_engine.h"
#include "../core/template_engine.h"
#include "../core/organize_engine.h"
#include "../core/m3u_generator.h"
#include "../core/verification_engine.h"
#include "../metadata/screenscraper_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/provider_orchestrator.h"
#include "../metadata/metadata_cache.h"
#include "../metadata/artwork_downloader.h"
#include "../metadata/local_database_provider.h"
#include "../core/chd_converter.h"
#include "../core/archive_extractor.h"
#include "../core/space_calculator.h"
#include "../core/header_detector.h"
#include "../core/patch_engine.h"
#include "../core/constants/constants.h"
#include "../core/logging_categories.h"
#include "terminal_image.h"
#include "interactive_session.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logCli)
#define qInfo() qCInfo(logCli)
#define qWarning() qCWarning(logCli)
#define qCritical() qCCritical(logCli)

using namespace Remus;
using namespace Remus::Constants;
using namespace Remus::Cli;

static bool hasFlag(const QStringList &args, const QString &flag)
{
    return args.contains(flag);
}

static bool hasAnyAction(const QStringList &args)
{
    const QStringList actionFlags = {
        "--scan", "-s", "--hash", "--hash-all", "--list", "--stats", "--info",
        "--header-info", "--show-art", "--metadata", "--search", "--match",
        "--match-report", "--verify", "--verify-report", "--process", "--organize",
        "--download-artwork", "--generate-m3u", "--convert-chd", "--chd-extract",
        "--chd-verify", "--chd-info", "--extract-archive", "--space-report",
        "--export", "--patch-apply", "--patch-create", "--patch-info",
        "--patch-tools", "--checksum-verify"
    };

    for (const QString &arg : args) {
        if (actionFlags.contains(arg)) {
            return true;
        }
    }
    return false;
}

static bool isArchivePath(const QString &path)
{
    const QString lower = path.toLower();
    return lower.endsWith(".zip") || lower.endsWith(".7z") || lower.endsWith(".rar") ||
           lower.endsWith(".tar") || lower.endsWith(".tar.gz") || lower.endsWith(".tgz") ||
           lower.endsWith(".tar.bz2") || lower.endsWith(".tbz2");
}

/**
 * @brief Select the best hash for matching based on system's preferred algorithm
 * @param file The file record with multiple hash values
 * @return The preferred hash string (CRC32, MD5, or SHA1)
 * 
 * Disc-based systems (PlayStation, Saturn, etc.) prefer MD5/SHA1.
 * Cartridge-based systems (NES, SNES, GBA, etc.) prefer CRC32.
 */
static QString selectBestHash(const FileRecord &file)
{
    // Get system info to determine preferred hash algorithm
    if (Systems::SYSTEMS.contains(file.systemId)) {
        const Systems::SystemDef &sysDef = Systems::SYSTEMS[file.systemId];
        const QString preferred = sysDef.preferredHash.toLower();
        
        // Return the preferred hash if available
        if (preferred == "md5" && !file.md5.isEmpty()) return file.md5;
        if (preferred == "sha1" && !file.sha1.isEmpty()) return file.sha1;
        if (preferred == "crc32" && !file.crc32.isEmpty()) return file.crc32;
    }
    
    // Fallback: CRC32 → SHA1 → MD5 (original behavior)
    if (!file.crc32.isEmpty()) return file.crc32;
    if (!file.sha1.isEmpty()) return file.sha1;
    if (!file.md5.isEmpty()) return file.md5;
    
    return QString();
}

static HashResult hashFileRecord(const FileRecord &file, Hasher &hasher)
{
    const QString archivePath = file.archivePath.isEmpty() ? file.currentPath : file.archivePath;
    const bool treatAsArchive = file.isCompressed || isArchivePath(archivePath);

    if (!treatAsArchive) {
        int headerSize = Hasher::detectHeaderSize(file.currentPath, file.extension);
        bool stripHeader = (headerSize > 0);
        return hasher.calculateHashes(file.currentPath, stripHeader, headerSize);
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
        // Fallback: extract entire archive and choose a file matching the extension
        extraction = extractor.extract(archivePath, tempDir.path(), false);
        if (!extraction.success || extraction.extractedFiles.isEmpty()) {
            result.error = extraction.error.isEmpty()
                ? QString("Failed to extract %1 from archive").arg(internalPath)
                : extraction.error;
            return result;
        }

        QString picked;
        for (const QString &path : extraction.extractedFiles) {
            if (path.endsWith(file.extension, Qt::CaseInsensitive)) {
                picked = path;
                break;
            }
        }
        if (picked.isEmpty()) {
            picked = extraction.extractedFiles.first();
        }
        const QString extractedPath = picked;
        int headerSize = Hasher::detectHeaderSize(extractedPath, file.extension);
        bool stripHeader = (headerSize > 0);
        return hasher.calculateHashes(extractedPath, stripHeader, headerSize);
    }

    const QString extractedPath = extraction.extractedFiles.first();
    int headerSize = Hasher::detectHeaderSize(extractedPath, file.extension);
    bool stripHeader = (headerSize > 0);
    return hasher.calculateHashes(extractedPath, stripHeader, headerSize);
}

static std::unique_ptr<ProviderOrchestrator> buildOrchestrator(const QCommandLineParser &parser)
{
    auto orchestrator = std::make_unique<ProviderOrchestrator>();

    // Hasheous (hash-first)
    auto hasheousProvider = new HasheousProvider();
    const auto hasheousInfo = Providers::getProviderInfo(Providers::HASHEOUS);
    const int hasheousPriority = hasheousInfo ? hasheousInfo->priority : 100;
    orchestrator->addProvider(Providers::HASHEOUS, hasheousProvider, hasheousPriority);

    // ScreenScraper (requires creds)
    if (parser.isSet("ss-user") && parser.isSet("ss-pass")) {
        auto ssProvider = new ScreenScraperProvider();
        ssProvider->setCredentials(parser.value("ss-user"), parser.value("ss-pass"));
        if (parser.isSet("ss-devid") && parser.isSet("ss-devpass")) {
            ssProvider->setDeveloperCredentials(parser.value("ss-devid"), parser.value("ss-devpass"));
        }
        const auto ssInfo = Providers::getProviderInfo(Providers::SCREENSCRAPER);
        const int ssPriority = ssInfo ? ssInfo->priority : 90;
        orchestrator->addProvider(Providers::SCREENSCRAPER, ssProvider, ssPriority);
    }

    // TheGamesDB (name fallback)
    auto tgdbProvider = new TheGamesDBProvider();
    const auto tgdbInfo = Providers::getProviderInfo(Providers::THEGAMESDB);
    const int tgdbPriority = tgdbInfo ? tgdbInfo->priority : 50;
    orchestrator->addProvider(Providers::THEGAMESDB, tgdbProvider, tgdbPriority);

    // IGDB (rich metadata)
    auto igdbProvider = new IGDBProvider();
    const auto igdbInfo = Providers::getProviderInfo(Providers::IGDB);
    const int igdbPriority = igdbInfo ? igdbInfo->priority : 40;
    orchestrator->addProvider(Providers::IGDB, igdbProvider, igdbPriority);

    return orchestrator;
}

static QList<FileRecord> getHashedFiles(Database &db)
{
    QList<FileRecord> files = db.getExistingFiles();
    QList<FileRecord> filtered;
    for (const FileRecord &f : files) {
        if (f.hashCalculated && (!f.crc32.isEmpty() || !f.md5.isEmpty() || !f.sha1.isEmpty())) {
            filtered.append(f);
        }
    }
    return filtered;
}

static int persistMetadata(Database &db, const FileRecord &file, const GameMetadata &metadata)
{
    int systemId = db.getSystemId(metadata.system);
    if (systemId == 0) {
        systemId = file.systemId; // fallback to detected system
    }

    const QString genres = metadata.genres.join(", ");
    const QString players = metadata.players > 0 ? QString::number(metadata.players) : QString();
    int gameId = db.insertGame(metadata.title, systemId, metadata.region, metadata.publisher,
                               metadata.developer, metadata.releaseDate, metadata.description,
                               genres, players, metadata.rating);

    if (gameId == 0) {
        return 0;
    }

    const int confidence = metadata.matchScore > 0 ? static_cast<int>(metadata.matchScore * 100) : 0;
    const QString method = metadata.matchMethod.isEmpty() ? QStringLiteral("auto") : metadata.matchMethod;
    db.insertMatch(file.id, gameId, confidence, method);
    return gameId;
}

static void printFileInfo(const FileRecord &file)
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

void printBanner()
{
    qInfo() << "╔════════════════════════════════════════╗";
    qInfo() << "║  Remus - Retro Game Library Manager   ║";
    qInfo() << "║  M4.5: File Conversion & Compression  ║";
    qInfo() << "╚════════════════════════════════════════╝";
    qInfo() << "";
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("remus-cli");
    QCoreApplication::setOrganizationName("Remus");
    QCoreApplication::setApplicationVersion(Constants::APP_VERSION);

    printBanner();

    QStringList activeArgs = app.arguments();
    const bool interactiveFlag = hasFlag(activeArgs, "--interactive");
    const bool noInteractiveFlag = hasFlag(activeArgs, "--no-interactive");
    const bool actionsProvided = hasAnyAction(activeArgs);

    if (interactiveFlag || (!noInteractiveFlag && !actionsProvided)) {
        InteractiveSession session;
        InteractiveResult selection = session.run();
        if (!selection.valid || selection.args.isEmpty()) {
            return 0;
        }
        activeArgs = selection.args;
    }

    QCommandLineParser parser;
    parser.setApplicationDescription("Remus CLI - Scan and catalog retro game ROMs");
    parser.addHelpOption();
    parser.addVersionOption();

    // Add options
    parser.addOption({{"s", "scan"}, "Scan a directory for ROMs", "path"});
    parser.addOption({{"d", "db"}, "Database file path", "database", Constants::DATABASE_FILENAME});
    parser.addOption(QCommandLineOption("hash", "Calculate hashes for scanned files"));
    parser.addOption(QCommandLineOption("hash-all", "Calculate hashes for all files in database that lack hashes"));
    parser.addOption({{"l", "list"}, "List scanned files by system"});
    parser.addOption(QCommandLineOption("stats", "Show library statistics"));
    parser.addOption(QCommandLineOption("info", "Show detailed info for a file id", "fileId"));
    parser.addOption(QCommandLineOption("header-info", "Inspect ROM header for a file", "file"));
    parser.addOption(QCommandLineOption("show-art", "Display an image in terminal (path to image)", "image"));
    
    // Metadata options
    parser.addOption({{"m", "metadata"}, "Fetch metadata by file hash", "hash"});
    parser.addOption(QCommandLineOption("search", "Search for game by name", "title"));
    parser.addOption(QCommandLineOption("system", "Specify system for search", "system"));
    const QString providerHelp = QString("Metadata provider (%1, %2, %3, auto)")
        .arg(Providers::SCREENSCRAPER)
        .arg(Providers::THEGAMESDB)
        .arg(Providers::IGDB);
    parser.addOption(QCommandLineOption("provider", providerHelp, "provider", "auto"));
    parser.addOption(QCommandLineOption("ss-user", "ScreenScraper username", "username"));
    parser.addOption(QCommandLineOption("ss-pass", "ScreenScraper password", "password"));
    parser.addOption(QCommandLineOption("ss-devid", "ScreenScraper dev ID", "devid"));
    parser.addOption(QCommandLineOption("ss-devpass", "ScreenScraper dev password", "devpassword"));
    
    // M3 Matching options
    parser.addOption(QCommandLineOption("match", "Match scanned files with metadata (M3 intelligent matching)"));
    parser.addOption(QCommandLineOption("min-confidence", "Minimum confidence threshold for matches (0-100)", "confidence", "60"));
    parser.addOption(QCommandLineOption("match-report", "Generate detailed matching report with confidence scores"));
    parser.addOption(QCommandLineOption("report-file", "Output file for reports (default: stdout)", "file"));

    // Verification options
    parser.addOption(QCommandLineOption("verify", "Verify files against DAT file", "dat-file"));
    parser.addOption(QCommandLineOption("verify-report", "Generate detailed verification report"));

    // Cover art & artwork options
    parser.addOption(QCommandLineOption("download-artwork", "Download cover art for matched games"));
    parser.addOption(QCommandLineOption("artwork-dir", "Directory to store artwork (default: ~/.local/share/Remus/artwork/)", "directory"));
    parser.addOption(QCommandLineOption("artwork-types", "Types of artwork to download (box|screen|manual|all - default: box)", "types", "box"));

    // Checksum verification options
    parser.addOption(QCommandLineOption("checksum-verify", "Verify specific file checksum", "file"));
    parser.addOption(QCommandLineOption("expected-hash", "Expected hash for verification (crc32|md5|sha1)", "hash"));
    parser.addOption(QCommandLineOption("hash-type", "Hash type to verify (crc32, md5, sha1 - default: crc32)", "type", "crc32"));

    // M4 Organize & Rename options
    parser.addOption(QCommandLineOption("organize", "Organize and rename files using template", "destination"));
    parser.addOption(QCommandLineOption("template", "Naming template (default: No-Intro standard)", "template", Constants::Templates::DEFAULT_NO_INTRO));
    parser.addOption(QCommandLineOption("dry-run", "Preview changes without modifying files"));
    parser.addOption(QCommandLineOption("generate-m3u", "Generate M3U playlists for multi-disc games"));
    parser.addOption(QCommandLineOption("m3u-dir", "Directory for M3U playlists (default: same as game files)", "directory"));
    parser.addOption(QCommandLineOption("dry-run-all", "Preview file outputs for all file-writing actions"));

    // Patch options
    parser.addOption(QCommandLineOption("patch-apply", "Apply patch to base file", "basefile"));
    parser.addOption(QCommandLineOption("patch-patch", "Patch file to apply", "patchfile"));
    parser.addOption(QCommandLineOption("patch-output", "Output file path (optional)", "output"));
    parser.addOption(QCommandLineOption("patch-create", "Create patch from modified file", "modifiedfile"));
    parser.addOption(QCommandLineOption("patch-original", "Original file for patch creation", "originalfile"));
    parser.addOption(QCommandLineOption("patch-format", "Patch format (ips|bps|ups|xdelta|ppf)", "format", "bps"));
    parser.addOption(QCommandLineOption("patch-info", "Detect patch format for file", "patchfile"));
    parser.addOption(QCommandLineOption("patch-tools", "List patch tool availability"));

    // Export options
    parser.addOption(QCommandLineOption("export", "Export library (retroarch|emustation|launchbox|csv|json)", "format"));
    parser.addOption(QCommandLineOption("export-path", "Export output path (file or directory)", "path"));
    parser.addOption(QCommandLineOption("export-systems", "Comma-separated systems to include", "systems"));

    // Processing pipeline
    parser.addOption(QCommandLineOption("process", "Run scan→hash→match pipeline on directory", "path"));

    // M4.5 Conversion & Compression options
    parser.addOption(QCommandLineOption("convert-chd", "Convert disc image to CHD format", "path"));
    parser.addOption(QCommandLineOption("chd-codec", "CHD compression codec (lzma, zlib, flac, huff, auto)", "codec", "auto"));
    parser.addOption(QCommandLineOption("chd-extract", "Extract CHD back to BIN/CUE", "chdfile"));
    parser.addOption(QCommandLineOption("chd-verify", "Verify CHD file integrity", "chdfile"));
    parser.addOption(QCommandLineOption("chd-info", "Show CHD file information", "chdfile"));
    parser.addOption(QCommandLineOption("extract-archive", "Extract archive (ZIP/7z/RAR)", "path"));
    parser.addOption(QCommandLineOption("space-report", "Show potential CHD conversion savings", "directory"));
    parser.addOption(QCommandLineOption("output-dir", "Output directory for conversions/extractions", "directory"));

    // Interactive options
    parser.addOption(QCommandLineOption("interactive", "Launch interactive TUI (default when no actions provided)"));
    parser.addOption(QCommandLineOption("no-interactive", "Disable interactive TUI (script-friendly)"));

    parser.process(activeArgs);

    // Initialize database
    QString dbPath = parser.value("db");
    Database db;
    if (!db.initialize(dbPath)) {
        qCritical() << "Failed to initialize database";
        return 1;
    }

    // Initialize system detector
    SystemDetector detector;
    
    // Pre-populate systems in database
    QStringList systemNames = Systems::getSystemInternalNames();
    
    for (const QString &name : systemNames) {
        SystemInfo info = detector.getSystemInfo(name);
        if (!info.name.isEmpty()) {
            db.insertSystem(info);
        }
    }

    // Stats command
    if (parser.isSet("stats")) {
        QList<FileRecord> files = db.getExistingFiles();
        QMap<QString, int> counts = db.getFileCountBySystem();
        int hashed = 0;
        for (const FileRecord &f : files) {
            if (f.hashCalculated) hashed++;
        }
        qInfo() << "=== Library Stats ===";
        qInfo() << "Libraries:" << systemNames.size();
        qInfo() << "Files:" << files.size();
        qInfo() << "Hashed:" << hashed << "/" << files.size();
        qInfo() << "By system:";
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
            qInfo().noquote() << QString("  %1: %2").arg(it.key()).arg(it.value());
        }
    }

    // Info command
    if (parser.isSet("info")) {
        bool ok = false;
        int fileId = parser.value("info").toInt(&ok);
        if (!ok) {
            qCritical() << "Invalid file id";
            return 1;
        }
        FileRecord file = db.getFileById(fileId);
        if (file.id == 0) {
            qCritical() << "File not found";
            return 1;
        }
        qInfo() << "=== File Info ===";
        printFileInfo(file);
        auto match = db.getMatchForFile(fileId);
        if (match.matchId != 0) {
            qInfo() << "Match:" << match.gameTitle << "(" << match.confidence << "%)" << match.matchMethod;
        }
    }

    // Header inspection
    if (parser.isSet("header-info")) {
        QString path = parser.value("header-info");
        HeaderDetector hd;
        HeaderInfo info = hd.detect(path);
        if (!info.valid) {
            qWarning() << "Header not detected or invalid";
        } else {
            qInfo() << "=== Header Info ===";
            qInfo() << "Has header:" << info.hasHeader;
            qInfo() << "Header size:" << info.headerSize;
            qInfo() << "Type:" << info.headerType;
            qInfo() << "System hint:" << info.systemHint;
            if (!info.info.isEmpty()) {
                qInfo() << "Info:" << info.info;
            }
        }
    }

    // Show artwork/image in terminal
    if (parser.isSet("show-art")) {
        QString imagePath = parser.value("show-art");
        if (!TerminalImage::display(imagePath)) {
            qCritical() << "Failed to display image:" << imagePath;
        }
    }

    const bool dryRunAll = parser.isSet("dry-run-all");
    const bool processRequested = parser.isSet("process");
    const bool scanRequested = parser.isSet("scan") || processRequested;
    QString scanPath = parser.isSet("scan") ? parser.value("scan") : parser.value("process");

    // Handle scan command (direct or via process pipeline)
    if (scanRequested) {
        if (scanPath.isEmpty()) {
            qCritical() << "Scan path not provided";
            return 1;
        }
        qInfo() << "Scanning directory:" << scanPath;
        qInfo() << "";

        // Create scanner
        Scanner scanner;
        scanner.setExtensions(detector.getAllExtensions());

        // Connect signals
        QObject::connect(&scanner, &Scanner::fileFound, [](const QString &path) {
            qDebug() << "Found:" << path;
        });

        QObject::connect(&scanner, &Scanner::scanProgress, [](int processed, int total) {
            if (processed % 50 == 0) {
                qInfo() << "Processed" << processed << "files...";
            }
        });

        // Scan directory
        QList<ScanResult> results = scanner.scan(scanPath);
        qInfo() << "";
        qInfo() << "Scan complete:" << results.size() << "files found";

        // Insert into database
        int libraryId = db.insertLibrary(scanPath);
        int insertedCount = 0;
        int skippedCount = 0;

        for (const ScanResult &result : results) {
            // Detect system
            QString systemDetectPath = result.isCompressed && !result.archiveInternalPath.isEmpty()
                ? result.archiveInternalPath
                : result.path;
            QString systemName = detector.detectSystem(result.extension, systemDetectPath);
            int systemId = systemName.isEmpty() ? 0 : db.getSystemId(systemName);

            // Create file record
            FileRecord record;
            record.libraryId = libraryId;
            record.originalPath = result.path;
            record.currentPath = result.path;
            record.filename = result.filename;
            record.extension = result.extension;
            record.fileSize = result.fileSize;
            record.isCompressed = result.isCompressed;
            record.archivePath = result.archivePath;
            record.archiveInternalPath = result.archiveInternalPath;
            record.systemId = systemId;
            record.isPrimary = result.isPrimary;
            record.lastModified = result.lastModified;

            int fileId = db.insertFile(record);
            if (fileId > 0) {
                insertedCount++;
            } else {
                skippedCount++;
            }
        }

        qInfo() << "";
        qInfo() << "Database updated:";
        qInfo() << "  - Inserted:" << insertedCount << "files";
        qInfo() << "  - Skipped:" << skippedCount << "files";

        // Calculate hashes if requested or pipeline
        if (parser.isSet("hash") || processRequested) {
            qInfo() << "";
            qInfo() << "Calculating hashes...";

            Hasher hasher;
            QList<FileRecord> filesToHash = db.getFilesWithoutHashes();
            int hashedCount = 0;

            for (const FileRecord &file : filesToHash) {
                // Calculate hashes (handles archives via extraction)
                HashResult hashResult = hashFileRecord(file, hasher);

                if (hashResult.success) {
                    db.updateFileHashes(file.id, hashResult.crc32, 
                                        hashResult.md5, hashResult.sha1);
                    hashedCount++;

                    if (hashedCount % 10 == 0) {
                        qInfo() << "  Hashed" << hashedCount << "of" << filesToHash.size() << "files...";
                    }
                } else {
                    qWarning() << "  Hash failed for" << file.filename << ":" << hashResult.error;
                }
            }

            qInfo() << "Hash calculation complete:" << hashedCount << "files hashed";
        }
    }

    // Handle list command
    // Handle list command
    if (parser.isSet("list")) {
        qInfo() << "";
        qInfo() << "Files by system:";
        qInfo() << "─────────────────────────────────────";

        QMap<QString, int> counts = db.getFileCountBySystem();
        int total = 0;

        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
            qInfo().noquote() << QString("%1: %2 files").arg(it.key()).arg(it.value());
            total += it.value();
        }

        qInfo() << "─────────────────────────────────────";
        qInfo() << "Total:" << total << "files";
    }

    // Hash all existing files lacking hashes
    if (parser.isSet("hash-all")) {
        qInfo() << "";
        qInfo() << "Hashing files without hashes...";
        Hasher hasher;
        QList<FileRecord> filesToHash = db.getFilesWithoutHashes();
        int hashedCount = 0;

        for (const FileRecord &file : filesToHash) {
            HashResult hashResult = hashFileRecord(file, hasher);
            if (hashResult.success) {
                db.updateFileHashes(file.id, hashResult.crc32,
                                    hashResult.md5, hashResult.sha1);
                hashedCount++;
                if (hashedCount % 10 == 0) {
                    qInfo() << "  Hashed" << hashedCount << "of" << filesToHash.size() << "files...";
                }
            } else {
                qWarning() << "  Hash failed for" << file.filename << ":" << hashResult.error;
            }
        }

        qInfo() << "Hashing complete:" << hashedCount << "files hashed";
    }

    // Handle metadata fetch command
    if (parser.isSet("metadata")) {
        QString hash = parser.value("metadata");
        QString system = parser.value("system");
        QString providerName = parser.value("provider");
        
        qInfo() << "";
        qInfo() << "Fetching metadata for hash:" << hash;
        qInfo() << "System:" << (system.isEmpty() ? "auto-detect" : system);
        qInfo() << "Provider:" << providerName;
        qInfo() << "";

        MetadataProvider *provider = nullptr;

        if (providerName == Providers::SCREENSCRAPER) {
            auto ssProvider = new ScreenScraperProvider();
            
            if (parser.isSet("ss-user") && parser.isSet("ss-pass")) {
                ssProvider->setCredentials(parser.value("ss-user"), parser.value("ss-pass"));
            }
            if (parser.isSet("ss-devid") && parser.isSet("ss-devpass")) {
                ssProvider->setDeveloperCredentials(parser.value("ss-devid"), parser.value("ss-devpass"));
            }
            
            provider = ssProvider;
        } else if (providerName == Providers::THEGAMESDB) {
            provider = new TheGamesDBProvider();
        } else if (providerName == Providers::IGDB) {
            provider = new IGDBProvider();
        }

        if (provider) {
            GameMetadata metadata = provider->getByHash(hash, system);
            
            if (!metadata.title.isEmpty()) {
                qInfo() << "✓ Match found!";
                qInfo() << "─────────────────────────────────────";
                qInfo() << "Title:" << metadata.title;
                qInfo() << "System:" << metadata.system;
                qInfo() << "Region:" << metadata.region;
                qInfo() << "Developer:" << metadata.developer;
                qInfo() << "Publisher:" << metadata.publisher;
                qInfo() << "Release Date:" << metadata.releaseDate;
                qInfo() << "Genres:" << metadata.genres.join(", ");
                qInfo() << "Players:" << metadata.players;
                qInfo() << "Rating:" << metadata.rating << "/ 10";
                qInfo() << "";
                qInfo() << "Description:";
                qInfo().noquote() << metadata.description;
            } else {
                qInfo() << "✗ No match found for hash:" << hash;
            }
            
            delete provider;
        }
    }

    // Handle search command
    if (parser.isSet("search")) {
        QString title = parser.value("search");
        QString system = parser.value("system");
        QString providerName = parser.value("provider");
        
        qInfo() << "";
        qInfo() << "Searching for:" << title;
        qInfo() << "System:" << (system.isEmpty() ? "any" : system);
        qInfo() << "Provider:" << providerName;
        qInfo() << "";

        MetadataProvider *provider = nullptr;

        if (providerName == Providers::SCREENSCRAPER) {
            auto ssProvider = new ScreenScraperProvider();
            
            if (parser.isSet("ss-user") && parser.isSet("ss-pass")) {
                ssProvider->setCredentials(parser.value("ss-user"), parser.value("ss-pass"));
            }
            if (parser.isSet("ss-devid") && parser.isSet("ss-devpass")) {
                ssProvider->setDeveloperCredentials(parser.value("ss-devid"), parser.value("ss-devpass"));
            }
            
            provider = ssProvider;
        } else if (providerName == Providers::THEGAMESDB) {
            provider = new TheGamesDBProvider();
        } else if (providerName == Providers::IGDB) {
            provider = new IGDBProvider();
        }

        if (provider) {
            QList<SearchResult> results = provider->searchByName(title, system);
            
            if (!results.isEmpty()) {
                qInfo() << "Found" << results.size() << "result(s):";
                qInfo() << "─────────────────────────────────────";
                
                for (int i = 0; i < results.size(); ++i) {
                    const SearchResult &result = results[i];
                    qInfo().noquote() << QString("%1. %2 (%3)")
                        .arg(i + 1)
                        .arg(result.title)
                        .arg(result.releaseYear);
                    qInfo() << "   System:" << result.system;
                    qInfo() << "   Match Score:" << QString::number(result.matchScore * 100, 'f', 0) + "%";
                    qInfo() << "   Provider ID:" << result.id;
                    qInfo() << "";
                }
            } else {
                qInfo() << "No results found for:" << title;
            }
            
            delete provider;
        }
    }

    // Handle M3 matching command
    if (parser.isSet("match") || processRequested) {
        qInfo() << "";
        qInfo() << "=== Intelligent Metadata Matching (M3) ===";
        qInfo() << "";

        auto orchestrator = buildOrchestrator(parser);

        QObject::connect(orchestrator.get(), &ProviderOrchestrator::tryingProvider,
                        [](const QString &name, const QString &method) {
            qInfo() << "  [TRYING]" << name << "(" << method << ")";
        });

        QObject::connect(orchestrator.get(), &ProviderOrchestrator::providerSucceeded,
                        [](const QString &name, const QString &method) {
            qInfo() << "  [SUCCESS]" << name << "matched via" << method;
        });

        QObject::connect(orchestrator.get(), &ProviderOrchestrator::providerFailed,
                        [](const QString &name, const QString &error) {
            qInfo() << "  [FAILED]" << name << "-" << error;
        });

        QList<FileRecord> files = getHashedFiles(db);
        int minConfidence = parser.value("min-confidence").toInt();

        qInfo() << "Matching" << files.size() << "files with minimum confidence:" << minConfidence << "%";
        qInfo() << "Provider fallback order:";
        QStringList providers = orchestrator->getEnabledProviders();
        for (const QString &p : providers) {
            QString hashSupport = orchestrator->providerSupportsHash(p) ? "✓ hash" : "✗ name only";
            qInfo() << "  -" << p << "(" << hashSupport << ")";
        }
        qInfo() << "";

        int matched = 0;
        int failed = 0;

        for (const FileRecord &file : files) {
            auto existing = db.getMatchForFile(file.id);
            if (existing.matchId != 0) {
                continue; // already matched
            }

            qInfo() << "Matching:" << file.filename;

            GameMetadata metadata = orchestrator->searchWithFallback(
                selectBestHash(file),
                file.filename,
                "",
                file.crc32,
                file.md5,
                file.sha1
            );

            if (!metadata.title.isEmpty()) {
                int confidence = metadata.matchScore > 0 ? static_cast<int>(metadata.matchScore * 100) : 0;

                if (confidence >= minConfidence) {
                    int gameId = persistMetadata(db, file, metadata);
                    qInfo() << "  ✓ MATCHED:" << metadata.title << "(" << confidence << "% confidence)";
                    qInfo() << "    Provider:" << metadata.providerId;
                    qInfo() << "    Method:" << metadata.matchMethod;
                    qInfo() << "    System:" << metadata.system;
                    qInfo() << "    Game ID:" << gameId;
                    matched++;
                } else {
                    qInfo() << "  ⚠ Low confidence:" << confidence << "% (threshold:" << minConfidence << "%)";
                    failed++;
                }
            } else {
                qInfo() << "  ✗ No match found";
                failed++;
            }

            qInfo() << "";
        }

        qInfo() << "=== Matching Complete ===";
        qInfo() << "Matched:" << matched;
        qInfo() << "Failed:" << failed;
        if (matched + failed > 0) {
            qInfo() << "Success rate:" << QString::number((matched * 100.0) / (matched + failed), 'f', 1) + "%";
        }
    }

    // Handle match report command
    if (parser.isSet("match-report")) {
        qInfo() << "";
        qInfo() << "=== Matching Report with Confidence Scores ===";
        qInfo() << "";

        auto orchestrator = buildOrchestrator(parser);

        QList<FileRecord> files = getHashedFiles(db);
        int minConfidence = parser.value("min-confidence").toInt();

        // Open report file if specified
        QFile reportFile;
        QTextStream outStream(stdout);
        
        if (parser.isSet("report-file")) {
            reportFile.setFileName(parser.value("report-file"));
            if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qCritical() << "Failed to open report file:" << parser.value("report-file");
                return 1;
            }
            outStream.setDevice(&reportFile);
        }

        outStream << "\n=== Matching Confidence Report ===\n";
        outStream << QString("Generated: %1\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
        outStream << QString("Total files: %1\n").arg(files.size());
        outStream << QString("Minimum confidence threshold: %1%\n\n").arg(minConfidence);

        // Table header
        outStream << "┌────────────┬──────────────────────────────┬──────────┬──────────┬──────────────────────┐\n";
        outStream << "│ ID         │ Filename                     │ Conf %   │ Method   │ Title                │\n";
        outStream << "├────────────┼──────────────────────────────┼──────────┼──────────┼──────────────────────┤\n";

        for (const FileRecord &file : files) {
            GameMetadata metadata = orchestrator->searchWithFallback(
                selectBestHash(file),
                file.filename,
                "",
                file.crc32,
                file.md5,
                file.sha1
            );

            int confidence = metadata.matchScore > 0 ? static_cast<int>(metadata.matchScore * 100) : 0;
            QString confidenceStr = QString::number(confidence);
            QString method = metadata.matchMethod.isEmpty() ? "N/A" : metadata.matchMethod;
            QString title = metadata.title.isEmpty() ? "No match" : metadata.title;

            // Confidence color indicator
            QString confidenceIndicator;
            if (confidence >= 90) confidenceIndicator = "✓✓✓";
            else if (confidence >= 70) confidenceIndicator = "✓✓";
            else if (confidence >= 50) confidenceIndicator = "✓";
            else confidenceIndicator = "✗";

            outStream << QString("│ %1 │ %2 │ %3 %4 │ %5 │ %6 │\n")
                .arg(QString::number(file.id).leftJustified(10))
                .arg(file.filename.left(28).leftJustified(28))
                .arg(confidenceStr.rightJustified(4))
                .arg(confidenceIndicator.rightJustified(3))
                .arg(method.leftJustified(8))
                .arg(title.left(19).leftJustified(19));
        }

        outStream << "└────────────┴──────────────────────────────┴──────────┴──────────┴──────────────────────┘\n";
        outStream << "\nLegend:\n";
        outStream << "  ✓✓✓ = Excellent confidence (≥90%)\n";
        outStream << "  ✓✓  = Good confidence (70-89%)\n";
        outStream << "  ✓   = Fair confidence (50-69%)\n";
        outStream << "  ✗   = Low confidence (<50%)\n";

        if (parser.isSet("report-file")) {
            reportFile.close();
            qInfo() << "✓ Report saved to:" << parser.value("report-file");
        }
    }

    // Handle checksum verify command
    if (parser.isSet("checksum-verify")) {
        QString filePath = parser.value("checksum-verify");
        QString expectedHash = parser.value("expected-hash");
        QString hashType = parser.value("hash-type").toLower();

        qInfo() << "";
        qInfo() << "=== Verify Checksum ===";
        qInfo() << "File:" << filePath;
        qInfo() << "Hash Type:" << hashType;
        qInfo() << "Expected Hash:" << expectedHash;
        qInfo() << "";

        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            qCritical() << "✗ File not found:" << filePath;
            return 1;
        }

        Hasher hasher;
        QString calculatedHash;

        if (hashType == "md5") {
            HashResult result = hasher.calculateHashes(filePath, false, 0);
            calculatedHash = result.md5.toLower();
        } else if (hashType == "sha1") {
            HashResult result = hasher.calculateHashes(filePath, false, 0);
            calculatedHash = result.sha1.toLower();
        } else {
            HashResult result = hasher.calculateHashes(filePath, false, 0);
            calculatedHash = result.crc32.toLower();
        }

        qInfo() << "Calculated Hash:" << calculatedHash;

        if (calculatedHash.toLower() == expectedHash.toLower()) {
            qInfo() << "";
            qInfo() << "✓ HASH MATCH - File is valid!";
            qInfo() << "  File Size:" << SpaceCalculator::formatBytes(fileInfo.size());
        } else {
            qWarning() << "";
            qWarning() << "✗ HASH MISMATCH - File may be corrupted or modified!";
            qWarning() << "  Expected: " << expectedHash;
            qWarning() << "  Got:      " << calculatedHash;
            return 1;
        }
    }

    // Handle verify command (verify against DAT file)
    if (parser.isSet("verify")) {
        QString datFile = parser.value("verify");
        bool generateReport = parser.isSet("verify-report");

        qInfo() << "";
        qInfo() << "=== Verify Files Against DAT ===";
        qInfo() << "DAT File:" << datFile;
        qInfo() << "";

        QFileInfo datInfo(datFile);
        if (!datInfo.exists()) {
            qCritical() << "✗ DAT file not found:" << datFile;
            return 1;
        }

        VerificationEngine verifier(&db);
        
        // Detect system from DAT filename or ask user
        QString systemName = detector.detectSystem("", datFile);
        if (systemName.isEmpty()) {
            // Try extracting from filename
            QString baseName = datInfo.completeBaseName();
            systemName = baseName;  // Use basename as system name
        }

        if (verifier.importDat(datFile, systemName) <= 0) {
            qCritical() << "✗ Failed to import DAT file";
            return 1;
        }

        qInfo() << "✓ DAT file loaded successfully";
        qInfo() << "  System:" << systemName;
        qInfo() << "";

        // Verify all files
        QList<VerificationResult> results = verifier.verifyLibrary(systemName);
        VerificationSummary summary = verifier.getLastSummary();

        qInfo() << "=== Verification Results ===";
        qInfo() << QString("Total files: %1").arg(summary.totalFiles);
        qInfo() << QString("✓ Verified: %1").arg(summary.verified);
        qInfo() << QString("⚠ Mismatched: %1").arg(summary.mismatched);
        qInfo() << QString("✗ Not in DAT: %1").arg(summary.notInDat);
        qInfo() << QString("? No hash: %1").arg(summary.noHash);
        qInfo() << "";

        // Show detailed results for non-verified files
        if (!results.isEmpty()) {
            qInfo() << "Detailed Results:";
            qInfo() << "";

            int detailedShown = 0;
            for (const VerificationResult &result : results) {
                if (result.status == VerificationStatus::Verified) {
                    qInfo() << "✓" << result.filename << "- VERIFIED";
                    qInfo() << "  Title:" << result.datDescription;
                } else if (result.status == VerificationStatus::Mismatch) {
                    qWarning() << "✗" << result.filename << "- HASH MISMATCH";
                    qWarning() << "  Expected:" << result.datHash;
                    qWarning() << "  Got:     " << result.fileHash;
                } else if (result.status == VerificationStatus::NotInDat) {
                    qInfo() << "?" << result.filename << "- NOT IN DAT";
                } else if (result.status == VerificationStatus::HashMissing) {
                    qInfo() << "?" << result.filename << "- NO HASH (calculate with --hash)";
                }
                
                detailedShown++;
                if (detailedShown >= 50) {
                    qInfo() << "";
                    qInfo() << "... and" << (results.size() - detailedShown) << "more results";
                    break;
                }
            }
        }

        if (generateReport && parser.isSet("report-file")) {
            QString reportPath = parser.value("report-file");
            if (verifier.exportReport(results, reportPath, "csv")) {
                qInfo() << "";
                qInfo() << "✓ CSV report saved to:" << reportPath;
            }
        }
    }

    // Handle download artwork command
    if (parser.isSet("download-artwork")) {
        QString artworkDirStr = parser.value("artwork-dir");
        QString artworkTypes = parser.value("artwork-types");

        qInfo() << "";
        qInfo() << "=== Download Artwork ===";
        
        if (artworkDirStr.isEmpty()) {
            artworkDirStr = QDir::homePath() + "/.local/share/Remus/artwork/";
        }
        
        qInfo() << "Artwork directory:" << artworkDirStr;
        qInfo() << "Types to download:" << artworkTypes;
        qInfo() << "";

        QDir().mkpath(artworkDirStr);

        ArtworkDownloader downloader;
        downloader.setMaxConcurrent(4);

        auto orchestrator = buildOrchestrator(parser);
        int downloadedCount = 0;
        int failedCount = 0;

        QList<FileRecord> files = getHashedFiles(db);

        for (const FileRecord &file : files) {
            qInfo() << "Processing:" << file.filename;
            GameMetadata metadata = orchestrator->searchWithFallback(
                selectBestHash(file),
                file.filename,
                "",
                file.crc32,
                file.md5,
                file.sha1
            );

            if (metadata.boxArtUrl.isEmpty()) {
                qInfo() << "  ✗ No box art URL";
                failedCount++;
                continue;
            }

            QUrl url(metadata.boxArtUrl);
            if (!url.isValid()) {
                qInfo() << "  ✗ Invalid URL" << metadata.boxArtUrl;
                failedCount++;
                continue;
            }

            QString destPath = artworkDirStr + "/" + QFileInfo(file.filename).completeBaseName() + ".jpg";
            if (dryRunAll) {
                qInfo() << "  [DRY-RUN] would save" << destPath << "from" << url.toString();
                downloadedCount++;
                continue;
            }

            if (downloader.download(url, destPath)) {
                qInfo() << "  ✓ Saved" << destPath;
                downloadedCount++;
            } else {
                qInfo() << "  ✗ Download failed" << url.toString();
                failedCount++;
            }
        }

        qInfo() << "";
        qInfo() << "Artwork download complete:";
        qInfo() << "  Downloaded:" << downloadedCount;
        qInfo() << "  Failed:" << failedCount;
    }

    // Handle M4 organize command
    if (parser.isSet("organize")) {
        QString destination = parser.value("organize");
        QString templateStr = parser.value("template");
        bool dryRun = parser.isSet("dry-run") || dryRunAll;

        qInfo() << "";
        qInfo() << "=== Organize & Rename Files (M4) ===";
        qInfo() << "Destination:" << destination;
        qInfo() << "Template:" << templateStr;
        qInfo() << "Mode:" << (dryRun ? "DRY RUN (preview only)" : "EXECUTE");
        qInfo() << "";

        // Create organize engine
        OrganizeEngine organizer(db);
        organizer.setTemplate(templateStr);
        organizer.setDryRun(dryRun);
        organizer.setCollisionStrategy(CollisionStrategy::Rename);

        // Connect signals for progress
        QObject::connect(&organizer, &OrganizeEngine::operationStarted, 
            [](int fileId, const QString &oldPath, const QString &newPath) {
                qInfo() << "→ File" << fileId << ":" << oldPath << "->" << newPath;
            });

        QObject::connect(&organizer, &OrganizeEngine::operationCompleted,
            [](int fileId, bool success, const QString &error) {
                if (success) {
                    qInfo() << "  ✓ Success";
                } else {
                    qInfo() << "  ✗ Failed:" << error;
                }
            });

        QObject::connect(&organizer, &OrganizeEngine::dryRunPreview,
            [](const QString &oldPath, const QString &newPath, FileOperation op) {
                QString opName = (op == FileOperation::Move) ? "MOVE" : "COPY";
                qInfo() << "  [PREVIEW]" << opName << ":" << oldPath << "→" << newPath;
            });

        QMap<int, Database::MatchResult> matches = db.getAllMatches();
        QList<FileRecord> files = db.getExistingFiles();

        if (files.isEmpty()) {
            qInfo() << "No files to organize";
        } else {
            qInfo() << "Processing" << files.size() << "files...";
            qInfo() << "";

            for (const FileRecord &file : files) {
                if (!matches.contains(file.id)) {
                    continue; // skip unmatched
                }

                const auto match = matches.value(file.id);
                GameMetadata metadata;
                metadata.title = match.gameTitle;
                metadata.region = match.region;
                metadata.system = db.getSystemDisplayName(file.systemId);

                organizer.organizeFile(file.id, metadata, destination, FileOperation::Move);
            }

            qInfo() << "";
            qInfo() << "Organization" << (dryRun ? "preview" : "complete");
        }
    }

    // Handle M4 M3U generation command
    if (parser.isSet("generate-m3u")) {
        QString m3uDir = parser.value("m3u-dir");

        if (dryRunAll) {
            qInfo() << "[DRY-RUN] Skipping M3U generation";
        } else {
            qInfo() << "";
            qInfo() << "=== Generate M3U Playlists ===";
            if (!m3uDir.isEmpty()) {
                qInfo() << "Output directory:" << m3uDir;
            } else {
                qInfo() << "Output: Same directory as game files";
            }
            qInfo() << "";

            M3UGenerator generator(db);

            // Connect signals
            QObject::connect(&generator, &M3UGenerator::playlistGenerated,
                [](const QString &path, int discCount) {
                    qInfo() << "✓ Generated:" << path << "(" << discCount << "discs)";
                });

            QObject::connect(&generator, &M3UGenerator::errorOccurred,
                [](const QString &error) {
                    qWarning() << "✗ Error:" << error;
                });

            // Generate all M3U playlists
            int count = generator.generateAll(QString(), m3uDir);

            qInfo() << "";
            qInfo() << "Generated" << count << "M3U playlists";
        }
    }

    // Handle M4.5 CHD conversion command
    if (parser.isSet("convert-chd")) {
        QString inputPath = parser.value("convert-chd");
        QString outputDir = parser.value("output-dir");
        QString codecStr = parser.value("chd-codec");

        qInfo() << "";
        qInfo() << "=== Convert to CHD Format (M4.5) ===";
        qInfo() << "Input:" << inputPath;

        CHDConverter converter;

        // Check chdman availability
        if (!converter.isChdmanAvailable()) {
            qCritical() << "✗ chdman not found. Install MAME tools (mame-tools package)";
            return 1;
        }

        qInfo() << "chdman version:" << converter.getChdmanVersion();

        // Set codec
        CHDCodec codec = CHDCodec::Auto;
        if (codecStr == "lzma") codec = CHDCodec::LZMA;
        else if (codecStr == "zlib") codec = CHDCodec::ZLIB;
        else if (codecStr == "flac") codec = CHDCodec::FLAC;
        else if (codecStr == "huff") codec = CHDCodec::Huffman;
        
        converter.setCodec(codec);

        // Determine output path
        QFileInfo info(inputPath);
        QString outputPath;
        if (outputDir.isEmpty()) {
            outputPath = info.absolutePath() + "/" + info.completeBaseName() + ".chd";
        } else {
            QDir().mkpath(outputDir);
            outputPath = outputDir + "/" + info.completeBaseName() + ".chd";
        }

        qInfo() << "Output:" << outputPath;
        qInfo() << "Codec:" << codecStr;
        qInfo() << "";

        // Convert based on file type
        QString ext = info.suffix().toLower();
        CHDConversionResult result;

        if (dryRunAll) {
            qInfo() << "[DRY-RUN] Would convert" << inputPath << "to" << outputPath << "using" << codecStr;
        } else {
            if (ext == "cue") {
                result = converter.convertCueToCHD(inputPath, outputPath);
            } else if (ext == "iso" || ext == "img") {
                result = converter.convertIsoToCHD(inputPath, outputPath);
            } else if (ext == "gdi") {
                result = converter.convertGdiToCHD(inputPath, outputPath);
            } else {
                qCritical() << "✗ Unsupported format:" << ext;
                qInfo() << "Supported formats: .cue, .iso, .img, .gdi";
                return 1;
            }

            if (result.success) {
                qInfo() << "✓ Conversion successful!";
                qInfo() << "  Original size:" << SpaceCalculator::formatBytes(result.inputSize);
                qInfo() << "  CHD size:" << SpaceCalculator::formatBytes(result.outputSize);
                qInfo() << "  Saved:" << SpaceCalculator::formatBytes(result.inputSize - result.outputSize);
                qInfo() << "  Compression:" << QString::number((1.0 - result.compressionRatio) * 100, 'f', 1) << "%";
            } else {
                qCritical() << "✗ Conversion failed:" << result.error;
                return 1;
            }
        }
    }

    // Handle CHD extraction command
    if (parser.isSet("chd-extract")) {
        QString chdPath = parser.value("chd-extract");
        QString outputDir = parser.value("output-dir");

        qInfo() << "";
        qInfo() << "=== Extract CHD to BIN/CUE (M4.5) ===";
        qInfo() << "Input:" << chdPath;

        CHDConverter converter;
        if (!converter.isChdmanAvailable()) {
            qCritical() << "✗ chdman not found";
            return 1;
        }

        QFileInfo info(chdPath);
        QString outputPath;
        if (outputDir.isEmpty()) {
            outputPath = info.absolutePath() + "/" + info.completeBaseName() + ".cue";
        } else {
            QDir().mkpath(outputDir);
            outputPath = outputDir + "/" + info.completeBaseName() + ".cue";
        }

        qInfo() << "Output:" << outputPath;
        qInfo() << "";

        if (dryRunAll) {
            qInfo() << "[DRY-RUN] Would extract" << chdPath << "to" << outputPath;
        } else {
            CHDConversionResult result = converter.extractCHDToCue(chdPath, outputPath);

            if (result.success) {
                qInfo() << "✓ Extraction successful!";
                qInfo() << "  Extracted to:" << outputPath;
            } else {
                qCritical() << "✗ Extraction failed:" << result.error;
                return 1;
            }
        }
    }

    // Handle CHD verify command
    if (parser.isSet("chd-verify")) {
        QString chdPath = parser.value("chd-verify");

        qInfo() << "";
        qInfo() << "=== Verify CHD Integrity (M4.5) ===";
        qInfo() << "File:" << chdPath;
        qInfo() << "";

        CHDConverter converter;
        if (!converter.isChdmanAvailable()) {
            qCritical() << "✗ chdman not found";
            return 1;
        }

        CHDVerifyResult result = converter.verifyCHD(chdPath);

        if (result.valid) {
            qInfo() << "✓ CHD is valid!";
            qInfo() << "  " << result.details;
        } else {
            qCritical() << "✗ CHD verification failed!";
            qInfo() << "  Error:" << result.error;
            return 1;
        }
    }

    // Handle CHD info command
    if (parser.isSet("chd-info")) {
        QString chdPath = parser.value("chd-info");

        qInfo() << "";
        qInfo() << "=== CHD File Information (M4.5) ===";
        qInfo() << "File:" << chdPath;
        qInfo() << "";

        CHDConverter converter;
        if (!converter.isChdmanAvailable()) {
            qCritical() << "✗ chdman not found";
            return 1;
        }

        CHDInfo info = converter.getCHDInfo(chdPath);

        if (info.version > 0) {
            qInfo() << "  CHD Version:" << info.version;
            qInfo() << "  Compression:" << info.compression;
            qInfo() << "  Logical Size:" << SpaceCalculator::formatBytes(info.logicalSize);
            qInfo() << "  Physical Size:" << SpaceCalculator::formatBytes(info.physicalSize);
            double ratio = (info.logicalSize > 0) ? 
                static_cast<double>(info.physicalSize) / static_cast<double>(info.logicalSize) : 0.0;
            qInfo() << "  Compression Ratio:" << QString::number((1.0 - ratio) * 100, 'f', 1) << "%";
            qInfo() << "  SHA1:" << info.sha1;
        } else {
            qCritical() << "✗ Failed to read CHD info";
            return 1;
        }
    }

    // Handle archive extraction command
    if (parser.isSet("extract-archive")) {
        QString archivePath = parser.value("extract-archive");
        QString outputDir = parser.value("output-dir");

        qInfo() << "";
        qInfo() << "=== Extract Archive (M4.5) ===";
        qInfo() << "Archive:" << archivePath;

        ArchiveExtractor extractor;

        // Check available tools
        QMap<ArchiveFormat, bool> tools = extractor.getAvailableTools();
        QStringList available;
        if (tools.value(ArchiveFormat::ZIP)) available << "unzip";
        if (tools.value(ArchiveFormat::SevenZip)) available << "7z";
        if (tools.value(ArchiveFormat::RAR)) available << "unrar";

        if (available.isEmpty()) {
            qCritical() << "✗ No extraction tools found (need unzip, 7z, or unrar)";
            return 1;
        }
        qInfo() << "Available tools:" << available.join(", ");

        // Detect format
        ArchiveFormat format = extractor.detectFormat(archivePath);
        if (format == ArchiveFormat::Unknown) {
            qCritical() << "✗ Unknown archive format";
            return 1;
        }

        // Determine output directory
        if (outputDir.isEmpty()) {
            QFileInfo info(archivePath);
            outputDir = info.absolutePath() + "/" + info.completeBaseName();
        }
        QDir().mkpath(outputDir);

        qInfo() << "Output:" << outputDir;
        qInfo() << "";

        if (dryRunAll) {
            qInfo() << "[DRY-RUN] Would extract" << archivePath << "to" << outputDir;
        } else {
            ExtractionResult result = extractor.extract(archivePath, outputDir);

            if (result.success) {
                qInfo() << "✓ Extraction successful!";
                qInfo() << "  Files extracted:" << result.filesExtracted;
                for (const QString &path : result.extractedFiles) {
                    qInfo() << "    " << path;
                }
            } else {
                qCritical() << "✗ Extraction failed:" << result.error;
                return 1;
            }
        }
    }

    // Handle space report command
    if (parser.isSet("space-report")) {
        QString dirPath = parser.value("space-report");

        qInfo() << "";
        qInfo() << "=== CHD Conversion Savings Report (M4.5) ===";
        qInfo() << "";

        SpaceCalculator calculator;

        QObject::connect(&calculator, &SpaceCalculator::scanProgress,
            [](int count, const QString &path) {
                if (count % 50 == 0) {
                    qInfo() << "  Scanned" << count << "files...";
                }
            });

        qInfo() << "Scanning:" << dirPath;
        qInfo() << "";

        ConversionSummary summary = calculator.scanDirectory(dirPath, true);

        qInfo().noquote() << calculator.formatSavingsReport(summary);
    }

    // Export command
    if (parser.isSet("export")) {
        const QString format = parser.value("export").toLower();
        QString outputPath = parser.value("export-path");
        const QString systemsArg = parser.value("export-systems");
        const QStringList systemFilters = systemsArg.isEmpty() ? QStringList() : systemsArg.split(',', Qt::SkipEmptyParts);

        if (dryRunAll) {
            qInfo() << "[DRY-RUN] Export outputs will not be written";
        }

        if (outputPath.isEmpty()) {
            if (format == "retroarch") outputPath = "remus.lpl";
            else if (format == "emustation") outputPath = "gamelist.xml";
            else if (format == "launchbox") outputPath = "launchbox-games.xml";
            else if (format == "csv") outputPath = "remus-export.csv";
            else outputPath = "remus-export.json";
        }

        QMap<int, Database::MatchResult> matches = db.getAllMatches();
        QList<FileRecord> files = db.getExistingFiles();

        struct ExportRow {
            FileRecord file;
            Database::MatchResult match;
        };

        QList<ExportRow> rows;
        for (const FileRecord &file : files) {
            if (!matches.contains(file.id)) continue;
            const auto match = matches.value(file.id);
            QString systemName = db.getSystemDisplayName(file.systemId);
            if (!systemFilters.isEmpty() && !systemFilters.contains(systemName)) continue;
            rows.append({file, match});
        }

        if (rows.isEmpty()) {
            qWarning() << "No matched files to export";
        } else if (format == "retroarch") {
            if (dryRunAll) {
                qInfo() << "[DRY-RUN] Would write RetroArch playlist to" << outputPath << "(" << rows.size() << " entries)";
            } else {
                QFile f(outputPath);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    qCritical() << "Failed to open" << outputPath;
                    return 1;
                }
                QTextStream out(&f);
                for (const auto &row : rows) {
                    QString systemName = db.getSystemDisplayName(row.file.systemId);
                    out << row.file.currentPath << "\n";
                    out << (!row.match.gameTitle.isEmpty() ? row.match.gameTitle : row.file.filename) << "\n";
                    out << "DETECT\nDETECT\n";
                    out << (row.file.crc32.isEmpty() ? "00000000" : row.file.crc32) << "|crc\n";
                    out << systemName << ".lpl\n";
                }
                qInfo() << "✓ RetroArch playlist exported to" << outputPath;
            }
        } else if (format == "emustation") {
            if (dryRunAll) {
                qInfo() << "[DRY-RUN] Would write EmulationStation gamelist to" << outputPath << "(" << rows.size() << " entries)";
            } else {
                QFile f(outputPath);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    qCritical() << "Failed to open" << outputPath;
                    return 1;
                }
                QTextStream out(&f);
                out << "<gameList>\n";
                for (const auto &row : rows) {
                    out << "  <game>\n";
                    out << "    <path>" << row.file.currentPath << "</path>\n";
                    out << "    <name>" << (!row.match.gameTitle.isEmpty() ? row.match.gameTitle : row.file.filename) << "</name>\n";
                    out << "    <desc>" << row.match.description << "</desc>\n";
                    out << "    <genre>" << row.match.genre << "</genre>\n";
                    out << "    <players>" << row.match.players << "</players>\n";
                    out << "    <region>" << row.match.region << "</region>\n";
                    out << "  </game>\n";
                }
                out << "</gameList>\n";
                qInfo() << "✓ EmulationStation gamelist exported to" << outputPath;
            }
        } else if (format == "launchbox") {
            if (dryRunAll) {
                qInfo() << "[DRY-RUN] Would write LaunchBox XML to" << outputPath << "(" << rows.size() << " entries)";
            } else {
                QFile f(outputPath);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    qCritical() << "Failed to open" << outputPath;
                    return 1;
                }
                QTextStream out(&f);
                out << "<LaunchBox>\n";
                for (const auto &row : rows) {
                    out << "  <Game>\n";
                    out << "    <Title>" << (!row.match.gameTitle.isEmpty() ? row.match.gameTitle : row.file.filename) << "</Title>\n";
                    out << "    <ApplicationPath>" << row.file.currentPath << "</ApplicationPath>\n";
                    out << "    <Region>" << row.match.region << "</Region>\n";
                    out << "    <Genre>" << row.match.genre << "</Genre>\n";
                    out << "  </Game>\n";
                }
                out << "</LaunchBox>\n";
                qInfo() << "✓ LaunchBox XML exported to" << outputPath;
            }
        } else if (format == "csv") {
            if (dryRunAll) {
                qInfo() << "[DRY-RUN] Would write CSV to" << outputPath << "(" << rows.size() << " entries)";
            } else {
                QFile f(outputPath);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    qCritical() << "Failed to open" << outputPath;
                    return 1;
                }
                QTextStream out(&f);
                out << "file_id,title,system,path,region,confidence\n";
                for (const auto &row : rows) {
                    QString systemName = db.getSystemDisplayName(row.file.systemId);
                    QString title = row.match.gameTitle;
                    title.replace(',', ' ');
                    out << row.file.id << "," << title << "," << systemName
                        << "," << row.file.currentPath << "," << row.match.region << "," << row.match.confidence << "\n";
                }
                qInfo() << "✓ CSV exported to" << outputPath;
            }
        } else {
            if (dryRunAll) {
                qInfo() << "[DRY-RUN] Would write JSON to" << outputPath << "(" << rows.size() << " entries)";
            } else {
                QFile f(outputPath);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    qCritical() << "Failed to open" << outputPath;
                    return 1;
                }
                QJsonArray arr;
                for (const auto &row : rows) {
                    QJsonObject obj;
                    obj["fileId"] = row.file.id;
                    obj["title"] = row.match.gameTitle;
                    obj["system"] = db.getSystemDisplayName(row.file.systemId);
                    obj["path"] = row.file.currentPath;
                    obj["region"] = row.match.region;
                    obj["confidence"] = row.match.confidence;
                    arr.append(obj);
                }
                QJsonDocument doc(arr);
                f.write(doc.toJson());
                qInfo() << "✓ JSON exported to" << outputPath;
            }
        }
    }

    // Patch operations
    if (parser.isSet("patch-tools")) {
        PatchEngine pe;
        auto tools = pe.checkToolAvailability();
        qInfo() << "=== Patch Tool Availability ===";
        for (auto it = tools.constBegin(); it != tools.constEnd(); ++it) {
            qInfo() << it.key() << ":" << (it.value() ? "✓" : "✗");
        }
    }

    if (parser.isSet("patch-info")) {
        PatchEngine pe;
        PatchInfo info = pe.detectFormat(parser.value("patch-info"));
        if (info.valid) {
            qInfo() << "Format:" << info.formatName;
            qInfo() << "Size:" << info.size;
            if (!info.sourceChecksum.isEmpty()) {
                qInfo() << "Source CRC:" << info.sourceChecksum;
                qInfo() << "Target CRC:" << info.targetChecksum;
                qInfo() << "Patch CRC:" << info.patchChecksum;
            }
        } else {
            qWarning() << "Could not detect patch format:" << info.error;
        }
    }

    if (parser.isSet("patch-apply") && parser.isSet("patch-patch")) {
        QString basePath = parser.value("patch-apply");
        QString patchPath = parser.value("patch-patch");
        QString outputPath = parser.value("patch-output");

        PatchEngine pe;
        PatchInfo info = pe.detectFormat(patchPath);
        if (!info.valid) {
            qCritical() << "Invalid patch file" << info.error;
            return 1;
        }

        if (dryRunAll) {
            qInfo() << "[DRY-RUN] Would apply patch" << patchPath << "to" << basePath << "->" << outputPath;
        } else {
            PatchResult result = pe.apply(basePath, info, outputPath);
            if (result.success) {
                qInfo() << "✓ Patch applied:" << result.outputPath;
            } else {
                qCritical() << "✗ Patch failed:" << result.error;
                return 1;
            }
        }
    }

    if (parser.isSet("patch-create") && parser.isSet("patch-original")) {
        QString modified = parser.value("patch-create");
        QString original = parser.value("patch-original");
        QString patchPath = parser.value("patch-patch");
        QString fmtStr = parser.value("patch-format").toLower();

        PatchFormat format = PatchFormat::BPS;
        if (fmtStr == "ips") format = PatchFormat::IPS;
        else if (fmtStr == "ups") format = PatchFormat::UPS;
        else if (fmtStr == "xdelta") format = PatchFormat::XDelta3;
        else if (fmtStr == "ppf") format = PatchFormat::PPF;

        PatchEngine pe;
        if (patchPath.isEmpty()) {
            QFileInfo baseInfo(original);
            QFileInfo modInfo(modified);
            QString baseName = baseInfo.completeBaseName();
            QString modName = modInfo.completeBaseName();
            QString ext = (format == PatchFormat::IPS) ? "ips" :
                          (format == PatchFormat::UPS) ? "ups" :
                          (format == PatchFormat::XDelta3) ? "xdelta" :
                          (format == PatchFormat::PPF) ? "ppf" : "bps";
            patchPath = baseInfo.absolutePath() + "/" + baseName + "_to_" + modName + "." + ext;
        }

        if (dryRunAll) {
            qInfo() << "[DRY-RUN] Would create patch" << patchPath << "from" << original << "to" << modified;
        } else if (pe.createPatch(original, modified, patchPath, format)) {
            qInfo() << "✓ Patch created:" << patchPath;
        } else {
            qCritical() << "✗ Failed to create patch";
            return 1;
        }
    }

    qInfo() << "";
    qInfo() << "Done!";

    return 0;
}
