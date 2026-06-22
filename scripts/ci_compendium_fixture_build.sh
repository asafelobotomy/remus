#!/usr/bin/env bash
# Build a minimal compendium from the test DAT fixture and run ingest validation gates.
# Used by CI to exercise --build-compendium, disc-set topology, and 0005 FAIL checks
# without syncing the full libretro-database catalogue.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="$ROOT_DIR/build/remus-cli"
FIXTURE_DAT="$ROOT_DIR/tests/fixtures/test_compendium_source.dat"
VALIDATE="$ROOT_DIR/.github/scripts/validate-compendium-db.sh"

if [[ ! -x "$CLI" ]]; then
    echo "error: remus-cli not found at $CLI (build the project first)" >&2
    exit 1
fi

if [[ ! -f "$FIXTURE_DAT" ]]; then
    echo "error: fixture DAT not found: $FIXTURE_DAT" >&2
    exit 1
fi

if ! command -v sqlite3 >/dev/null 2>&1; then
    echo "error: sqlite3 is required but not installed" >&2
    exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cp "$FIXTURE_DAT" "$WORK/GameCube.dat"
MANIFEST="$WORK/manifest.json"
OUTPUT_DB="$WORK/remus_compendium.db"

cat > "$MANIFEST" <<EOF
{
  "build_id": "ci-fixture-$(date -u +%Y-%m-%d)",
  "schema_version": 1,
  "sources": [{
    "source_id": "ci-fixture-gc",
    "display_name": "CI GameCube Fixture",
    "source_type": "dat",
    "snapshot_id": "ci-fixture-gc-$(date -u +%Y-%m-%d)",
    "snapshot_label": "CI fixture",
    "path": "$WORK/GameCube.dat",
    "license_id": "CC0-1.0",
    "license_url": "https://creativecommons.org/publicdomain/zero/1.0/",
    "fetched_at": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
    "enabled": true,
    "priority": 10,
    "attribution_required": false
  }]
}
EOF

echo "==> CI fixture compendium build"
"$CLI" --build-compendium --compendium-manifest "$MANIFEST" --compendium-output "$OUTPUT_DB"

echo "==> Fixture DB counts"
sqlite3 -header -column "$OUTPUT_DB" "
SELECT 'games' AS metric, COUNT(*) AS value FROM games
UNION ALL SELECT 'game_disc_sets', COUNT(*) FROM game_disc_sets
UNION ALL SELECT 'game_disc_tracks', COUNT(*) FROM game_disc_tracks;
"

echo "==> Disc set schema validation (0004)"
bash "$VALIDATE" "$OUTPUT_DB" "$ROOT_DIR/data/compendium/validation/0004_disc_set_checks.sql"

echo "==> Disc set ingest validation (0005, WARN allowed)"
bash "$VALIDATE" "$OUTPUT_DB" "$ROOT_DIR/data/compendium/validation/0005_disc_set_ingest_checks.sql" --warn-only

echo "==> Quick quality validation (0002, warn-only on fixture)"
bash "$ROOT_DIR/scripts/apply_compendium_migrations.sh" "$OUTPUT_DB"
bash "$VALIDATE" "$OUTPUT_DB" "$ROOT_DIR/data/compendium/validation/0002_phase2_quality_checks.sql" --warn-only

echo "==> Extended validation (0003, warn-only on fixture)"
bash "$VALIDATE" "$OUTPUT_DB" "$ROOT_DIR/data/compendium/validation/0003_phase2_extended_checks.sql" --warn-only

echo "==> Per-system disc set coverage smoke"
"$CLI" --disc-set-coverage --compendium-output "$OUTPUT_DB" | head -5

echo "==> CI fixture compendium build passed"
