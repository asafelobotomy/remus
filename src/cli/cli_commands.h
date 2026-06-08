#pragma once
// Shared context and handler declarations for CLI command dispatch.
// All handler functions accept a CliContext reference and return:
//   0 → success (or command not applicable — caller continues)
//   1 → fatal error (caller should propagate as process exit code)

#include <QCommandLineParser>
#include <QSet>
#include "../core/database.h"
#include "../core/system_detector.h"

using namespace Remus;

struct CliContext {
    QCommandLineParser &parser;
    Database &db;
    SystemDetector &detector;
    bool dryRunAll;
    bool processRequested;
    bool processHandled;
    int processSystemIdFilter;
    QSet<int> processFileScopeIds;
    // Preset-resolved overrides (empty = use explicit CLI values)
    QString presetBundleFormat;
    QString presetDiscFormat;
    QString presetFolderNaming;
    QString presetDisplayName;
    /// Persistent artwork cache dir for the process pipeline.
    /// Pre-populated during the enrich phase; the bundle phase checks here first
    /// to avoid duplicate provider round-trips across per-system batches.
    QString processArtworkCacheDir;
    /// Canonical source path resolved from --process or --library.
    QString processSourcePath;
    /// Canonical output path resolved from --process-output, --bundle, or --output.
    /// Empty means no output step (hash/match/enrich only).
    QString processOutputPath;
};

// ── Info / inspection ──────────────────────────────────────────────────────────
// --stats, --info, --header-info, --show-art, --scan, --list, --hash-all
int handleStatsCommand(CliContext &ctx);
int handleInfoCommand(CliContext &ctx);
int handleInspectCommands(CliContext &ctx); // header-info + show-art
int handleScanCommand(CliContext &ctx);
int handleListCommand(CliContext &ctx);
int handleHashAllCommand(CliContext &ctx);
int handleReclassifyIsoCommand(CliContext &ctx);
int handleCheckToolsCommand(CliContext &ctx);

// ── Metadata ──────────────────────────────────────────────────────────────────
// --metadata, --search, --enrich
int handleMetadataCommand(CliContext &ctx);
int handleSearchCommand(CliContext &ctx);
int handleEnrichCommand(CliContext &ctx);

// ── Matching ──────────────────────────────────────────────────────────────────
// --match (+ pipeline), --match-report
int handleMatchCommand(CliContext &ctx);
int handleMatchReportCommand(CliContext &ctx);

// ── Verification ──────────────────────────────────────────────────────────────
// --checksum-verify, --verify, --patch-dat-*
int handleChecksumVerifyCommand(CliContext &ctx);
int handleVerifyCommand(CliContext &ctx);
int handlePatchDatCommand(CliContext &ctx);

// ── Organise, bundle & artwork ────────────────────────────────────────────────
// --download-artwork, --bundle, --organize, --generate-m3u
int handleArtworkCommand(CliContext &ctx);
int handleBundleCommand(CliContext &ctx);
int handleOrganizeCommand(CliContext &ctx);
int handleGenerateM3uCommand(CliContext &ctx);

// ── CHD / archive / space ─────────────────────────────────────────────────────
// --convert-chd, --chd-extract, --chd-verify, --chd-info,
// --extract-archive, --space-report
int handleConvertChdCommand(CliContext &ctx);
int handleChdExtractCommand(CliContext &ctx);
int handleChdVerifyCommand(CliContext &ctx);
int handleChdInfoCommand(CliContext &ctx);
int handleExtractArchiveCommand(CliContext &ctx);
int handleSpaceReportCommand(CliContext &ctx);

// ── RVZ / CSO / WBFS conversion ───────────────────────────────────────────────
// --convert-rvz, --rvz-extract, --rvz-verify,
// --convert-cso, --cso-extract,
// --convert-wbfs, --wbfs-extract,
// --export-pbp
int handleConvertRvzCommand(CliContext &ctx);
int handleRvzExtractCommand(CliContext &ctx);
int handleRvzVerifyCommand(CliContext &ctx);
int handleConvertCsoCommand(CliContext &ctx);
int handleCsoExtractCommand(CliContext &ctx);
int handleConvertWbfsCommand(CliContext &ctx);
int handleWbfsExtractCommand(CliContext &ctx);
int handleExportPBPCommand(CliContext &ctx);

// ── Export ────────────────────────────────────────────────────────────────────
// --export
int handleExportCommand(CliContext &ctx);

// ── Patch ─────────────────────────────────────────────────────────────────────
// --patch-tools, --patch-info, --patch-apply, --patch-create
int handlePatchCommands(CliContext &ctx);

// ── Mod Workflow ──────────────────────────────────────────────────────────────
// --mod-catalog, --mod-list, --mod-show, --mod-system, --mod-systems,
// --mod-author, --mod-type, --mod-min-rating,
// --mod-install, --mod-installed, --mod-uninstall,
// --mod-catalog-build, --mod-enrich-ra
int handleModCommands(CliContext &ctx);
int handleModCatalogBuildCommand(CliContext &ctx);

// ── DAT Management & Metadata Editing ─────────────────────────────────────────
// --update-dats, --import-dat, --remove-dat, --list-dats, --dat-coverage, --edit-metadata
int handleBuildCompendiumCommand(CliContext &ctx);
int handleEnrichCompendiumCommand(CliContext &ctx);
int handleIngestSourceCommand(CliContext &ctx);
int handleCoverageReportCommand(CliContext &ctx);
int handleUpdateDatsCommand(CliContext &ctx);
int handleImportDatCommand(CliContext &ctx);
int handleRemoveDatCommand(CliContext &ctx);
int handleListDatsCommand(CliContext &ctx);
int handleDatCoverageCommand(CliContext &ctx);
int handleEditMetadataCommand(CliContext &ctx);
