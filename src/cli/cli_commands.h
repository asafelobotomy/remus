#pragma once
// Shared context and handler declarations for CLI command dispatch.
// All handler functions accept a CliContext reference and return:
//   0 → success (or command not applicable — caller continues)
//   1 → fatal error (caller should propagate as process exit code)

#include <QCommandLineParser>
#include "../core/database.h"
#include "../core/system_detector.h"

using namespace Remus;

struct CliContext {
    QCommandLineParser &parser;
    Database           &db;
    SystemDetector     &detector;
    bool                dryRunAll;
    bool                processRequested;
};

// ── Info / inspection ──────────────────────────────────────────────────────────
// --stats, --info, --header-info, --show-art, --scan, --list, --hash-all
int handleStatsCommand(CliContext &ctx);
int handleInfoCommand(CliContext &ctx);
int handleInspectCommands(CliContext &ctx);  // header-info + show-art
int handleScanCommand(CliContext &ctx);
int handleListCommand(CliContext &ctx);
int handleHashAllCommand(CliContext &ctx);

// ── Metadata ──────────────────────────────────────────────────────────────────
// --metadata, --search
int handleMetadataCommand(CliContext &ctx);
int handleSearchCommand(CliContext &ctx);

// ── Matching ──────────────────────────────────────────────────────────────────
// --match (+ pipeline), --match-report
int handleMatchCommand(CliContext &ctx);
int handleMatchReportCommand(CliContext &ctx);

// ── Verification ──────────────────────────────────────────────────────────────
// --checksum-verify, --verify
int handleChecksumVerifyCommand(CliContext &ctx);
int handleVerifyCommand(CliContext &ctx);

// ── Organise & artwork ────────────────────────────────────────────────────────
// --download-artwork, --organize, --generate-m3u
int handleArtworkCommand(CliContext &ctx);
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

// ── Export ────────────────────────────────────────────────────────────────────
// --export
int handleExportCommand(CliContext &ctx);

// ── Patch ─────────────────────────────────────────────────────────────────────
// --patch-tools, --patch-info, --patch-apply, --patch-create
int handlePatchCommands(CliContext &ctx);
