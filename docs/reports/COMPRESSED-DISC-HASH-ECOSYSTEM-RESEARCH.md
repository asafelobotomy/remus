# Compressed Disc Hash Ecosystem — Research Report

**Date:** 2026-06-18  
**Context:** Follow-up after shipping `chd_sha1` at hash time (`6d87da2`). This report surveys how
peer ROM/library tools handle CHD, RVZ, CSO, Hasheous offline data, and multi-file hashing, and
recommends a Remus roadmap informed by industry practice.

**Related docs:**

- [COMPENDIUM-MULTI-DISC-SHA256-RESEARCH.md](COMPENDIUM-MULTI-DISC-SHA256-RESEARCH.md) — compendium-side hash bridges
- [ROM-MATCHING-AUDIT.md](ROM-MATCHING-AUDIT.md) — Remus match cascade
- [COMPENDIUM-DATA-SOURCES.md](COMPENDIUM-DATA-SOURCES.md) — DAT/source inventory

---

## Executive summary

The ecosystem has converged on a **two-layer hash model** for compressed disc images:

| Layer | What it is | Used for |
|-------|------------|----------|
| **Container hashes** | CRC/MD5/SHA1 of the `.chd`/`.rvz` file bytes | ScreenScraper KO logging, local audit, negative-cache |
| **Content/header hashes** | CHD header SHA1 (`chd_sha1`), dolphin-tool ISO hash for RVZ | Hasheous, PlayMatch, MAME Redump DATs, compendium bridges |

Remus now stores layer 2 for CHD and RVZ. Completed roadmap (2026-06-18):

1. **P1** — CHD backfill for libraries hashed before `chd_sha1`; confirm we index the same SHA1 Hasheous uses. **Done**
2. **P2** — Ingest MAME Redump CHD DATs; add `dolphin-tool` content hash for RVZ. **Done**
3. **P3** — Self-hosted Hasheous URL; CSO verification via maxcso; CHD v5 native header read. **Done**

**Do not wait on Hasheous bulk offline dumps** — the offline path is self-hosted Hasheous + local DAT import, or Remus compendium (Gaseous “LocalOnly” model).

---

## Comparable projects

### RomM — closest peer

RomM 4.9+ ([release](https://github.com/rommapp/romm/releases/tag/4.9.0)) is the reference implementation:

- Stores **raw container** CRC/MD5/SHA1 **and** `chd_sha1_hash` ([commit 01f0b1d](https://github.com/rommapp/romm/commit/01f0b1d2b50ba050565832a66ec8d2cab8c93ea4)).
- ScreenScraper receives container hashes; Hasheous receives **only** `chd_sha1_hash` ([PR #3498](https://github.com/rommapp/romm/pull/3498)).
- Multi-file Hasheous array: one hash object per top-level file (Remus aligned).
- Archive composite hashing ([PR #3412](https://github.com/rommapp/romm/pull/3412)): hash every ZIP member in ASCII order, not largest-file-only.
- Stored DB hash fallback on UNMATCHED/UPDATE scans when filesystem re-hash is skipped.
- CHD v5: optional in-process header parse ([PR #2678](https://github.com/rommapp/romm/pull/2678)) instead of spawning `chdman` per file.

Community discussion on [RomM #2241](https://github.com/rommapp/romm/issues/2241) clarifies:

- `chdman info` exposes **SHA1** (MAME/header index) and **Data SHA1** (uncompressed payload) — different values.
- MAME Redump DATs index header **SHA1** (combined raw+meta).
- Multi-track CD CHDs are hit-or-miss for single header hash; DVD single-track ISO CHDs work cleanly.

### PlayMatch + Hasheous — bridge services

**PlayMatch** ([RetroRealm/playmatch](https://github.com/RetroRealm/playmatch)):

- Daily No-Intro/Redump DAT sync; Rust microservice.
- Community DATs for RVZ, WUX, decrypted PS3 ([interview](https://lemmy.world/post/33524899)).
- RomM forwards manual match suggestions to improve community coverage.
- CHD/RVZ hosted API still maturing; priority is **CHD-hash DATs** not on-the-fly decompress ([RomM #2241](https://github.com/rommapp/romm/issues/2241)).

**Hasheous** ([gaseous-project/hasheous](https://github.com/gaseous-project/hasheous)):

- No public bulk SQLite/zip export.
- Offline = **self-host** `hasheous-server` + DAT folders under `Data/Signatures/`.
- MCP `hasheous_lookup_hashes` supports CRC/MD5/SHA1/SHA256.
- API misses reported even when website lists CHD hashes ([RomM #2241 comments](https://github.com/rommapp/romm/issues/2241)).

### Gaseous — local signatures model

- `SignatureSource`: `LocalOnly` (import DATs) vs `Hasheous` ([wiki](https://github.com/gaseous-project/gaseous-server/wiki/Signatures)).
- Remus compendium + `remus-cli --build-compendium` is the analogue.

### Igir — verification tooling standard

- v3+ bundles `chdman`, `maxcso`, `dolphin-tool` ([v3.0.0](https://github.com/emmercm/igir/releases/tag/v3.0.0)).
- CHD **quick scan**: header SHA1 only ([matching docs](https://igir.io/roms/matching/)).
- Full verify = decompress (slow). CSO quick CRC via maxcso.
- MAME Redump CHD DATs work with quick SHA1; raw Redump DATs do not match CHD without conversion ([issue #937](https://github.com/emmercm/igir/issues/937)).

### MAME Redump — compressed-disc DAT authority

[MetalSlug/MAMERedump](https://github.com/MetalSlug/MAMERedump):

- CHD sets indexed by **SHA1 from CHD header** (combined raw+meta).
- GC/Wii RVZ sets often **name-matched** today, not full content-hash coverage.
- Standardizes chdman version, codec, cue/gdi/iso metadata — users must match params for hits.

### verifydump — audit, not metadata

[j68k/verifydump](https://github.com/j68k/verifydump): decompress CHD/RVZ → compare to raw Redump DATs. Correct for verification; too heavy for scan-time matching.

### ES-DE scrapers (Scrauper, scrapegoat)

Hash-first ScreenScraper (CRC/MD5/SHA1). No CHD/RVZ special casing — same ScreenScraper limitation.

---

## Remus status vs peers

| Capability | RomM 4.9 | Remus (post-6d87da2) | Gap |
|------------|----------|----------------------|-----|
| `chd_sha1` at scan | ✓ | ✓ | Backfill pre-change libraries |
| Raw + content dual store | ✓ | Container only in bridges | ScreenScraper path N/A |
| Hasheous array multi-disc | ✓ | ✓ | — |
| Hasheous CHD-only payload | ✓ | ✓ | — |
| CHD backfill CLI | implicit re-scan | **P1** `--hash-chd-backfill` | This sprint |
| MAME Redump CHD DAT ingest | via agents | compendium raw Redump only | P2 |
| RVZ content hash | dolphin-tool | not started | P2 |
| Archive composite hash | ✓ | largest-file risk | P2 |
| Hasheous offline dumps | self-host | compendium | document, optional URL config |

---

## CHD SHA1 field semantics (critical)

`chdman info` output includes:

| Label | Meaning | Hasheous / MAME Redump |
|-------|---------|------------------------|
| **SHA1** | Header index (compressed data + metadata stream) | **Yes** — bridge key |
| **Data SHA1** | Hash of uncompressed payload | No — verification only |

Remus must store **SHA1** (header), not Data SHA1, in `files.chd_sha1`. See `CHDInfo::hasheousDiscSha1()` in `chd_converter`.

---

## Recommended roadmap

### P1 — CHD backfill + field validation

| Task | Status | Notes |
|------|--------|-------|
| `--hash-chd-backfill` CLI | **Done** | Fill `chd_sha1` for `.chd` rows missing it |
| Persist `chdSha1`/`raMd5` in all `updateFileHashes` call sites | **Done** | CLI/GUI hash paths |
| Parse SHA1 vs Data SHA1 explicitly | **Done** | Tests lock Hasheous field choice |
| Match uses stored `chd_sha1` | **Done** | Via `selectContentSha1()` + compendium cascade |

### P2 — Compressed format coverage

| Task | Status | Notes |
|------|--------|-------|
| Ingest MAME Redump CHD DATs into compendium | **Done** | `update_dats.sh` + manifest `mame-redump-chd` (priority 35) |
| `dolphin-tool verify -a sha1` for RVZ/GCZ | **Done** | `files.rvz_sha1`, `--hash-rvz-backfill` |
| PlayMatch / Hasheous bridge for RVZ at runtime | **Done** | `rvzSha1` on `HasheousHashEntry`; content SHA1 in compendium match |
| Composite archive hashing | **Done** | Multi-ROM ZIP: SHA1 of sorted member SHA1s (RomM #3412 pattern) |

### P3 — Ops / defer

| Task | Status | Notes |
|------|--------|-------|
| `HASHEOUS_BASE_URL` config | **Done** | `REMUS_HASHEOUS_BASE_URL`, settings `hasheous/base_url`, `--hasheous-base-url` |
| CSO via maxcso | **Done** | `--cso-verify` round-trip decompress check |
| CHD v5 in-process header read | **Done** | `readChdHeaderDigest()`; backfill/hash without `chdman` for v5 |
| `game_discs` compendium table | **Plan** | See [COMPENDIUM-DISC-SETS-PLAN.md](COMPENDIUM-DISC-SETS-PLAN.md) — `game_disc_sets` + `game_disc_tracks` |

---

## What not to do

- **Do not** wait for Hasheous hosted bulk dumps — use compendium or self-host.
- **Do not** decompress CHD/RVZ on every metadata scan — use header/content quick hashes + CHD DATs.
- **Do not** use Data SHA1 for Hasheous — wrong field for MAME Redump index.
- **Do not** merge all disc tracks into one DAT envelope — loses per-disc hashes.

---

## External references

| Topic | URL |
|-------|-----|
| RomM CHD hashing | https://github.com/rommapp/romm/commit/01f0b1d |
| RomM Hasheous array | https://github.com/rommapp/romm/pull/3498 |
| RomM CHD/RVZ feature thread | https://github.com/rommapp/romm/issues/2241 |
| MAME Redump | https://github.com/MetalSlug/MAMERedump |
| Igir CHD matching | https://igir.io/roms/matching/ |
| Hasheous project | https://github.com/gaseous-project/hasheous |
| PlayMatch | https://github.com/RetroRealm/playmatch |
| verifydump | https://github.com/j68k/verifydump |
| dolphin-tool verify | https://github.com/dolphin-emu/dolphin/pull/10252 |
| CHD format notes | https://wiki.romcenter.com/wiki/doku.php?id=chd |

---

*Generated from codebase review and external research, 2026-06-18.*
