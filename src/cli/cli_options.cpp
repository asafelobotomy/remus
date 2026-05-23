#include "cli_options.h"
#include "../core/constants/constants.h"

using namespace Remus::Constants;

namespace Remus {

void registerAllOptions(QCommandLineParser &parser, QSet<QString> &actionOptions)
{
    actionOptions.insert(QStringLiteral("help"));
    actionOptions.insert(QStringLiteral("h"));
    actionOptions.insert(QStringLiteral("version"));

    const auto addOption = [&parser](const QCommandLineOption &option) {
        parser.addOption(option);
    };
    const auto addActionOption = [&parser, &actionOptions](const QCommandLineOption &option) {
        parser.addOption(option);
        for (const auto &name : option.names()) {
            actionOptions.insert(name);
        }
    };

    addActionOption({{"s", "scan"}, "Scan a directory or file for ROMs (repeatable: -s dir1 -s dir2 -s file.zip)", "path"});
    addOption({{"d", "db"}, "Database file path", "database", Constants::DatabaseSchema::DATABASE_FILENAME});
    addActionOption(QCommandLineOption("hash", "Calculate hashes for scanned files"));
    addActionOption(QCommandLineOption("hash-all", "Calculate hashes for all files in database that lack hashes"));
    addActionOption(QCommandLineOption("reclassify-iso", "Reclassify ISO file rows using current system detection heuristics"));
    addActionOption({{"l", "list"}, "List scanned files by system"});
    addActionOption(QCommandLineOption("stats", "Show library statistics"));
    addActionOption(QCommandLineOption("info", "Show detailed info for a file id", "fileId"));
    addActionOption(QCommandLineOption("header-info", "Inspect ROM header for a file", "file"));
    addActionOption(QCommandLineOption("show-art", "Display an image in terminal (path to image)", "image"));
    addActionOption(QCommandLineOption("check-tools", "Show availability of optional external tools (chdman, dolphin-tool, maxcso)"));

    addActionOption({{"m", "metadata"}, "Fetch metadata by file hash", "hash"});
    addActionOption(QCommandLineOption("search", "Search for game by name", "title"));
    addOption(QCommandLineOption("system", "Specify system for search", "system"));
    const QString providerHelp = QString("Metadata provider (%1, %2, %3, auto)")
        .arg(Providers::SCREENSCRAPER).arg(Providers::THEGAMESDB).arg(Providers::IGDB);
    addOption(QCommandLineOption(Constants::Cli::Options::PROVIDER, providerHelp, "provider", Constants::Cli::Defaults::PROVIDER));
    addOption(QCommandLineOption("tgdb-api-key", "TheGamesDB API key (env: REMUS_TGDB_API_KEY)", "apiKey"));
    addOption(QCommandLineOption("hasheous-api-key", "Hasheous client API key (env: REMUS_HASHEOUS_API_KEY)", "apiKey"));
    addOption(QCommandLineOption("ss-user",    "ScreenScraper username (env: REMUS_SS_USER)",      "username"));
    addOption(QCommandLineOption("ss-pass",    "ScreenScraper password (env: REMUS_SS_PASS)",      "password"));
    addOption(QCommandLineOption("ss-devid",   "ScreenScraper dev ID (env: REMUS_SS_DEVID)",       "devid"));
    addOption(QCommandLineOption("ss-devpass", "ScreenScraper dev password (env: REMUS_SS_DEVPASS)", "devpassword"));
    addOption(QCommandLineOption("igdb-client-id", "IGDB client ID (env: REMUS_IGDB_CLIENT_ID)", "clientId"));
    addOption(QCommandLineOption("igdb-client-secret", "IGDB client secret (env: REMUS_IGDB_CLIENT_SECRET)", "clientSecret"));
    addOption(QCommandLineOption("ra-user",    "RetroAchievements username (env: REMUS_RA_USERNAME)",  "username"));
    addOption(QCommandLineOption("ra-api-key", "RetroAchievements web API key (env: REMUS_RA_API_KEY)", "apiKey"));

    addActionOption(QCommandLineOption("match", "Match scanned files with metadata (M3 intelligent matching)"));
    addOption(QCommandLineOption(Constants::Cli::Options::MIN_CONFIDENCE, "Minimum confidence threshold for matches (0-100)", "confidence", QString::number(static_cast<int>(Constants::Confidence::Thresholds::DEFAULT_MINIMUM))));
    addActionOption(QCommandLineOption("match-report", "Generate detailed matching report with confidence scores"));
    addOption(QCommandLineOption("report-file", "Output file for reports (default: stdout)", "file"));
    addActionOption(QCommandLineOption("enrich", "Enrich matched games with metadata from providers (fills empty description, genre, players)"));

    addActionOption(QCommandLineOption("verify",        "Verify files against DAT file",          "dat-file"));
    addOption(QCommandLineOption("verify-report", "Generate detailed verification report"));
    addActionOption(QCommandLineOption("patch-dat-import", "Deprecated no-op: manual patch catalog import has been replaced by bundled compendium data", "dat-file"));
    addOption(QCommandLineOption("patch-dat-system", "Legacy system name argument for deprecated patch catalog import", "system"));
    addActionOption(QCommandLineOption("patch-dat-list", "List patch catalogs available from the bundled compendium when present"));
    addActionOption(QCommandLineOption("patch-dat-remove", "Deprecated no-op: manual patch catalog removal is no longer required", "system"));

    addActionOption(QCommandLineOption("download-artwork", "Download cover art for matched games"));
    addOption(QCommandLineOption("artwork-dir",   "Directory to store artwork (default: ~/.local/share/Remus/artwork/)", "directory"));
    addOption(QCommandLineOption("artwork-types", "Types of artwork to download (box|screen|manual|all - default: box)", "types", "box"));

    addActionOption(QCommandLineOption("checksum-verify", "Verify specific file checksum (hashes the file as-is; for archives, hashes the container)", "file"));
    addOption(QCommandLineOption("expected-hash",   "Expected hash for verification (crc32|md5|sha1)", "hash"));
    addOption(QCommandLineOption("hash-type",       "Hash type to verify (crc32, md5, sha1 - default: crc32)", "type", "crc32"));

    addActionOption(QCommandLineOption("organize",    "Organize and rename files using template", "destination"));
    addOption(QCommandLineOption("template",    "Naming template (default: No-Intro standard)", "template", Constants::Templates::DEFAULT_NO_INTRO));
    addOption(QCommandLineOption("folder-naming", "Sort into system subfolders (none|default|batocera|retropie|emudeck|romm)", "scheme", "none"));
    addOption(QCommandLineOption("dry-run",     "Preview changes without modifying files"));
    addActionOption(QCommandLineOption("generate-m3u","Generate M3U playlists for multi-disc games"));
    addOption(QCommandLineOption("m3u-dir",     "Directory for M3U playlists (default: same as game files)", "directory"));
    addOption(QCommandLineOption("dry-run-all", "Preview file outputs for all file-writing actions"));

    addActionOption(QCommandLineOption("bundle",        "Fetch metadata, download box art, and repack matched ROMs into self-contained archives", "destination"));
    addOption(QCommandLineOption("bundle-format", "Output archive format for bundles (zip|7z, default: zip)", "format", Constants::Cli::Defaults::BUNDLE_FORMAT));
    addOption(QCommandLineOption("bundle-art-dir","Optional pre-downloaded artwork directory (avoids re-downloading box art)", "directory"));
    addOption(QCommandLineOption("bundle-disc-format", "Disc media packaging inside bundles (original|chd|rvz, default: chd). When chd is requested, the planner auto-selects: RVZ for GameCube/Wii, CSO for PSP, CHD for all other disc formats.", "format", Constants::Cli::Defaults::BUNDLE_DISC_FORMAT));

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
    addActionOption(QCommandLineOption("mod-installed",  "List installed mods"));
    addActionOption(QCommandLineOption("mod-uninstall",  "Remove an installed mod by ID",      "installId"));
    addActionOption(QCommandLineOption("mod-catalog-build",  "Build mod catalog JSON from a local RAPatches clone", "repoPath"));
    addOption(QCommandLineOption("mod-catalog-output", "Output path for generated catalog JSON", "path"));
    addActionOption(QCommandLineOption("mod-enrich-ra",  "Enrich a catalog with RetroAchievements PatchUrl data"));

    addActionOption(QCommandLineOption(Constants::Cli::Options::EXPORT, "Export library (retroarch|emustation|launchbox|csv|json)", "format"));
    addOption(QCommandLineOption(Constants::Cli::Options::EXPORT_PATH, "Export output path (file or directory)", "path"));
    addOption(QCommandLineOption(Constants::Cli::Options::EXPORT_SYSTEMS, "Comma-separated systems to include", "systems"));

    addActionOption(QCommandLineOption("library",
        "Full pipeline: scan→hash→match→enrich→bundle→organize on a ROM directory. "
        "Defaults: zip bundles, CHD disc conversion, system subfolders. "
        "Use --output to specify the destination.",
        "path"));
    addOption(QCommandLineOption("output",
        "Output directory for --library pipeline (organized library root).",
        "directory"));

    addActionOption(QCommandLineOption("process", "Full pipeline: scan→hash→match→enrich→bundle on a directory", "path"));
    addOption(QCommandLineOption("process-output", "Output directory for --process pipeline bundles/organized files", "directory"));
    addOption(QCommandLineOption("process-preset",
        "Frontend preset for --process (es-de|retrodeck|emudeck|batocera|retropie|romm). "
        "Auto-configures bundle format, disc format, and folder naming.",
        "preset"));

    addActionOption(QCommandLineOption("build-compendium", "Build a canonical compendium database from a manifest"));
    addActionOption(QCommandLineOption("enrich-compendium", "Run enrichment passes (GameTDB) against an existing compendium database without rebuilding"));
    addActionOption(QCommandLineOption("ingest-source", "Incrementally ingest a single DAT file into an existing compendium database", "dat-file"));
    addOption(QCommandLineOption("source-id", "Source identifier for --ingest-source (default: derived from filename)", "id"));
    addOption(QCommandLineOption("source-priority", "Source priority for --ingest-source (default: 10; no-intro=20, redump=30)", "n", "10"));
    addActionOption(QCommandLineOption("coverage-report", "Emit a per-source signature-yield coverage report for a compendium database (TSV to stdout)"));
    addOption(QCommandLineOption("compendium-manifest", "Path to compendium source manifest JSON", "path"));
    addOption(QCommandLineOption("compendium-output", "Output SQLite path for compiled compendium", "path", "data/compendium/remus_compendium.db"));

    addActionOption(QCommandLineOption("update-dats", "Deprecated no-op: raw DAT update workflow has been replaced by bundled compendium data"));
    addOption(QCommandLineOption("update-dats-all", "Legacy no-op flag retained for compatibility with --update-dats"));
    addActionOption(QCommandLineOption("import-dat",  "Deprecated no-op: manual DAT import has been replaced by bundled compendium data", "dat-file"));
    addActionOption(QCommandLineOption("remove-dat",  "Deprecated no-op: manual DAT removal is no longer required",                "name"));
    addActionOption(QCommandLineOption("list-dats",   "Deprecated no-op: bundled builds do not manage installed raw DAT files"));
    addActionOption(QCommandLineOption("dat-coverage", "Report DAT coverage against known systems and list uncovered systems"));
    addActionOption(QCommandLineOption("edit-metadata","Edit metadata for a matched file by ID",             "fileId"));
    addOption(QCommandLineOption("set-title",     "Set game title (use with --edit-metadata)",     "title"));
    addOption(QCommandLineOption("set-region",    "Set game region (use with --edit-metadata)",    "region"));
    addOption(QCommandLineOption("set-genre",     "Set game genre (use with --edit-metadata)",     "genre"));
    addOption(QCommandLineOption("set-developer", "Set game developer (use with --edit-metadata)", "developer"));
    addOption(QCommandLineOption("set-publisher", "Set game publisher (use with --edit-metadata)", "publisher"));

    addActionOption(QCommandLineOption("convert-chd",     "Convert disc image to CHD format",                    "path"));
    addOption(QCommandLineOption("chd-codec",       "CHD compression codec (lzma, zlib, flac, huff, auto)", "codec", "auto"));
    addActionOption(QCommandLineOption("chd-extract",     "Extract CHD back to BIN/CUE",                         "chdfile"));
    addActionOption(QCommandLineOption("chd-verify",      "Verify CHD file integrity",                           "chdfile"));
    addActionOption(QCommandLineOption("chd-info",        "Show CHD file information",                           "chdfile"));
    addActionOption(QCommandLineOption("extract-archive", "Extract archive (ZIP/7z/RAR/tar/gz/xz via libarchive)", "path"));
    addActionOption(QCommandLineOption("space-report",    "Show potential CHD conversion savings",               "directory"));
    addActionOption(QCommandLineOption("convert-rvz",     "Convert GameCube/Wii ISO to RVZ format",              "path"));
    addOption(QCommandLineOption("rvz-compression", "RVZ compression (zstd, bzip2, lzma, lzma2, none, auto)", "compression", "auto"));
    addActionOption(QCommandLineOption("rvz-extract",     "Extract RVZ back to ISO",                             "rvzfile"));
    addActionOption(QCommandLineOption("rvz-verify",      "Verify RVZ file integrity",                           "rvzfile"));
    addActionOption(QCommandLineOption("convert-cso",     "Convert PSP ISO to CSO format",                       "path"));
    addActionOption(QCommandLineOption("cso-extract",     "Extract CSO back to ISO",                             "csofile"));
    addActionOption(QCommandLineOption("convert-wbfs",    "Convert GameCube/Wii ISO to WBFS format (requires wit)", "path"));
    addActionOption(QCommandLineOption("wbfs-extract",    "Extract WBFS back to ISO (requires wit)",             "wbfsfile"));
    addActionOption(QCommandLineOption("export-pbp",      "Export PS1 CUE/ISO/M3U to PBP (requires PSXPackager)", "path"));
    addOption(QCommandLineOption("output-dir",      "Output directory for conversions/extractions",         "directory"));

    addOption(QCommandLineOption(Constants::Cli::Options::NO_INTERACTIVE, "Accepted for backwards compatibility (this is a CLI-only build)"));
    addOption(QCommandLineOption(QStringLiteral("log-file"),
                                 QStringLiteral("Write full CLI output to a log file (opt-in; specify a path to enable tee logging)"),
                                 QStringLiteral("path")));
    addOption(QCommandLineOption(QStringLiteral("file-id"),
                                 QStringLiteral("Scope hash/match/enrich/bundle/organize/artwork to a specific database file ID (repeatable: --file-id 1 --file-id 2)"),
                                 QStringLiteral("id")));
}

} // namespace Remus
