#!/usr/bin/env python3
"""Web MCP — DuckDuckGo search + URL fetch in one self-owned server.

Tools
-----
web_search : Query the web via DuckDuckGo's public HTML endpoint.
             Returns ranked results (title, URL, snippet).  No API key.
fetch      : Fetch a specific URL and return Markdown-converted content.
             Supports chunked pagination via start_index + max_length.

Design
------
- Search POSTs to DuckDuckGo's html.duckduckgo.com endpoint (no API key).
  A soft rate-limit of 20 searches/minute guards against accidental hammering.
- Fetch uses httpx for robust HTTP handling and markdownify for high-quality
  HTML→Markdown conversion that preserves headers, links, and code blocks.
- SSRF mitigation: loopback, RFC-1918, link-local, and cloud-metadata address
  ranges are blocked on all fetch calls before a connection is opened.

Dependencies (injected via --with at startup, not installed globally):
    httpx, markdownify, beautifulsoup4

Transport: stdio  |  Run: see mcp.json for the full uvx invocation.
"""
from __future__ import annotations

import ipaddress
import socket
import sys
import time
from urllib.parse import urlparse

try:
    import httpx
    from bs4 import BeautifulSoup
    from markdownify import markdownify as md
    from mcp.server.fastmcp import FastMCP
except ImportError as _exc:  # pragma: no cover
    sys.stderr.write(
        f"ERROR: required dependency missing — {_exc}\n"
        "Ensure the server is started with:\n"
        "  uvx --with httpx --with markdownify --with beautifulsoup4 "
        "--from 'mcp[cli]' mcp run <this-file>\n"
    )
    sys.exit(1)

mcp = FastMCP("xanadWeb")

# ---------------------------------------------------------------------------
# SSRF block list
# ---------------------------------------------------------------------------

_BLOCKED: list[ipaddress.IPv4Network | ipaddress.IPv6Network] = [
    ipaddress.ip_network("127.0.0.0/8"),    # loopback
    ipaddress.ip_network("::1/128"),         # IPv6 loopback
    ipaddress.ip_network("10.0.0.0/8"),      # RFC-1918 class A
    ipaddress.ip_network("172.16.0.0/12"),   # RFC-1918 class B
    ipaddress.ip_network("192.168.0.0/16"),  # RFC-1918 class C
    ipaddress.ip_network("169.254.0.0/16"),  # link-local / cloud metadata
    ipaddress.ip_network("fe80::/10"),       # IPv6 link-local
    ipaddress.ip_network("fd00::/8"),        # IPv6 ULA
    ipaddress.ip_network("100.64.0.0/10"),   # CGNAT / shared address space
]


def _guard_ssrf(url: str) -> None:
    """Raise ValueError if url resolves to a blocked network range.

    Note: DNS resolution is performed once here as a pre-flight check.  The
    HTTP client resolves the hostname independently when opening the
    connection (TOCTOU window).  True DNS rebinding would require an
    attacker to control the target's DNS TTL, which is outside the threat
    model for a developer-local MCP server.  The redirect hook
    (_ssrf_redirect_hook) covers the redirect-based variant.
    """
    parsed = urlparse(url)
    host = parsed.hostname
    if not host:
        raise ValueError(f"Cannot parse hostname from URL: {url!r}")
    try:
        addr = ipaddress.ip_address(socket.gethostbyname(host))
    except socket.gaierror as exc:
        raise ValueError(f"Cannot resolve hostname {host!r}: {exc}") from exc
    for net in _BLOCKED:
        if addr in net:
            raise ValueError(
                f"Fetch blocked: {host!r} resolves to {addr}, "
                f"which is in the blocked range {net}."
            )


def _ssrf_redirect_hook(response: "httpx.Response") -> None:  # type: ignore[name-defined]
    """httpx response event hook — validate redirect targets before following."""
    if response.is_redirect:
        loc = str(response.headers.get("location", ""))
        if loc:
            _guard_ssrf(loc)


# ---------------------------------------------------------------------------
# Rate limiter — search only
# ---------------------------------------------------------------------------

class _RateLimiter:
    def __init__(self, calls: int, period: float) -> None:
        self._calls = calls
        self._period = period
        self._log: list[float] = []

    def check(self) -> None:
        now = time.monotonic()
        self._log = [t for t in self._log if now - t < self._period]
        if len(self._log) >= self._calls:
            raise RuntimeError(
                f"Rate limit reached: max {self._calls} searches per "
                f"{self._period:.0f}s. Wait before searching again."
            )
        self._log.append(now)


_limiter = _RateLimiter(calls=20, period=60.0)

# ---------------------------------------------------------------------------
# Tool: web_search
# ---------------------------------------------------------------------------

_DDG_ENDPOINT = "https://html.duckduckgo.com/html"
_DDG_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0"
    ),
    "Accept": "text/html,application/xhtml+xml;q=0.9,*/*;q=0.8",
    "Content-Type": "application/x-www-form-urlencoded",
}


@mcp.tool()
def web_search(
    query: str,
    max_results: int = 10,
    region: str = "",
) -> str:  # pragma: no cover
    """Search the web via DuckDuckGo and return ranked results.

    Args:
        query: Search query string.
        max_results: Number of results to return, 1–20 (default 10).
        region: Optional locale code: 'us-en', 'uk-en', 'de-de', 'fr-fr',
                'jp-ja', 'cn-zh'.  Omit for global results.
    """
    if not query.strip():
        raise ValueError("query must not be empty.")
    max_results = max(1, min(20, max_results))
    _limiter.check()

    form: dict[str, str] = {"q": query}
    if region:
        form["kl"] = region

    with httpx.Client(headers=_DDG_HEADERS, follow_redirects=True, timeout=15,
                       event_hooks={"response": [_ssrf_redirect_hook]}) as client:
        resp = client.post(_DDG_ENDPOINT, data=form)
        resp.raise_for_status()

    soup = BeautifulSoup(resp.text, "html.parser")
    items = []
    for el in soup.select(".result")[:max_results]:
        title = (el.select_one(".result__title") or el).get_text(strip=True)
        url   = (el.select_one(".result__url") or el).get_text(strip=True)
        snip  = (el.select_one(".result__snippet") or el).get_text(strip=True)
        if title or url:
            items.append(f"{len(items) + 1}. {title}\n   {url}\n   {snip}")

    if not items:
        return f"No results found for: {query!r}"
    return f"Found {len(items)} result(s) for {query!r}:\n\n" + "\n\n".join(items)


# ---------------------------------------------------------------------------
# Tool: fetch
# ---------------------------------------------------------------------------

_FETCH_HEADERS = {
    "User-Agent": "xanadWeb/1.0 (developer workspace fetch; +https://github.com/asafelobotomy/xanadassistant)",
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
}


def _to_markdown(html: str) -> str:
    soup = BeautifulSoup(html, "html.parser")
    for tag in soup(["script", "style", "nav", "header", "footer", "aside"]):
        tag.decompose()
    return md(str(soup), heading_style="ATX").strip()


@mcp.tool()
def fetch(
    url: str,
    max_length: int = 5000,
    start_index: int = 0,
    raw: bool = False,
) -> str:  # pragma: no cover
    """Fetch a URL and return its content as Markdown (or raw text).

    Internal and private network addresses are blocked.  For long pages,
    use start_index to paginate: the response appends the next start_index
    when more content is available.

    Args:
        url: The URL to fetch (http/https only; internal addresses blocked).
        max_length: Max characters to return per call (default 5000).
        start_index: Character offset for pagination (default 0).
        raw: When True, skip HTML-to-Markdown conversion (default False).
    """
    if urlparse(url).scheme not in ("http", "https"):
        raise ValueError(f"Only http and https URLs are supported; got: {url!r}")
    _guard_ssrf(url)
    max_length = max(1, min(1_000_000, max_length))

    with httpx.Client(headers=_FETCH_HEADERS, follow_redirects=True, timeout=30,
                       event_hooks={"response": [_ssrf_redirect_hook]}) as client:
        resp = client.get(url)
        resp.raise_for_status()

    content_type = resp.headers.get("content-type", "")
    is_html = "text/html" in content_type or resp.text.lstrip()[:100].lower().startswith("<!doc")

    content = _to_markdown(resp.text) if (is_html and not raw) else resp.text

    chunk = content[start_index: start_index + max_length]
    remaining = len(content) - start_index - len(chunk)
    if remaining > 0:
        next_idx = start_index + len(chunk)
        chunk += (
            f"\n\n[Truncated — {remaining:,} characters remain. "
            f"Call fetch with start_index={next_idx} to continue.]"
        )
    return chunk


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":  # pragma: no cover
    mcp.run()
