# Mod Workflow — Implementation Plan

> Companion to [ADR-0001](../adr/adr-0001-remote-mod-catalog-and-patch-workflow.md).
> Complexity: **XL** — decomposed into 5 independent phases.

---

## Summary

Users select a matched game, browse available mods from a remote catalog, pick one,
and Remus downloads the patch, applies it to the original ROM (leaving it intact),
and bundles the patched ROM with metadata and artwork.

---

## Phase 1 — Local JSON Catalog + CLI Prototype

**Goal**: End-to-end mod install from a local catalog file, CLI-only.
**Dependencies**: None (existing patch engine, bundler, DB).
**Estimated LOC**: ~400 new, ~50 modified.

### 1.1 JSON Catalog Schema (v1)

File: any local `.json` file the user provides via `--mod-catalog <path>`.

```json
{
  "catalog_version": 1,
  "updated_at": "2026-03-23T00:00:00Z",
  "mods": [
    {
      "id": "dq3-eng-v2",
      "title": "Dragon Quest III English Translation",
      "author": "DQ Translations",
      "version": "2.0",
      "description": "Full English translation of DQ3 for SFC.",
      "type": "translation",
      "system": "Super Nintendo",
      "format": "bps",
      "patch_url": "https://example.com/patches/dq3-eng-v2.bps",
      "patch_sha1": "abc123...",
      "patch_size": 524288,
      "base_rom_hashes": {
        "crc32": "AABBCCDD",
        "md5": "0123456789abcdef0123456789abcdef",
        "sha1": "da39a3ee5e6b4b0d3255bfef95601890afd80709"
      },
      "source_url": "https://www.romhacking.net/translations/1234/",
      "rating": 4.9,
      "downloads": 50000
    }
  ]
}
```

Key design decisions:
- `base_rom_hashes` keys the mod to a specific verified dump (hash-first matching).
- `type` maps directly to `FileRecord::fileType` (`translation`, `hack`, `improvement`, `homebrew`).
- `format` maps to `PatchFormat` enum (ips, bps, ups, xdelta, ppf).
- `patch_sha1` enables integrity verification after download.
- `patch_url` is the download URL. Phase 1 supports `file://` for local testing.

### 1.2 New Class: `ModCatalogProvider`

**File**: `src/services/mod_catalog_provider.h` / `.cpp`

```cpp
namespace Remus {

struct ModEntry {
    QString id;
    QString title;
    QString author;
    QString version;
    QString description;
    QString type;         // "translation", "hack", "improvement", "homebrew"
    QString system;
    QString format;       // "ips", "bps", "ups", "xdelta", "ppf"
    QString patchUrl;
    QString patchSha1;
    qint64  patchSize = 0;

    // Base ROM identification (hash-first)
    QString baseCrc32;
    QString baseMd5;
    QString baseSha1;

    // Display metadata
    QString sourceUrl;
    double  rating = 0.0;
    int     downloads = 0;
};

class ModCatalogProvider {
public:
    /// Load catalog from a local JSON file.
    bool loadFromFile(const QString &path);

    /// Load catalog from a remote URL (Phase 3).
    // bool loadFromUrl(const QUrl &url);

    /// Find mods whose base_rom_hashes match the given file hashes.
    QList<ModEntry> findModsForRom(const QString &crc32,
                                   const QString &md5,
                                   const QString &sha1) const;

    /// Find mods by system name.
    QList<ModEntry> findModsBySystem(const QString &system) const;

    /// Get a single mod by ID.
    std::optional<ModEntry> getModById(const QString &id) const;

    /// All loaded mods.
    const QList<ModEntry> &allMods() const;

private:
    QList<ModEntry> m_mods;
};

} // namespace Remus
```

### 1.3 New Class: `ModWorkflowService`

**File**: `src/services/mod_workflow_service.h` / `.cpp`

Orchestrates the full pipeline: download patch → verify → apply → register → bundle.

```cpp
namespace Remus {

struct ModInstallResult {
    bool    success = false;
    QString error;
    QString patchedRomPath;    // Path to the patched ROM file
    QString bundlePath;        // Path to the final bundle archive (if bundled)
    int     patchedFileId = 0; // files.id of the new patched ROM record
};

class ModWorkflowService {
public:
    using ProgressCallback = std::function<void(const QString &stage, int percent)>;

    ModWorkflowService(Database &db, PatchService &patchService);

    /// Install a mod for a matched file.
    ///
    /// Steps:
    /// 1. Download patch from modEntry.patchUrl to a temp file
    /// 2. Verify patch SHA1 against modEntry.patchSha1
    /// 3. Extract base ROM if compressed
    /// 4. Apply patch via PatchService::apply()
    /// 5. Insert patched ROM as new files row (is_patched=true, parent_file_id)
    /// 6. Record in mod_installations table
    /// 7. Optionally bundle via RomBundler::bundleStaged()
    ///
    /// The original FileRecord is NEVER modified.
    ModInstallResult install(const FileRecord           &baseFile,
                             const Database::MatchResult &baseMatch,
                             const ModEntry              &mod,
                             const QString               &outputDir,
                             bool                         bundle = true,
                             ProgressCallback             cb = nullptr);

    /// List installed mods for a file.
    QList<ModInstallResult> getInstalledMods(int baseFileId);

    /// Uninstall a mod (delete patched file + remove DB records).
    bool uninstall(int modInstallationId);

private:
    Database     &m_db;
    PatchService &m_patchSvc;
};

} // namespace Remus
```

### 1.4 Database Migration: `mod_installations` Table

Added in `Database::runMigrations()` as a new migration step.

```sql
CREATE TABLE IF NOT EXISTS mod_installations (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    base_file_id      INTEGER NOT NULL,
    patched_file_id   INTEGER,
    catalog_mod_id    TEXT NOT NULL,
    mod_title         TEXT NOT NULL,
    mod_author        TEXT,
    mod_version       TEXT,
    mod_type          TEXT DEFAULT 'hack',
    patch_format      TEXT,
    patch_url         TEXT,
    patch_sha1        TEXT,
    source_url        TEXT,
    installed_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (base_file_id) REFERENCES files(id) ON DELETE CASCADE,
    FOREIGN KEY (patched_file_id) REFERENCES files(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_mod_installations_base
    ON mod_installations(base_file_id);
CREATE INDEX IF NOT EXISTS idx_mod_installations_catalog
    ON mod_installations(catalog_mod_id);
```

**Relationship to existing tables**:
- `applied_patches` continues to store the raw patch lineage (base hashes, output
  hashes, patch checksums). `ModWorkflowService::install()` writes both tables.
- `mod_installations` adds the catalog layer: which catalog mod was installed, from
  where, by whom, and links the patched `files` row to the base.

### 1.5 `RomBundler::bundleStaged()` — Non-Mutating Bundle

New method on `RomBundler` that bundles a file *without* calling `updateFilePath` or
`markFileProcessed` on the base record. It operates on the patched file's own record.

```cpp
/// Bundle a staged file (e.g. a freshly patched ROM) into an archive.
///
/// Unlike bundle(), this does NOT mutate the passed-in FileRecord in the DB.
/// Instead, it marks the patched file's own record as processed.
BundleResult bundleStaged(const FileRecord              &patchedFile,
                          const Database::MatchResult   &baseMatch,
                          const GameMetadata            &metadata,
                          const QString                 &destinationDir,
                          const BundleConfig            &config);
```

Implementation: extract the non-mutating parts of `bundle()` into a shared private
helper, then have both `bundle()` and `bundleStaged()` call it, differing only in
which `files.id` gets `markFileProcessed`.

### 1.6 CLI Commands

New options in `main.cpp`, handler in a new `src/cli/cli_commands_mods.cpp`:

```
--mod-catalog <path>      Load mod catalog from JSON file
--mod-list <fileId>       List available mods for a matched file
--mod-install <modId>     Install a mod (requires --mod-catalog and --db)
  --mod-file <fileId>       Base file to apply the mod to
  --mod-output <dir>        Output directory for patched ROM (default: beside original)
  --mod-bundle              Bundle the patched ROM (default: true)
  --mod-no-bundle           Skip bundling
--mod-installed            List all installed mods
--mod-uninstall <installId> Remove an installed mod
```

Example session:
```
$ remus-cli --db library.db --mod-catalog mods.json --mod-list 42

=== Available Mods for "Dragon Quest III (Japan)" ===
ID             Title                              Type         Format  Rating
dq3-eng-v2     Dragon Quest III English v2.0       translation  BPS     4.9
dq3-balance    Dragon Quest III Balance Hack        hack         IPS     3.8

$ remus-cli --db library.db --mod-catalog mods.json \
    --mod-install dq3-eng-v2 --mod-file 42 --mod-output ./patched

Downloading patch: dq3-eng-v2.bps (512 KB)...  ✓
Verifying patch SHA1...  ✓
Extracting base ROM...  ✓
Applying BPS patch...  ✓
Registering patched ROM in database...  ✓
Bundling: Dragon Quest III (Japan) [English v2.0].zip...  ✓

Done. Installed mod "dq3-eng-v2" → ./patched/Dragon Quest III (Japan) [English v2.0].zip
Original ROM untouched: ./roms/Dragon Quest III (Japan).zip
```

### 1.7 Test Plan

New test file: `tests/test_mod_workflow.cpp`

| # | Test | Validates |
|---|------|-----------|
| 1 | `loadCatalog_validJson` | `ModCatalogProvider::loadFromFile` parses sample catalog |
| 2 | `loadCatalog_invalidJson` | Graceful failure on malformed JSON |
| 3 | `findModsForRom_hashMatch` | `findModsForRom` returns mods matching SHA1 |
| 4 | `findModsForRom_noMatch` | Returns empty list when no hashes match |
| 5 | `install_happyPath` | Full pipeline: download (file://) → patch → register → bundle |
| 6 | `install_patchVerificationFails` | SHA1 mismatch after download aborts install |
| 7 | `install_baseRomUnchanged` | Asserts base FileRecord is identical before/after install |
| 8 | `install_patchedFileRegistered` | New files row exists with is_patched=true, correct parent |
| 9 | `install_modInstallationRecorded` | mod_installations row exists with correct fields |
| 10 | `bundleStaged_doesNotMutateBase` | bundleStaged calls markFileProcessed on patched, not base |
| 11 | `uninstall_removesFiles` | Patched file and DB records cleaned up |

---

## Phase 2 — Remote Catalog Download + Caching

**Goal**: Fetch the catalog JSON from a URL and cache locally.
**Dependencies**: Phase 1.
**Estimated LOC**: ~150 new.

### 2.1 Changes

- Add `ModCatalogProvider::loadFromUrl(const QUrl &url)` using `ArtworkDownloader::downloadToMemory`.
- Cache downloaded catalog to `~/.local/share/remus/mod_catalog_cache.json`.
- Add `--mod-catalog-url <url>` CLI option as an alternative to `--mod-catalog <path>`.
- Cache TTL: 24 hours by default, `--mod-catalog-refresh` forces re-download.
- ETag/If-Modified-Since header support for bandwidth efficiency.

### 2.2 New DB Table: `mod_catalog_cache`

```sql
CREATE TABLE IF NOT EXISTS mod_catalog_cache (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    source_url  TEXT NOT NULL UNIQUE,
    etag        TEXT,
    fetched_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    mod_count   INTEGER DEFAULT 0
);
```

This tracks when each catalog source was last fetched, enabling smart refresh.

### 2.3 Test Plan

| # | Test | Validates |
|---|------|-----------|
| 1 | `loadFromUrl_fetchesAndCaches` | Downloads catalog, writes cache file |
| 2 | `loadFromUrl_usesCacheWhenFresh` | Returns cached data without hitting network |
| 3 | `loadFromUrl_refreshesWhenStale` | Re-fetches after TTL expires |
| 4 | `loadFromUrl_networkError` | Falls back to stale cache gracefully |

---

## Phase 3 — Patch Download with Integrity Verification

**Goal**: Secure patch downloading with SHA1 verification and progress reporting.
**Dependencies**: Phase 1 (uses `ModWorkflowService::install()`).
**Estimated LOC**: ~100 new.

### 3.1 Changes

- Extract patch download into `ModWorkflowService::downloadPatch()` private method.
- Verify downloaded patch file SHA1 against `ModEntry::patchSha1`.
- Report download progress via `ProgressCallback` (stage="downloading", percent=0..100).
- Support HTTP redirects (romhacking.net uses them).
- Temp file cleanup on failure.
- CLI progress bar for downloads.

### 3.2 Security Considerations

- **SHA1 verification is mandatory** — if `patchSha1` is present in the catalog, the
  downloaded file must match. Abort on mismatch.
- **No arbitrary URL following** — only download from URLs in the loaded catalog.
- **Temp directory isolation** — patches download to a `QTemporaryDir`, cleaned on exit.
- **No shell command construction from URLs** — patches are applied via the existing
  `PatchEngine` which uses `QProcess` with argument lists, not shell strings.

---

## Phase 4 — GUI and TUI Integration

**Goal**: Expose mod browsing and installation in the GUI (QML) and TUI.
**Dependencies**: Phases 1-3.
**Estimated LOC**: ~350 new.

### 4.1 New Controller: `ModController`

**File**: `src/ui/controllers/mod_controller.h` / `.cpp`

QML-exposed controller wrapping `ModCatalogProvider` and `ModWorkflowService`:

```cpp
class ModController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList availableMods READ availableMods NOTIFY modsChanged)
    Q_PROPERTY(QVariantList installedMods READ installedMods NOTIFY modsChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)

public:
    Q_INVOKABLE void loadCatalog(const QString &pathOrUrl);
    Q_INVOKABLE void refreshMods(int fileId);
    Q_INVOKABLE void installMod(int fileId, const QString &modId, const QString &outputDir);
    Q_INVOKABLE void uninstallMod(int installationId);

signals:
    void modsChanged();
    void loadingChanged();
    void installProgress(const QString &stage, int percent);
    void installComplete(bool success, const QString &message);
};
```

### 4.2 QML View: `ModBrowserView.qml`

A panel accessible from the matched game detail view:

- **Mod list**: title, author, type badge, rating stars, download count
- **Install button**: triggers download → patch → bundle pipeline with progress
- **Installed tab**: lists installed mods with uninstall option
- **Base ROM indicator**: shows which verified ROM the mod targets

### 4.3 TUI: `ModScreen`

**File**: `src/tui/mod_screen.cpp`

Table-based mod browser using the existing TUI framework:

```
╔══════════════════════════════════════════════════════════════╗
║ Mods for: Dragon Quest III (Japan)                [4 found] ║
╠══════════════════════════════════════════════════════════════╣
║ ▶ [1] Dragon Quest III English Translation v2.0             ║
║       Type: Translation | BPS | ⭐ 4.9 | 50K downloads      ║
║   [2] Dragon Quest III Balance Remaster                     ║
║       Type: Hack | IPS | ⭐ 3.8 | 12K downloads             ║
╠══════════════════════════════════════════════════════════════╣
║ [i] Install  [d] Details  [q] Back                          ║
╚══════════════════════════════════════════════════════════════╝
```

---

## Phase 5 — Community Catalog + romhacking.net Scraping

**Goal**: Build a catalog automatically from romhacking.net metadata.
**Dependencies**: Phases 1-3. Independent of Phase 4.
**Estimated LOC**: ~300 new.
**Note**: This phase carries the most risk due to scraping fragility.

### 5.1 New Class: `RomhackingScraper`

**File**: `src/metadata/romhacking_scraper.h` / `.cpp`

Scrapes romhacking.net to build a `ModEntry` list:

- Rate-limited: 1 request per 2 seconds (per requirements doc).
- Respects `robots.txt`.
- Extracts: title, author, version, system, format, base ROM CRC32, download URL, rating.
- Outputs: JSON catalog file compatible with `ModCatalogProvider`.
- **Offline tool** — runs as a CLI command to generate the catalog, not at browse time.

### 5.2 CLI Commands

```
--mod-scrape <system>          Scrape romhacking.net for patches for a system
  --mod-scrape-output <path>   Output JSON catalog path
  --mod-scrape-limit <n>       Max pages to scrape (default: 10)
```

### 5.3 Community Catalog Hosting

The generated catalog JSON can be:
- Hosted on a GitHub Pages site under the project org
- Shared via any static file host
- Users point `--mod-catalog-url` at the hosted catalog

This decouples the scraping cadence from Remus releases.

---

## File Inventory

### New Files

| File | Phase | Purpose | Est. LOC |
|------|-------|---------|----------|
| `src/services/mod_catalog_provider.h` | 1 | Catalog loading and querying | 40 |
| `src/services/mod_catalog_provider.cpp` | 1 | Catalog implementation | 120 |
| `src/services/mod_workflow_service.h` | 1 | Install orchestration interface | 50 |
| `src/services/mod_workflow_service.cpp` | 1 | Install orchestration implementation | 200 |
| `src/cli/cli_commands_mods.cpp` | 1 | CLI handlers for mod commands | 150 |
| `tests/test_mod_workflow.cpp` | 1 | Unit tests for catalog + workflow | 250 |
| `tests/fixtures/test_mod_catalog.json` | 1 | Sample catalog for tests | 30 |
| `src/ui/controllers/mod_controller.h` | 4 | QML mod controller | 60 |
| `src/ui/controllers/mod_controller.cpp` | 4 | QML mod controller impl | 150 |
| `src/tui/mod_screen.cpp` | 4 | TUI mod browser | 120 |
| `src/metadata/romhacking_scraper.h` | 5 | Scraper interface | 30 |
| `src/metadata/romhacking_scraper.cpp` | 5 | Scraper implementation | 250 |

### Modified Files

| File | Phase | Change | Est. LOC Δ |
|------|-------|--------|------------|
| `src/core/database.h` | 1 | Add `ModInstallationRecord`, `insertModInstallation()`, `getModInstallations()`, `removeModInstallation()` | +30 |
| `src/core/database.cpp` | 1 | Migration for `mod_installations` table + CRUD methods | +80 |
| `src/core/rom_bundler.h` | 1 | Add `bundleStaged()` declaration | +8 |
| `src/core/rom_bundler.cpp` | 1 | Extract shared helper, implement `bundleStaged()` | +40, refactor ~30 |
| `src/cli/main.cpp` | 1 | Register `--mod-*` options, dispatch to handler | +25 |
| `src/cli/cli_commands.h` | 1 | Declare `handleModCommands()` | +2 |
| `src/services/CMakeLists.txt` | 1 | Add new source files | +4 |
| `tests/CMakeLists.txt` | 1 | Add `test_mod_workflow` | +5 |

---

## Sequencing Diagram

```
Phase 1 ─── Local catalog + CLI prototype
  │           └── Tests: catalog parsing, install pipeline, bundleStaged
  │
Phase 2 ─── Remote catalog fetch + cache
  │           └── Tests: network fetch, cache freshness, offline fallback
  │
Phase 3 ─── Patch download + SHA1 verification
  │           └── Tests: download integrity, progress reporting
  │
  ├── Phase 4 ─── GUI (QML) + TUI integration
  │                 └── Manual testing: browse, install, uninstall
  │
  └── Phase 5 ─── romhacking.net scraper + community catalog
                    └── Tests: scraper output, rate limiting, catalog generation
```

Phase 4 and 5 are independent of each other and can proceed in parallel.

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| romhacking.net changes HTML structure | Scraper breaks | Decouple scraping from runtime; regenerate catalog offline |
| Patch URL returns 404 or different file | Install fails | SHA1 verification catches mismatches; clear error message |
| Large patch files (disc games, XDelta) | Slow download, disk usage | Progress reporting; temp file cleanup; size shown before install |
| Base ROM not in library | Mod found but can't apply | `findModsForRom` only returns mods matching library hashes |
| Catalog grows very large | Slow loading | Index by hash in-memory (QHash); lazy-load by system |

---

## Success Criteria

- [ ] Phase 1 passing all 11 tests in `test_mod_workflow.cpp`
- [ ] `--mod-list` shows mods matching a library ROM by hash
- [ ] `--mod-install` produces a bundled patched ROM with metadata
- [ ] Original ROM file and DB record are provably unchanged after install
- [ ] Patched ROM appears in `--list` output with `[patched]` indicator
- [ ] `--mod-uninstall` cleanly removes the patched ROM and DB records
