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

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/services/hash_service.cpp` / `.h` | Add skip-reason enum and accumulator — ~30 LOC |
| `src/cli/cli_commands_info.cpp` | Surface hashed count, skipped count, and per-file reason in summary — ~20 LOC |
| `src/cli/cli_commands_process.cpp` | Propagate skip reason from service into summary output — ~15 LOC |
| `tests/test_hash_service.cpp` | Add fixture covering skip path and reason string — ~25 LOC |

**Acceptance criterion.**
Running `remus-cli --hash-all` on a library that includes at least one unextractable archive
member prints: hashed count, skipped count, and one reason line per skipped file. No skipped
file is silently folded into a success-only summary.

**Dependencies.** None; this is self-contained within `HashService` and the two CLI command
files.

---

### C2 — Artwork downloader teardown warning

**Problem.**
`ArtworkDownloader` reads from a `QNetworkReply` after the request loop returns. The 2026-05-20
E2E run produced `QIODevice::read (QSslSocket): device not open` during bundle-time artwork
fetch. The bundle succeeds, but the warning indicates reply/socket lifecycle is not clean and
will erode operator trust in batch runs. Unchecked, this pattern risks reading from a closed
device in a future Qt version or on slower network paths.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/metadata/artwork_downloader.cpp` | Guard reply read behind `isOpen()` check; ensure `reply->deleteLater()` order is correct — ~15 LOC |
| `tests/test_artwork_downloader.cpp` | Add test that simulates `QIODevice::read (QSslSocket): device not open` under the same network teardown scenario — ~20 LOC |

**Acceptance criterion.**
No `QIODevice::read (QSslSocket): device not open` warning appears in `--bundle` runs. The
test exercises a reply that is closed before the read path and confirms no read is attempted
from a closed device.

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
bundle naming, and report surfaces all lack persistent region data. The export fallback added
in the March plan is present in `src/cli/cli_commands_export.cpp`, but region should become
stored metadata visible before export time.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/metadata/filename_normalizer.cpp` / `.h` | Confirm `parseRegion()` is public and handles mixed tags like `(USA) (En,Fr,De)` and multi-region tags like `(Japan, Europe)` — ~20 LOC if fixes needed |
| `src/core/scanner.cpp` | Call `FilenameNormalizer::parseRegion()` on discovered files and persist result — ~20 LOC |
| `src/cli/cli_commands_export.cpp` | Remove or demote the export-time fallback once region is reliably persisted upstream — ~10 LOC |
| DB persistence layer (existing schema) | Store region field alongside existing scan-stage attributes — ~20–50 LOC depending on schema delta |

**Acceptance criterion.**
After `--scan`, `remus-cli --info` for a ROM with a recognizable region tag reports a
non-empty region value. The export-time fallback is either removed or relegated to a genuine
last-resort with a visible diagnostic rather than a silent default.

**Dependencies.** None blocking, but region parsing fixes should include fixture-backed tests
for mixed and multi-region tags before persisting values upstream.

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
performs similar checks but is scoped to patching, not general preflight inspection.

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
this means the full decompressed image must fit in temp storage before hashing can proceed. When
temp space is insufficient, the file is silently skipped (addressed in H1 for reporting, but
not for capability). The fix is to generalize the streaming/chunked path already used for
magic-byte prefix reads so that hashing can consume an archive member in bounded chunks without
requiring the full decompressed size in temp.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/services/hash_service.cpp` / `.h` | Add chunked/streaming member hash path; gate on available temp size — ~30–80 LOC |
| `src/core/archive_extractor.cpp` / `.h` | Expose incremental read interface if not already present from streaming work — ~20–40 LOC |
| `src/core/archive_extractor_extract.cpp` | Adapt extraction path to support bounded chunked reads — ~20 LOC |
| `src/core/archive_extractor_info.cpp` | Verify member size reporting is accurate for planning purposes — ~5–10 LOC |
| `src/core/hasher.cpp` / `.h` | Confirm incremental digest interface supports streaming feed — ~10 LOC |
| `tests/test_hash_service.cpp` | Add fixture-backed characterization test for large member that exceeds conservative temp estimate — ~30 LOC |

**Acceptance criterion.**
`--hash-all` on a library containing a multi-GB archived ISO completes without skipping the
member when the streaming path is available. If temp space is genuinely exhausted, a clear skip
reason is reported (per H1) rather than a silent omission. The existing test suite continues to
pass with the new chunked path enabled.

**Dependencies.** H1 (skip reporting) should land first so the streaming path can report
meaningful diagnostics while it is being validated.

---

### M1 — Hasheous MetadataProxy enrichment for sparse titles

**Problem.**
`hasheous_client_api_key` configuration is absent in common operator setups. The 2026-05-20 E2E
run showed good identity matches via Hasheous but thin metadata (missing release dates,
descriptions, publisher) because the enrichment proxy path was not reachable. There is no
explicit CLI diagnostic when enrichment is skipped due to missing configuration, and the fallback
story for release dates and descriptions without proxy enrichment is undocumented.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/metadata/hasheous_provider.cpp` | Emit a diagnostic when API key is absent and enrichment is skipped — ~10–15 LOC |
| `src/metadata/hasheous_provider_enrichment.cpp` | Add test coverage for partial enrichment path (identity matched, metadata fields skipped) — ~20 LOC |
| `src/cli/cli_commands_enrich.cpp` | Surface "enrichment skipped: missing API key" in `--enrich` summary — ~10 LOC |
| `src/cli/cli_helpers_providers.cpp` | Document provider setup and key requirements as inline diagnostic hints — ~10 LOC |
| `tests/test_hasheous_parsing.cpp` | Add fixture verifying non-empty publisher for representative IGDB-backed titles — ~20 LOC |
| `tests/test_provider_orchestrator.cpp` | Add test that verifies partial enrichment when proxy is disabled — ~20 LOC |
| `docs/` (metadata-providers.md or equivalent) | Document the proxy-disabled partial enrichment scenario and configuration steps — ~20 LOC |

**Acceptance criterion.**
`remus-cli --enrich` with no API key configured emits an explicit "earned sparse data"
diagnostic rather than silently producing thin metadata. When a key is configured, publisher and
release date are non-empty for representative IGDB-backed titles already identified by Hasheous.
The sparse path is covered by at least one provider-orchestrator fixture test.

**Dependencies.** None blocking; can proceed in parallel with H2.

---

### E1 — EmulationStation export completeness from existing metadata

**Problem.**
The EmulationStation exporter fields (`<releasedate>`, `<publisher>`, `<genre>`, `<players>`,
`<desc>`, `<region>`) are already present in `src/cli/cli_commands_export.cpp` as confirmed by
March plan review. Export field completeness is therefore only as good as upstream persistence and
provider enrichment. The export-time region fallback (March fix) is present, but region and other
fields read as empty for titles where match/enrich did not fully persist their values.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_commands_export.cpp` | Audit which fields have fallback logic vs. hard DB reads; add fallback or warning for fields that are missing due to incomplete enrichment rather than absent source data — ~20–50 LOC |
| `src/cli/cli_commands_enrich.cpp` | Ensure genre, players, and desc are persisted from enrichment results — ~20 LOC |
| DB/match persistence layer | Verify region, genre, players fields exist as storable columns — ~0–20 LOC depending on schema state |
| `tests/test_cli_smoke.cpp` | Add assertion that export for a title with known DB values produces non-empty region, genre, players, and desc fields — ~20 LOC |

**Acceptance criterion.**
For a title with a complete DB row (populated by scan + match + enrich), the EmulationStation
`<gamelist.xml>` export contains non-empty values for `region`, `genre`, `players`, and `desc`.
Any remaining empty field is attributable to missing provider configuration or absent source
data, not silent blanks from a missing persistence step.

**Dependencies.** R1 (region persistence) and M1 (enrichment completeness) are soft
prerequisites; this item provides the integration test that confirms both are wired end-to-end.

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
| `src/core/scanner.cpp` / `.h` | Introduce file-level concurrency using `QThreadPool` or a worker queue; ensure duplicate row protection — ~40–80 LOC |
| Archive/disc helper files (`archive_extractor.cpp`, `archive_extractor_info.cpp`) | Verify thread-safety or add worker-local instances — ~10–20 LOC |
| `tests/test_scanner.cpp` | Add concurrent scan test asserting no duplicate rows and no broken multi-file set linking — ~30 LOC |

**Acceptance criterion.**
`--scan` on a library of 50+ files executes faster on multi-core hardware without introducing
duplicate rows or broken multi-file set links. Concurrent scans do not hide failures behind
noisier logs.

**Dependencies.** Phase 0 complete; H1 skip accounting must be in place before increasing
parallelism.

---

### P2 — Match-stage file-level parallelism with provider-budget controls

**Problem.**
`provider_orchestrator_fallback.cpp` (and `matchWithFallback()`) walks providers serially for
each file. The 2026-05-20 E2E report shows wall time dominated by Hasheous lookups even though
all final matches resolve at the compendium step, meaning the current scheduling model pays
unnecessary latency on large batches. Safe overlap across different games or different provider
classes is possible provided the existing 1 req/s rate limiter is respected per provider.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_commands_match.cpp` | Dispatch per-file match tasks to a worker pool; collect results preserving stable persistence semantics — ~30–60 LOC |
| `src/metadata/provider_orchestrator_fallback.cpp` | Move rate-limiter ownership to per-provider budget so concurrent file tasks share the limiter correctly — ~20–80 LOC |
| Provider orchestrator headers | Expose thread-safe match interface — ~10–20 LOC |
| `tests/test_provider_orchestrator.cpp` | Add fixture verifying that concurrent match calls respect rate limits and do not duplicate DB rows — ~25 LOC |

**Acceptance criterion.**
`--match` on a 50-file library with a mock provider runs measurably faster than sequential
baseline. Remote latency no longer dominates already-resolved items. The existing rate limit
contract per provider is preserved and verified by the new test. Output is not unreadable due
to interleaved concurrent logging.

**Dependencies.** P1 recommended first; Phase 0 skip accounting mandatory.

---

### P3 — Enrich-stage concurrency with per-provider throttling

**Problem.**
Enrich-stage execution in `cli_commands_enrich.cpp` and `provider_orchestrator_enrich.cpp` is
sequential. The 1 req/s limiter is correct for individual providers, but the pipeline serializes
the whole title queue rather than allowing safe overlap across different games or different
provider classes.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_commands_enrich.cpp` | Dispatch per-title enrich tasks to a worker pool — ~30–50 LOC |
| `src/metadata/provider_orchestrator_enrich.cpp` | Centralize per-provider throttle ownership — ~20–60 LOC |
| Provider files (hasheous, gametdb, compendium providers) | Confirm thread-safe use under concurrent calls — ~10–20 LOC |
| `tests/test_provider_orchestrator.cpp` | Extend fixture to cover concurrent enrich calls with throttle verification — ~20 LOC |

**Acceptance criterion.**
`--enrich` on a 50-title library with a mock provider runs faster than the sequential baseline
while respecting the existing rate limit contract per provider. Enrichment completeness is
unchanged (same fields populated for the same titles).

**Dependencies.** P2 recommended first to establish the worker-pool pattern; Phase 0 skip
accounting mandatory.

---

### P4 — Hash-stage parallelism revisit (extraction bottleneck)

**Problem.**
`HashService::computeHashes()` already uses `QThreadPool` for digest calculation, but the
2026-05-20 E2E measurement still appeared effectively single-threaded at the pipeline level.
The bottleneck is archive extraction into temp storage, not the digest itself. `--hash-all` and
`--process` do not fully exploit the existing service batch path consistently, and
archive extraction can dominate. This phase aligns CLI entry points with the existing worker-pool
implementation and characterizes extraction as the remaining bottleneck before adding more raw
threads.

**Files / estimated LOC.**

| File | Change scope |
|------|-------------|
| `src/cli/cli_commands_info.cpp` | Ensure `--hash-all` paths through the batch API rather than single-file loops — ~10–20 LOC |
| `src/cli/cli_commands_process.cpp` | Same alignment for `--process` hash step — ~10–20 LOC |
| `src/services/hash_service.cpp` | Profile extraction-vs-digest split; add instrumentation or comment noting where temp I/O dominates — ~10–20 LOC |
| `tests/test_hash_service.cpp` | Add performance-characterization fixture noting baseline before and after alignment — ~20 LOC |

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

1. **M1** (Hasheous enrichment diagnostics) — unblocks E1 enrichment assertions.
2. **H2** (streaming/chunked hashing) — unblocks GTA San Andreas end-to-end and P4.
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

---

*Generated from the 2026-05-20 E2E CLI pipeline run findings. Source-of-truth E2E report:
`docs/reports/E2E-CLI-PIPELINE-REPORT-2026-05-20.md`.*
