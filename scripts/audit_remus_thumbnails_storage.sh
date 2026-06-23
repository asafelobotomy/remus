#!/usr/bin/env bash
# purpose: audit remus-thumbnails blob storage efficiency vs acquisition sources.
# when: after consolidate or before prune-acquisition-sources.
# usage: ./scripts/audit_remus_thumbnails_storage.sh [--compendium-db PATH] [--thumbnail-dir PATH] [--acquisition-dir PATH]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_PATH="$ROOT_DIR/data/compendium/remus_compendium.db"
THUMB_DIR="$ROOT_DIR/data/remus-thumbnails"
ACQ_DIR="$ROOT_DIR/data/acquisition/libretro-thumbnails"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --compendium-db) DB_PATH="$2"; shift 2 ;;
        --thumbnail-dir) THUMB_DIR="$2"; shift 2 ;;
        --acquisition-dir) ACQ_DIR="$2"; shift 2 ;;
        --help|-h)
            cat <<'EOF'
Usage: ./scripts/audit_remus_thumbnails_storage.sh [options]

Reports blob format mix, dedup ratio, PNG→WebP upgrade candidates, and acquisition overlap.
EOF
            exit 0
            ;;
        *) echo "unknown option: $1" >&2; exit 1 ;;
    esac
done

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 required" >&2
    exit 1
fi

echo "==> remus-thumbnails storage audit"
echo "    db=$DB_PATH"
echo "    blobs=$THUMB_DIR/blobs"
echo "    acquisition=$ACQ_DIR"

if [[ -d "$THUMB_DIR/blobs" ]]; then
    blob_bytes=$(du -sb "$THUMB_DIR/blobs" 2>/dev/null | awk '{print $1}')
    echo ""
    echo "==> On-disk blob store"
    du -sh "$THUMB_DIR/blobs"
    echo "blob_file_counts:"
    find "$THUMB_DIR/blobs" -type f | sed 's/.*\.//' | sort | uniq -c | sort -rn | sed 's/^/  /'
else
    blob_bytes=0
    echo ""
    echo "==> No blob directory at $THUMB_DIR/blobs"
fi

if [[ -f "$ACQ_DIR" ]] || [[ -d "$ACQ_DIR" ]]; then
    echo ""
    echo "==> Acquisition tree (libretro PNG)"
    du -sh "$ACQ_DIR" 2>/dev/null || true
fi

if [[ ! -f "$DB_PATH" ]]; then
    echo ""
    echo "note: compendium DB not found — skipping SQL metrics"
    exit 0
fi

if ! sqlite3 -batch "$DB_PATH" "SELECT 1 FROM sqlite_master WHERE type='table' AND name='game_assets' LIMIT 1;" | grep -q '^1$'; then
    echo ""
    echo "note: game_assets missing — apply migration 0012"
    exit 0
fi

echo ""
echo "==> game_assets summary"
sqlite3 -header -column "$DB_PATH" "
SELECT mime_type, COUNT(*) AS assets, SUM(byte_size) AS bytes,
       ROUND(100.0 * SUM(byte_size) / (SELECT SUM(byte_size) FROM game_assets), 1) AS pct_bytes
FROM game_assets GROUP BY mime_type ORDER BY bytes DESC;
"

echo ""
echo "==> Per asset type"
sqlite3 -header -column "$DB_PATH" "
SELECT asset_type, COUNT(*) AS assets, SUM(byte_size) AS bytes,
       ROUND(AVG(byte_size)) AS avg_bytes
FROM game_assets GROUP BY asset_type ORDER BY bytes DESC;
"

echo ""
echo "==> Dedup efficiency"
sqlite3 -header -column "$DB_PATH" "
SELECT COUNT(*) AS game_asset_rows,
       COUNT(DISTINCT content_sha256) AS unique_blobs,
       COUNT(*) - COUNT(DISTINCT content_sha256) AS dedup_rows_saved
FROM game_assets;
"

echo ""
echo "==> Source-path reuse (same libretro PNG → multiple games)"
sqlite3 -header -column "$DB_PATH" "
SELECT COUNT(*) AS rows_with_source,
       COUNT(DISTINCT source_path) AS unique_sources,
       COUNT(*) - COUNT(DISTINCT source_path) AS reusable_source_hits
FROM game_assets WHERE source_path IS NOT NULL AND TRIM(source_path) != '';
"

echo ""
png_webp_upgrade=$(sqlite3 -batch "$DB_PATH" "SELECT COUNT(*) FROM game_assets WHERE mime_type = 'image/png';")
if [[ "${png_webp_upgrade:-0}" -gt 0 ]]; then
    echo ""
    echo "==> Lossless upgrade candidates"
    echo "  PNG blobs in DB: $png_webp_upgrade (re-run consolidate with cwebp-bin for WebP lossless -m 6)"
fi

if [[ -d "$THUMB_DIR/blobs" && "${blob_bytes:-0}" -gt 0 ]]; then
    db_bytes=$(sqlite3 -batch "$DB_PATH" "SELECT COALESCE(SUM(byte_size), 0) FROM game_assets;")
    if [[ -d "$ACQ_DIR" ]]; then
        acq_bytes=$(du -sb "$ACQ_DIR" 2>/dev/null | awk '{print $1}')
        if [[ "${acq_bytes:-0}" -gt 0 ]]; then
            ratio=$(awk -v b="$blob_bytes" -v a="$acq_bytes" 'BEGIN { printf "%.1f", 100.0 * (1.0 - b/a) }')
            echo ""
            echo "==> Acquisition vs canonical store"
            echo "  acquisition_bytes=$acq_bytes"
            echo "  canonical_blob_bytes=$blob_bytes"
            echo "  canonical_vs_acquisition_savings_pct=$ratio"
        fi
    fi
    echo "  indexed_byte_sum=$db_bytes"
fi

if [[ -x "$ROOT_DIR/node_modules/.bin/cwebp" ]]; then
    echo ""
    echo "==> cwebp lossless backend: available (npm cwebp-bin)"
else
    echo ""
    echo "==> cwebp lossless backend: missing (run npm install in repo root)"
fi
