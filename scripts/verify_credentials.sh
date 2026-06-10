#!/usr/bin/env bash
# verify_credentials.sh — Probe configured provider credentials (never prints secrets).
#
# Usage:
#   scripts/verify_credentials.sh [--env-file PATH] [--provider NAME] [--quiet]
#
# Loads REMUS_* env vars from .env.local (or --env-file), then performs minimal
# live API probes for each provider that has non-empty credentials.
#
# Exit codes:
#   0 — all configured providers passed (or none configured and --allow-none)
#   1 — one or more configured providers failed
#   2 — no credentials configured
#
# Examples:
#   scripts/verify_credentials.sh
#   scripts/verify_credentials.sh --provider igdb
#   set -a; source .env.local; set +a; REMUS_RUN_LIVE_CREDENTIAL_TESTS=1 \
#     ctest --test-dir build -R CredentialsLiveTest --output-on-failure

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ENV_FILE="${REPO_ROOT}/.env.local"
PROVIDER_FILTER=""
QUIET=0
ALLOW_NONE=0

usage() {
    sed -n '/^# Usage:/,/^# Examples:/p' "$0" | sed 's/^# \?//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --env-file) ENV_FILE="$2"; shift 2 ;;
        --provider) PROVIDER_FILTER="${2,,}"; shift 2 ;;
        --quiet|-q) QUIET=1; shift ;;
        --allow-none) ALLOW_NONE=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
    esac
done

log() {
    [[ ${QUIET} -eq 1 ]] || echo "$@"
}

fail() {
    echo "✗ $1" >&2
}

pass() {
    log "✓ $1"
}

if [[ -f "${ENV_FILE}" ]]; then
    log "Loading ${ENV_FILE}"
    # shellcheck disable=SC1090
    set -a
    source "${ENV_FILE}"
    set +a
else
    log "No env file at ${ENV_FILE} — using existing environment"
fi

want_provider() {
    [[ -z "${PROVIDER_FILTER}" || "${PROVIDER_FILTER}" == "$1" ]]
}

configured=0
failures=0

probe_igdb() {
    want_provider igdb || return 0
    [[ -n "${REMUS_IGDB_CLIENT_ID:-}" && -n "${REMUS_IGDB_CLIENT_SECRET:-}" ]] || return 0
    configured=$((configured + 1))

    local http_code response token
    http_code="$(curl -sS --max-time 20 -o /tmp/remus_igdb_token.json -w '%{http_code}' \
        -X POST "https://id.twitch.tv/oauth2/token" \
        -d "client_id=${REMUS_IGDB_CLIENT_ID}&client_secret=${REMUS_IGDB_CLIENT_SECRET}&grant_type=client_credentials" \
        2>/dev/null || echo 000)"
    response="$(cat /tmp/remus_igdb_token.json 2>/dev/null || true)"
    rm -f /tmp/remus_igdb_token.json
    token="$(printf '%s' "${response}" | python3 -c 'import json,sys
try:
    d=json.load(sys.stdin)
except json.JSONDecodeError:
    sys.exit(1)
print(d.get("access_token",""))' 2>/dev/null || true)"
    if [[ -n "${token}" ]]; then
        pass "IGDB (Twitch OAuth)"
    else
        fail "IGDB (Twitch OAuth HTTP ${http_code})"
        failures=$((failures + 1))
    fi
}

probe_ra() {
    want_provider ra || return 0
    [[ -n "${REMUS_RA_API_KEY:-}" ]] || return 0
    configured=$((configured + 1))

    local http_code response ok
    http_code="$(curl -sS --max-time 20 -o /tmp/remus_ra.json -w '%{http_code}' \
        -G "https://retroachievements.org/API/API_GetGameList.php" \
        --data-urlencode "i=7" \
        --data-urlencode "y=${REMUS_RA_API_KEY}" \
        --data-urlencode "h=0" \
        2>/dev/null || echo 000)"
    response="$(cat /tmp/remus_ra.json 2>/dev/null || true)"
    rm -f /tmp/remus_ra.json
    ok="$(printf '%s' "${response}" | python3 -c 'import json,sys
try:
    d=json.load(sys.stdin)
except json.JSONDecodeError:
    sys.exit(1)
sys.exit(0 if isinstance(d,list) and len(d)>0 else 1)' 2>/dev/null && echo yes || echo no)"
    if [[ "${ok}" == yes ]]; then
        pass "RetroAchievements (API key)"
    else
        fail "RetroAchievements (API key HTTP ${http_code})"
        failures=$((failures + 1))
    fi
}

probe_screenscraper() {
    want_provider screenscraper || return 0
    [[ -n "${REMUS_SS_USER:-}" && -n "${REMUS_SS_PASS:-}" ]] || return 0
    configured=$((configured + 1))

    local -a curl_args=(
        -sS --max-time 30 -o /tmp/remus_ss.json -w '%{http_code}'
        -G "https://api.screenscraper.fr/api2/jeuRecherche.php"
        --data-urlencode "output=json"
        --data-urlencode "softname=remus"
        --data-urlencode "ssid=${REMUS_SS_USER}"
        --data-urlencode "sspassword=${REMUS_SS_PASS}"
        --data-urlencode "recherche=Mario"
        --data-urlencode "systemeid=3"
    )
    if [[ -n "${REMUS_SS_DEVID:-}" && -n "${REMUS_SS_DEVPASS:-}" ]]; then
        curl_args+=(--data-urlencode "devid=${REMUS_SS_DEVID}" --data-urlencode "devpassword=${REMUS_SS_DEVPASS}")
    fi

    local http_code response err
    http_code="$(curl "${curl_args[@]}" 2>/dev/null || echo 000)"
    response="$(cat /tmp/remus_ss.json 2>/dev/null || true)"
    rm -f /tmp/remus_ss.json
    err="$(printf '%s' "${response}" | python3 -c 'import json,sys
raw = sys.stdin.read()
try:
    data = json.loads(raw)
except json.JSONDecodeError:
    print("non_json_response")
    sys.exit(0)
if isinstance(data, dict) and data.get("response", {}).get("error"):
    print(data["response"]["error"])
else:
    print("")' 2>/dev/null || echo parse_error)"
    if [[ -z "${err}" ]]; then
        pass "ScreenScraper"
    else
        fail "ScreenScraper (${err}, HTTP ${http_code})"
        failures=$((failures + 1))
    fi
}

probe_tgdb() {
    want_provider tgdb || return 0
    [[ -n "${REMUS_TGDB_API_KEY:-}" ]] || return 0
    configured=$((configured + 1))
    log "○ TheGamesDB key is set (no live probe implemented yet)"
    pass "TheGamesDB (present, unchecked)"
}

probe_hasheous() {
    want_provider hasheous || return 0
    [[ -n "${REMUS_HASHEOUS_API_KEY:-}" ]] || return 0
    configured=$((configured + 1))
    log "○ Hasheous key is set (no live probe implemented yet)"
    pass "Hasheous (present, unchecked)"
}

probe_igdb
probe_ra
probe_screenscraper
probe_tgdb
probe_hasheous

if [[ ${configured} -eq 0 ]]; then
    log "No provider credentials configured in environment."
    [[ ${ALLOW_NONE} -eq 1 ]] && exit 0
    exit 2
fi

if [[ ${failures} -gt 0 ]]; then
    exit 1
fi

log "All configured provider probes passed (${configured} checked)."
exit 0
