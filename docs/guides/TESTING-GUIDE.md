# Remus Testing Guide

Use the canonical repository-local paths from [TEST-DATA-POLICY.md](TEST-DATA-POLICY.md): keep source ROMs under `roms/` and keep processed test runs under `test_output/` with no more than 5 active cases.

## Verify the supported build

Remus is tested as a CLI-first application. Both `src/cli/` (CLI) and `src/gui/` (Qt Quick GUI) are active and are part of the supported build. The legacy TUI is archived under `archive/gui-tui/` and is not part of the default verification path.

Build the workspace:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

## Run the default regression suite

Run the full suite before marking a non-trivial change done:

```bash
ctest --test-dir build -j$(nproc)
```

If you need failing test output, use:

```bash
ctest --test-dir build -j$(nproc) --output-on-failure
```

## Run a focused test while iterating

Use a focused run when you are changing one subsystem and want a tighter loop:

```bash
ctest --test-dir build -R ArchiveExtractorTest --output-on-failure
ctest --test-dir build -R ModWorkflowTest --output-on-failure
ctest --test-dir build -R DatabaseTest --output-on-failure
```

Rebuild before rerunning a test when you changed code or test sources.

## GUI smoke tests

When changing `src/gui/`, run the controller smoke suite:

```bash
ctest --test-dir build -R GuiControllersSmokeTest --output-on-failure
```

Optional manual launch:

```bash
./build/src/gui/remus-gui
```

## Exercise CLI workflows manually

Sanity-check the shipped interface with the built executable:

```bash
./build/remus-cli --help
./build/remus-cli --scan roms --hash
./build/remus-cli --list
```

Use small repository-local samples for manual verification. Record follow-up items in the tracked `test_output/attention.log` when a run exposes a gap that is not part of the current patch.

## Verify metadata matching safely

Recommended flow for local verification:
1. Put a small sample set under `roms/`.
2. Run `--scan` and `--hash`.
3. Run matching commands with the provider configuration you want to verify.
4. Inspect output and database state.

Useful checks:

```bash
sqlite3 ~/.local/share/Remus/Remus/remus.db \
  "SELECT filename, crc32, md5, sha1 FROM files LIMIT 10"
```

```bash
sqlite3 ~/.local/share/Remus/Remus/remus.db \
  "SELECT provider, confidence, title FROM matches LIMIT 10"
```

## Watch for common failures

- Missing external tools: CI installs `zip`, `unzip`, and `p7zip-full` via the build-deps script. Locally, also install `chdman`, `maxcso`, `xdelta3`, or `flips` before testing workflows that depend on them.
- Missing provider credentials: ScreenScraper, IGDB, and RetroAchievements need credentials before authenticated calls succeed.
- Stale binaries: rebuild after source changes before interpreting a test result.
- Large manual runs: keep artifacts under `test_output/` and trim them after review.

## Treat this as done

A change is ready when:
- the project builds cleanly
- the relevant focused tests pass
- `ctest --test-dir build -j$(nproc)` passes
- any manual CLI workflow you changed has been exercised at least once
- GUI changes pass `GuiControllersSmokeTest` (or broader GUI verification when appropriate)
