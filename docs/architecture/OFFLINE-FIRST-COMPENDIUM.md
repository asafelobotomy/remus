# Offline-First Compendium Architecture

Remus is designed as a self-contained, offline-first ROM library manager. Daily match, enrich, and organize operations use **only** `remus_compendium.db` and its local asset blobs. Network access and API credentials are for **compendium construction** only.

## Two phases

| Phase | When | Providers | Credentials |
|-------|------|-----------|-------------|
| **Build** | First use / extend catalog | DAT ingest + enrichment passes (offline files + optional APIs) | `.env.local`, enrichment-credentials.json, Settings (build) |
| **Runtime** | Library work | `compendium` only (default) | Not used |

## Runtime modes

- **Compendium-only (default)** when `data/compendium/remus_compendium.db` exists. CLI: `--compendium-only` / `--offline`. GUI: `metadata/runtime_mode=compendium-only`.
- **Online fallback (legacy)** — remote providers may fill metadata gaps. CLI: `--online-fallback`. GUI: `metadata/runtime_mode=online-fallback`.

## First build

**GUI (recommended):** Settings → Compendium → **Open Compendium Wizard…** — runs bootstrap schema when needed, then `build_compendium_full.sh` (detached by default).

**CLI:**

```bash
bash scripts/init_compendium.sh
# or
remus-cli --init-compendium
```

## Extend without full rebuild

**GUI:** Compendium Wizard → **Extend source** — select `--enrich-source` keys and optional artwork consolidate.

**CLI:**

```bash
remus-cli --enrich-compendium --enrich-source igdb
remus-cli --enrich-compendium --enrich-source screenscraper
remus-cli --ingest-source path/to/new.dat
bash scripts/build_compendium_full.sh --skip-update  # after new offline mirrors only
```

Enrichment uses COALESCE semantics: existing fields are never overwritten.

## Compendium contents

Identity (`game_signatures`), metadata (`games`, `game_facts`), artwork (`game_assets`, `data/remus-thumbnails/`), disc sets, patch catalog, FTS search — all resolved locally at runtime.

## Gaps

When compendium-only mode cannot fill a field, Remus logs `compendium_gap: <field>` and continues without network calls. Extend the compendium to close gaps.
