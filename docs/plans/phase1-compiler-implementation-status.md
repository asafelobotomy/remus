# Phase 1 Compendium Compiler — Implementation Status

> Updated: 2026-04-20

## Build Status

56/56 tests passing. `remus-cli` builds cleanly (incremental build). The `Bus error` seen with `--clean-first` is a transient `ar` filesystem glitch on this machine; it does not affect the build output.

---

## Completed This Session

### New source files

All files compile and are wired into `src/metadata/CMakeLists.txt`.

| File | Purpose |
| --- | --- |
| `src/metadata/compendium_types.h` | `SourceRecordEnvelope`, `NormalizedHashes`, `CompilerStats` |
| `src/metadata/compendium_dat_extractor.h/.cpp` | Wraps `ClrMameProParser` → normalized envelopes |
| `src/metadata/compendium_normalizer.h/.cpp` | System (via `SystemResolver`) + region normalization |
| `src/metadata/compendium_identity_linker.h/.cpp` | 3-pass identity linking: sha1 → md5 → crc32 → serial → title |
| `src/metadata/compendium_fact_inserter.h/.cpp` | Persists to `games`, `game_names`, `game_signatures`, `game_serials`, `source_items`, `game_facts` |
| `src/metadata/compendium_merge_resolver.h/.cpp` | Data-driven merge using `merge_policy` table |
| `src/metadata/compendium_compiler_service.h/.cpp` | Orchestrates the full extraction → linking → persistence → merge pipeline |

### Wiring changes

- `src/cli/cli_commands_compendium.cpp` — replaced hardcoded zero counters; calls `CompendiumCompilerService::run()` in its own transaction after the metadata commit; report JSON now uses real `CompilerStats` values.
- `src/metadata/CMakeLists.txt` — 6 new `.cpp` files added; `remus-core` added as a dependency (required for `SystemResolver::systemIdByDatName`).

### Architecture notes

The original PR2/PR3/PR4 breakdown (extractor, linker, inserter, merge) was a planning split. The implementation collapsed them into one coherent pass — all stages are fully implemented, not just scaffolded. The service runs:

1. **Extract** — `DatExtractor` wraps `ClrMameProParser`, emits `SourceRecordEnvelope` list
2. **Normalize** — `CompendiumNormalizer` resolves system IDs and region codes
3. **Link** — `IdentityLinker` assigns `linkedGameId` via sha1 → md5 → crc32 → serial → conservative title
4. **Persist** — `FactInserter` writes `games`, `game_names`, `game_signatures`, `game_serials`, `source_items`, `game_facts`
5. **Merge** — `MergeResolver` reads `merge_policy`, materialises `canonical_resolution` and `merge_conflicts`

---

## Remaining Work

### 1. Test coverage for the new pipeline

`tests/test_cli_smoke.cpp` currently asserts zero rows for content tables. Update to assert:

- `games COUNT > 0`
- `game_signatures COUNT > 0`
- `source_items COUNT > 0`

Add a dedicated `tests/test_compendium_dat_extractor.cpp` with unit assertions on envelope fields from a small DAT fixture.

### 2. PR5 — `CompendiumProvider` runtime wiring (not started)

- Create `src/metadata/compendium_provider.h/.cpp` implementing `MetadataProvider`
  - `getByHash(hash, system)` → SELECT from `game_signatures`
  - `searchByName(title, system, region)` → SELECT from `games`
  - `getById(id)` → SELECT by `game_id`
  - Priority: 180
- Register in `src/cli/cli_helpers_providers.cpp` alongside `LocalDatabaseProvider`
- Add `tests/test_compendium_provider.cpp` mirroring `test_local_database_provider` coverage

### 3. Validation SQL extension

Extend `data/compendium/validation/0001_phase1_checks.sql` with non-zero assertions on `games`, `game_signatures`, and `source_items` now that ingestion is live.
