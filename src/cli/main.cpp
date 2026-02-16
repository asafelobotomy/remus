#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include "../core/scanner.h"
#include "../core/system_detector.h"
#include "../core/hasher.h"
#include "../core/database.h"
#include "../core/matching_engine.h"
#include "../core/template_engine.h"
#include "../core/organize_engine.h"
#include "../core/m3u_generator.h"
#include "../metadata/screenscraper_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/provider_orchestrator.h"
#include "../metadata/metadata_cache.h"
#include "../core/chd_converter.h"
#include "../core/archive_extractor.h"
#include "../core/space_calculator.h"
#include "../core/constants/constants.h"
#include "../core/logging_categories.h"

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
    QCoreApplication::setApplicationVersion(Constants::APP_VERSION);

    printBanner();

    QCommandLineParser parser;
    parser.setApplicationDescription("Remus CLI - Scan and catalog retro game ROMs");
    parser.addHelpOption();
    parser.addVersionOption();

    // Add options
    parser.addOption({{"s", "scan"}, "Scan a directory for ROMs", "path"});
    parser.addOption({{"d", "db"}, "Database file path", "database", Constants::DATABASE_FILENAME});
    parser.addOption(QCommandLineOption("hash", "Calculate hashes for scanned files"));
    parser.addOption({{"l", "list"}, "List scanned files by system"});
    
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

    // M4 Organize & Rename options
    parser.addOption(QCommandLineOption("organize", "Organize and rename files using template", "destination"));
    parser.addOption(QCommandLineOption("template", "Naming template (default: No-Intro standard)", "template", Templates::DEFAULT_NO_INTRO));
    parser.addOption(QCommandLineOption("dry-run", "Preview changes without modifying files"));
    parser.addOption(QCommandLineOption("generate-m3u", "Generate M3U playlists for multi-disc games"));
    parser.addOption(QCommandLineOption("m3u-dir", "Directory for M3U playlists (default: same as game files)", "directory"));

    // M4.5 Conversion & Compression options
    parser.addOption(QCommandLineOption("convert-chd", "Convert disc image to CHD format", "path"));
    parser.addOption(QCommandLineOption("chd-codec", "CHD compression codec (lzma, zlib, flac, huff, auto)", "codec", "auto"));
    parser.addOption(QCommandLineOption("chd-extract", "Extract CHD back to BIN/CUE", "chdfile"));
    parser.addOption(QCommandLineOption("chd-verify", "Verify CHD file integrity", "chdfile"));
    parser.addOption(QCommandLineOption("chd-info", "Show CHD file information", "chdfile"));
    parser.addOption(QCommandLineOption("extract-archive", "Extract archive (ZIP/7z/RAR)", "path"));
    parser.addOption(QCommandLineOption("space-report", "Show potential CHD conversion savings", "directory"));
    parser.addOption(QCommandLineOption("output-dir", "Output directory for conversions/extractions", "directory"));

    parser.process(app);

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

    // Handle scan command
    if (parser.isSet("scan")) {
        QString scanPath = parser.value("scan");
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
            QString systemName = detector.detectSystem(result.extension, result.path);
            int systemId = systemName.isEmpty() ? 0 : db.getSystemId(systemName);

            // Create file record
            FileRecord record;
            record.libraryId = libraryId;
            record.originalPath = result.path;
            record.currentPath = result.path;
            record.filename = result.filename;
            record.extension = result.extension;
            record.fileSize = result.fileSize;
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

        // Calculate hashes if requested
        if (parser.isSet("hash")) {
            qInfo() << "";
            qInfo() << "Calculating hashes...";

            Hasher hasher;
            QList<FileRecord> filesToHash = db.getFilesWithoutHashes();
            int hashedCount = 0;

            for (const FileRecord &file : filesToHash) {
                // Detect if header stripping needed
                int headerSize = Hasher::detectHeaderSize(file.currentPath, file.extension);
                bool stripHeader = (headerSize > 0);

                // Calculate hashes
                HashResult hashResult = hasher.calculateHashes(
                    file.currentPath, stripHeader, headerSize);

                if (hashResult.success) {
                    db.updateFileHashes(file.id, hashResult.crc32, 
                                        hashResult.md5, hashResult.sha1);
                    hashedCount++;

                    if (hashedCount % 10 == 0) {
                        qInfo() << "  Hashed" << hashedCount << "of" << filesToHash.size() << "files...";
                    }
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
    if (parser.isSet("match")) {
        qInfo() << "";
        qInfo() << "=== Intelligent Metadata Matching (M3) ===";
        qInfo() << "";
        
        // Set up provider orchestrator
        ProviderOrchestrator orchestrator;
        
        // Add Hasheous (highest priority for hash matching, no auth required)
        auto hasheousProvider = new HasheousProvider();
        const auto hasheousInfo = Providers::getProviderInfo(Providers::HASHEOUS);
        const int hasheousPriority = hasheousInfo ? hasheousInfo->priority : 100;
        orchestrator.addProvider(Providers::HASHEOUS, hasheousProvider, hasheousPriority);
        
        // Add ScreenScraper (if credentials provided)
        if (parser.isSet("ss-user") && parser.isSet("ss-pass")) {
            auto ssProvider = new ScreenScraperProvider();
            ssProvider->setCredentials(parser.value("ss-user"), parser.value("ss-pass"));
            if (parser.isSet("ss-devid") && parser.isSet("ss-devpass")) {
                ssProvider->setDeveloperCredentials(parser.value("ss-devid"), parser.value("ss-devpass"));
            }
            const auto ssInfo = Providers::getProviderInfo(Providers::SCREENSCRAPER);
            const int ssPriority = ssInfo ? ssInfo->priority : 90;
            orchestrator.addProvider(Providers::SCREENSCRAPER, ssProvider, ssPriority);
        }
        
        // Add TheGamesDB (name fallback)
        auto tgdbProvider = new TheGamesDBProvider();
        const auto tgdbInfo = Providers::getProviderInfo(Providers::THEGAMESDB);
        const int tgdbPriority = tgdbInfo ? tgdbInfo->priority : 50;
        orchestrator.addProvider(Providers::THEGAMESDB, tgdbProvider, tgdbPriority);
        
        // Add IGDB (richest metadata fallback)
        auto igdbProvider = new IGDBProvider();
        const auto igdbInfo = Providers::getProviderInfo(Providers::IGDB);
        const int igdbPriority = igdbInfo ? igdbInfo->priority : 40;
        orchestrator.addProvider(Providers::IGDB, igdbProvider, igdbPriority);
        
        // Connect orchestrator signals for progress tracking
        QObject::connect(&orchestrator, &ProviderOrchestrator::tryingProvider,
                        [](const QString &name, const QString &method) {
            qInfo() << "  [TRYING]" << name << "(" << method << ")";
        });
        
        QObject::connect(&orchestrator, &ProviderOrchestrator::providerSucceeded,
                        [](const QString &name, const QString &method) {
            qInfo() << "  [SUCCESS]" << name << "matched via" << method;
        });
        
        QObject::connect(&orchestrator, &ProviderOrchestrator::providerFailed,
                        [](const QString &name, const QString &error) {
            qInfo() << "  [FAILED]" << name << "-" << error;
        });
        
        // Get files without metadata from database
        QList<FileRecord> files = db.getFilesWithoutHashes();
        int minConfidence = parser.value("min-confidence").toInt();
        
        qInfo() << "Matching" << files.size() << "files with minimum confidence:" << minConfidence << "%";
        qInfo() << "Provider fallback order:";
        QStringList providers = orchestrator.getEnabledProviders();
        for (const QString &p : providers) {
            QString hashSupport = orchestrator.providerSupportsHash(p) ? "✓ hash" : "✗ name only";
            qInfo() << "  -" << p << "(" << hashSupport << ")";
        }
        qInfo() << "";
        
        // Match each file
        MatchingEngine matcher;
        int matched = 0;
        int failed = 0;
        
        for (const FileRecord &file : files) {
            qInfo() << "Matching:" << file.filename;
            
            // Try to get metadata with intelligent fallback
            GameMetadata metadata = orchestrator.searchWithFallback(
                file.crc32,  // Try hash first
                file.filename,
                "" // System would come from file.systemId lookup
            );
            
            if (!metadata.title.isEmpty()) {
                // Use actual confidence from orchestrator
                int confidence = static_cast<int>(metadata.matchScore * 100);
                
                if (confidence >= minConfidence) {
                    qInfo() << "  ✓ MATCHED:" << metadata.title << "(" << confidence << "% confidence)";
                    qInfo() << "    Provider:" << metadata.providerId;
                    qInfo() << "    Method:" << metadata.matchMethod;
                    qInfo() << "    System:" << metadata.system;
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
        qInfo() << "Success rate:" << QString::number((matched * 100.0) / (matched + failed), 'f', 1) + "%";
    }

    // Handle M4 organize command
    if (parser.isSet("organize")) {
        QString destination = parser.value("organize");
        QString templateStr = parser.value("template");
        bool dryRun = parser.isSet("dry-run");

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

        // Get all matched files (requires matches in database)
        // For now, just get all files as a simple implementation
        QList<FileRecord> files = db.getAllFiles();
        
        if (files.isEmpty()) {
            qInfo() << "No files to organize";
        } else {
            qInfo() << "Processing" << files.size() << "files...";
            qInfo() << "";

            // For a simple demo, we'll just preview the first few files
            // Full implementation would require fetching metadata for each file
            int processed = 0;
            for (const FileRecord &file : files) {
                if (processed >= 10) break;  // Limit to 10 for demo
                
                // Create dummy metadata (in real use, fetch from database/matches)
                GameMetadata metadata;
                metadata.title = "Example Game";
                metadata.region = "USA";
                metadata.system = "NES";
                
                OrganizeResult result = organizer.organizeFile(file.id, metadata, destination, FileOperation::Move);
                processed++;
            }

            qInfo() << "";
            qInfo() << "Organization" << (dryRun ? "preview" : "complete");
        }
    }

    // Handle M4 M3U generation command
    if (parser.isSet("generate-m3u")) {
        QString m3uDir = parser.value("m3u-dir");

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

        CHDConversionResult result = converter.extractCHDToCue(chdPath, outputPath);

        if (result.success) {
            qInfo() << "✓ Extraction successful!";
            qInfo() << "  Extracted to:" << outputPath;
        } else {
            qCritical() << "✗ Extraction failed:" << result.error;
            return 1;
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

    qInfo() << "";
    qInfo() << "Done!";

    return 0;
}
