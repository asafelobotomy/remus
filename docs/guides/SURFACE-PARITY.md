# CLI vs GUI Surface Parity

## Runtime (library work)

| Capability | CLI | GUI | Notes |
|------------|-----|-----|-------|
| Scan / hash | `--scan`, `--hash-all` | Pipeline / workbench | Shared engine |
| Match / enrich | `--match`, `--enrich` | Match & Enrich dialog | **Compendium-only by default** |
| Organize | `--organize` | Rename/organize dialog | Offline |
| Verify | `--verify`, `--verify-set` | Verify tab | GUI uses imported DAT catalogs |
| Export | `--export` | Export tab | M3U, RetroArch, ES-DE, etc. |
| Patch / mods | `--patch-*`, `--mod-*` | Utilities drawer | |

## Compendium build (CLI / scripts)

| Capability | Entry |
|------------|-------|
| First build | `scripts/init_compendium.sh`, `remus-cli --init-compendium` |
| Full rebuild | `scripts/build_compendium_full.sh`, `remus-cli --build-compendium` |
| Extend source | `remus-cli --enrich-compendium --enrich-source <name>` |
| Add DAT | `remus-cli --ingest-source <path>` |

GUI: Settings → Compendium → **Compendium Wizard** — full `build_compendium_full.sh` (detached, credentials, progress) or **Extend source** mode (`--enrich-compendium`). Packaged AppImages bundle `scripts/` and `data/compendium/` under `usr/share/remus` with `REMUS_DATA_DIR` set by the launcher.

## CLI-only automation

- `--json`, `--match-report`, `--log-file`, `--dry-run`
- `--library`, `--process`, `--process-preset`
- Compendium maintenance (`--dedup-compendium`, `--coverage-report`, …)
- Standalone converters (`--chd-extract`, `--space-report`, …)

## GUI-only UX

- Match confirm/reject, organize preview/undo, inspector artwork, settings wizards

See [OFFLINE-FIRST-COMPENDIUM.md](../architecture/OFFLINE-FIRST-COMPENDIUM.md) for the offline runtime model.
