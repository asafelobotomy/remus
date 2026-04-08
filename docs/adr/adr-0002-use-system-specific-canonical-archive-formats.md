---
title: "ADR-0002: Use System-Specific Canonical Archive Formats"
status: "Accepted"
date: "2026-04-05"
authors: "Solon"
tags: ["architecture", "formats", "archival", "policy"]
supersedes: ""
superseded_by: ""
---

## Status

Accepted.

## Context

Remus already recognizes or documents a growing set of disc and container
formats: CHD, RVZ, CSO, WBFS, GCZ, PBP, and others. That flexibility is useful
for import and export, but it also creates drift.

The codebase already has first-class wrappers for `chdman`, `dolphin-tool`, and
`maxcso`, but the user-facing documentation does not consistently distinguish
between these three roles:

- a format Remus should keep as the canonical archive in the library
- a legacy or hardware-oriented format Remus should normalize on ingest
- a compatibility export format Remus should generate only when explicitly asked

Without a stable policy, every new format discussion turns into a fresh design
debate. That increases documentation drift, confuses presets, and raises the risk
that future format work expands scope without a clear preservation goal.

Key constraints:

- **Preservation first**: canonical library formats must remain lossless and
  reversible where practical.
- **System specificity**: no single format is the best choice for every disc
  family.
- **Tool realism**: Remus should prefer mature external backends over custom
  in-process implementations.
- **Scope control**: legacy formats should not become canonical outputs by
  default just because they are common in the wild.

## Decision

Remus adopts one canonical archive output per relevant system family and treats
all other disc formats as either normalization inputs or explicit export targets.

### Canonical archive outputs

- **CHD** for PlayStation, Sega CD, Saturn, Dreamcast, and PlayStation 2
- **RVZ** for GameCube and Wii
- **CSO** for PSP

### Normalization-only formats

- **WBFS** for Wii ingest and optional future hardware export
- **GCZ** for GameCube or Wii ingest
- **WIA** for future GameCube or Wii ingest if added
- **CISO/WBI** for future normalization only
- **NKit** only as a future import path, never as a canonical Remus output

### Export-only formats

- **PBP / EBOOT.PBP** for explicit PS1 export workflows targeting PSP or PS3-style
  compatibility use cases

### Operational rules

- Remus does not replace canonical library files with export-only artifacts.
- Remus preserves original source files until conversion or export succeeds and,
  where supported, verification passes.
- Any future format work must declare whether the format is canonical,
  normalization-only, or export-only before implementation begins.

## Consequences

### Positive

- **POS-001**: Future format work has a stable policy anchor.
- **POS-002**: User-facing presets become easier to explain and maintain.
- **POS-003**: Legacy formats such as WBFS and GCZ can be supported without
  inflating the number of canonical library targets.
- **POS-004**: PS1 compatibility exports such as PBP can ship without weakening
  CHD as the archival default.
- **POS-005**: Documentation, CLI help, and implementation backlog can all point
  to one policy.

### Negative

- **NEG-001**: Some accepted input formats will remain second-class citizens even
  after they are supported.
- **NEG-002**: Users who expect a single universal format may need clearer
  education around system-specific defaults.
- **NEG-003**: Some code-level extension mappings may need cleanup to match the
  policy precisely.

## Alternatives Considered

### Treat every supported format as a peer

- **ALT-001**: **Description**: Any format that Remus can read or write becomes a
  first-class archive target.
- **ALT-002**: **Rejection Reason**: This causes policy drift, documentation
  sprawl, and ambiguous presets.

### Use CHD for every disc-based system

- **ALT-003**: **Description**: Standardize all optical media on CHD regardless
  of platform.
- **ALT-004**: **Rejection Reason**: GameCube and Wii workflows align better with
  RVZ, especially in Dolphin-class environments.

### Keep format decisions ad hoc by feature

- **ALT-005**: **Description**: Let each future feature choose formats case by
  case without a stable policy.
- **ALT-006**: **Rejection Reason**: This repeats the same decision process and
  creates long-term maintenance drift.

## Implementation Notes

- **IMP-001**: The system policy is summarized in
  [../format-matrix.md](../format-matrix.md).
- **IMP-002**: The concrete delivery work is tracked in
  [../plans/archive-format-backlog.md](../plans/archive-format-backlog.md).
- **IMP-003**: Requirements, CLI help, and preset behavior should be updated to
  reflect this ADR whenever a format capability changes.

## References

- **REF-001**: [../format-matrix.md](../format-matrix.md)
- **REF-002**: [../requirements.md](../requirements.md)
- **REF-003**: Dolphin Progress Report: May and June 2020
- **REF-004**: Wiimm ISO Tools documentation
- **REF-005**: `maxcso` documentation
- **REF-006**: `PSXPackager` documentation
