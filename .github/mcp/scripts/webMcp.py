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
- SSRF mitigation: any destination that is not globally routable is blocked on
    all fetch calls before a connection is opened.

Dependencies (injected via --with at startup, not installed globally):
    httpx, markdownify, beautifulsoup4

Transport: stdio  |  Run: see mcp.json for the full uvx invocation.
"""
from __future__ import annotations

import concurrent.futures
import ipaddress
import socket
import sys
import threading
import time
from urllib.parse import parse_qs, unquote, urlparse

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


_DNS_TIMEOUT = 5.0  # seconds — cap on pre-flight DNS resolution in _guard_ssrf


def _normalize_ip(addr: ipaddress._BaseAddress) -> ipaddress._BaseAddress:
    if isinstance(addr, ipaddress.IPv6Address) and addr.ipv4_mapped is not None:
        return addr.ipv4_mapped
    return addr


def _is_public_address(addr: ipaddress._BaseAddress) -> bool:
    normalized = _normalize_ip(addr)
    return normalized.is_global


def _guard_ssrf(url: str) -> None:
    """Raise ValueError if url resolves to a non-public network destination.

    All A and AAAA records are resolved and normalized; the first address that
    is not globally routable raises immediately. DNS resolution runs in a
    background thread with a hard timeout (_DNS_TIMEOUT) to prevent a slow
    resolver from stalling the MCP server.

    Note: This is a pre-flight check (TOCTOU window exists).  True DNS
    rebinding requires attacker-controlled DNS TTL, which is outside the
    threat model for a developer-local MCP server.
    """
    parsed = urlparse(url)
    host = parsed.hostname
    if not host:
        raise ValueError(f"Cannot parse hostname from URL: {url!r}")
    ex = concurrent.futures.ThreadPoolExecutor(max_workers=1, thread_name_prefix="xanad-dns")
    future = ex.submit(socket.getaddrinfo, host, None)
    try:
        infos = future.result(timeout=_DNS_TIMEOUT)
    except concurrent.futures.TimeoutError:
        raise ValueError(f"DNS timeout resolving {host!r}") from None
    except socket.gaierror as exc:
        raise ValueError(f"Cannot resolve hostname {host!r}: {exc}") from exc
    finally:
        ex.shutdown(wait=False)
    for *_, sockaddr in infos:
        addr_str = sockaddr[0].split("%")[0]  # strip IPv6 scope ID (e.g. fe80::1%eth0)
        try:
            addr = ipaddress.ip_address(addr_str)
        except ValueError:
            continue
        normalized = _normalize_ip(addr)
        if not _is_public_address(normalized):
            raise ValueError(
                f"Fetch blocked: {host!r} resolves to {addr}, "
                "which is not a public routable address."
            )


def _ssrf_redirect_hook(response: "httpx.Response") -> None:  # type: ignore[name-defined]
    """httpx response event hook — validate redirect targets before following.

    Relative redirects (e.g. Location: /path) stay on the same host — no
    SSRF risk — so the guard is skipped for URLs without a hostname.
    Non-http/https schemes in absolute redirect targets are rejected
    explicitly rather than relying on httpx to refuse them.
    """
    if response.is_redirect:
        loc = str(response.headers.get("location", ""))
        if not loc:
            return
        parsed = urlparse(loc)
        if not parsed.hostname:
            return  # relative redirect — same host, no SSRF risk
        if parsed.scheme and parsed.scheme not in ("http", "https"):
            raise ValueError(f"Redirect to unsupported scheme blocked: {loc!r}")
        _guard_ssrf(loc)


# ---------------------------------------------------------------------------
# Rate limiter — search only
# ---------------------------------------------------------------------------

class _RateLimiter:
    def __init__(self, calls: int, period: float) -> None:
        self._calls = calls
        self._period = period
        self._log: list[float] = []
        self._lock = threading.Lock()

    def check(self) -> None:
        now = time.monotonic()
        with self._lock:
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


def _extract_ddg_url(el) -> str:
    """Extract the real destination URL from a DuckDuckGo result element.

    DDG wraps result links via /l/?uddg=<encoded-url>.  This function
    unwraps the redirect and returns the actual destination href.
    Falls back to the display-text URL span when no anchor is found.
    """
    anchor = el.select_one(".result__title a")
    if anchor is None:
        return (el.select_one(".result__url") or el).get_text(strip=True)
    href = anchor.get("href", "")
    if not href:
        return (el.select_one(".result__url") or el).get_text(strip=True)
    parsed = urlparse(href)
    params = parse_qs(parsed.query)
    if parsed.path in ("/l/", "/l") and "uddg" in params:
        return unquote(params["uddg"][0])
    return href


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
        url   = _extract_ddg_url(el)
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
_MAX_DOWNLOAD_BYTES = 10_000_000  # 10 MB hard cap — body is capped before decode/conversion


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

    Non-public or otherwise non-globally-routable destinations are blocked.
    Downloads are capped at 10 MB before decoding. For long pages, use start_index to
    paginate: the response appends a structured comment with the next
    start_index when more content is available.

    Args:
        url: The URL to fetch (http/https only; non-public destinations blocked).
        max_length: Max characters to return per call (default 5000).
        start_index: Character offset for pagination (default 0, clamped to >= 0).
        raw: When True, skip HTML-to-Markdown conversion (default False).
    """
    if urlparse(url).scheme not in ("http", "https"):
        raise ValueError(f"Only http and https URLs are supported; got: {url!r}")
    _guard_ssrf(url)
    max_length = max(1, min(1_000_000, max_length))
    start_index = max(0, start_index)

    parts: list[bytes] = []
    downloaded = 0
    with httpx.Client(headers=_FETCH_HEADERS, follow_redirects=True, timeout=30,
                       event_hooks={"response": [_ssrf_redirect_hook]}) as client:
        with client.stream("GET", url) as resp:
            resp.raise_for_status()
            content_type = resp.headers.get("content-type", "")
            for part in resp.iter_bytes(chunk_size=65536):
                parts.append(part)
                downloaded += len(part)
                if downloaded >= _MAX_DOWNLOAD_BYTES:
                    break

    raw_bytes = b"".join(parts)
    charset = "utf-8"
    if "charset=" in content_type:
        charset = content_type.split("charset=")[-1].split(";")[0].strip() or "utf-8"
    text = raw_bytes.decode(charset, errors="replace")

    is_html = "text/html" in content_type or text.lstrip()[:100].lower().startswith("<!doc")
    content = _to_markdown(text) if (is_html and not raw) else text

    chunk = content[start_index: start_index + max_length]
    remaining = len(content) - start_index - len(chunk)
    if remaining > 0:
        next_idx = start_index + len(chunk)
        chunk += f"\n<!-- xanad:truncation remaining={remaining} next_start_index={next_idx} -->"
    return chunk


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":  # pragma: no cover
    mcp.run()
