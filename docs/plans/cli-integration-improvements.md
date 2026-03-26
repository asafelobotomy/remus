# CLI Integration Improvements Plan

> Generated from integration test run on 2026-03-26.
> Priority: items ordered by user impact (high → low).

## Background

A full integration test exercised all CLI functions against 4 real ROMs (Genesis, SNES x2, PlayStation). Three bugs were found and fixed in this session. The remaining items are UX improvements and metadata enrichment gaps.

## Items

### 1. Export `--export-path` directory handling

**Problem**: Passing a directory to `--export-path` (e.g., `exports/`) fails with "Failed to open" because it tries to open the directory as a file.

**Fix**: In `cli_commands_export.cpp`, detect when `outputPath` is a directory (using `QFileInfo::isDir()`), and auto-append the default filename for the chosen format.

**Scope**: ~10 LOC in `handleExportCommand()`.

**Files**: [src/cli/cli_commands_export.cpp](../src/cli/cli_commands_export.cpp)

---

### 2. Empty region field in exports and metadata

**Problem**: All 3 matched games show empty `region` in CSV, JSON, and EmulationStation exports. Hasheous returns game titles and box art but no region data.

**Options** (choose one):
- **A. Parse region from filename** — filenames like `(USA, Europe)` and `(Japan)` already contain region info. Extract during scan and populate `FileRecord.region`. Low-cost, high-value.
- **B. Enrich from secondary provider** — after hasheous match, query thegamesdb for region/genre/players to fill gaps. Higher cost, more complete.
- **C. Both A + B** — filename parse first (instant), API enrichment as fallback.

**Recommendation**: Option A first — it covers the common case with zero API calls.

**Files**: [src/core/scanner.cpp](../src/core/scanner.cpp), [src/metadata/filename_normalizer.h](../src/metadata/filename_normalizer.h)

---

### 3. Sparse EmulationStation export metadata

**Problem**: EmulationStation gamelist has empty `<desc>`, `<genre>`, `<players>`, `<region>` fields because hasheous provides minimal metadata.

**Root cause**: Same as item 2 — hasheous returns title, system, and box art, but not description/genre/players.

**Fix**: After implementing item 2 (region from filename), add a `--enrich` flag to the match pipeline that queries a secondary provider (thegamesdb or IGDB) for rich metadata fields — description, genre, players, release date. Store in the database and include in exports.

**Scope**: Medium — new enrichment pass in match pipeline, DB schema for extra fields.

---

### 4. `--checksum-verify` on compressed files

**Problem**: `--checksum-verify` hashes the ZIP container, not the inner ROM. This is correct behavior (it verifies what's on disk), but it can confuse users who expect the inner ROM hash.

**Options**:
- **A. Add `--inner-hash` flag** — extract to temp, hash inner file, clean up. Explicit opt-in.
- **B. Auto-detect** — if the file is a known archive and contains a single ROM, hash the inner file by default.
- **C. Document behavior** — add a note to `--help` text explaining that compressed files are hashed as-is.

**Recommendation**: Option C first (cheapest), then A if users request it.

**Files**: [src/cli/cli_commands_verify.cpp](../src/cli/cli_commands_verify.cpp)

---

### 5. `--header-info` scope documentation

**Problem**: `--header-info` returns "not detected" on Genesis ROMs, which may confuse users. This is correct — Genesis ROMs don't have copier headers (the feature detects iNES, SMC, Lynx, FDS, A78 copier headers only).

**Fix**: Improve the "not detected" message to clarify scope: "No copier header detected. Supported: iNES, NES2.0, SMC/SWC, Lynx, FDS, A78."

**Scope**: ~3 LOC.

**Files**: [src/cli/cli_commands_info.cpp](../src/cli/cli_commands_info.cpp)

---

### 6. thegamesdb search returns no results without API key

**Problem**: `--search "Silent Hill" --provider thegamesdb` returns "No results found" even though the game exists. Likely needs API key configuration.

**Fix**: When thegamesdb returns no results and no API key is configured, print a hint: "No API key configured for TheGamesDB. Set with --tgdb-api-key or in settings."

**Scope**: ~5 LOC in `handleSearchCommand()`.

**Files**: [src/cli/cli_commands_metadata.cpp](../src/cli/cli_commands_metadata.cpp)

---

## Priority Matrix

| # | Item | Effort | Impact | Priority | Status |
|---|------|--------|--------|----------|--------|
| 1 | Export path directory handling | S | Medium | High | ✅ Done |
| 2 | Region from filename | S | High | High | ✅ Done |
| 5 | Header-info message clarity | S | Low | Medium | ✅ Done |
| 6 | thegamesdb API key hint | S | Low | Medium | ✅ Done |
| 4 | Checksum-verify documentation | S | Low | Medium | ✅ Done |
| 3 | Rich metadata enrichment | L | High | Low (depends on 2) | ✅ Done |

S = Small (< 20 LOC), M = Medium (20–100 LOC), L = Large (100+ LOC)
