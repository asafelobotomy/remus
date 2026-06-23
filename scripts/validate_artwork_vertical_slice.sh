#!/usr/bin/env bash
# Genesis artwork vertical slice: acquire thumbnails, seed minimal games from PNG names,
# consolidate into remus-thumbnails, report dedup/coverage stats (Phase 0 measurement).
#
# Usage:
#   scripts/validate_artwork_vertical_slice.sh [--skip-acquire] [--max-games N]
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSTEM_NAME="Sega - Mega Drive - Genesis"
SYSTEM_ID=10
ACQUISITION_DIR="$ROOT_DIR/data/acquisition/libretro-thumbnails"
THUMBNAIL_DIR="$ROOT_DIR/data/remus-thumbnails"
SLICE_DB="$ROOT_DIR/data/compendium/remus_artwork_slice.db"
MAX_GAMES=500
SKIP_ACQUIRE=false

usage() {
    cat <<'USAGE'
Usage:
  scripts/validate_artwork_vertical_slice.sh [options]

Options:
  --skip-acquire   Use existing acquisition tree only
  --max-games N    Cap seeded games from boxart PNGs (default: 500)
  --db PATH        Slice SQLite path (default: data/compendium/remus_artwork_slice.db)
  -h, --help       Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-acquire)
            SKIP_ACQUIRE=true
            shift
            ;;
        --max-games)
            MAX_GAMES="$2"
            shift 2
            ;;
        --db)
            SLICE_DB="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 required" >&2
    exit 1
fi

if [[ ! -x "$ROOT_DIR/build/remus-cli" ]]; then
    echo "error: build/remus-cli missing — run: cmake --build build --target remus-cli" >&2
    exit 1
fi

if ! $SKIP_ACQUIRE; then
    echo "==> Acquiring libretro-thumbnails: $SYSTEM_NAME"
    bash "$ROOT_DIR/scripts/update_libretro_thumbnails.sh" --system "$SYSTEM_NAME"
fi

BOXART_DIR="$ACQUISITION_DIR/$SYSTEM_NAME/Named_Boxarts"
if [[ ! -d "$BOXART_DIR" ]]; then
    echo "error: missing boxart dir: $BOXART_DIR" >&2
    echo "hint: run without --skip-acquire or sync submodules manually" >&2
    exit 1
fi

echo "==> Bootstrapping slice database: $SLICE_DB"
bash "$ROOT_DIR/scripts/setup_compendium_db.sh" "$SLICE_DB" >/dev/null

echo "==> Seeding games from boxart PNG filenames (max $MAX_GAMES)"
count=0
while IFS= read -r -d '' png; do
    title="$(basename "$png" .png)"
    [[ -n "$title" ]] || continue
  # Stable game_id from title
    hash="$(printf '%s' "$title" | sha256sum | awk '{print $1}')"
    game_id="slice-${hash:0:24}"
    esc_title="${title//\'/\'\'}"
    sqlite3 "$SLICE_DB" "INSERT OR IGNORE INTO games (game_id, system_id, canonical_title, updated_at)
      VALUES ('$game_id', $SYSTEM_ID, '$esc_title', datetime('now'));"
    count=$((count + 1))
    if [[ "$count" -ge "$MAX_GAMES" ]]; then
        break
    fi
done < <(find "$BOXART_DIR" -maxdepth 1 -type f -name '*.png' -print0 | sort -z)

echo "  seeded_games=$count"

# shellcheck disable=SC1091
source "$ROOT_DIR/scripts/ensure_npm_build_tools.sh"
ensure_npm_build_tools

if ! command -v cwebp >/dev/null 2>&1; then
    echo "  note: cwebp not found — consolidate will use PNG blob fallback (run npm install for cwebp-bin)"
fi

echo "==> Consolidating thumbnails"
cd "$ROOT_DIR"
./build/remus-cli --consolidate-thumbnails \
    --compendium-output "$SLICE_DB" \
    --acquisition-dir "$ACQUISITION_DIR" \
    --thumbnail-output-dir "$THUMBNAIL_DIR" \
    --thumbnail-system "$SYSTEM_NAME"

echo ""
echo "==> Slice statistics"
sqlite3 -header -column "$SLICE_DB" "
SELECT 'games' AS metric, COUNT(*) AS value FROM games
UNION ALL
SELECT 'game_assets_box', COUNT(*) FROM game_assets WHERE asset_type = 'box'
UNION ALL
SELECT 'unique_blobs', COUNT(DISTINCT content_sha256) FROM game_assets
UNION ALL
SELECT 'cover_url_local', COUNT(*) FROM games WHERE cover_url LIKE 'data/remus-thumbnails/blobs/%';
"

blob_count=$(find "$THUMBNAIL_DIR/blobs" -type f \( -name '*.webp' -o -name '*.png' \) 2>/dev/null | wc -l | tr -d ' ')
games_with_box=$(sqlite3 "$SLICE_DB" "SELECT COUNT(*) FROM game_assets WHERE asset_type='box';")
total_assets=$(sqlite3 "$SLICE_DB" "SELECT COUNT(*) FROM game_assets;")
unique_blobs=$(sqlite3 "$SLICE_DB" "SELECT COUNT(DISTINCT content_sha256) FROM game_assets;")
dedup_saved=$((total_assets - unique_blobs))

echo ""
echo "==> Dedup prototype (Phase 0)"
echo "  box_assets=$games_with_box total_assets=$total_assets unique_blobs=$unique_blobs dedup_hits=$dedup_saved on_disk_blobs=$blob_count"
if [[ "$total_assets" -gt 0 ]]; then
    pct=$(awk "BEGIN { printf \"%.2f\", ($unique_blobs / $total_assets) * 100 }")
    echo "  unique_blob_ratio_pct=$pct (lower = more dedup across asset types)"
fi

if [[ -f "$ROOT_DIR/data/compendium/validation/0013_artwork_coverage.sql" ]]; then
    echo ""
    echo "==> Validation 0013 (slice DB)"
    sqlite3 -header -column "$SLICE_DB" <"$ROOT_DIR/data/compendium/validation/0013_artwork_coverage.sql" || true
fi

echo ""
echo "Slice complete. DB: $SLICE_DB"
