# Phase 1 Compendium Compiler — Implementation Status

> Updated: 2026-04-21

## Build Status

`remus-cli` builds cleanly. The compendium compiler, runtime provider, and targeted verification tests pass. Full suite status is 56/57 passing; the only remaining failure is the unrelated `RomBundlerTest` baseline in `tests/test_rom_bundler.cpp` and `tests/test_rom_bundler_disc.cpp`.

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
| `src/metadata/compendium_provider.h/.cpp` | Runtime `MetadataProvider` backed by the compiled compendium SQLite database |

### New test and fixture files

| File | Purpose |
| --- | --- |
| `tests/test_compendium_dat_extractor.cpp` | Unit coverage for DAT envelope extraction and normalization |
| `tests/test_compendium_provider.cpp` | Provider coverage for `getByHash`, `searchByName`, and `getById` |
| `tests/fixtures/test_compendium_source.dat` | Reusable DAT fixture for extractor and smoke verification |

### Wiring changes

- `src/cli/cli_commands_compendium.cpp` — replaced hardcoded zero counters; calls `CompendiumCompilerService::run()` in its own transaction after the metadata commit; report JSON now uses real `CompilerStats` values.
- `src/metadata/CMakeLists.txt` — compiler pipeline and `CompendiumProvider` sources are wired into `remus-metadata`; `remus-core` remains a dependency for system resolution.
- `src/core/constants/providers.h` — added the `compendium` provider ID, display name, capabilities, and priority `180`.
- `src/cli/cli_helpers_providers.cpp` — `buildOrchestrator()` now discovers `data/compendium/remus_compendium.db` and registers `CompendiumProvider` as an offline hash-capable provider.
- `tests/test_cli_smoke.cpp` — compendium smoke coverage now uses a real DAT fixture and asserts non-zero `games`, `game_signatures`, `source_items`, and report counters.
- `data/compendium/validation/0001_phase1_checks.sql` — now includes non-zero content assertions for `games`, `game_signatures`, and `source_items`.
- `tests/test_cli_helpers.cpp` — integration coverage now proves `buildOrchestrator()` loads and uses the compendium provider from the `data/compendium` directory.

### Architecture notes

The original PR2/PR3/PR4 breakdown (extractor, linker, inserter, merge) was a planning split. The implementation collapsed them into one coherent pass — all stages are fully implemented, not just scaffolded. The service runs:

1. **Extract** — `DatExtractor` wraps `ClrMameProParser`, emits `SourceRecordEnvelope` list
2. **Normalize** — `CompendiumNormalizer` resolves system IDs and region codes
3. **Link** — `IdentityLinker` assigns `linkedGameId` via sha1 → md5 → crc32 → serial → conservative title
4. **Persist** — `FactInserter` writes `games`, `game_names`, `game_signatures`, `game_serials`, `source_items`, `game_facts`
5. **Merge** — `MergeResolver` reads `merge_policy`, materialises `canonical_resolution` and `merge_conflicts`

The runtime adoption step is also complete:

1. **Load** — `CompendiumProvider` opens `remus_compendium.db` from `data/compendium`
2. **Query** — hash lookups read `game_signatures`; detail rows come from `games` overlaid with `canonical_resolution`
3. **Register** — `buildOrchestrator()` inserts the provider ahead of remote hash services

---

## Remaining Work

### 1. Optional follow-up coverage

The remaining useful compendium-specific tests are broader integration cases rather than missing core coverage, for example:

- provider fallback precedence when both local DATs and compendium DB are present
- compendium `searchByName()` behavior with multiple aliases/regions
- validation SQL execution in a dedicated CLI or pipeline test

### 2. Baseline suite cleanup

The only current red test is unrelated to compendium work:

- `RomBundlerTest` in `tests/test_rom_bundler.cpp`
- `RomBundlerTest` in `tests/test_rom_bundler_disc.cpp`
