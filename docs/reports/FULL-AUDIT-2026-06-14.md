# Remus Full Audit — 2026-06-14

> **Version audited:** 0.10.1 (`main`, commit `8033dbb`)  
> **Scope:** Build, tests, CI hygiene, security, CLI/GUI parity, documentation, compendium data, ROM matching remediation status

---

## Executive summary

Remus is in **good shape for a 0.10.1 release candidate**. The active dual-binary build (`remus-cli` + `remus-gui`) compiles cleanly on Ubuntu 24.04, all **90** unit/integration tests pass, and the ROM matching remediation roadmap (P1–P7, G1–G11) is complete per [ROM-MATCHING-AUDIT.md](ROM-MATCHING-AUDIT.md).

**Findings by severity:**

| Severity | Count | Summary |
|----------|-------|---------|
| **High** | 0 | — |
| **Medium** | 4 | CLI argv secrets; GUI plaintext credential fallback; bundled compendium is schema-only; clang-format drift (fixed in this branch) |
| **Low** | 9 | Doc/binary name drift; unwired GUI M3U export; `--inner-hash` message without flag; shellcheck/qmllint warnings; stale milestone docs; flaky parallel `GuiControllersSmokeTest` |
| **Info** | 3 | Expected bootstrap compendium content gaps; user-configurable tool paths; archived TUI code |

**Overall grade: B+** — production-ready core with known parity and documentation gaps, not blocking correctness bugs in the shared engine.

---

## 1. Build & CI

### 1.1 Release build

| Check | Result |
|-------|--------|
| `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DREMUS_ENABLE_WARNINGS=ON` | Pass |
| `cmake --build build -j$(nproc)` | Pass (both `remus-cli` and `remus-gui`) |
| Warnings | One Qt `Q_GLOBAL_STATIC` deprecation in GUI QML cache build |

### 1.2 Test suite

| Metric | Value |
|--------|-------|
| Tests | 90 |
| Passed | 90 |
| Failed | 0 |
| Wall time | ~16 s |

Labels: `live` (1), `network` (1).

### 1.3 Static analysis & lint

| Job | Result | Notes |
|-----|--------|-------|
| **clang-format** | **Fail on `main`** | Multiple files drifted from `.clang-format`; fixed on `cursor/full-audit-c94d` |
| shellcheck | Warn | SC2155 in `run-qmllint.sh`; SC1090 in `verify_credentials.sh` |
| qmllint | Warn | Unqualified property access in `VerifyView.qml` (info-level) |
| clang-tidy | `continue-on-error: true` | Spot-check only |

### 1.4 Coverage

Local coverage build failed: missing `libclang_rt.profile-x86_64.a` for LLVM 18 when linking with `-fprofile-arcs`. CI installs `lcov` on ubuntu-24.04 and may succeed there; local parity requires matching compiler profile runtime.

CI threshold: **50%** (`check-coverage-threshold.sh`).

### 1.5 Compendium validation

After `scripts/setup_compendium_db.sh`:

- Schema/seed checks: **PASS** (112 systems, 21 regions, 21 merge policies)
- Content checks: **FAIL** (expected for bootstrap DB)
  - `content.games_nonzero`
  - `content.game_signatures_nonzero`
  - `content.source_items_nonzero`

The bundled `data/compendium/remus_compendium.db` in the repo is a **bootstrap schema**, not a populated catalog. Offline hash matching requires `remus-cli --build-compendium` or ingest. Documented in `data/compendium/README.md`.

---

## 2. Codebase metrics

| Metric | Value |
|--------|-------|
| C++ source files (`src/`) | ~305 |
| Lines of C++ (`src/`) | ~43,221 |
| Active surfaces | CLI, Qt Quick GUI |
| Archived | `archive/gui-tui/` (TUI + Qt Widgets) |
| Shared layers | `src/core`, `src/metadata`, `src/services` |

No `TODO` / `FIXME` markers in `src/` or `tests/`.

---

## 3. ROM matching audit status

Reference: [ROM-MATCHING-AUDIT.md](ROM-MATCHING-AUDIT.md)

| Item | Status |
|------|--------|
| P1–P7 remediation | Complete |
| G1–G8 (hash cascade, provider order, PlayMatch, multi-signal) | Resolved |
| G9 (RA MD5 vs No-Intro MD5) | Fixed — `RaHasher`, `ra_md5` field |
| G10 (size corroboration after hash hit) | Fixed — `getByHash(..., fileSize)` |
| G11 (GameTDB cascade alignment) | Fixed — `getByHashes` |

Provider priorities verified in `src/core/constants/providers.h`:

```text
Compendium 210 → GameTDB 150 → Hasheous 91 → ScreenScraper 89 → PlayMatch 88 → IGDB 70 → RA 60 → TGDB 50 → Wikidata 40
```

Canonical hash cascade: `VerificationHashMatcher::orderedOfficialHashTypes()` — shared by verification and metadata paths.

**Open from observed issues** ([observed-issues.md](../notes/observed-issues.md)):

- Rate limiter tuning (500 ms default; per-provider config deferred)

---

## 4. CLI vs GUI feature parity

Both surfaces share `src/core`, `src/metadata`, and `src/services`. Largest gaps:

### CLI-only (no GUI equivalent)

| Area | Examples |
|------|----------|
| Compendium ops | `--build-compendium`, `--dedup-compendium`, `--enrich-compendium`, `--ingest-source`, `--import-patch-catalog` |
| Batch pipelines | `--library`, `--process`, `--process-preset` (es-de, retrodeck, batocera, …) |
| Folder naming schemes | `--folder-naming` (6 schemes vs GUI binary default/none) |
| Standalone utilities | `--chd-extract`, `--rvz-extract`, `--cso-extract`, `--space-report`, `--checksum-verify` |
| Mod catalog tooling | `--mod-systems`, `--mod-author`, `--mod-catalog-build`, `--mod-scrape`, filter/sort flags |
| Reporting / automation | `--match-report`, `--json`, `--log-file`, `--dry-run-all` |
| DAT verify from file | `--verify <dat-file>` |

### GUI-only (or richer in GUI)

| Area | Examples |
|------|----------|
| Match confirm/reject | Pipeline panel, confirm-all |
| Match & Enrich dialog | Per-field import toggles, artwork option |
| Organize preview + undo | `RenameOrganizeDialog`, `undoLast()` |
| Settings UI | Credential browse, tool auto-detect, erase-library wizard |
| Inspector / queue UX | Artwork preview, stage buckets, progress cards |
| Export preview | Per-system counts before export |

### Shared but uneven

| Capability | Gap |
|------------|-----|
| M3U export | `ExportController::exportM3u()` exists; **not wired in QML** |
| Patch create | `PatchController::createPatch()` exists; no GUI surface |
| Verify against external DAT | CLI `--verify <path>`; GUI uses loaded/bundled catalogs only |

---

## 5. Security

Reference: [SECURITY.md](../../SECURITY.md)

### Strengths

| Control | Location |
|---------|----------|
| Archive path traversal filter | `archive_extractor.cpp` — rejects `..`, absolute paths |
| Organize destination containment | `organize_engine.cpp` |
| Safe subprocess (no shell) | `external_tool_runner.cpp`, `patch_engine_apply.cpp` |
| Keychain-first secrets | `secret_store.cpp`, `credential_manager.cpp` |
| argv secret warning | `cli_helpers_providers.cpp` (`resolveSecret`) |
| Mod catalog SSRF guards | `mod_catalog_provider.cpp` |
| Patch SHA1 verification | `mod_workflow_service.cpp` |

### Residual risks

| Severity | Issue |
|----------|-------|
| Medium | Secrets still accepted via argv (`--ss-pass`, `--igdb-client-secret`, `--ra-api-key`) — visible in `ps` |
| Medium | GUI falls back to `QSettings` when keychain read is empty — stale plaintext possible |
| Low | User-configurable external tool paths → arbitrary execution if settings compromised |
| Low | `--inner-hash` suggested in verify output but flag not registered |
| Info | `unzip` via PATH for RAPatches catalog build |

No shell-string command injection found in reviewed subprocess paths.

---

## 6. Documentation audit

### Aligned

- `README.md`, `docs/setup/BUILD.md`, `docs/plan.md`, `docs/metadata-providers.md`
- `ROM-MATCHING-AUDIT.md` (post G9–G11)
- `data/compendium/README.md` (bootstrap vs populated DB)

### Stale or inconsistent

| Document | Issue |
|----------|-------|
| `docs/verification-and-patching.md` | Uses `remus --verify` instead of `remus-cli --verify <dat>` |
| `docs/quick-reference.md` | Same binary name drift |
| `docs/reports/ROM-MATCHING-AUDIT.md:303` | Example `remus-cli --verify --system NES` — no `--system` on verify |
| `docs/cli/README.md` | Broken link to `IMPLEMENTATION-SUMMARY.md` (actual: `CLI-IMPLEMENTATION-SUMMARY.md`) |
| `docs/plans/mod-workflow-plan.md` | Says GUI mod browsing deferred; `ModView.qml` is active |
| `docs/archive/FRONTEND-STATUS.md` | CLI-only narrative (header marks historical) |
| `docs/cli/CLI-IMPLEMENTATION-SUMMARY.md` | Milestone metrics (285 lines, 4.2 MB) outdated |
| `src/cli/cli_options.cpp` | `--no-interactive` help says "CLI-only build" |
| `docs/reports/METADATA-VERIFICATION-REPORT.md` | February 2026 snapshot; 83% multi-signal test rate cited |

---

## 7. Recommendations (prioritized)

### P1 — CI hygiene

1. **Merge clang-format fixes** — `main` currently fails the lint job locally with clang-format 22.
2. Add a pre-commit or CI note that `find src tests -name '*.cpp' -o -name '*.h' | xargs clang-format-22 -i` must run when format drifts.
3. **`GuiControllersSmokeTest`** — use `QTRY_COMPARE` instead of fixed event-loop polling so parallel `ctest -j` does not time out under CPU contention.

### P2 — Documentation sweep

1. Global replace `remus --` → `remus-cli --` in user-facing docs (except AppImage context where both binaries ship).
2. Fix `ROM-MATCHING-AUDIT.md` verify example to use `--verify <dat-file>`.
3. Update `mod-workflow-plan.md` Phase 4 status.
4. Fix broken links in `docs/cli/README.md`.

### P3 — GUI parity (product)

1. Wire `exportM3u()` in `ExportView.qml`.
2. Expose patch-create in Patch tab or defer and document CLI-only.
3. Add folder-naming scheme picker (or document CLI-only presets).
4. Optional: DAT file picker on Verify tab.

### P4 — Security hardening

1. Deprecate argv secrets (warn + env/keychain only in a future major).
2. Remove QSettings plaintext fallback after keychain migration period.
3. Implement or remove `--inner-hash` reference in verify output.

### P5 — Compendium distribution

1. Clarify in README whether releases ship populated compendium or bootstrap-only.
2. Consider CI gate: bootstrap validation passes; full content validation runs only on `--build-compendium` output artifact.

### P6 — Observability

1. Per-provider rate limit configuration (from observed-issues).
2. Expand GUI controller test coverage (`GuiExtraControllersTest` exists; continue pattern).

---

## 8. Verification commands (re-run after changes)

```bash
# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DREMUS_ENABLE_WARNINGS=ON
cmake --build build -j$(nproc)

# Tests
ctest --test-dir build --output-on-failure

# Format
bash .github/scripts/install-clang-format.sh
clang-format-22 --dry-run --Werror $(find src tests -name '*.cpp' -o -name '*.h')

# Compendium bootstrap
bash scripts/setup_compendium_db.sh
bash .github/scripts/validate-compendium-db.sh data/compendium/remus_compendium.db

# Shell / QML lint
bash .github/scripts/run-shellcheck.sh
bash .github/scripts/run-qmllint.sh

# Offline metadata (requires populated compendium)
build/remus-cli --metadata 811b027eaf99c2def7b933c5208636de --provider compendium
```

---

## 9. Change log

| Date | Change |
|------|--------|
| 2026-06-14 | Audit remediation P1–P6 implemented (GUI parity, docs, security, compendium CI, rate limits) |
| 2026-06-14 | CI shellcheck/qmllint fixes; VerifyView delegate qualification |
| 2026-06-14 | Initial full audit report |
| 2026-06-14 | clang-format drift fixed across `src/` and `tests/` |
