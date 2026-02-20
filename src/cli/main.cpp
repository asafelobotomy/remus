#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QStringList>
#include "cli_commands.h"
#include "cli_helpers.h"
#include "../core/constants/constants.h"
#include "interactive_session.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Cli;
using namespace Remus::Constants;

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
        if (actionFlags.contains(arg)) return true;
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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("remus-cli");
    QCoreApplication::setOrganizationName("Remus");
    QCoreApplication::setApplicationVersion(Constants::APP_VERSION);

    printBanner();

    QStringList activeArgs = app.arguments();
    const bool interactiveFlag   = hasFlag(activeArgs, "--interactive");
    const bool noInteractiveFlag = hasFlag(activeArgs, "--no-interactive");
    const bool actionsProvided   = hasAnyAction(activeArgs);

    if (interactiveFlag || (!noInteractiveFlag && !actionsProvided)) {
        InteractiveSession session;
        InteractiveResult selection = session.run();
        if (!selection.valid || selection.args.isEmpty()) return 0;
        activeArgs = selection.args;
    }

    QCommandLineParser parser;
    parser.setApplicationDescription("Remus CLI - Scan and catalog retro game ROMs");
    parser.addHelpOption();
    parser.addVersionOption();

    // Core options
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
        .arg(Providers::SCREENSCRAPER).arg(Providers::THEGAMESDB).arg(Providers::IGDB);
    parser.addOption(QCommandLineOption("provider", providerHelp, "provider", "auto"));
    parser.addOption(QCommandLineOption("ss-user",    "ScreenScraper username",      "username"));
    parser.addOption(QCommandLineOption("ss-pass",    "ScreenScraper password",      "password"));
    parser.addOption(QCommandLineOption("ss-devid",   "ScreenScraper dev ID",        "devid"));
    parser.addOption(QCommandLineOption("ss-devpass", "ScreenScraper dev password",  "devpassword"));

    // M3 Matching options
    parser.addOption(QCommandLineOption("match", "Match scanned files with metadata (M3 intelligent matching)"));
    parser.addOption(QCommandLineOption("min-confidence", "Minimum confidence threshold for matches (0-100)", "confidence", "60"));
    parser.addOption(QCommandLineOption("match-report", "Generate detailed matching report with confidence scores"));
    parser.addOption(QCommandLineOption("report-file", "Output file for reports (default: stdout)", "file"));

    // Verification options
    parser.addOption(QCommandLineOption("verify",        "Verify files against DAT file",          "dat-file"));
    parser.addOption(QCommandLineOption("verify-report", "Generate detailed verification report"));

    // Artwork options
    parser.addOption(QCommandLineOption("download-artwork", "Download cover art for matched games"));
    parser.addOption(QCommandLineOption("artwork-dir",   "Directory to store artwork (default: ~/.local/share/Remus/artwork/)", "directory"));
    parser.addOption(QCommandLineOption("artwork-types", "Types of artwork to download (box|screen|manual|all - default: box)", "types", "box"));

    // Checksum verification
    parser.addOption(QCommandLineOption("checksum-verify", "Verify specific file checksum", "file"));
    parser.addOption(QCommandLineOption("expected-hash",   "Expected hash for verification (crc32|md5|sha1)", "hash"));
    parser.addOption(QCommandLineOption("hash-type",       "Hash type to verify (crc32, md5, sha1 - default: crc32)", "type", "crc32"));

    // M4 Organise & Rename options
    parser.addOption(QCommandLineOption("organize",    "Organize and rename files using template", "destination"));
    parser.addOption(QCommandLineOption("template",    "Naming template (default: No-Intro standard)", "template", Constants::Templates::DEFAULT_NO_INTRO));
    parser.addOption(QCommandLineOption("dry-run",     "Preview changes without modifying files"));
    parser.addOption(QCommandLineOption("generate-m3u","Generate M3U playlists for multi-disc games"));
    parser.addOption(QCommandLineOption("m3u-dir",     "Directory for M3U playlists (default: same as game files)", "directory"));
    parser.addOption(QCommandLineOption("dry-run-all", "Preview file outputs for all file-writing actions"));

    // Patch options
    parser.addOption(QCommandLineOption("patch-apply",    "Apply patch to base file",          "basefile"));
    parser.addOption(QCommandLineOption("patch-patch",    "Patch file to apply",               "patchfile"));
    parser.addOption(QCommandLineOption("patch-output",   "Output file path (optional)",       "output"));
    parser.addOption(QCommandLineOption("patch-create",   "Create patch from modified file",   "modifiedfile"));
    parser.addOption(QCommandLineOption("patch-original", "Original file for patch creation",  "originalfile"));
    parser.addOption(QCommandLineOption("patch-format",   "Patch format (ips|bps|ups|xdelta|ppf)", "format", "bps"));
    parser.addOption(QCommandLineOption("patch-info",     "Detect patch format for file",      "patchfile"));
    parser.addOption(QCommandLineOption("patch-tools",    "List patch tool availability"));

    // Export options
    parser.addOption(QCommandLineOption("export",         "Export library (retroarch|emustation|launchbox|csv|json)", "format"));
    parser.addOption(QCommandLineOption("export-path",    "Export output path (file or directory)", "path"));
    parser.addOption(QCommandLineOption("export-systems", "Comma-separated systems to include",     "systems"));

    // Processing pipeline
    parser.addOption(QCommandLineOption("process", "Run scan->hash->match pipeline on directory", "path"));

    // M4.5 Conversion & Compression options
    parser.addOption(QCommandLineOption("convert-chd",     "Convert disc image to CHD format",                    "path"));
    parser.addOption(QCommandLineOption("chd-codec",       "CHD compression codec (lzma, zlib, flac, huff, auto)", "codec", "auto"));
    parser.addOption(QCommandLineOption("chd-extract",     "Extract CHD back to BIN/CUE",                         "chdfile"));
    parser.addOption(QCommandLineOption("chd-verify",      "Verify CHD file integrity",                           "chdfile"));
    parser.addOption(QCommandLineOption("chd-info",        "Show CHD file information",                           "chdfile"));
    parser.addOption(QCommandLineOption("extract-archive", "Extract archive (ZIP/7z/RAR)",                        "path"));
    parser.addOption(QCommandLineOption("space-report",    "Show potential CHD conversion savings",               "directory"));
    parser.addOption(QCommandLineOption("output-dir",      "Output directory for conversions/extractions",         "directory"));

    // Interactive options
    parser.addOption(QCommandLineOption("interactive",    "Launch interactive TUI (default when no actions provided)"));
    parser.addOption(QCommandLineOption("no-interactive", "Disable interactive TUI (script-friendly)"));

    parser.process(activeArgs);

    // -- Database & system initialisation ---------------------------------------

    Database db;
    if (!db.initialize(parser.value("db"))) {
        qCritical() << "Failed to initialize database";
        return 1;
    }

    SystemDetector detector;
    for (const QString &name : Systems::getSystemInternalNames()) {
        SystemInfo info = detector.getSystemInfo(name);
        if (!info.name.isEmpty()) db.insertSystem(info);
    }

    // -- Build shared context --------------------------------------------------

    CliContext ctx{parser, db, detector,
                   /*dryRunAll*/        parser.isSet("dry-run-all"),
                   /*processRequested*/ parser.isSet("process")};

    // -- Dispatch commands -----------------------------------------------------

    if (int rc = handleStatsCommand(ctx))          return rc;
    if (int rc = handleInfoCommand(ctx))           return rc;
    if (int rc = handleInspectCommands(ctx))       return rc;
    if (int rc = handleScanCommand(ctx))           return rc;
    if (int rc = handleListCommand(ctx))           return rc;
    if (int rc = handleHashAllCommand(ctx))        return rc;
    if (int rc = handleMetadataCommand(ctx))       return rc;
    if (int rc = handleSearchCommand(ctx))         return rc;
    if (int rc = handleMatchCommand(ctx))          return rc;
    if (int rc = handleMatchReportCommand(ctx))    return rc;
    if (int rc = handleChecksumVerifyCommand(ctx)) return rc;
    if (int rc = handleVerifyCommand(ctx))         return rc;
    if (int rc = handleArtworkCommand(ctx))        return rc;
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

    qInfo() << "";
    qInfo() << "Done!";
    return 0;
}
