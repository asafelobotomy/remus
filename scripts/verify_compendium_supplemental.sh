#!/usr/bin/env bash
# verify_compendium_supplemental.sh — smoke-check supplemental DAT sync + manifest generation.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "==> Supplemental manifest smoke"

SUPP_ROOT="$ROOT/data/databases/supplemental"
mkdir -p "$SUPP_ROOT/homebrew" "$SUPP_ROOT/libretro-dats" "$SUPP_ROOT/tosec"

# Minimal Logiqx DAT for manifest scanner (homebrew prefix).
FIXTURE_DAT="$SUPP_ROOT/homebrew/Smoke Test.dat"
if [[ ! -f "$FIXTURE_DAT" ]]; then
    cat >"$FIXTURE_DAT" <<'DAT'
<?xml version="1.0"?>
<!DOCTYPE datafile SYSTEM "logiqx.dtd">
<datafile>
  <header>
    <name>Smoke Test</name>
    <description>Agent supplemental manifest smoke fixture</description>
  </header>
  <game name="Smoke Game">
    <rom name="smoke.bin" size="1" crc="00000000"/>
  </game>
</datafile>
DAT
    echo "  Created fixture DAT: $FIXTURE_DAT"
fi

OUT="$(mktemp -t compendium-manifest-smoke.XXXXXX.json)"
trap 'rm -f "$OUT"' EXIT

bash scripts/generate_compendium_manifest.sh \
    --dat-dir "$ROOT/data/databases" \
    --output "$OUT" \
    --build-id "supplemental-smoke" \
    --snapshot-label "smoke" \
    --fetched-at "$(date -u +%Y-%m-%dT%H:%M:%SZ)"

python3 - "$OUT" <<'PY'
import json, sys
path = sys.argv[1]
with open(path, encoding="utf-8") as f:
    data = json.load(f)
sources = data.get("sources") or []
keys = [s.get("source_id", "") for s in sources]
if not any(k.startswith("libretro-homebrew") for k in keys):
    print("error: no libretro-homebrew manifest source", file=sys.stderr)
    sys.exit(1)
print(f"OK: {len(sources)} manifest sources; libretro-homebrew present")
PY

echo "==> LaunchBox offline enrichment smoke (unit test)"

if [[ -x build/remus-cli ]]; then
    bash scripts/update_launchbox_metadata.sh --source "$ROOT/tests/fixtures/launchbox_metadata_sample.xml"
    TMP_DB="$(mktemp -t remus-compendium-smoke.XXXXXX.db)"
    trap 'rm -f "$OUT" "$TMP_DB"' EXIT
    bash scripts/setup_compendium_db.sh "$TMP_DB" >/dev/null
    ./build/remus-cli --enrich-compendium \
        --enrich-source launchbox \
        --compendium-output "$TMP_DB" 2>&1 | tail -5
else
    echo "  Skipping remus-cli smoke (build/remus-cli missing)"
fi

echo "==> Done"
