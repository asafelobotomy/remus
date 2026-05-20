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
import re
import socket
import sys
import threading
import time
from urllib import robotparser
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


def _collapse_whitespace(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def _format_search_result(index: int, title: str, url: str, snippet: str) -> str:
    lines = [f"{index}. {_collapse_whitespace(title)}", f"   {url}"]
    cleaned_snippet = _collapse_whitespace(snippet)
    if cleaned_snippet:
        lines.append(f"   {cleaned_snippet}")
    return "\n".join(lines)


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
            items.append(_format_search_result(len(items) + 1, title, url, snip))

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
_TEXTUAL_CONTENT_MARKERS = ("json", "xml", "javascript", "x-www-form-urlencoded")
_FETCH_RETRY_STATUS_CODES = {429, 502, 503, 504}
_FETCH_MAX_ATTEMPTS = 3
_FETCH_RETRY_BASE_DELAY_SECONDS = 0.25
_BLOCK_PREVIEW_BYTES = 32_768
_ROBOTS_CACHE_TTL_SECONDS = 300.0
_ROBOTS_CACHE: dict[tuple[str, str], tuple[float, object | None]] = {}


def _normalize_fetch_url(url: str) -> str:
    candidate = url.strip()
    if not candidate:
        raise ValueError("url must not be empty.")

    if candidate.startswith("//"):
        candidate = f"https:{candidate}"

    parsed = urlparse(candidate)
    if parsed.scheme in ("http", "https"):
        if not parsed.hostname:
            raise ValueError(f"Cannot parse hostname from URL: {url!r}")
        return candidate
    if parsed.scheme:
        raise ValueError(f"Only http and https URLs are supported; got: {url!r}")

    normalized = f"https://{candidate}"
    parsed = urlparse(normalized)
    if not parsed.hostname:
        raise ValueError(f"Cannot parse hostname from URL: {url!r}")
    return normalized


def _to_markdown(html: str) -> str:
    soup = BeautifulSoup(html, "html.parser")
    for tag in soup(["script", "style", "nav", "header", "footer", "aside"]):
        tag.decompose()
    return md(str(soup), heading_style="ATX").strip()


def _append_truncation_hint(chunk: str, remaining: int, next_index: int) -> str:
    return (
        f"{chunk}\n\n"
        f"Continue with start_index={next_index} to read the next {remaining} character(s).\n"
        f"<!-- xanad:truncation remaining={remaining} next_start_index={next_index} -->"
    )


def _is_textual_content_type(content_type: str) -> bool:
    media_type = content_type.split(";", 1)[0].strip().lower()
    if not media_type:
        return True
    if media_type.startswith("text/"):
        return True
    return any(marker in media_type for marker in _TEXTUAL_CONTENT_MARKERS)


def _format_non_text_response(url: str, content_type: str, byte_count: int) -> str:
    display_type = content_type.split(";", 1)[0].strip() or "application/octet-stream"
    return (
        "Non-text content fetched; returning metadata only.\n"
        f"URL: {url}\n"
        f"Content-Type: {display_type}\n"
        f"Downloaded bytes: {byte_count}"
    )


def _robots_url_for(url: str) -> str:
    parsed = urlparse(url)
    return f"{parsed.scheme}://{parsed.netloc}/robots.txt"


def _read_robots_policy(url: str, client: "httpx.Client") -> object | None:
    robots_url = _robots_url_for(url)
    cache_key = (urlparse(url).scheme, urlparse(url).netloc)
    now = time.monotonic()
    cached = _ROBOTS_CACHE.get(cache_key)
    if cached and now - cached[0] < _ROBOTS_CACHE_TTL_SECONDS:
        return cached[1]

    try:
        response = client.get(robots_url)
    except httpx.HTTPError:
        policy = None
    else:
        if response.status_code in (401, 403):
            policy = "disallow_all"
        elif response.status_code != 200 or not _is_textual_content_type(response.headers.get("content-type", "text/plain")):
            policy = None
        else:
            parser = robotparser.RobotFileParser()
            parser.parse(response.text.splitlines())
            policy = parser

    _ROBOTS_CACHE[cache_key] = (now, policy)
    return policy


def _format_robots_blocked_response(url: str, robots_url: str) -> str:
    return (
        "Fetch disallowed by robots.txt.\n"
        f"URL: {url}\n"
        f"Robots: {robots_url}\n"
        f"User-Agent: {_FETCH_HEADERS['User-Agent']}"
    )


def _check_robots_allowed(url: str, client: "httpx.Client") -> str | None:
    policy = _read_robots_policy(url, client)
    if policy == "disallow_all":
        return _format_robots_blocked_response(url, _robots_url_for(url))
    if policy is None:
        return None
    if not policy.can_fetch(_FETCH_HEADERS["User-Agent"], url):
        return _format_robots_blocked_response(url, _robots_url_for(url))
    return None


def _format_blocked_fetch_response(url: str, category: str, status_code: int, detail: str) -> str:
    return (
        "Fetch blocked by remote site.\n"
        f"URL: {url}\n"
        f"Category: {category}\n"
        f"HTTP status: {status_code}\n"
        f"Detail: {detail}"
    )


def _classify_blocked_fetch(url: str, status_code: int, headers: dict[str, str], text: str) -> str | None:
    lowered = text.lower()
    server = headers.get("server", "").lower()
    has_cloudflare = "cloudflare" in server or bool(headers.get("cf-ray"))
    if status_code == 429:
        return _format_blocked_fetch_response(url, "rate_limited", status_code, "Remote site rate-limited the request.")
    if "captcha" in lowered or "recaptcha" in lowered or "hcaptcha" in lowered:
        return _format_blocked_fetch_response(url, "captcha_required", status_code, "Captcha or human verification page detected.")
    if has_cloudflare or "attention required" in lowered or "verify you are human" in lowered or "/cdn-cgi/challenge-platform/" in lowered:
        return _format_blocked_fetch_response(url, "blocked_by_waf", status_code, "Cloudflare or WAF challenge detected.")
    if status_code in (401, 407) or (status_code == 403 and "login" in lowered):
        return _format_blocked_fetch_response(url, "login_required", status_code, "Remote site requires authentication.")
    if status_code == 403 and "access denied" in lowered:
        return _format_blocked_fetch_response(url, "blocked_by_waf", status_code, "Access denied page detected.")
    return None


def _sleep_before_retry(attempt: int) -> None:
    time.sleep(_FETCH_RETRY_BASE_DELAY_SECONDS * attempt)


def _fetch_response_bytes(client: "httpx.Client", normalized_url: str) -> tuple[str, bytes] | str:
    for attempt in range(1, _FETCH_MAX_ATTEMPTS + 1):
        try:
            with client.stream("GET", normalized_url) as resp:
                status_code = resp.status_code
                content_type = resp.headers.get("content-type", "")

                if status_code in _FETCH_RETRY_STATUS_CODES and attempt < _FETCH_MAX_ATTEMPTS:
                    _sleep_before_retry(attempt)
                    continue

                parts: list[bytes] = []
                downloaded = 0
                byte_cap = _BLOCK_PREVIEW_BYTES if status_code >= 400 else _MAX_DOWNLOAD_BYTES
                for part in resp.iter_bytes(chunk_size=65536):
                    parts.append(part)
                    downloaded += len(part)
                    if downloaded >= byte_cap:
                        break

                raw_bytes = b"".join(parts)
                if status_code >= 400:
                    preview = raw_bytes.decode("utf-8", errors="replace")
                    classified = _classify_blocked_fetch(normalized_url, status_code, dict(resp.headers), preview)
                    if classified:
                        return classified
                    resp.raise_for_status()

                return content_type, raw_bytes
        except httpx.TransportError:
            if attempt >= _FETCH_MAX_ATTEMPTS:
                raise
            _sleep_before_retry(attempt)

    raise RuntimeError(f"Failed to fetch {normalized_url!r} after {_FETCH_MAX_ATTEMPTS} attempts.")


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
    normalized_url = _normalize_fetch_url(url)
    _guard_ssrf(normalized_url)
    max_length = max(1, min(1_000_000, max_length))
    start_index = max(0, start_index)

    with httpx.Client(headers=_FETCH_HEADERS, follow_redirects=True, timeout=30,
                       event_hooks={"response": [_ssrf_redirect_hook]}) as client:
        robots_blocked = _check_robots_allowed(normalized_url, client)
        if robots_blocked:
            return robots_blocked
        response_payload = _fetch_response_bytes(client, normalized_url)

    if isinstance(response_payload, str):
        return response_payload

    content_type, raw_bytes = response_payload
    if not _is_textual_content_type(content_type):
        return _format_non_text_response(normalized_url, content_type, len(raw_bytes))

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
        chunk = _append_truncation_hint(chunk, remaining, next_idx)
    return chunk


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":  # pragma: no cover
    mcp.run()
