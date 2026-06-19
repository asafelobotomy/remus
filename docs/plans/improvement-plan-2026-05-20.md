# Remus Improvement Plan — 2026-05-20

## Scope

This plan converts the findings from the 2026-05-20 end-to-end (E2E) CLI pipeline run and
still-open CLI integration gaps into a phased execution roadmap. Each item is anchored to
confirmed source files; no speculative paths are included. The goal is to move from a
working-but-fragile pipeline toward a robust, operator-visible, and performant one without
disrupting the current passing test suite.

---

## Current Baseline

- **70 tests green** across the test suite as of the 2026-05-20 baseline.
- **8 of 9 E2E ROMs** complete the full scan → hash → match → enrich → bundle sequence.
  GTA San Andreas is the one confirmed gap (hash-stage silent skip due to archive extraction
  constraints).
- **Magic-byte archive streaming** (streaming header reads via `ArchiveExtractor::readMemberPrefix`
  and `DiscMagicDetector::detectFromArchive`) is already landed and should be treated as
  regression coverage, not a pending item.
- Several items from the March 2026 plan are already implemented and are reframed here as
  **regression guardrails or polish** rather than fresh work: export-path directory detection,
  region export fallback, `--header-info` supported-format listing, and the TheGamesDB
  API-key hint message.

---

## Phase 0 — Immediate Stabilization

> Fix operator-invisible failures and known crash-adjacent regressions before any new feature
> work. All items in this phase should be closed before merging to main.

---

### H1 — Hash-stage silent skip accounting

**Problem.**
When `HashService` cannot hash an archived member (insufficient temp space, extraction
failure, or unsupported container), the CLI summary counts only successfully hashed files.
GTA San Andreas was silently excluded from downstream match and enrich stages; the pipeline
summary read as complete when it was not. There is no per-file skip reason surfaced to the
operator, and `--process` / `--hash-all` do not emit a skipped count.

**Implementation note.**
`--hash-all` (in `cli_commands_info.cpp`) and `--process` (in `cli_commands_process.cpp`)
both call `hashFileRecord()` from `src/cli/cli_helpers.cpp` directly, bypassing `HashService`.
Skip-reason tracking must be added to this shared helper, not only to `HashService`, otherwise
the CLI paths remain unaccounted. The `HashBatchResult::skipped` field in `hash_service.h`
is currently a plain `bool` with no reason payload; both that struct and `hashFileRecord()`
need a reason string.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/services/hash_service.h` | Add skip-reason string to `HashBatchResult` — ~5 LOC |
| `src/services/hash_service.cpp` | Populate skip reason in `computeHashes()` accumulator — ~25 LOC |
| `src/cli/cli_helpers.cpp` | Add skip-reason output to `hashFileRecord()` and its callers — ~25 LOC |
| `src/cli/cli_commands_info.cpp` | Surface hashed count, skipped count, and per-file reason in `--hash-all` summary — ~20 LOC |
| `src/cli/cli_commands_process.cpp` | Propagate skip reason in `--process` hash step summary — ~15 LOC |
| `tests/test_hash_service.cpp` | Add fixture covering skip path and reason string — ~25 LOC |

**Acceptance criterion.**
Running `remus-cli --hash-all` on a library that includes at least one unextractable archive
member prints: hashed count, skipped count, and one reason line per skipped file. No skipped
file is silently folded into a success-only summary. The same accounting applies when hashing
runs through `--process`.

**Dependencies.** `cli_helpers.cpp` is a shared dependency with P4; changes here should be
coordinated so both items leave the hashing helper in a consistent state.

---

### C2 — Artwork downloader teardown warning

**Problem.**
`ArtworkDownloader::downloadToMemory()` calls `reply->readAll()` after `loop.exec()` returns
under `reply->error() == QNetworkReply::NoError`. The 2026-05-20 E2E run produced
`QIODevice::read (QSslSocket): device not open` during bundle-time artwork fetch, indicating
the underlying SSL socket was not in an open state at the point of the read. The exact trigger
(race in the SSL layer, partial TLS teardown on slow paths, or a Qt-version-specific behaviour)
has not been isolated; the root cause should be treated as "closed SSL socket read after
event-loop exit" rather than a confirmed `deleteLater()` ordering bug. The bundle succeeds,
but the warning erodes operator trust and risks a read-on-closed-device failure on slower
networks or future Qt versions.

**Implementation note.**
`tests/test_artwork_downloader.cpp` currently has no injectable network manager or fake-reply
fixture, so the original ~20 LOC test estimate is optimistic. Reproducing the teardown
condition requires either a mock `QNetworkReply` subclass or an injectable seam. Budget for
~35–45 LOC for the test including the fake-reply infrastructure.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/metadata/artwork_downloader.cpp` | Guard `reply->readAll()` behind an `isOpen()` / `isReadable()` check; add a `qWarning()` if the guard fires — ~20 LOC |
| `tests/test_artwork_downloader.cpp` | Add fake-reply infrastructure and a test confirming no read is attempted from a closed device and the download returns an empty result rather than crashing — ~40 LOC |

**Acceptance criterion.**
No `QIODevice::read (QSslSocket): device not open` warning appears in `--bundle` runs. The
test injects a reply that is closed before the read path and confirms no read is attempted
from a closed device and the download returns an empty result rather than crashing.

**Dependencies.** None.

---

### X1 — `--export-path` directory regression coverage

**Problem.**
`src/cli/cli_commands_export.cpp` already detects when `--export-path` points to an existing
directory and appends a default filename; that fix is confirmed present in current code and
should be treated as already closed. What is still missing is direct regression coverage for
the exact failure mode, so this item is a stability backstop before further export work.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `tests/test_cli_smoke.cpp` | Add smoke assertion that passing a directory path to `--export-path` produces a file beneath that directory for at least one export format — ~20 LOC |

**Acceptance criterion.**
`test_cli_smoke` includes a case where `--export-path` is a directory and the test asserts
a file is created beneath that directory. No new product code is required.

**Dependencies.** None; this is test-only.

---

## Phase 1 — Near-Term Quality

> Improve operator experience, tighten messaging, and harden the parts of the pipeline
> confirmed working in E2E. Each item is a bounded improvement with no dependency on Phase 2.

---

### R1 — Persist region extraction earlier in the pipeline

**Problem.**
Region is currently extracted from the filename at export time via `FilenameNormalizer` as a
fallback. It is not stored as a library attribute during scan or match, so match, enrich,
bundle naming, and report surfaces all lack persistent region data. A secondary filename-based
fallback also exists in `src/cli/cli_helpers.cpp` during match persistence. Neither fallback
is visible to the operator.

**Data-model prerequisite (must be decided first).**
Neither `ScanResult` (`src/core/scanner.h`) nor `FileRecord` (`src/core/database_types.h`)
has a `region` field today. Before any scanner or persistence changes, decide where region
lives: file-level (extracted from filename at scan time, stored on the file row) or
match/game-level (stored on the match/metadata row after enrichment). The choice determines
which DB table gets the column, which insert/update paths change, and whether export can read
region before a match exists. This decision must be captured in a short ADR before work begins.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/core/scanner.h` / `database_types.h` | Add `region` field to `ScanResult` and/or `FileRecord` per the data-model decision — ~10 LOC |
| DB schema (migration) | Add `region` column to the appropriate table with a backward-compatible `ALTER TABLE` — ~10–20 LOC |
| `src/core/scanner.cpp` | Call `FilenameNormalizer::parseRegion()` / `extractRegion()` on discovered files and populate the new field — ~20 LOC |
| `src/metadata/filename_normalizer.cpp` / `.h` | Confirm `parseRegion()` handles mixed tags like `(USA) (En,Fr,De)` and multi-region tags like `(Japan, Europe)` — ~20 LOC if fixes needed |
| DB insert/update paths | Write region through the scan-insert and match-update paths — ~20–30 LOC |
| `src/cli/cli_commands_export.cpp` | Remove or demote the export-time fallback once region is reliably persisted upstream — ~10 LOC |
| `src/cli/cli_helpers.cpp` | Remove or demote the match-persistence-time region fallback — ~10 LOC |
| `tests/` (scanner, filename-normalizer, db-fixture) | Cover mixed/multi-region parsing, scan-time persistence, and schema backward compat — ~40 LOC |

**Acceptance criterion.**
After `--scan` (with no match step), `remus-cli --info` for a ROM with a recognizable region
tag reports a non-empty region value from the file row. The export-time and match-time
fallbacks are either removed or relegated to genuine last-resorts with visible diagnostics.
Existing DB fixture tests pass without modification after the schema migration.

**Dependencies.** Data-model ADR must be written and agreed before any code changes. Region
parsing fixture tests must pass before the field is persisted upstream.

---

### I1 — `--header-info` scope message cleanup

**Problem.**
`src/cli/cli_commands_info.cpp` already lists supported formats in the `--header-info` response;
this was addressed as part of the March plan. The remaining gap is a wording issue: the message
currently reads as a generic failure followed by a separate footnote rather than one scoped
diagnostic. Operators can be confused about whether the limitation is a runtime error or an
expected format boundary.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_commands_info.cpp` | Consolidate the two-line output into one message that reads as a scoped diagnostic — ~5–10 LOC |

**Acceptance criterion.**
For an unsupported format, `--header-info` emits a single message that names the format and
explicitly states that the command only detects copier headers for the listed supported formats.
No second "see supported formats" line appears separately.

**Dependencies.** None.

---

### T1 — `--check-tools` command

**Problem.**
Users currently have no dedicated command to inspect archive/converter readiness before a long
batch run. Tool availability (chdman, maxcso, dolphin-tool) is validated at use time and
surfaced via fallback messages only. The `--patch-tools` path in `src/cli/cli_commands_export.cpp`
performs similar checks but is scoped to patching, not general preflight inspection. Similar
readiness text exists in `src/cli/cli_commands_process.cpp` preflight output.

**Scope decision required.**
Before implementing, define whether `--check-tools` reports only required tools (chdman,
maxcso, dolphin-tool), optional tools, or both, and what the exit-code contract is (non-zero
only when a *required* tool is absent vs. any tool absent). This affects both the output format
and the test assertions.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_options.cpp` | Register `--check-tools` flag — ~10 LOC |
| `src/cli/cli_commands_process.cpp` | Implement handler that calls existing tool-check logic and emits structured output — ~25–60 LOC |
| `src/cli/cli_commands_export.cpp` | Extract shared tool-check helper if not already standalone — ~0–20 LOC |
| `docs/` (CLI reference or quick-reference) | Document `--check-tools` output format and expected exit codes — ~10 LOC |

**Acceptance criterion.**
`remus-cli --check-tools` prints each external tool name, detected path (or "not found"), and
version (or "unavailable") in a stable, parseable order. Exit code is non-zero if any required
tool is absent. The command works correctly whether each tool is absent or present, and
degraded behavior is confirmed not to hide failures behind noisier logs.

**Dependencies.** None.

---

## Phase 2 — Feature Completion

> Close the remaining functional gaps: the GTA San Andreas hashing gap, sparse metadata
> enrichment, and export field completeness. These depend on Phase 0 being stable but not
> on each other.

---

### H2 — Large archived disc images: streaming/chunked hashing

**Problem.**
`HashService` currently uses temp extraction for archive members, with a conservative free-space
estimate and a full-extract fallback. For multi-GB archived ISOs (such as GTA San Andreas on PS2),
there are two distinct failure modes:

1. **Temp space exhausted** — the decompressed file does not fit on disk before hashing begins.
2. **Memory exhausted** — `Hasher::readFileData()` loads the extracted file entirely into a
   `QByteArray` via `file.readAll()`, so even after successful extraction the full image must
   fit in RAM. This is a separate and additional constraint not addressed by extractor streaming
   alone.

The fix requires two sub-deliverables, both required before H2 is considered complete.

**Sub-deliverables.**

1. **Incremental hash API for `Hasher`** — replace `readAll()` with a chunked feed loop that
   updates the CRC32/MD5/SHA1 digests incrementally without holding the full file in memory.
2. **Streaming archive member reader for `ArchiveExtractor`** — the existing `readMemberPrefix()`
   path reads only a header prefix; a general incremental read interface is needed so
   `HashService` can feed archive member data chunk-by-chunk without fully decompressing to
   temp storage first.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/core/hasher.cpp` / `.h` | Replace `readAll()` with an incremental digest loop; add `hashStream(QIODevice*)` or equivalent — ~40–60 LOC |
| `src/core/archive_extractor.cpp` / `.h` | Expose an incremental member-read interface (callback or iterator yielding fixed-size chunks) — ~40–70 LOC |
| `src/core/archive_extractor_extract.cpp` | Adapt extraction path to support bounded chunked reads — ~20–30 LOC |
| `src/core/archive_extractor_info.cpp` | Verify member size reporting is accurate for temp-space planning — ~5–10 LOC |
| `src/services/hash_service.cpp` / `.h` | Wire chunked member read into the hash path; gate on available temp size with streaming fallback — ~40–80 LOC |
| `tests/test_hash_service.cpp` | Fixtures: member exceeds temp estimate (streaming path used), member fits temp (existing path unchanged), member too large for `readAll()` in-memory path — ~50 LOC |
| `tests/test_archive_extractor.cpp` | Cover incremental read interface: partial reads, boundary chunks, error mid-stream — ~30 LOC |

**Acceptance criterion.**
`--hash-all` on a library containing a multi-GB archived ISO completes without skipping the
member when the streaming path is available and temp space is insufficient for full extraction.
In-memory load is bounded to the configured chunk size regardless of member size. If temp
space and the streaming path are both unavailable, a clear skip reason is reported (per H1)
rather than a silent omission. The existing test suite continues to pass with the new chunked
path enabled.

**Dependencies.** H1 must land first. Sub-deliverable 1 (incremental Hasher API) should be
reviewed and merged before sub-deliverable 2 (streaming extractor) to keep the diff reviewable.

---

### M1 — Surface Hasheous MetadataProxy enrichment state in the CLI summary

**Problem.**
`hasheous_client_api_key` configuration is absent in common operator setups, producing thin
metadata (missing release dates, descriptions, publisher) after enrichment because the proxy
path is not reachable. The 2026-05-20 E2E run showed this silently: matches were found via
Hasheous but the `--enrich` summary gave no indication that IGDB enrichment was available but
not enabled.

**Implementation note.**
The provider-level diagnostic already exists: `hasheous_provider.cpp` emits a `qInfo()` message
("IGDB enrichment available … but MetadataProxy is disabled. Set hasheous_client_api_key …")
when a title has an IGDB ID but the proxy is off. The real gap is that this message appears at
the individual-title level during enrichment and is not aggregated into the `--enrich` command
summary. This item is therefore primarily a **CLI-summary surfacing task**, not a new provider
diagnostic. Provider-layer changes should be limited to exposing a count that the CLI can
aggregate; no new enrichment path or provider contract is required.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/metadata/hasheous_provider.cpp` | Expose the proxy-disabled count (titles with IGDB ID skipped) as a return value or signal rather than only a log line — ~15 LOC |
| `src/cli/cli_commands_enrich.cpp` | Aggregate and surface "N titles had IGDB data available but MetadataProxy is disabled" in the `--enrich` summary — ~15 LOC |
| `tests/test_provider_orchestrator.cpp` | Add test that verifies the proxy-disabled count is correct when enrichment runs without a key — ~20 LOC |
| `docs/` (metadata-providers.md or equivalent) | Document the proxy-disabled partial enrichment scenario and configuration steps — ~20 LOC |

**Acceptance criterion.**
`remus-cli --enrich` with no API key configured and at least one IGDB-identified title emits a
summary line such as "N titles have IGDB data available — set hasheous_client_api_key for richer
metadata." The message is counted and de-duplicated, not repeated per title. The sparse path is
covered by at least one provider-orchestrator fixture test. CI acceptance criteria must not
require live external metadata quality; use seeded fixtures only.

**Dependencies.** None blocking; can proceed in parallel with H2.

---

### E1 — EmulationStation export completeness from existing metadata

**Problem.**
The current EmulationStation exporter writes `<desc>`, `<genre>`, `<players>`, and `<region>`
but does **not** write `<releasedate>` or `<publisher>`. The March plan review noted these
fields as present, but inspection of `src/cli/cli_commands_export.cpp` confirms they are absent
from the ES block. This item has two distinct scopes:

1. **Add `<releasedate>` and `<publisher>` to the ES exporter** — these require explicit
   product-code work in the exporter and confirmation that the corresponding fields are
   persisted through the match/enrich pipeline.
2. **Validate completeness of existing fields** — for the fields already in the exporter
   (`desc`, `genre`, `players`, `region`), confirm they are populated end-to-end and not
   silently empty due to missing persistence steps.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_commands_export.cpp` | Add `<releasedate>` and `<publisher>` fields to the ES exporter; audit existing fields for fallback vs. hard DB read — ~30–60 LOC |
| `src/cli/cli_commands_enrich.cpp` | Verify genre, players, desc, releasedate, and publisher are persisted from enrichment results — ~20 LOC |
| DB/match persistence layer | Confirm releasedate and publisher fields exist as storable columns; add if missing — ~10–30 LOC depending on schema state |
| `tests/test_cli_smoke.cpp` | Add assertion that ES export for a title with known DB values produces non-empty region, genre, players, desc, releasedate, and publisher — ~25 LOC |

**Acceptance criterion.**
For a title with a complete DB row (populated by scan + match + enrich using seeded fixture
data), the EmulationStation `<gamelist.xml>` export contains non-empty values for `region`,
`genre`, `players`, `desc`, `releasedate`, and `publisher`. Any remaining empty field is
attributable to missing provider configuration or absent source data, not silent blanks from a
missing persistence or exporter step.

**Dependencies.** R1 (region persistence) and M1 (enrichment summary surfacing) are soft
prerequisites; this item provides the integration test that confirms both are wired end-to-end.
The `releasedate`/`publisher` scope may reveal schema additions that block this item until they
land.

---

## Phase 3 — Architecture and Performance

> Concurrency and throughput improvements. These should only begin once Phase 0–2 items have
> landed and the pipeline surfaces accurate state and failure reasons. Each item depends on
> the Phase 0 skip reporting being in place so that concurrency does not hide failures behind
> noisier logs.

---

### P1 — Scan-stage concurrency

**Problem.**
The scan iterator in `src/core/scanner.cpp` is currently single-threaded. Archive introspection
and disc probing dominate scan wall time for large libraries. The existing magic-byte streaming
work reduced per-file latency, but the overall scan remains sequential.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/core/scanner.cpp` / `.h` | Introduce file-level concurrency using `QThreadPool` or a worker queue; ensure duplicate row protection — ~50–90 LOC |
| Archive/disc helper files (`archive_extractor.cpp`, `archive_extractor_info.cpp`) | Verify thread-safety; use worker-local instances rather than shared state — ~15–25 LOC |
| `tests/test_scanner.cpp` | Add concurrent scan tests covering correctness invariants (see below) — ~50 LOC |

**Acceptance criterion.**
Correctness invariants (verified by the test suite before any throughput measurement):

- No duplicate file rows are inserted when two worker threads discover the same path.
- Multi-file set links (primary/companion `.bin` to `.cue`, etc.) are intact in the DB after
  a concurrent scan.
- Per-file skip reasons from H1 are reported correctly under concurrent execution.

Throughput (informational only, not a CI gate): `--scan` on a fixed library of 50+ plain files
runs measurably faster on multi-core hardware. Document the baseline and result in a comment or
commit message; do not gate merges on wall-clock time.

**Dependencies.** Phase 0 complete; H1 skip accounting must be in place before increasing
parallelism. `ArchiveExtractor` thread-safety audit must be completed and documented before
concurrent scan is enabled.

---

### P2 — Match-stage file-level parallelism with provider-budget controls

**Problem.**
`provider_orchestrator_fallback.cpp` (and `matchWithFallback()`) walks providers serially for
each file. The 2026-05-20 E2E report shows wall time dominated by Hasheous lookups even though
all final matches resolve at the compendium step, meaning the current scheduling model pays
unnecessary latency on large batches. Safe overlap across different games or different provider
classes is possible provided the existing 1 req/s rate limiter is respected per provider.

**Hard prerequisites before parallelism is enabled.**
Provider objects must be confirmed safe for concurrent calls. The primary risks are:

- Shared `QNetworkAccessManager` — Qt requires this to be used from a single thread or with
  an explicit thread-hop; per-worker-thread instances may be required.
- Rate-limiter shared state — the existing limiter must be moved to per-provider ownership
  so concurrent file tasks contend on the correct budget.
- DB write ordering — concurrent match results must be collected and written in a deterministic
  order to avoid non-reproducible row states.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_commands_match.cpp` | Dispatch per-file match tasks to a worker pool; collect results and write to DB in deterministic order — ~30–60 LOC |
| `src/metadata/provider_orchestrator_fallback.cpp` | Move rate-limiter ownership to per-provider budget; audit `QNetworkAccessManager` thread affinity — ~30–90 LOC |
| Provider headers / implementations | Expose thread-safe match interface; document whether provider instances are worker-local or shared — ~15–25 LOC |
| `tests/test_provider_orchestrator.cpp` | Fixtures: concurrent match calls respect rate limits; no duplicate DB rows; DB write order is deterministic — ~40 LOC |

**Acceptance criterion.**
Correctness invariants (verified by tests before benchmarking):

- No duplicate match rows are written when concurrent tasks resolve the same file.
- Rate limit contract (1 req/s per provider) is not violated under concurrent load.
- DB result ordering is deterministic across runs.

Throughput (informational): `--match` on a 50-file library with a mock provider runs faster
than the sequential baseline; log output remains readable (no interleaved per-title lines).

**Dependencies.** P1 recommended first to establish the worker-pool pattern; Phase 0 skip
accounting mandatory. Provider thread-safety audit must complete before this item merges.

---

### P3 — Enrich-stage concurrency with per-provider throttling

**Problem.**
Enrich-stage execution in `cli_commands_enrich.cpp` and `provider_orchestrator_enrich.cpp` is
sequential. The 1 req/s limiter is correct for individual providers, but the pipeline serializes
the whole title queue rather than allowing safe overlap across different games or different
provider classes.

The same hard prerequisites from P2 apply here — `QNetworkAccessManager` thread affinity,
rate-limiter ownership, and deterministic result persistence. Enrichment additionally involves
result *merging* (multiple providers contribute fields to one title row), so deterministic
merge semantics must be defined before concurrent dispatch is safe.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_commands_enrich.cpp` | Dispatch per-title enrich tasks to a worker pool; collect and merge results in deterministic order — ~35–55 LOC |
| `src/metadata/provider_orchestrator_enrich.cpp` | Centralize per-provider throttle ownership; define deterministic field-merge order — ~30–70 LOC |
| Provider files (hasheous, gametdb, compendium providers) | Confirm thread-safe use under concurrent calls; document any worker-local instance requirements — ~15–25 LOC |
| `tests/test_provider_orchestrator.cpp` | Extend fixture: concurrent enrich calls respect throttle; merged result is identical to sequential result for the same title — ~30 LOC |

**Acceptance criterion.**
Correctness invariants (verified by tests before benchmarking):

- Enrichment completeness is unchanged: the same fields are populated for the same titles as
  under sequential execution.
- Field-merge order is deterministic across concurrent runs.
- Rate limit contract per provider is not violated.

Throughput (informational): `--enrich` on a 50-title library with a mock provider runs faster
than the sequential baseline.

**Dependencies.** P2 recommended first to establish the worker-pool and thread-safety pattern;
Phase 0 skip accounting mandatory.

---

### P4 — Hash-stage parallelism revisit (extraction bottleneck)

**Problem.**
`HashService::computeHashes()` already uses `QThreadPool` for digest calculation, but the
2026-05-20 E2E measurement still appeared effectively single-threaded at the pipeline level.
The root cause is that `--hash-all` (in `cli_commands_info.cpp`) and `--process` (in
`cli_commands_process.cpp`) call `hashFileRecord()` from `src/cli/cli_helpers.cpp` in a serial
loop rather than routing through the `HashService::computeHashes()` batch API. This item closes
that gap and characterizes archive extraction as the remaining bottleneck before adding more
raw threads.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_helpers.cpp` | Consolidate or eliminate the duplicated `hashFileRecord()` helper in favour of routing callers to `HashService` — ~20–40 LOC (coordinate with H1 which also touches this file) |
| `src/cli/cli_commands_info.cpp` | Route `--hash-all` through `HashService::computeHashes()` batch API instead of single-file helper loop — ~15–25 LOC |
| `src/cli/cli_commands_process.cpp` | Same routing change for the `--process` hash step — ~15–25 LOC |
| `src/services/hash_service.cpp` | Add instrumentation or comment noting where temp I/O dominates vs. the digest worker pool — ~10–15 LOC |
| `tests/test_hash_service.cpp` | Add correctness fixture verifying batch-API results match single-file results for the same inputs — ~20 LOC |

**Acceptance criterion.**
After alignment, `--hash-all` throughput on a library of plain (non-archived) ROMs scales with
available CPU cores using the existing `QThreadPool`. Archived member hashing is identified as
the remaining single-threaded path, with a note in code or documentation that H2 (streaming)
is the prerequisite before parallelizing that path further. This item should be considered
complete if it removes the discrepancy between CLI entry-point behavior and the service-level
worker pool; it does not require a full extraction-parallel implementation.

**Dependencies.** H2 (streaming/chunked hashing) is a soft prerequisite — do not parallelize
extraction to temp storage if H2 is about to replace it.

---

## Recommended Execution Order Inside Phases

### Phase 0

1. **H1** (skip accounting) — unblocks all operator-visibility work and is prerequisite for
   Phase 3.
2. **C2** (artwork teardown) — independent; can run in parallel with H1.
3. **X1** (export-path regression test) — purely additive; can follow either.

### Phase 1

1. **I1** (header-info wording) — smallest change; good warm-up commit.
2. **R1** (region persistence) — foundational for E1; start early in the phase.
3. **T1** (check-tools command) — independent of R1; can proceed in parallel.

### Phase 2

1. **H2** (streaming/chunked hashing) — closes the remaining E2E functional gap (GTA San
   Andreas). Must land before E1 can pass end-to-end. M1 can begin in parallel once H2
   sub-deliverable 1 (incremental hash API) is in review.
2. **M1** (Hasheous enrichment diagnostics) — unblocks E1 enrichment assertions; can proceed
   in parallel with H2 after sub-deliverable 1 is stable.
3. **E1** (ES export completeness) — depends on R1 (Phase 1) and M1; final integration gate.

### Phase 3

1. **P4** (hash-stage alignment) — confirm worker-pool wiring before adding extraction
   parallelism; H2 must be stable first.
2. **P1** (scan concurrency) — self-contained; can proceed once Phase 0 is confirmed green.
3. **P2** (match parallelism) — after P1 establishes the worker pattern.
4. **P3** (enrich parallelism) — after P2; shares throttle-ownership design.

---

## Risk Register

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|-----------|--------|-----------|
| R1 | Chunked/streaming member hashing (H2) is more complex than estimated and slips Phase 2 | Medium | High — GTA San Andreas remains a gap; the E2E pass rate stays at 8/9 | Timebox H2 to two weeks; if streaming is not feasible in that window, land H1 (skip reporting) and document the capability gap explicitly in the operator guide |
| R2 | Region persistence (R1) reveals schema migrations that break existing DB fixtures | Low–Medium | Medium — test suite may regress on DB-fixture tests | Run the full test suite after each schema delta; use backward-compatible column additions rather than type changes |
| R3 | Concurrent scanner or match stage introduces duplicate rows or broken multi-file set links | Medium | High — corrupts library DB; hard to detect without explicit tests | Write the duplicate-row and set-link tests (P1 / P2 acceptance criteria) before enabling concurrency in CI |
| R4 | Hasheous API key requirement is not documented before M1 lands, leading to operator confusion about "sparse data" messages | Low | Medium — erodes trust in the enrichment pipeline | Ship the `docs/` portion of M1 simultaneously with the diagnostic code change; do not land the diagnostic without the documentation |
| R5 | Phase 3 parallelism work begins before Phase 0 skip accounting is stable, hiding failures in noisy concurrent logs | Medium | Medium — makes regressions hard to diagnose | Enforce Phase 0 completion as a hard gate for Phase 3; add a CI check that the H1 skip-accounting test passes before Phase 3 branches are merged |
| R6 | `Hasher::readFileData()` loads extracted files into RAM via `readAll()`, causing OOM for large ISOs even after temp-space extraction succeeds (H2 in-memory failure mode) | High | High — hashing silently fails or crashes for large titles; GTA San Andreas is a confirmed example | Both H2 sub-deliverables are required before closing H2; do not mark H2 done at sub-deliverable 1 alone |
| R7 | `hashFileRecord()` in `cli_helpers.cpp` and the `HashService` batch path diverge during H1/P4 development, causing `--hash-all` and `--process` to report inconsistent skip reasons | Medium | Medium — skip accounting is wrong for one of the two entry points; hard to notice without explicit pairwise tests | Coordinate H1 (skip-reason field) and P4 (routing) in the same branch; add a CI fixture testing both `--hash-all` and `--process` skip reporting against the same inputs |
| R8 | Enabling P2/P3 concurrency violates Qt `QNetworkAccessManager` thread affinity, causing non-deterministic crashes or silent data loss in provider calls | High | High — corrupts match/enrich results; may not be reproducible under low load | Complete the provider thread-safety audit (worker-local manager or thread-hop pattern) before any concurrent dispatch is merged; add a targeted concurrency test that exercises the network path under multi-thread conditions |
| R9 | Performance acceptance criteria for P1–P4 use wall-clock comparisons on non-fixed fixtures, producing flaky CI gates and masking real regressions | Medium | Low–Medium — CI instability erodes confidence in the test suite | Separate correctness invariant tests (duplicate rows, stable links, rate limits — CI gate) from throughput notes (informational only, documented in commit messages); never block a merge on wall-clock time alone |

---

*Generated from the 2026-05-20 E2E CLI pipeline run findings. Source-of-truth E2E report:
`docs/archive/reports/E2E-CLI-PIPELINE-REPORT-2026-05-20.md`.*
