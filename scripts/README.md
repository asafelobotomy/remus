# Scripts

Shell helpers for build, compendium, packaging, testing, and workspace hygiene. All paths are relative to the repository root unless noted.

## Workspace & quality

| Script | Purpose |
|--------|---------|
| [`clean-workspace.sh`](clean-workspace.sh) | Remove audit build trees (`build-coverage`, `build-asan`, `build-tidy`), local `*.db` files, and prune `test_output/` |
| [`bootstrap-dev-environment.sh`](bootstrap-dev-environment.sh) | **Fresh machine / Cursor workspace setup** — optional clone, OS packages, Remus profile binding, extensions, compendium, CMake, editor config |
| [`configure-workspace-extensions.sh`](configure-workspace-extensions.sh) | Ensure Remus profile + workspace binding, tool paths, SQLTools absolute DB path |
| [`lib/cursor-remus-profile.sh`](lib/cursor-remus-profile.sh) | Cursor profile registration and workspace association helpers |
| [`resolve_compendium_db.sh`](resolve_compendium_db.sh) | Locate (or bootstrap) `remus_compendium.db` and emit an absolute path for SQLTools |
| [`run-local-audit.sh`](run-local-audit.sh) | Full local CI mirror: Release build, tests, clang-format, shellcheck, qmllint, compendium bootstrap, coverage, clang-tidy spot-check, ASan tests |
| [`prune_test_output.sh`](prune_test_output.sh) | Drop transient files under `test_output/` (keeps `README.md` and `attention.log` by default) |
| [`verify_credentials.sh`](verify_credentials.sh) | Check that `.env.local` provider credentials are loadable |

## Compendium

| Script | Purpose |
|--------|---------|
| [`setup_compendium_db.sh`](setup_compendium_db.sh) | **Run once per clone** — create bootstrap `data/compendium/remus_compendium.db` (schema + seeds; gitignored) |
| [`apply_compendium_migrations.sh`](apply_compendium_migrations.sh) | Apply incremental migrations (0008+) on an existing populated DB |
| [`run_compendium_job.sh`](run_compendium_job.sh) | **Serialize** compendium sqlite/cli jobs (flock, optional timeout, busy_timeout) |
| [`validate_compendium_quick.sh`](validate_compendium_quick.sh) | Fast quality gate: phase-2 checks only (~1 min) |
| [`validate_compendium_extended.sh`](validate_compendium_extended.sh) | Extended checks (0003) after migrations (~30s) |
| [`verify_compendium_supplemental.sh`](verify_compendium_supplemental.sh) | Smoke: supplemental manifest prefixes + LaunchBox enrich on fixture |
| [`build_compendium_full.sh`](build_compendium_full.sh) | Full pipeline: refresh DATs, generate manifest, build populated DB, validate, per-source + disc-set coverage TSV |
| [`ci_compendium_fixture_build.sh`](ci_compendium_fixture_build.sh) | CI smoke: fixture DAT ingest + disc-set validation gates |
| [`generate_compendium_manifest.sh`](generate_compendium_manifest.sh) | Build `compendium-manifest-full.json` from DAT files (includes `supplemental/homebrew`, `supplemental/libretro-dats`, optional `supplemental/tosec`) |
| [`backfill_disc_sets.sh`](backfill_disc_sets.sh) | One-time backfill of `game_disc_sets` / tracks on an existing compendium DB |
| [`import_patch_catalog.sh`](import_patch_catalog.sh) | Import libretro patch DATs into compendium patch catalog tables |
| [`audit_shadowed_manifest_sources.sh`](audit_shadowed_manifest_sources.sh) | List manifest sources that ingest items but own zero signatures |

### Compendium job time budgets

| Job | Expected duration | Where to run |
|-----|-------------------|--------------|
| Phase 2 validation (`validate_compendium_quick.sh`) | ~1 min | Agent OK via `run_compendium_job.sh` |
| Extended validation (`validate_compendium_extended.sh`) | ~30s | Agent OK, **one at a time** |
| Offline enrichment (4 passes, optimized build) | ~4 min | Agent OK; use `--log-file`, not `/dev/null` |
| Full online enrichment (IGDB bulk) | 30+ min | User terminal |
| Full compendium build | hours | User terminal or CI |

Long jobs: always use `--log-file` and `tail` the log for progress. See [docs/setup/CURSOR-AGENT-TERMINAL.md](../docs/setup/CURSOR-AGENT-TERMINAL.md).

## Data refresh

| Script | Purpose |
|--------|---------|
| [`update_dats.sh`](update_dats.sh) | Download/update No-Intro/Redump DAT files under `data/databases/`; sync libretro homebrew + libretro-dats into `data/databases/supplemental/` (TOSEC: manual drop-in at `supplemental/tosec/`) |
| [`update_hasheous_dumps.sh`](update_hasheous_dumps.sh) | Download Hasheous offline platform dump ZIPs to `data/hasheous/dumps/` for offline hash enrichment |
| [`update_mame_listxml.sh`](update_mame_listxml.sh) | Fetch MAME `listxml.xml` for compendium enrichment |

## Testing

| Script | Purpose |
|--------|---------|
| [`run_pipeline_test.sh`](run_pipeline_test.sh) | Timestamped scan→match→report runs under `test_output/` (auto-prunes old runs) |
| [`test_enricher.sh`](test_enricher.sh) | Smoke-test compendium enrichment sources |

## Packaging

| Script | Purpose |
|--------|---------|
| [`package_cli_archive.sh`](package_cli_archive.sh) | Build versioned `remus-cli-*.tar.gz` (+ compendium bootstrap if present) |
| [`package_appimage.sh`](package_appimage.sh) | Build AppImage bundling CLI + GUI (+ compendium bootstrap if present) |

## CMake presets (aligned build dirs)

Use presets from [`CMakePresets.json`](../CMakePresets.json) for non-default builds:

```bash
cmake --preset default && cmake --build build -j$(nproc)   # Release → build/
cmake --preset debug   && cmake --build build-debug -j$(nproc)
cmake --preset asan    && cmake --build build-asan -j$(nproc)
cmake --preset coverage && cmake --build build-coverage -j$(nproc)
ctest --test-dir build-asan --output-on-failure
```

See also [`.github/scripts/`](../.github/scripts/) for CI-only helpers (clang-format install, coverage threshold, compendium validation).
