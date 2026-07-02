#!/usr/bin/env bash
# Write data/compendium/enrichment-credentials.json from REMUS_* env vars.
# Usage: source scripts/load_env_local.sh && scripts/sync_enrichment_credentials.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/data/compendium/enrichment-credentials.json}"

# shellcheck disable=SC1091
source "$ROOT/scripts/load_env_local.sh" "$ROOT/.env.local"

python3 - "$OUT" <<'PY'
import json, os, sys

out = sys.argv[1]
payload = {
    "igdb": {
        "client_id": os.environ.get("REMUS_IGDB_CLIENT_ID", ""),
        "client_secret": os.environ.get("REMUS_IGDB_CLIENT_SECRET", ""),
    },
    "retroachievements": {
        "username": os.environ.get("REMUS_RA_USERNAME") or os.environ.get("REMUS_RA_USER", ""),
        "api_key": os.environ.get("REMUS_RA_API_KEY", ""),
    },
    "screenscraper": {
        "username": os.environ.get("REMUS_SS_USER", ""),
        "password": os.environ.get("REMUS_SS_PASS", ""),
        "devid": os.environ.get("REMUS_SS_DEVID", ""),
        "devpassword": os.environ.get("REMUS_SS_DEVPASS", ""),
    },
    "hasheous": {
        "client_api_key": os.environ.get("REMUS_HASHEOUS_API_KEY", ""),
    },
    "thegamesdb": {
        "api_key": os.environ.get("REMUS_TGDB_API_KEY", ""),
    },
}

with open(out, "w", encoding="utf-8") as f:
    json.dump(payload, f, indent=2)
    f.write("\n")

configured = sum(1 for section in payload.values() for v in section.values() if v)
print(f"Wrote {out} ({configured} non-empty credential field(s))")
PY

if [[ -x "$ROOT/scripts/update_enrichment_fingerprint_sidecar.sh" ]]; then
    cred_sha="$(sha256sum "$OUT" | awk '{print $1}')"
    cred_mtime="$(stat -c %Y "$OUT" 2>/dev/null || echo 0)"
    bash "$ROOT/scripts/update_enrichment_fingerprint_sidecar.sh" credentials \
        "{\"sha256\":\"$cred_sha\",\"mtime\":$cred_mtime}"
fi
