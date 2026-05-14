#!/usr/bin/env python3
"""SQLite MCP — query and inspect local SQLite databases.

Tools
-----
execute_query  : Run a SQL statement and return results as a formatted table.
                 Read-only by default; pass allow_writes=True to enable writes.
list_tables    : List all tables and views in a database file.
describe_table : Return the CREATE statement for a table or view.

Security
--------
- ``db_path`` is unrestricted: any readable filesystem path is accepted; trust
  is placed in the agent to supply a safe path. No additional path sandboxing
  is performed by this server.
- Read-only connections use SQLite's URI mode (mode=ro) enforced at the driver
  level — no write operation can succeed regardless of the SQL sent.
- User-supplied values should be passed via the params list (? placeholders)
  rather than interpolated into the query string.
- Row output is capped at max_rows (default 100) to prevent runaway results.

Transport: stdio  |  Run: uvx --from "mcp[cli]" mcp run <this-file>
"""
from __future__ import annotations

import sqlite3
import sys
from pathlib import Path

try:
    from mcp.server.fastmcp import FastMCP
except ImportError as _exc:  # pragma: no cover
    sys.stderr.write(
        "ERROR: the 'mcp' package is required but not installed.\n"
        "Install it with: pip install 'mcp[cli]'\n"
        f"Details: {_exc}\n"
    )
    sys.exit(1)

mcp = FastMCP("xanadSQLite")

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _connect(db_path: str, allow_writes: bool) -> sqlite3.Connection:
    path = Path(db_path).resolve()
    if not path.exists():
        raise FileNotFoundError(f"Database file not found: {db_path!r}")
    if allow_writes:
        return sqlite3.connect(str(path))
    return sqlite3.connect(f"file:{path}?mode=ro", uri=True)


def _fmt(cursor: sqlite3.Cursor, max_rows: int) -> str:
    cols = [d[0] for d in (cursor.description or [])]
    if not cols:
        return "(statement executed — no rows returned)"
    rows = cursor.fetchmany(max_rows + 1)
    truncated = len(rows) > max_rows
    if truncated:
        rows = rows[:max_rows]
    widths = [
        max(len(c), max((len(str(r[i])) for r in rows), default=0))
        for i, c in enumerate(cols)
    ]
    sep = "+-" + "-+-".join("-" * w for w in widths) + "-+"
    header = "| " + " | ".join(c.ljust(widths[i]) for i, c in enumerate(cols)) + " |"
    body = [
        "| " + " | ".join(str(v).ljust(widths[i]) for i, v in enumerate(row)) + " |"
        for row in rows
    ]
    lines = [sep, header, sep, *body, sep]
    if truncated:
        lines.append(
            f"(output capped at {max_rows} rows — add LIMIT or increase max_rows)"
        )
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------

@mcp.tool()
def execute_query(
    db_path: str,
    query: str,
    params: list | None = None,
    allow_writes: bool = False,
    max_rows: int = 100,
) -> str:
    """Execute a SQL statement and return results as a formatted table.

    Args:
        db_path: Absolute or relative path to the SQLite database file.
        query: SQL statement to execute.
        params: Positional parameters for ? placeholders in the query.
                Always use this instead of string-interpolating user values.
        allow_writes: Open in read-write mode. Default False enforces a
                      read-only connection at the driver level.
        max_rows: Maximum rows to return (default 100).
    """
    conn = _connect(db_path, allow_writes)
    try:
        cur = conn.execute(query, params or [])
        if allow_writes:
            conn.commit()
        return _fmt(cur, max_rows)
    except sqlite3.Error as exc:
        raise RuntimeError(str(exc)) from exc
    finally:
        conn.close()


@mcp.tool()
def list_tables(db_path: str) -> str:
    """List all tables and views in a SQLite database file.

    Args:
        db_path: Absolute or relative path to the SQLite database file.
    """
    conn = _connect(db_path, allow_writes=False)
    try:
        cur = conn.execute(
            "SELECT type, name FROM sqlite_master "
            "WHERE type IN ('table', 'view') ORDER BY type, name"
        )
        rows = cur.fetchall()
    finally:
        conn.close()
    if not rows:
        return "No tables or views found in the database."
    return "\n".join(f"[{kind}] {name}" for kind, name in rows)


@mcp.tool()
def describe_table(db_path: str, table: str) -> str:
    """Return the CREATE statement for a table or view.

    Args:
        db_path: Absolute or relative path to the SQLite database file.
        table: Name of the table or view to describe.
    """
    conn = _connect(db_path, allow_writes=False)
    try:
        cur = conn.execute(
            "SELECT sql FROM sqlite_master WHERE name = ?", (table,)
        )
        row = cur.fetchone()
    finally:
        conn.close()
    if not row or not row[0]:
        return f"No table or view named {table!r} found in the database."
    return row[0]


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":  # pragma: no cover
    mcp.run()
