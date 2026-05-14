#!/usr/bin/env python3
"""Time MCP server — current time, elapsed, and timezone conversion.

Tools
-----
current_time     : Current date and time in any IANA timezone.
elapsed          : Human-readable duration between two ISO-8601 timestamps.
convert_timezone : Translate a timestamp from one timezone to another.
format_duration  : Format a number of seconds as a human-readable string.

All operations use stdlib datetime and zoneinfo — no network, no deps beyond
the mcp[cli] package itself.

Transport: stdio  |  Run: uvx --from "mcp[cli]" mcp run <this-file>
"""
from __future__ import annotations

import sys
from datetime import datetime, timezone
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

try:
    from mcp.server.fastmcp import FastMCP
except ImportError as _exc:  # pragma: no cover
    sys.stderr.write(
        "ERROR: the 'mcp' package is required but not installed.\n"
        "Install it with: pip install 'mcp[cli]'\n"
        f"Details: {_exc}\n"
    )
    sys.exit(1)

mcp = FastMCP("xanadTime")

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _parse_iso(ts: str) -> datetime:
    ts = ts.strip()
    try:
        dt = datetime.fromisoformat(ts)
    except ValueError as exc:
        raise ValueError(
            f"Cannot parse {ts!r} — expected ISO-8601 (e.g. '2026-05-10T14:30:00')."
        ) from exc
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return dt


def _tz(name: str) -> ZoneInfo:
    try:
        return ZoneInfo(name)
    except ZoneInfoNotFoundError:
        raise ValueError(
            f"Unknown timezone: {name!r}. Use an IANA name (e.g. 'America/New_York', 'UTC')."
        )


# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------

@mcp.tool()
def current_time(tz: str = "UTC") -> str:
    """Return the current date and time in the given IANA timezone.

    Args:
        tz: IANA timezone name (default 'UTC'). Examples: 'America/New_York',
            'Europe/London', 'Asia/Tokyo', 'US/Pacific', 'UTC'.
    """
    return datetime.now(_tz(tz)).isoformat(timespec="seconds")


@mcp.tool()
def elapsed(start: str, end: str = "") -> str:
    """Return the human-readable duration between two ISO-8601 timestamps.

    Args:
        start: Start timestamp in ISO-8601 format (timezone-aware or UTC-assumed).
        end: End timestamp in ISO-8601 format. Defaults to the current UTC time.
    """
    t0 = _parse_iso(start)
    t1 = _parse_iso(end) if end.strip() else datetime.now(timezone.utc)
    delta = t1 - t0
    total = abs(delta.total_seconds())
    suffix = " (negative — end is before start)" if delta.total_seconds() < 0 else ""
    return format_duration(total) + suffix


@mcp.tool()
def convert_timezone(timestamp: str, from_tz: str, to_tz: str) -> str:
    """Convert an ISO-8601 timestamp from one timezone to another.

    Args:
        timestamp: ISO-8601 datetime string (e.g. '2026-05-10T14:30:00').
        from_tz: Source IANA timezone name (used if timestamp has no tzinfo).
        to_tz: Target IANA timezone name.
    """
    dt = _parse_iso(timestamp)
    if dt.tzinfo is None or dt.tzinfo.utcoffset(dt) is None:  # pragma: no cover
        dt = dt.replace(tzinfo=_tz(from_tz))
    return dt.astimezone(_tz(to_tz)).isoformat(timespec="seconds")


@mcp.tool()
def format_duration(seconds: float) -> str:
    """Format a duration in seconds as a human-readable string.

    Args:
        seconds: Duration in seconds (fractional seconds accepted).
    """
    seconds = abs(seconds)
    if seconds < 1:
        return f"{seconds * 1000:.1f}ms"
    if seconds < 60:
        return f"{seconds:.2f}s"
    mins, secs = divmod(int(seconds), 60)
    if mins < 60:
        return f"{mins}m {secs}s"
    hrs, mins = divmod(mins, 60)
    if hrs < 24:
        return f"{hrs}h {mins}m {secs}s"
    days, hrs = divmod(hrs, 24)
    return f"{days}d {hrs}h {mins}m"


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":  # pragma: no cover
    mcp.run()
