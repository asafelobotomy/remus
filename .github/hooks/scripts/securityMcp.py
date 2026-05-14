#!/usr/bin/env python3
"""Security MCP — vulnerability lookup and package health.

Tools
-----
query_osv   : Query OSV (https://osv.dev) for known vulnerabilities affecting
              a package version.  Aggregates advisories from GitHub Security
              Advisories, PyPI, npm, crates.io, Go, Maven, NuGet, RubyGems,
              RustSec, and more.  No API key required.
query_deps  : Query deps.dev (https://deps.dev) for package health signals:
              vulnerability count, license, OpenSSF Scorecard rating, and
              direct dependency count.  No API key required.

Transport: stdio  |  Run: uvx --from "mcp[cli]" mcp run <this-file>
"""
from __future__ import annotations

import json
import sys
import urllib.error
import urllib.parse
import urllib.request

try:
    from mcp.server.fastmcp import FastMCP
except ImportError as _exc:  # pragma: no cover
    sys.stderr.write(
        "ERROR: the 'mcp' package is required but not installed.\n"
        "Install it with: pip install 'mcp[cli]'\n"
        f"Details: {_exc}\n"
    )
    sys.exit(1)

mcp = FastMCP("xanadSecurity")

_HEADERS = {"Content-Type": "application/json", "Accept": "application/json"}

_DEPS_DEV_SYSTEMS = frozenset({
    "pypi", "npm", "cargo", "go", "maven", "nuget", "rubygems",
    "packagist", "hex", "pub",
})


def _post(url: str, payload: dict) -> dict:  # pragma: no cover
    req = urllib.request.Request(
        url, data=json.dumps(payload).encode(), method="POST", headers=_HEADERS
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"HTTP {exc.code} from {url}: {body}") from exc


def _get(url: str) -> dict:  # pragma: no cover
    req = urllib.request.Request(url, headers=_HEADERS)
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"HTTP {exc.code} from {url}: {body}") from exc


# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------

@mcp.tool()
def query_osv(package: str, version: str, ecosystem: str) -> str:  # pragma: no cover
    """Query OSV for known vulnerabilities affecting a package version.

    Args:
        package: Package name (e.g. 'requests', 'lodash', 'tokio').
        version: Exact version string (e.g. '2.28.0').
        ecosystem: One of: PyPI, npm, crates.io, Go, Maven, NuGet,
                   RubyGems, Packagist, Hex, Pub, Linux, GitHub Actions.
    """
    data = _post(
        "https://api.osv.dev/v1/query",
        {"version": version, "package": {"name": package, "ecosystem": ecosystem}},
    )
    vulns = data.get("vulns", [])
    if not vulns:
        return f"No known vulnerabilities for {ecosystem}/{package}@{version}."

    lines = [f"{len(vulns)} vulnerability/ies for {ecosystem}/{package}@{version}:\n"]
    for v in vulns[:20]:
        vid = v.get("id", "?")
        summary = v.get("summary", "(no summary)")
        severity = ""
        for sev in v.get("severity", []):
            if sev.get("type") == "CVSS_V3":
                severity = f" [CVSS {sev['score']}]"
                break
        aliases = ", ".join(v.get("aliases", []))
        alias_str = f" (also: {aliases})" if aliases else ""
        lines.append(f"  {vid}{severity}: {summary}{alias_str}")

    if len(vulns) > 20:
        lines.append(f"\n  ... and {len(vulns) - 20} more. Query OSV directly for the full list.")
    return "\n".join(lines)


@mcp.tool()
def query_deps(package: str, version: str, system: str) -> str:  # pragma: no cover
    """Query deps.dev for package health: licenses, scorecard, vulnerability count.

    Args:
        package: Package name (e.g. 'requests', 'express', 'tokio').
        version: Exact version string (e.g. '2.28.0').
        system: Ecosystem — one of: pypi, npm, cargo, go, maven, nuget.
    """
    if system.lower() not in _DEPS_DEV_SYSTEMS:
        raise ValueError(
            f"Unknown ecosystem {system!r}. "
            f"Supported: {', '.join(sorted(_DEPS_DEV_SYSTEMS))}"
        )
    pkg_enc = urllib.parse.quote(package, safe="")
    ver_enc = urllib.parse.quote(version, safe="")
    url = f"https://api.deps.dev/v3alpha/systems/{system}/packages/{pkg_enc}/versions/{ver_enc}"
    data = _get(url)

    lines = [f"deps.dev report for {system}/{package}@{version}:"]

    lic = data.get("licenses") or data.get("license")
    if lic:
        lines.append(f"  License    : {', '.join(lic) if isinstance(lic, list) else lic}")

    score = data.get("scorecard", {})
    if score:
        lines.append(f"  Scorecard  : {score.get('overallScore', '?')}/10")

    vuln_count = data.get("vulnerabilityCount", 0)
    lines.append(f"  Known CVEs : {vuln_count}")

    dep_count = data.get("dependencyCount")
    if dep_count is not None:
        lines.append(f"  Direct deps: {dep_count}")

    advisories = data.get("advisoryKeys", [])
    if advisories:
        lines.append(f"  Advisories : {', '.join(a.get('id', '?') for a in advisories[:5])}")

    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":  # pragma: no cover
    mcp.run()
