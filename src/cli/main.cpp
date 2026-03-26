#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QMessageLogContext>
#include <QSet>
#include <QStringList>
#include <QTextStream>
#include "cli_commands.h"
#include "cli_helpers.h"
#include "../core/constants/constants.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

static bool hasFlag(const QStringList &args, const QString &flag)
{
    return args.contains(flag);
}

static QString normalizeOptionToken(const QString &arg)
{
    if (!arg.startsWith('-')) {
        return {};
    }

    int start = 0;
    while (start < arg.size() && arg.at(start) == '-') {
        ++start;
    }

    if (start >= arg.size()) {
        return {};
    }

    return arg.mid(start).section('=', 0, 0);
}

static bool hasAnyAction(const QStringList &args, const QSet<QString> &actionOptions)
{
    for (const QString &arg : args) {
        const QString token = normalizeOptionToken(arg);
        if (!token.isEmpty() && actionOptions.contains(token)) {
            return true;
        }
    }
    return false;
}

static void printBanner()
{
    qInfo() << "╔════════════════════════════════════════╗";
    qInfo() << "║  Remus - Retro Game Library Manager   ║";
    qInfo() << "║  M4.5: File Conversion & Compression  ║";
    qInfo() << "╚════════════════════════════════════════╝";
    qInfo() << "";
}

static void machineReadableMessageHandler(QtMsgType type,
                                          const QMessageLogContext &context,
                                          const QString &msg)
{
    Q_UNUSED(context);

    if (type == QtDebugMsg || type == QtInfoMsg) {
        return;
    }

    QTextStream stream(stderr);
    stream << msg << Qt::endl;

    if (type == QtFatalMsg) {
        abort();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(Constants::Cli::APPLICATION_NAME);
    QCoreApplication::setOrganizationName(Constants::SETTINGS_ORGANIZATION);
    QCoreApplication::setApplicationVersion(Constants::APP_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("Remus CLI - Scan and catalog retro game ROMs");
    parser.addHelpOption();
    parser.addVersionOption();

    QSet<QString> actionOptions = {
        QStringLiteral("help"),
        QStringLiteral("h"),
        QStringLiteral("version")
    };

    const auto addOption = [&](const QCommandLineOption &option) {
        parser.addOption(option);
    };
    const auto addActionOption = [&](const QCommandLineOption &option) {
        parser.addOption(option);
        for (const auto &name : option.names()) {
            actionOptions.insert(name);
        }
    };

    addActionOption({{"s", "scan"}, "Scan a directory for ROMs", "path"});
    addOption({{"d", "db"}, "Database file path", "database", Constants::DatabaseSchema::DATABASE_FILENAME});
    addActionOption(QCommandLineOption("hash", "Calculate hashes for scanned files"));
    addActionOption(QCommandLineOption("hash-all", "Calculate hashes for all files in database that lack hashes"));
    addActionOption({{"l", "list"}, "List scanned files by system"});
    addActionOption(QCommandLineOption("stats", "Show library statistics"));
    addActionOption(QCommandLineOption("info", "Show detailed info for a file id", "fileId"));
    addActionOption(QCommandLineOption("header-info", "Inspect ROM header for a file", "file"));
    addActionOption(QCommandLineOption("show-art", "Display an image in terminal (path to image)", "image"));

    addActionOption({{"m", "metadata"}, "Fetch metadata by file hash", "hash"});
    addActionOption(QCommandLineOption("search", "Search for game by name", "title"));
    addOption(QCommandLineOption("system", "Specify system for search", "system"));
    const QString providerHelp = QString("Metadata provider (%1, %2, %3, auto)")
        .arg(Providers::SCREENSCRAPER).arg(Providers::THEGAMESDB).arg(Providers::IGDB);
    addOption(QCommandLineOption(Constants::Cli::Options::PROVIDER, providerHelp, "provider", Constants::Cli::Defaults::PROVIDER));
    addOption(QCommandLineOption("tgdb-api-key", "TheGamesDB API key", "apiKey"));
    addOption(QCommandLineOption("ss-user",    "ScreenScraper username",      "username"));
    addOption(QCommandLineOption("ss-pass",    "ScreenScraper password",      "password"));
    addOption(QCommandLineOption("ss-devid",   "ScreenScraper dev ID",        "devid"));
    addOption(QCommandLineOption("ss-devpass", "ScreenScraper dev password",  "devpassword"));
    addOption(QCommandLineOption("igdb-client-id", "IGDB client ID", "clientId"));
    addOption(QCommandLineOption("igdb-client-secret", "IGDB client secret", "clientSecret"));

    addActionOption(QCommandLineOption("match", "Match scanned files with metadata (M3 intelligent matching)"));
    addOption(QCommandLineOption(Constants::Cli::Options::MIN_CONFIDENCE, "Minimum confidence threshold for matches (0-100)", "confidence", QString::number(static_cast<int>(Constants::Confidence::Thresholds::DEFAULT_MINIMUM))));
    addActionOption(QCommandLineOption("match-report", "Generate detailed matching report with confidence scores"));
    addOption(QCommandLineOption("report-file", "Output file for reports (default: stdout)", "file"));
    addActionOption(QCommandLineOption("enrich", "Enrich matched games with metadata from providers (fills empty description, genre, players)"));

    addActionOption(QCommandLineOption("verify",        "Verify files against DAT file",          "dat-file"));
    addActionOption(QCommandLineOption("verify-report", "Generate detailed verification report"));
    addActionOption(QCommandLineOption("patch-dat-import", "Import DAT-style patch catalog", "dat-file"));
    addOption(QCommandLineOption("patch-dat-system", "System name for imported patch catalog", "system"));
    addActionOption(QCommandLineOption("patch-dat-list", "List imported patch catalogs"));
    addActionOption(QCommandLineOption("patch-dat-remove", "Remove imported patch catalog for system", "system"));

    addActionOption(QCommandLineOption("download-artwork", "Download cover art for matched games"));
    addOption(QCommandLineOption("artwork-dir",   "Directory to store artwork (default: ~/.local/share/Remus/artwork/)", "directory"));
    addOption(QCommandLineOption("artwork-types", "Types of artwork to download (box|screen|manual|all - default: box)", "types", "box"));

    addActionOption(QCommandLineOption("checksum-verify", "Verify specific file checksum (hashes the file as-is; for archives, hashes the container)", "file"));
    addOption(QCommandLineOption("expected-hash",   "Expected hash for verification (crc32|md5|sha1)", "hash"));
    addOption(QCommandLineOption("hash-type",       "Hash type to verify (crc32, md5, sha1 - default: crc32)", "type", "crc32"));

    addActionOption(QCommandLineOption("organize",    "Organize and rename files using template", "destination"));
    addOption(QCommandLineOption("template",    "Naming template (default: No-Intro standard)", "template", Constants::Templates::DEFAULT_NO_INTRO));
    addOption(QCommandLineOption("dry-run",     "Preview changes without modifying files"));
    addActionOption(QCommandLineOption("generate-m3u","Generate M3U playlists for multi-disc games"));
    addOption(QCommandLineOption("m3u-dir",     "Directory for M3U playlists (default: same as game files)", "directory"));
    addOption(QCommandLineOption("dry-run-all", "Preview file outputs for all file-writing actions"));

    addActionOption(QCommandLineOption("bundle",        "Fetch metadata, download box art, and repack matched ROMs into self-contained archives", "destination"));
    addOption(QCommandLineOption("bundle-format", "Output archive format for bundles (zip|7z, default: zip)", "format", Constants::Cli::Defaults::BUNDLE_FORMAT));
    addOption(QCommandLineOption("bundle-art-dir","Optional pre-downloaded artwork directory (avoids re-downloading box art)", "directory"));
    addOption(QCommandLineOption("bundle-disc-format", "Disc media packaging inside bundles (original|chd, default: original)", "format", Constants::Cli::Defaults::BUNDLE_DISC_FORMAT));

    addActionOption(QCommandLineOption("patch-apply",    "Apply patch to base file",          "basefile"));
    addOption(QCommandLineOption("patch-patch",    "Patch file to apply",               "patchfile"));
    addOption(QCommandLineOption("patch-output",   "Output file path (optional)",       "output"));
    addActionOption(QCommandLineOption("patch-create",   "Create patch from modified file",   "modifiedfile"));
    addOption(QCommandLineOption("patch-original", "Original file for patch creation",  "originalfile"));
    addOption(QCommandLineOption("patch-format",   "Patch format (ips|bps|ups|xdelta|ppf)", "format", "bps"));
    addActionOption(QCommandLineOption("patch-info",     "Detect patch format for file",      "patchfile"));
    addActionOption(QCommandLineOption("patch-tools",    "List patch tool availability"));

    addOption(QCommandLineOption("mod-catalog",    "Load mod catalog from JSON file",    "path"));
    addOption(QCommandLineOption("mod-catalog-url","Load mod catalog from URL (cached)", "url"));
    addOption(QCommandLineOption("mod-catalog-refresh", "Force re-download of cached catalog"));
    addActionOption(QCommandLineOption("mod-list",       "List available mods for a file ID",  "fileId"));
    addActionOption(QCommandLineOption("mod-show",       "Show detailed information for a catalog mod", "modId"));
    addActionOption(QCommandLineOption("mod-system",     "List available mods for a system name", "system"));
    addActionOption(QCommandLineOption("mod-systems",    "List systems present in the loaded mod catalog"));
    addActionOption(QCommandLineOption("mod-author",     "List catalog mods whose author matches this text", "author"));
    addActionOption(QCommandLineOption("mod-type",       "List catalog mods of a specific type", "type"));
    addActionOption(QCommandLineOption("mod-format",     "List catalog mods with a specific patch format", "format"));
    addActionOption(QCommandLineOption("mod-source-url", "List catalog mods whose source URL matches this text", "text"));
    addActionOption(QCommandLineOption("mod-sort",       "Sort discovery results by title, author, system, type, format, rating, or downloads", "field"));
    addActionOption(QCommandLineOption("mod-min-rating", "List catalog mods with rating >= value", "rating"));
    addActionOption(QCommandLineOption("mod-min-downloads", "List catalog mods with downloads >= value", "count"));
    addOption(QCommandLineOption({Constants::Cli::Options::JSON, Constants::Cli::Options::MOD_JSON}, "Emit machine-readable JSON when supported by the selected command"));
    addOption(QCommandLineOption("mod-no-system-fallback", "Do not fall back to system-level catalog matches for --mod-list"));
    addActionOption(QCommandLineOption("mod-install",    "Install a mod by catalog ID",        "modId"));
    addOption(QCommandLineOption("mod-file",       "Base file ID to apply the mod to",   "fileId"));
    addOption(QCommandLineOption("mod-output",     "Output directory for patched ROM",   "directory"));
    addOption(QCommandLineOption("mod-no-bundle",  "Skip bundling the patched ROM"));
    addActionOption(QCommandLineOption("mod-installed",  "List installed mods"));
    addActionOption(QCommandLineOption("mod-uninstall",  "Remove an installed mod by ID",      "installId"));

    addActionOption(QCommandLineOption(Constants::Cli::Options::EXPORT, "Export library (retroarch|emustation|launchbox|csv|json)", "format"));
    addOption(QCommandLineOption(Constants::Cli::Options::EXPORT_PATH, "Export output path (file or directory)", "path"));
    addOption(QCommandLineOption(Constants::Cli::Options::EXPORT_SYSTEMS, "Comma-separated systems to include", "systems"));

    addActionOption(QCommandLineOption("process", "Run scan->hash->match pipeline on directory", "path"));

    addActionOption(QCommandLineOption("convert-chd",     "Convert disc image to CHD format",                    "path"));
    addOption(QCommandLineOption("chd-codec",       "CHD compression codec (lzma, zlib, flac, huff, auto)", "codec", "auto"));
    addActionOption(QCommandLineOption("chd-extract",     "Extract CHD back to BIN/CUE",                         "chdfile"));
    addActionOption(QCommandLineOption("chd-verify",      "Verify CHD file integrity",                           "chdfile"));
    addActionOption(QCommandLineOption("chd-info",        "Show CHD file information",                           "chdfile"));
    addActionOption(QCommandLineOption("extract-archive", "Extract archive (ZIP/7z/RAR)",                        "path"));
    addActionOption(QCommandLineOption("space-report",    "Show potential CHD conversion savings",               "directory"));
    addOption(QCommandLineOption("output-dir",      "Output directory for conversions/extractions",         "directory"));

    addOption(QCommandLineOption(Constants::Cli::Options::NO_INTERACTIVE, "Accepted for backwards compatibility (this is a CLI-only build)"));

    QStringList activeArgs = app.arguments();
    const bool jsonRequested = hasFlag(activeArgs, "--" + Constants::Cli::Options::JSON) || hasFlag(activeArgs, "--" + Constants::Cli::Options::MOD_JSON);
    const bool actionsProvided   = hasAnyAction(activeArgs, actionOptions);

    if (jsonRequested) {
        qInstallMessageHandler(machineReadableMessageHandler);
    }

    if (!jsonRequested) {
        printBanner();
    }

    parser.process(activeArgs);

    if (!actionsProvided) {
        parser.showHelp(0);
    }

    Database db;
    if (!db.initialize(parser.value("db"))) {
        qCritical() << "Failed to initialize database";
        return 1;
    }

    SystemDetector detector;

    CliContext ctx{parser, db, detector,
                   /*dryRunAll*/        parser.isSet(Constants::Cli::Options::DRY_RUN_ALL),
                   /*processRequested*/ parser.isSet("process")};

    if (int rc = handleStatsCommand(ctx))          return rc;
    if (int rc = handleInfoCommand(ctx))           return rc;
    if (int rc = handleInspectCommands(ctx))       return rc;
    if (int rc = handleScanCommand(ctx))           return rc;
    if (int rc = handleListCommand(ctx))           return rc;
    if (int rc = handleHashAllCommand(ctx))        return rc;
    if (int rc = handleMetadataCommand(ctx))       return rc;
    if (int rc = handleSearchCommand(ctx))         return rc;
    if (int rc = handleEnrichCommand(ctx))         return rc;
    if (int rc = handleMatchCommand(ctx))          return rc;
    if (int rc = handleMatchReportCommand(ctx))    return rc;
    if (int rc = handleChecksumVerifyCommand(ctx)) return rc;
    if (int rc = handleVerifyCommand(ctx))         return rc;
    if (int rc = handlePatchDatCommand(ctx))       return rc;
    if (int rc = handleArtworkCommand(ctx))        return rc;
    if (int rc = handleBundleCommand(ctx))         return rc;
    if (int rc = handleOrganizeCommand(ctx))       return rc;
    if (int rc = handleGenerateM3uCommand(ctx))    return rc;
    if (int rc = handleConvertChdCommand(ctx))     return rc;
    if (int rc = handleChdExtractCommand(ctx))     return rc;
    if (int rc = handleChdVerifyCommand(ctx))      return rc;
    if (int rc = handleChdInfoCommand(ctx))        return rc;
    if (int rc = handleExtractArchiveCommand(ctx)) return rc;
    if (int rc = handleSpaceReportCommand(ctx))    return rc;
    if (int rc = handleExportCommand(ctx))         return rc;
    if (int rc = handlePatchCommands(ctx))         return rc;
    if (int rc = handleModCommands(ctx))           return rc;

    if (!jsonRequested) {
        qInfo() << "";
        qInfo() << "Done!";
    }
    return 0;
}
