# Remus Pipeline Architecture Plan — 2026-05-21

## Purpose

This document defines the single-responsibility contract for each pipeline stage, maps the
current violations against that contract, and provides a phased execution plan to reach the
target architecture. It supersedes the per-stage improvement items in
`improvement-plan-2026-05-20.md` (E2E findings are preserved and re-phased below) and
establishes the stage-separation refactor as the highest-priority architectural work.

---

## Stage Contract (Target Architecture)

Each stage owns exactly one class of work. No stage may perform work owned by a later stage.

| Stage | `--` flag | Owns | Must NOT do |
|-------|-----------|------|-------------|
| **Scan** | `--scan` | Index files: path, filename, extension, size, compression envelope, archive membership, disc-image platform classification (from local magic bytes only) | Query any external provider; populate title, publisher, developer, description, genre, players, rating, release date, artwork |
| **Hash** | `--hash-all` | Compute and persist CRC32 / MD5 / SHA1 for each indexed file | Identify the ROM; populate any game metadata field |
| **Match** | `--match` | Identify which ROM entry a file corresponds to using its hash or normalised name; persist a match record (file → game identity link, confidence, method) | Populate publisher, developer, description, genre, players, rating, release date, box art URL |
| **Enrich** | `--enrich` / `--download-artwork` | Fetch and persist all enrichment data: description, genre, publisher, developer, release date, players, rating, box art path | Repack or move files |
| **Bundle** | `--bundle` | Repack matched ROMs into self-contained archives using already-persisted metadata and already-downloaded artwork | Fetch metadata from any provider; download artwork |
| **Organize** | `--organize` | Rename and move bundle files using the naming template | Anything other than file renaming/moving |

> **Guiding principle:** if a stage fails or is skipped, no downstream stage should silently
> produce incorrect results. The match stage produces an identity link; the enrich stage
> produces metadata. They are separate database operations that can be separately audited,
> retried, and reported on.

---

## Current Violations

### V1 — Match stage persists full metadata (Medium–High)

**Location:** `src/cli/cli_helpers.cpp` — `persistMetadata()` (line 298)

```cpp
int gameId = db.insertGame(metadata.title, systemId, region,
    metadata.publisher,   // ← enrichment data
    metadata.developer,   // ← enrichment data
    metadata.releaseDate, // ← enrichment data
    metadata.description, // ← enrichment data
    genres,               // ← enrichment data
    players,              // ← enrichment data
    metadata.rating);     // ← enrichment data
```

`persistMetadata()` is called during `--match`. The match stage is responsible only for
identity resolution (hash → game row + match record), but it currently writes every enrichment
field the provider returned in the same call. This means:

- Enrich-stage data (publisher, description, etc.) is written with match-stage confidence,
  not enrichment-stage provenance.
- If `--enrich` is run later it may silently overwrite or conflict with values already written
  during `--match`.
- The two operations are not independently auditable or retryable.
- `--match` implicitly depends on the provider returning enrichment data, coupling the identity
  lookup to the enrichment lookup in a single provider round-trip.

**Also present in:** `src/gui/controllers/match_controller.cpp` lines 297–314 — the GUI match
path makes the same `insertGame()` call with all enrichment fields.

---

### V2 — Bundle stage downloads artwork (Medium)

**Location:** `src/cli/cli_commands_bundle.cpp` — `handleBundleCommand()` (line ~165 onward)

The bundle command constructs an `ArtworkDownloader` and conditionally fetches box art from a
provider when `--bundle-art-dir` is not pre-populated. This means:

- Running `--bundle` without `--enrich` first silently triggers provider network calls.
- Artwork downloaded during bundling is stored in a `QTemporaryDir` scoped to the bundle run
  and discarded when the run ends — it is not persisted to the enrich-stage artwork store.
- The `QIODevice::read (QSslSocket): device not open` warning observed in the E2E run (finding
  C2) occurs during this bundle-time artwork fetch, not during `--enrich`.
- Artwork that was already downloaded by a prior `--enrich` run is not used unless
  `--bundle-art-dir` is explicitly passed.

---

### V3 — Scan stage populates system classification via external detection (Low–Medium)

**Location:** `src/services/library_service.cpp` — `persistScanResults()` (lines 212–228)

Scan calls `m_detector->detectSystem()` and `DiscMagicDetector::detectFromArchive()` to
derive and store `systemId` during the scan stage. This is a content-classification step
(reading magic bytes from the file itself), not a provider query, so it does not violate the
"no external provider" rule. However, it means the scan stage does more than index files: it
classifies them.

**Judgment call for this plan:** system classification from local disc magic is treated as an
extension of indexing (it reads the file's own bytes, not a remote source) and is permitted
at scan time. It is noted here for completeness so future contributors understand the boundary.
If the team later decides platform classification should be deferred to match, this is the
only scan-stage call to move.

---

### V4 — `--download-artwork` re-fetches match data from providers (Low)

**Location:** `src/cli/cli_commands_bundle.cpp` — `handleArtworkCommand()` (lines 41–49)

`handleArtworkCommand()` calls `orchestrator->searchWithFallback()` to obtain a `boxArtUrl`
for each file. This re-queries providers even though a match record is already in the DB. The
box art URL should be resolved from the DB match/game row, not by re-running a provider search.

---

## Refactor Plan

### A0 — Split `persistMetadata` into identity-only + enrichment-only write (High priority)

**Problem.**
`cli_helpers.cpp::persistMetadata()` does two things: creates a game identity row with all
enrichment fields populated, and creates the match record. The fix is to split it into two
operations that can be called independently.

**Target design.**

```
persistMatchIdentity(db, file, metadata)
    → insertGame(title, systemId, region)   // identity only
    → insertMatch(fileId, gameId, confidence, method)

persistEnrichmentData(db, gameId, metadata)
    → updateGame(gameId, publisher, developer, releaseDate,
                 description, genres, players, rating)
```

`--match` calls `persistMatchIdentity()` only.
`--enrich` calls `persistEnrichmentData()` (and already has access to `gameId` via the match
record).

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_helpers.h` / `cli_helpers.cpp` | Replace `persistMetadata()` with `persistMatchIdentity()` and `persistEnrichmentData()` — ~30 LOC change, ~10 LOC net |
| `src/cli/cli_commands_match.cpp` | Call `persistMatchIdentity()` only — ~5 LOC |
| `src/cli/cli_commands_enrich.cpp` | Call `persistEnrichmentData()` at the end of the enrich pass — ~10–20 LOC |
| `src/gui/controllers/match_controller.cpp` | Replace `insertGame()` call (lines 297–314) with `persistMatchIdentity()` — ~15 LOC |
| `src/core/database.h` / `database_games.cpp` | Confirm `updateGame()` already accepts all enrichment fields (it does); no schema change needed |
| `tests/test_cli_helpers.cpp` (new or existing) | Add fixture verifying match stage does NOT write publisher/description/genre; enrich stage does write them — ~30 LOC |

**Acceptance criterion.**
After `--match`, a game row in the `games` table has a non-empty `title` and `system_id` and
an associated `matches` row, but `publisher`, `developer`, `description`, `genres`, `players`,
`release_date`, and `rating` are all NULL or empty. After `--enrich`, those fields are
populated. The split is verified by the new test fixture.

**Dependencies.** None; self-contained DB write change. Does not require a schema migration.

---

### A1 — Remove artwork download from bundle stage (Medium priority)

**Problem.**
`handleBundleCommand()` constructs an `ArtworkDownloader` and fetches box art during
`--bundle`. This should not happen in the bundle stage. The bundle stage must consume
artwork that was already downloaded by `--enrich` / `--download-artwork`.

**Target design.**
The bundle stage reads `artworkPath` from the DB game row (populated by the enrich stage) or
from the `--bundle-art-dir` pre-downloaded directory. It does not construct an
`ArtworkDownloader` or call any provider. If no artwork is found in either location, the
bundle is created without artwork (existing fallback behaviour already supports this).

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_commands_bundle.cpp` | Remove `ArtworkDownloader downloader` and the provider-based box art URL fetch; read artwork path from DB `artworkPath` field or `--bundle-art-dir` — ~30–50 LOC removal |
| `src/metadata/artwork_downloader.h` / `.cpp` | No change — downloader stays in enrich stage |
| `src/core/database.h` / `database_games.cpp` | Confirm `artworkPath` (or equivalent local path column) is persisted by `--enrich`; add if missing — ~0–20 LOC |
| `src/cli/cli_commands_enrich.cpp` | Ensure `--enrich` / `--download-artwork` persists the local artwork path to the DB game row after download — ~10–20 LOC |
| `tests/test_cli_smoke.cpp` | Add smoke test that `--bundle` without prior `--enrich` produces a valid bundle (possibly without artwork) and does NOT make any provider network calls — ~20 LOC |

**Acceptance criterion.**
Running `--bundle` on a library where `--enrich` has NOT been run produces valid bundles with
no provider network calls and no `QSslSocket` warnings. Running `--bundle` after `--enrich`
includes artwork from the enrich-stage download path. The SSL teardown warning (E2E finding
C2) is eliminated as a side effect of removing bundle-time artwork download.

**Dependencies.** A0 recommended first (keeps the refactor in one coherent direction). Enrich
stage must persist artwork path to the DB for A1 to close cleanly.

---

### A2 — Fix `--download-artwork` re-fetch (Low priority)

**Problem.**
`handleArtworkCommand()` calls `orchestrator->searchWithFallback()` to get a box art URL even
though a match record (and therefore a provider ID and external ID) is already in the DB. The
call is a wasted provider round-trip that can return a different result than the original match.

**Target design.**
`handleArtworkCommand()` reads the provider ID and external ID from the DB match row and
constructs the box art URL directly from the provider's URL template, without a search call.
If no match record exists for a file, emit a skip reason rather than running a search.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_commands_bundle.cpp` | Replace `searchWithFallback()` call in `handleArtworkCommand()` with DB-backed URL construction — ~20–30 LOC |
| `src/metadata/provider_orchestrator.h` / `.cpp` | Expose `artworkUrlForMatch(providerId, externalId)` or similar — ~10–20 LOC |
| Provider implementations (hasheous, gametdb) | Implement `artworkUrlForMatch()` — ~10–20 LOC each |

**Acceptance criterion.**
`--download-artwork` on a library where `--match` has already run does not call
`searchWithFallback()`. Box art is downloaded from the URL derived from the stored match
record. Network call count equals the number of matched files with a valid external ID, not
the total file count.

**Dependencies.** A0 and A1 recommended first.

---

## E2E Findings (from `docs/archive/reports/E2E-CLI-PIPELINE-REPORT-2026-05-20.md`)

These items are retained from the 2026-05-20 E2E run. They are sequenced after A0–A2 because
the stage-separation refactor touches some of the same call paths.

---

### H1 — Hash-stage silent skip accounting (Medium)

**Problem.**
`HashService` silently drops files it cannot hash (GTA San Andreas, 4.2 GB PS2 ISO in a 7z,
was omitted with no warning). The CLI summary counted only successfully hashed files.

**Files.**

| File | Change scope |
|------|-------------|
| `src/services/hash_service.cpp` / `.h` | Add skip-reason enum and accumulator — ~30 LOC |
| `src/cli/cli_commands_info.cpp` | Surface hashed count, skipped count, per-file reason — ~20 LOC |
| `src/cli/cli_commands_process.cpp` | Propagate skip reason — ~15 LOC |
| `tests/test_hash_service.cpp` | Fixture covering skip path and reason string — ~25 LOC |

**Acceptance criterion.**
`--hash-all` on a library with at least one unextractable member prints: hashed count,
skipped count, and one reason line per skipped file.

---

### H2 — Large archived disc images: streaming/chunked hashing (Medium)

**Problem.**
The 4.2 GB PS2 ISO requires full extraction to temp storage before hashing. This fails when
temp space is insufficient. The streaming/chunked path already used for magic-byte prefix
reads (`ArchiveExtractor::readMemberPrefix`) should be generalised for incremental digest
computation.

**Files.**

| File | Change scope |
|------|-------------|
| `src/services/hash_service.cpp` / `.h` | Chunked/streaming member hash path — ~30–80 LOC |
| `src/core/archive_extractor.cpp` / `.h` | Incremental read interface (generalise `readMemberPrefix`) — ~20–40 LOC |
| `src/core/hasher.cpp` / `.h` | Confirm incremental digest interface — ~10 LOC |
| `tests/test_hash_service.cpp` | Fixture for large member that exceeds temp estimate — ~30 LOC |

**Acceptance criterion.**
`--hash-all` on a library containing a multi-GB archived ISO completes without skipping the
member when temp space is insufficient, using the streaming digest path.

**Dependencies.** H1 first (skip reporting must be in place to surface streaming-path failures
clearly).

---

### C2 — SSL socket teardown warning (Low)

**Problem.**
`QIODevice::read (QSslSocket): device not open` during bundle-time artwork fetch. Root cause:
reply read attempted after socket closes. This warning is eliminated as a side effect of A1
(artwork download moves out of the bundle stage), but a guard should also be added to the
enrich-stage artwork path to prevent recurrence there.

**Files.**

| File | Change scope |
|------|-------------|
| `src/metadata/artwork_downloader.cpp` | Guard reply read behind `isOpen()` check — ~15 LOC |
| `tests/test_artwork_downloader.cpp` | Test confirming no read from closed device — ~20 LOC |

**Acceptance criterion.**
No `QIODevice::read (QSslSocket): device not open` warning appears in any pipeline run.

**Dependencies.** A1 (removes bundle-time download). This item hardens the enrich path.

---

### R1 — Region persistence during scan (Low–Medium)

**Problem.**
Region is not stored at scan time. It is extracted from the filename at export time as a
fallback. `region` should be persisted during scan (from filename parsing) so match, enrich,
bundle naming, and report surfaces all have a reliable region value.

**Files.**

| File | Change scope |
|------|-------------|
| `src/metadata/filename_normalizer.cpp` / `.h` | Confirm `parseRegion()` handles mixed tags — ~20 LOC if fixes needed |
| `src/core/scanner.cpp` | Persist `region` from filename during scan — ~20 LOC |
| DB scan persistence | Store region alongside existing scan attributes — ~20–50 LOC |

**Acceptance criterion.**
After `--scan`, `--info` for a ROM with a recognisable region tag reports a non-empty region.

---

### M1 — Hasheous MetadataProxy enrichment completeness (Low)

**Problem.**
`hasheous_client_api_key` absent in common setups. `--enrich` produces thin metadata for
IGDB-backed titles with no diagnostic. (Note: after A0 lands, the match stage no longer
writes enrichment data from the match call, so this is a pure enrich-stage concern.)

**Files.**

| File | Change scope |
|------|-------------|
| `src/metadata/hasheous_provider.cpp` | Emit diagnostic when API key is absent — ~10–15 LOC |
| `src/cli/cli_commands_enrich.cpp` | Surface "enrichment skipped: missing API key" in summary — ~10 LOC |
| `docs/metadata-providers.md` | Document proxy-disabled scenario and config steps — ~20 LOC |

---

### T1 — `--check-tools` command (Low)

**Problem.**
No preflight command to inspect archive/converter readiness. Tool availability is only
surfaced as fallback warnings during a run.

**Files.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_options.cpp` | Register `--check-tools` flag — ~10 LOC |
| `src/cli/cli_commands_process.cpp` | Implement handler emitting structured tool status — ~25–60 LOC |

**Acceptance criterion.**
`--check-tools` prints each tool name, detected path (or "not found"), and version. Exit code
is non-zero if any required tool is absent.

---

## Recommended Execution Order

```
A0  Split persistMetadata (identity vs. enrichment)
  └── CLI helpers + match command + GUI match controller
      Tests: match stage writes identity only; enrich stage writes enrichment fields

A1  Remove artwork download from bundle stage
  └── Bundle command + DB artwork path persistence in enrich stage
      Tests: --bundle without --enrich makes no provider calls; no SSL warning

H1  Hash-stage skip accounting
  └── HashService + CLI summary
      Tests: skip reason surface

A2  Fix --download-artwork re-fetch           (parallel with H1)
  └── Artwork command + provider URL helper

R1  Region persistence during scan            (parallel with A2 / H1)

H2  Streaming/chunked hashing for large files
  └── Depends on H1 (skip reporting in place first)

C2  SSL socket guard in artwork downloader    (parallel with H2)

M1  Hasheous enrichment diagnostics
T1  --check-tools command
```

**Phase 3 (concurrency) items from the prior plan — scan, match, enrich, hash-pool alignment —
should not begin until A0 and H1 are stable.** Concurrency across stages that do not yet have
clean single-responsibility boundaries will obscure failures and make regressions hard to
diagnose.

---

## Risk Register

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|-----------|--------|-----------|
| R1 | A0 split causes enrich stage to find `publisher`/`description` unexpectedly empty for titles that were previously enriched at match time, breaking existing libraries | Medium | Medium — existing DBs will have enrichment fields in the game row from match; new DBs will not | Document the one-time migration: run `--enrich` after updating to re-populate enrichment fields from the provider. Add a migration note to CHANGELOG. |
| R2 | A1 removal of bundle-time artwork download breaks `--process` pipelines that did not run `--enrich` first | Medium | Medium — users may notice missing artwork in bundles | Add a visible warning in `--bundle` output when no artwork path is found in the DB, directing users to run `--enrich` first. The bundle still succeeds without artwork. |
| R3 | H2 streaming digest is more complex than estimated and slips, leaving GTA San Andreas unprocessed | Medium | Low–Medium — H1 (skip reporting) already makes the gap visible | Timebox H2; if it slips, the skip reason from H1 is sufficient to close the silent-failure issue. |
| R4 | Separating `persistMetadata` introduces a race in the GUI match path if the user triggers enrich while a match is in progress | Low | Low — GUI currently runs match and enrich sequentially | Guard DB operations with the existing transaction pattern; no new locking needed. |
| R5 | Phase 3 concurrency work starts before A0 is stable, hiding enrichment field violations behind concurrent log noise | Medium | Medium | Enforce A0 as a hard gate for Phase 3 branches. |

---

*Source documents: `docs/archive/reports/E2E-CLI-PIPELINE-REPORT-2026-05-20.md`,
`docs/plans/improvement-plan-2026-05-20.md`.*
