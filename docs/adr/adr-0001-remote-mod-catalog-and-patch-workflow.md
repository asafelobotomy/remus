---
title: "ADR-0001: Remote Mod Catalog and Integrated Patch Workflow"
status: "Proposed"
date: "2026-03-23"
authors: "Solon"
tags: ["architecture", "patching", "metadata", "mods"]
supersedes: ""
superseded_by: ""
---

# ADR-0001: Remote Mod Catalog and Integrated Patch Workflow

## Status

**Proposed**

## Context

Remus currently supports manual patching (apply a local patch file to a local ROM) and
records lineage in `applied_patches`. The requirements doc defines three tiers:

| Tier | Description | Status |
|------|-------------|--------|
| M8 — Manual Patch | User selects base ROM + local patch file | Implemented |
| M9 — Semi-Automatic | User provides a romhacking.net URL, Remus downloads and applies | Not started |
| Future — Patch Discovery | Remus queries a remote catalog, matches patches to library, one-click apply | Not started |

Users want to: select a matched game → browse available mods → pick one → Remus
downloads the patch, applies it to the original ROM (leaving it intact), and bundles
the result with metadata and artwork, identical to how verified ROMs are bundled today.

Key constraints:

- **Original ROM preservation**: the base ROM file record must never be mutated.
- **Catalog keying**: mods must be matched by base ROM hash (not filename), because
  filenames vary while hashes are stable.
- **No official API**: romhacking.net has no public API; scraping must comply with
  `robots.txt` and be rate-limited. A local JSON catalog is needed as an interim format.
- **Bundler coupling**: `RomBundler::bundle()` currently mutates the original
  `FileRecord` (`updateFilePath`, `markFileProcessed`). The mod workflow needs a
  variant that operates on a staged patched file without touching the base record.

## Decision

Implement the mod workflow in five phases, each independently shippable and testable.
Introduce three new components and one database migration:

1. **`ModCatalogProvider`** — fetches and caches a remote JSON mod catalog
2. **`ModWorkflowService`** — orchestrates download → patch → bundle for a mod
3. **New DB table `mod_installations`** — tracks installed mods as derived artifacts
4. **`RomBundler::bundleStaged()`** — bundles a staged file without mutating the base record
5. **CLI commands + GUI/TUI integration** for browsing and installing mods

### Data model: patched ROMs as derived artifacts

A patched ROM is a *derived artifact* linked to its base file, not a replacement:

```
files (id=42, original ROM, never mutated)
  └── mod_installations (base_file_id=42, catalog_mod_id="xxxx", output_path=...)
        └── files (id=99, the patched ROM, is_patched=true, parent_file_id=42)
```

The patched ROM gets its own `files` row (hashable, matchable, bundleable) with
`is_patched=true` and `parent_file_id` pointing back to the base.

## Consequences

### Positive

- **POS-001**: Original ROMs are never modified or lost
- **POS-002**: Patched ROMs are first-class `files` entries — they flow through hash,
  match, verify, and bundle like any other ROM
- **POS-003**: Catalog-keyed-by-hash means mods auto-match regardless of filename
  conventions
- **POS-004**: Local JSON catalog allows offline use and community-contributed catalogs
- **POS-005**: Each phase ships independently; value is delivered incrementally

### Negative

- **NEG-001**: Scraping romhacking.net is fragile; site changes break the scraper
- **NEG-002**: Dual "original + patched" files increase library size
- **NEG-003**: `bundleStaged()` introduces a second code path that must stay in sync
  with `bundle()`

## Alternatives Considered

### In-place replacement

- **Description**: Replace the original ROM with the patched version, update the
  existing `files` row
- **Rejection Reason**: Violates the preservation principle. Users lose the verified
  original. Impossible to undo without re-downloading the ROM.

### Fully embedded catalog (no remote)

- **Description**: Ship a curated catalog inside the Remus binary
- **Rejection Reason**: Catalog staleness, bloats binary size, requires a release
  for every catalog update. Remote + local cache is strictly better.

### Use RHDN API directly at runtime without caching

- **Description**: Query romhacking.net on every mod-list request
- **Rejection Reason**: No official API exists, scraping is slow, rate-limited, and
  fails offline. A cached catalog is essential.

## Implementation Notes

- **IMP-001**: See the companion implementation plan in `docs/plans/mod-workflow-plan.md`
- **IMP-002**: Phase 1 (CLI prototype with local JSON) can ship without any network dependency
- **IMP-003**: Existing `applied_patches` table continues to store raw patch lineage;
  `mod_installations` adds the catalog/workflow layer on top

## References

- **REF-001**: [docs/requirements.md](../requirements.md) §ROM Patching — M8/M9/Future tiers
- **REF-002**: [docs/verification-and-patching.md](../verification-and-patching.md) — patching overview
- **REF-003**: romhacking.net — primary community patch source (no official API)
