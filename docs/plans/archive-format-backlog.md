# Archive Format Backlog

> Companion to [../format-matrix.md](../format-matrix.md) and [../adr/adr-0002-use-system-specific-canonical-archive-formats.md](../adr/adr-0002-use-system-specific-canonical-archive-formats.md).
> Delivery order: **CSO hardening → WBFS normalization → PBP export**.

---

## Summary

This backlog turns the format matrix into implementation work that can ship in
small, testable steps.

The policy is stable:

- **CHD** stays the canonical archival output for PS1, Sega CD, Saturn,
  Dreamcast, and PS2.
- **RVZ** stays the canonical archival output for GameCube and Wii.
- **CSO** becomes the canonical compact output for PSP.
- **WBFS** stays a normalization input or hardware-specific export, not a
  canonical library format.
- **PBP** stays an explicit PS1 export path, not a canonical archive format.

## Workstream 1 — Harden CSO As A First-Class Path

**Goal**: promote the existing `maxcso` integration from a thin CLI wrapper to a
fully tested, policy-aligned conversion path for PSP and optional PS2 export.

**Dependencies**:

- Existing `CSOConverter` and `ConversionService`
- `maxcso` tool discovery and error reporting
- Canonical policy from [../format-matrix.md](../format-matrix.md)

**Likely files**:

- `src/core/cso_converter.h`
- `src/core/cso_converter.cpp`
- `src/services/conversion_service.h`
- `src/services/conversion_service.cpp`
- `src/cli/cli_commands_convert.cpp`
- `src/cli/main.cpp`
- `src/core/rom_bundler.cpp`
- `src/core/systems.cpp`
- `tests/test_cso_converter.cpp` (new)
- `tests/test_conversion_service.cpp`
- `tests/test_cli_smoke.cpp`
- `tests/test_rom_bundler_disc.cpp`

### Phase 1.1 — Add Converter And CLI Test Coverage

- Add a dedicated `CSOConverter` unit suite that mirrors the CHD coverage level.
- Test availability detection, version parsing, convert, extract, and common
  failure paths.
- Extend CLI smoke coverage for `--convert-cso` and `--cso-extract`.

### Phase 1.2 — Add Output Verification Or Round-Trip Checks

- Add a verification method for CSO outputs if the backend provides one.
- If `maxcso` has no standalone verify command, add a conservative round-trip
  check path: decompress to ISO in a temp location and compare expected size or
  checksum.
- Make verification opt-in for large batches and mandatory before deleting
  originals.

### Phase 1.3 — Align User-Facing Policy With The Matrix

- Treat CSO as **PSP-first** in user-facing help, presets, and docs.
- Keep PS2 support as an explicit optional export path, not the default archive
  target.
- Remove GameCube and Wii from any user-facing CSO recommendation path.

### Phase 1.4 — Integrate CSO Into Presets And Bundling

- Add a PSP-specific bundle or export preset that emits `.cso`.
- Decide whether bundling needs a `DiscOutputFormat::Cso` branch or a separate
  export step outside `RomBundler`.
- Keep PS2 defaults on CHD even if CSO export is available.

### Phase 1.5 — Add Measurement And Safety Controls

- Add a preflight size estimate or measurement mode before large PSP migrations.
- Require dry-run coverage for bulk CSO conversion commands.
- Keep original ISOs until conversion and verification succeed.

### CSO Acceptance Criteria

- `--convert-cso` and `--cso-extract` are unit-tested and smoke-tested.
- PSP workflows can emit `.cso` through a documented preset or export command.
- PS2 defaults remain CHD.
- Requirements and help text no longer imply CSO is a general GameCube or Wii
  target.

## Workstream 2 — Normalize WBFS Without Making It Canonical

**Goal**: accept `.wbfs` as a Wii input, normalize it to ISO internally, and
then apply the canonical Wii rule of ISO or RVZ output.

**Dependencies**:

- External backend decision: prefer `wit` for conversion, optionally `wwt` for
  WBFS-specific verify or repair workflows
- Wii process and bundling presets already favoring RVZ

**Likely files**:

- `src/core/wbfs_converter.h` (new)
- `src/core/wbfs_converter.cpp` (new)
- `src/services/conversion_service.h`
- `src/services/conversion_service.cpp`
- `src/cli/main.cpp`
- `src/cli/cli_commands_convert.cpp` or a new Wii-specific command file
- `src/core/rom_bundler.cpp`
- `src/core/systems.cpp`
- `src/core/constants/files.h`
- `tests/test_wbfs_converter.cpp` (new)
- `tests/test_cli_smoke.cpp`
- `tests/test_rom_bundler_disc.cpp`

### Phase 2.1 — Choose The Backend Contract

- Use `wit` as the primary converter because it can read and write Wii and
  GameCube image formats on the fly.
- Treat `wwt` as optional support for WBFS-specific verification, repair, or
  hardware-facing operations.
- Define one wrapper API around the chosen commands so the rest of Remus does not
  depend on shell details.

### Phase 2.2 — Add A WBFS Converter Wrapper

- Implement tool availability and version checks.
- Support `WBFS → ISO` as the required normalization path.
- Defer `ISO → WBFS` until the hardware-export phase.

### Phase 2.3 — Integrate Normalization Into Process And Bundle Flows

- Allow `.wbfs` files to scan, match, and normalize before canonical output is
  selected.
- For Wii presets, normalize `WBFS → ISO`, then convert `ISO → RVZ` when
  `dolphin-tool` is available.
- Fall back to ISO only when RVZ conversion is unavailable or explicitly disabled.

### Phase 2.4 — Expose Explicit Commands

- Add a normalization command such as `--normalize-wbfs <path>` or
  `--wbfs-extract <path>`.
- Add a later explicit `--export-wbfs` command only if Remus needs a real-hardware
  Wii export path.
- Keep WBFS out of canonical bundle defaults.

### Phase 2.5 — Leave Room For GCZ And WIA Follow-On Work

- Design the wrapper and service APIs so `.gcz` and `.wia` can reuse the same
  normalization interface later.
- Do not block the first WBFS delivery on broader legacy-format support.

### WBFS Acceptance Criteria

- `.wbfs` files can enter the process pipeline without manual pre-conversion.
- Wii presets still output `.rvz` as the canonical archive when the toolchain is
  available.
- The documentation clearly says WBFS is normalization-only unless the user picks
  an explicit hardware export.

## Workstream 3 — Add PBP As An Explicit Export Feature

**Goal**: support PS1-to-PBP export for PSP or PS3-style compatibility workflows
without changing the canonical PS1 library format.

**Dependencies**:

- External backend choice, likely `PSXPackager`
- Existing PS1 disc handling and `.m3u` multi-disc support
- Original-file preservation and non-mutating export rules

**Likely files**:

- `src/core/pbp_exporter.h` (new)
- `src/core/pbp_exporter.cpp` (new)
- `src/services/conversion_service.h`
- `src/services/conversion_service.cpp`
- `src/cli/main.cpp`
- `src/cli/cli_commands_convert.cpp` or a new PS1 export command file
- `src/core/systems.cpp`
- `tests/test_pbp_exporter.cpp` (new)
- `tests/test_cli_smoke.cpp`

### Phase 3.1 — Lock The Backend And Tool Contract

- Evaluate `PSXPackager` as the default backend because it supports `BIN/CUE`,
  `ISO`, and multi-disc `.m3u` inputs.
- Wrap the backend with explicit availability, version, and error reporting.
- Treat the backend as an external optional dependency, not a bundled library.

### Phase 3.2 — Support Single-Disc Export

- Add explicit export commands such as `--export-pbp <path>`.
- Support `CUE/BIN → PBP` and `ISO → PBP` for single-disc PS1 titles.
- Keep the source files unchanged after export.

### Phase 3.3 — Support Multi-Disc Export

- Accept `.m3u` playlists as the canonical multi-disc input for PBP export.
- Preserve disc order during export.
- Document that multi-disc PBP is a convenience package, not the default archive
  representation in the library.

### Phase 3.4 — Defer Cosmetic Resource Support

- Keep resource customization such as icons and splash art out of the first
  implementation.
- Ship a plain export path first.
- Add resource import or customization only if the export feature proves useful.

### PBP Acceptance Criteria

- PS1 users can export a `.cue` or `.m3u` workflow to `.pbp` with a single
  explicit command.
- The PS1 canonical archive format in the main library remains CHD.
- Requirements and docs treat PBP as export-only.

## Cross-Cutting Guardrails

- Preserve source files until conversion or export succeeds and passes any
  verification gate.
- Prefer one wrapper class per external backend instead of sprinkling raw process
  calls through CLI code.
- Add fake-process unit tests for every new wrapper.
- Keep the requirements, the format matrix, and the ADR aligned whenever policy
  changes.

## Not In Scope For This Backlog

- First-class canonical output support for WIA, GCZ, NKit, or CISO/WBI.
- Automatic replacement of canonical library files with PBP exports.
- Treating WBFS as a preservation-grade default.

## Recommended Sequence

1. Finish CSO hardening because the core wrapper already exists.
2. Add WBFS normalization next because it unlocks more Wii ingest with low policy risk.
3. Add PBP export last because it is explicitly non-canonical and should remain a
   separate workflow.
