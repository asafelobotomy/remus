#!/usr/bin/env bash
# Merge a section into data/compendium/.enrichment-inputs-fingerprint.json (mtime+hash cache).
# Usage:
#   scripts/update_enrichment_fingerprint_sidecar.sh <section> '<json-object>'
# Example:
#   scripts/update_enrichment_fingerprint_sidecar.sh launchbox '{"xml_mtime":123,"xml_sha256":"abc"}'
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIDECAR="$ROOT_DIR/data/compendium/.enrichment-inputs-fingerprint.json"
LOCK_FILE="${SIDECAR}.lock"
SECTION="${1:-}"
PAYLOAD="${2:-}"

if [[ -z "$SECTION" || -z "$PAYLOAD" ]]; then
    echo "usage: $(basename "$0") <section> '<json-object>'" >&2
    exit 1
fi

mkdir -p "$(dirname "$SIDECAR")"
exec 9>"$LOCK_FILE"
if ! flock 9; then
    echo "error: could not acquire sidecar lock: $LOCK_FILE" >&2
    exit 1
fi

python3 - "$SIDECAR" "$SECTION" "$PAYLOAD" <<'PY'
import json, sys, time
from pathlib import Path

sidecar, section, payload_raw = sys.argv[1], sys.argv[2], sys.argv[3]
path = Path(sidecar)
data = {}
if path.exists():
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        data = {}
if not isinstance(data, dict):
    data = {}
section_obj = json.loads(payload_raw)
if not isinstance(section_obj, dict):
    raise SystemExit("payload must be a JSON object")
section_obj["updated_at"] = int(time.time())
data[section] = section_obj
data["metadata"] = {
    "updated_at": int(time.time()),
    "sidecar_version": 1,
}
path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
print(f"Updated fingerprint sidecar section: {section}")
PY
