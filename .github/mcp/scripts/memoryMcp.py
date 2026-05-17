#!/usr/bin/env python3
"""Memory MCP — persistent, scoped, SQLite-backed agent memory.

Tools
-----
memory_set        : Upsert a fact; optional TTL and validity windows.
memory_get        : Single lookup; falls back to agent='shared'; cross-agent peek.
memory_list       : All active keys; optional cross-agent key union.
memory_remove     : Hard-delete one entry.
memory_invalidate : Soft-delete (sets invalidated_at; row kept for audit).
memory_dump       : Session wake-up: rules → recent facts → older → shared.
memory_prune      : Delete expired / stale rows; syncs FTS index.
rule_add          : Add an authoritative rule agents must follow.
rule_list         : List rules for agent/scope.
rule_remove       : Delete a rule by id.
diary_add         : Append a decision/action entry (append-only; FTS-indexed).
diary_get         : Recent diary entries, newest first.
diary_search      : FTS5 full-text search across entry and tags.

Security
--------
- All user-supplied values use ? placeholders — no SQL string interpolation.
- agent, scope, and rule_type are validated against allowlists at entry.
- DB lives at WORKSPACE_ROOT/.github/xanadAssistant/memory/memory.db.
  WORKSPACE_ROOT must be set in the environment; raises ValueError if absent.
- FTS index (agent_diary_fts) is kept in sync via explicit dual-write in
  diary_add and dual-delete in memory_prune. No triggers are used.

Transport: stdio  |  Run: uvx --from "mcp[cli]" mcp run <this-file>
"""
from __future__ import annotations

from contextlib import closing
import json
import math
import os
import re
import sqlite3
import subprocess
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

try:
    from mcp.server.fastmcp import FastMCP
except ImportError as _exc:  # pragma: no cover
    sys.stderr.write(
        "ERROR: the 'mcp' package is required.\n"
        "Install with: pip install 'mcp[cli]'\n"
        f"Details: {_exc}\n"
    )
    sys.exit(1)

mcp = FastMCP("xanadMemory")
_VALID_SCOPES = {"workspace", "project", "branch", "session"}
_VALID_RULE_TYPES = {"never", "always", "prefer", "avoid"}
_AGENT_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9\-]*$")

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _workspace_root() -> str:
    v = os.environ.get("WORKSPACE_ROOT")
    if not v:
        raise ValueError("WORKSPACE_ROOT environment variable is not set.")
    return str(Path(v).resolve())


def _current_branch(root: str) -> str:
    try:
        r = subprocess.run(
            ["git", "-C", root, "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True, text=True, timeout=5,
        )
        if r.returncode == 0:
            return r.stdout.strip()
    except Exception:
        pass
    return ""


def _get_conn() -> sqlite3.Connection:
    root = _workspace_root()
    path = Path(root) / ".github" / "xanadAssistant" / "memory" / "memory.db"
    path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(path))
    conn.row_factory = sqlite3.Row
    _init_db(conn)
    return conn


def _init_db(conn: sqlite3.Connection) -> None:
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS advisory_memory (
            agent TEXT NOT NULL, scope TEXT NOT NULL, branch TEXT NOT NULL DEFAULT '',
            key TEXT NOT NULL, value TEXT NOT NULL,
            confidence REAL NOT NULL DEFAULT 1.0,
            source TEXT NOT NULL DEFAULT 'agent-discovered',
            updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
            expires_at TEXT, valid_from TEXT, valid_until TEXT, invalidated_at TEXT,
            PRIMARY KEY (agent, scope, branch, key)
        );
        CREATE TABLE IF NOT EXISTS rules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            agent TEXT, scope TEXT NOT NULL DEFAULT 'workspace', branch TEXT,
            rule_type TEXT NOT NULL, description TEXT NOT NULL,
            created_by TEXT NOT NULL DEFAULT 'user',
            created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
        );
        CREATE TABLE IF NOT EXISTS agent_diary (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            agent TEXT NOT NULL, scope TEXT NOT NULL DEFAULT 'workspace',
            branch TEXT NOT NULL DEFAULT '', entry TEXT NOT NULL,
            tags TEXT NOT NULL DEFAULT '',
            recorded_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now'))
        );
        CREATE VIRTUAL TABLE IF NOT EXISTS agent_diary_fts
            USING fts5(id UNINDEXED, agent UNINDEXED, scope UNINDEXED, branch UNINDEXED, entry, tags);
    """)
    conn.commit()
    _migrate_fts_if_needed(conn)


def _migrate_fts_if_needed(conn: sqlite3.Connection) -> None:
    """Recreate agent_diary_fts if it was created without UNINDEXED on agent/scope/branch."""
    row = conn.execute(
        "SELECT sql FROM sqlite_master WHERE type='table' AND name='agent_diary_fts'"
    ).fetchone()
    if row and row[0] and "agent UNINDEXED" not in row[0]:
        conn.execute("DROP TABLE agent_diary_fts")
        conn.execute(
            "CREATE VIRTUAL TABLE agent_diary_fts "
            "USING fts5(id UNINDEXED, agent UNINDEXED, scope UNINDEXED, branch UNINDEXED, entry, tags)"
        )
        conn.execute(
            "INSERT INTO agent_diary_fts (id,agent,scope,branch,entry,tags) "
            "SELECT id,agent,scope,branch,entry,tags FROM agent_diary"
        )
        conn.commit()


def _chk_scope(v: str) -> None:
    if v not in _VALID_SCOPES:
        raise ValueError(f"scope must be one of {sorted(_VALID_SCOPES)}; got {v!r}")


def _chk_rule_type(v: str) -> None:
    if v not in _VALID_RULE_TYPES:
        raise ValueError(f"rule_type must be one of {sorted(_VALID_RULE_TYPES)}; got {v!r}")


def _chk_agent(v: str | None) -> None:
    if v is not None and not _AGENT_RE.match(v):
        raise ValueError(f"agent must be alphanumeric+hyphens; got {v!r}")


_BRANCH_RE = re.compile(r"^[A-Za-z0-9._/\-]{1,256}$")


def _chk_branch(v: str | None) -> None:
    if v is not None and not _BRANCH_RE.match(v):
        raise ValueError(f"branch must be alphanumeric with ._/-; got {v!r}")


def _chk_confidence(v: float) -> float:
    try:
        confidence = float(v)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"confidence must be numeric; got {v!r}") from exc
    if not math.isfinite(confidence) or not 0.0 <= confidence <= 1.0:
        raise ValueError(f"confidence must be between 0.0 and 1.0; got {v!r}")
    return confidence


def _advisory_branch(scope: str, root: str) -> str:
    return _current_branch(root) if scope == "branch" else ""


def _rows(rows: list[sqlite3.Row]) -> list[dict[str, Any]]:
    return [dict(zip(r.keys(), tuple(r))) for r in rows]


def _row(row: sqlite3.Row) -> dict[str, Any]:
    return dict(zip(row.keys(), tuple(row)))


def _render_rows(rows: list[sqlite3.Row], empty_message: str, render_row) -> str:
    if not rows:
        return empty_message
    return "\n".join(render_row(row) for row in rows)


def _prune_extra(agent: str | None, scope: str | None) -> tuple[str, list[Any]]:
    where, params = "", []
    if agent:
        where += " AND agent=?"
        params.append(agent)
    if scope:
        where += " AND scope=?"
        params.append(scope)
    return where, params


def _active_fact_where() -> str:
    return (
        "  AND invalidated_at IS NULL "
        "  AND (expires_at IS NULL OR expires_at > strftime('%Y-%m-%dT%H:%M:%SZ','now')) "
        "  AND (valid_from IS NULL OR valid_from <= strftime('%Y-%m-%dT%H:%M:%SZ','now')) "
        "  AND (valid_until IS NULL OR valid_until > strftime('%Y-%m-%dT%H:%M:%SZ','now'))"
    )


@mcp.tool()
def memory_set(
    agent: str, key: str, value: str,
    scope: str = "workspace", confidence: float = 1.0,
    source: str = "agent-discovered",
    ttl_days: float | None = None,
    valid_from: str | None = None, valid_until: str | None = None,
) -> str:
    """Upsert a fact into advisory memory.

    Args:
        agent: Agent identifier (alphanumeric + hyphens).
        key: Fact key (e.g. 'ci.commands', 'repo.language').
        value: Fact value as a string.
        scope: One of workspace/project/branch/session.
        confidence: 0.0–1.0 (1.0 = fully confident).
        source: Origin tag (default 'agent-discovered').
        ttl_days: If set, fact expires after this many days.
        valid_from: ISO-8601; fact not applicable before this date.
        valid_until: ISO-8601; soft validity end.
    """
    _chk_agent(agent)
    _chk_scope(scope)
    root = _workspace_root()
    branch = _advisory_branch(scope, root)
    confidence = _chk_confidence(confidence)
    expires_at: str | None = None
    if ttl_days is not None:
        expires_at = (
            datetime.now(timezone.utc) + timedelta(days=ttl_days)
        ).strftime("%Y-%m-%dT%H:%M:%SZ")
    with closing(_get_conn()) as conn:
        conn.execute(
            """
            INSERT INTO advisory_memory
                (agent, scope, branch, key, value, confidence, source,
                 expires_at, valid_from, valid_until, invalidated_at)
            VALUES (?,?,?,?,?,?,?,?,?,?,NULL)
            ON CONFLICT(agent,scope,branch,key) DO UPDATE SET
                value=excluded.value, confidence=excluded.confidence,
                source=excluded.source,
                updated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now'),
                expires_at=excluded.expires_at, valid_from=excluded.valid_from,
                valid_until=excluded.valid_until, invalidated_at=NULL
            """,
            (agent, scope, branch, key, value, confidence, source,
             expires_at, valid_from, valid_until),
        )
        conn.commit()
    return f"Stored {key!r} for {agent!r} (scope={scope!r}, branch={branch!r})."


@mcp.tool()
def memory_get(
    agent: str, key: str, scope: str = "workspace",
    include_agents: list[str] | None = None,
) -> str:
    """Retrieve one currently applicable fact.

    Falls back to agent='shared'; cross-agent peek via include_agents.

    Args:
        agent: Primary agent identifier.
        key: Fact key to look up.
        scope: Scope to search within.
        include_agents: Additional agents to search (explicit opt-in).
    """
    _chk_agent(agent)
    _chk_scope(scope)
    root = _workspace_root()
    branch = _advisory_branch(scope, root)
    agents = [agent]
    for a in (include_agents or []):
        _chk_agent(a)
        if a not in agents:
            agents.append(a)
    if "shared" not in agents:
        agents.append("shared")
    with closing(_get_conn()) as conn:
        for a in agents:
            row = conn.execute(
                "SELECT * FROM advisory_memory "
                "WHERE agent=? AND scope=? AND branch=? AND key=? "
                + _active_fact_where(),
                (a, scope, branch, key),
            ).fetchone()
            if row:
                return json.dumps(_row(row))
    return f"No active fact for {key!r} (agent={agent!r}, scope={scope!r})."


@mcp.tool()
def memory_list(
    agent: str, scope: str = "workspace",
    include_shared: bool = True, include_agents: list[str] | None = None,
) -> str:
    """List all currently applicable fact keys for agent/scope.

    Cross-agent union via include_agents.

    Args:
        agent: Agent identifier.
        scope: Scope to list.
        include_shared: Also include agent='shared' keys.
        include_agents: Additional agents to union (explicit opt-in; same semantics as memory_get).
    """
    _chk_agent(agent)
    _chk_scope(scope)
    root = _workspace_root()
    branch = _advisory_branch(scope, root)
    agents = [agent]
    if include_shared and "shared" not in agents:
        agents.append("shared")
    for a in (include_agents or []):
        _chk_agent(a)
        if a not in agents:
            agents.append(a)
    ph = ",".join("?" * len(agents))
    with closing(_get_conn()) as conn:
        rows = conn.execute(
            f"SELECT agent, key, confidence, updated_at FROM advisory_memory "
            f"WHERE agent IN ({ph}) AND scope=? AND branch=? "
            f"{_active_fact_where()} "
            f"ORDER BY agent, key",
            (*agents, scope, branch),
        ).fetchall()
    return _render_rows(
        rows,
        f"No active facts for {agent!r} in scope {scope!r}.",
        lambda row: f"{row['agent']}/{row['key']}  conf={row['confidence']}  updated={row['updated_at']}",
    )


@mcp.tool()
def memory_remove(agent: str, key: str, scope: str = "workspace") -> str:
    """Hard-delete one advisory memory entry.

    Args:
        agent: Agent identifier.
        key: Fact key to delete.
        scope: Scope of the fact.
    """
    _chk_agent(agent)
    _chk_scope(scope)
    branch = _advisory_branch(scope, _workspace_root())
    with closing(_get_conn()) as conn:
        cur = conn.execute(
            "DELETE FROM advisory_memory WHERE agent=? AND scope=? AND branch=? AND key=?",
            (agent, scope, branch, key),
        )
        conn.commit()
        deleted = cur.rowcount
    return f"Removed {key!r}." if deleted else f"No entry for {key!r}."


@mcp.tool()
def memory_invalidate(agent: str, key: str, scope: str = "workspace") -> str:
    """Soft-delete a fact: sets invalidated_at; row kept for audit.

    Args:
        agent: Agent identifier.
        key: Fact key to invalidate.
        scope: Scope of the fact.
    """
    _chk_agent(agent)
    _chk_scope(scope)
    branch = _advisory_branch(scope, _workspace_root())
    with closing(_get_conn()) as conn:
        cur = conn.execute(
            "UPDATE advisory_memory "
            "SET invalidated_at=strftime('%Y-%m-%dT%H:%M:%SZ','now') "
            "WHERE agent=? AND scope=? AND branch=? AND key=? AND invalidated_at IS NULL",
            (agent, scope, branch, key),
        )
        conn.commit()
        updated = cur.rowcount
    return (
        f"Invalidated {key!r} (row kept for audit)." if updated
        else f"No active entry for {key!r}."
    )


@mcp.tool()
def memory_dump(agent: str) -> str:
    """Session wake-up: return all rules and facts as JSON.

    Order: rules → agent facts (recent <24h first, then older; both sorted by
    confidence descending) → shared facts (same order within each tier).
    Each fact includes updated_at, expires_at, valid_from, valid_until.
    Facts are returned only when they are currently applicable.

    Args:
        agent: Agent identifier.
    """
    _chk_agent(agent)
    root = _workspace_root()
    branch = _current_branch(root)
    with closing(_get_conn()) as conn:
        rules = _rows(conn.execute(
            "SELECT id, rule_type, description, scope, branch, created_at FROM rules "
            "WHERE (agent IS NULL OR agent=?) "
            "  AND (branch IS NULL OR branch='' OR branch=?) ORDER BY created_at",
            (agent, branch),
        ).fetchall())
        facts = _rows(conn.execute(
            """
            SELECT agent, scope, branch, key, value, confidence, source,
                   updated_at, expires_at, valid_from, valid_until
            FROM advisory_memory
            WHERE agent IN (?, ?) AND branch IN (?, ?)
                            AND invalidated_at IS NULL
                            AND (expires_at IS NULL OR expires_at > strftime('%Y-%m-%dT%H:%M:%SZ','now'))
                            AND (valid_from IS NULL OR valid_from <= strftime('%Y-%m-%dT%H:%M:%SZ','now'))
                            AND (valid_until IS NULL OR valid_until > strftime('%Y-%m-%dT%H:%M:%SZ','now'))
            ORDER BY
                CASE WHEN agent=? THEN 0 ELSE 1 END,
                CASE WHEN updated_at >= strftime('%Y-%m-%dT%H:%M:%SZ',
                     datetime('now', '-1 day')) THEN 0 ELSE 1 END,
                confidence DESC
            """,
            (agent, "shared", "", branch, agent),
        ).fetchall())
    return json.dumps({"rules": rules, "facts": facts})


@mcp.tool()
def memory_prune(
    agent: str | None = None,
    scope: str | None = None,
    max_age_days: float | None = None,
) -> str:
    """Delete expired rows and optionally stale rows older than max_age_days. Syncs FTS.

    Args:
        agent: If set, restrict pruning to this agent.
        scope: If set, restrict pruning to this scope.
        max_age_days: Also delete advisory_memory and diary rows older than this.
    """
    if agent is not None:
        _chk_agent(agent)
    if scope is not None:
        _chk_scope(scope)
    extra, extra_params = _prune_extra(agent, scope)
    with closing(_get_conn()) as conn:
        cur = conn.execute(
            "DELETE FROM advisory_memory WHERE expires_at IS NOT NULL "
            f"AND expires_at <= strftime('%Y-%m-%dT%H:%M:%SZ','now'){extra}",
            extra_params,
        )
        exp_del = cur.rowcount
        age_del = diary_del = 0
        if max_age_days is not None:
            cutoff = max(0, round(float(max_age_days)))  # rounded, not truncated
            cur2 = conn.execute(
                f"DELETE FROM advisory_memory WHERE updated_at <= "
                f"strftime('%Y-%m-%dT%H:%M:%SZ',datetime('now','-{cutoff} days'))"
                f"{extra}",
                extra_params,
            )
            age_del = cur2.rowcount
            old_ids = [
                r[0] for r in conn.execute(
                    f"SELECT id FROM agent_diary WHERE recorded_at <= "
                    f"strftime('%Y-%m-%dT%H:%M:%SZ',datetime('now','-{cutoff} days'))"
                    f"{extra}",
                    extra_params,
                ).fetchall()
            ]
            if old_ids:
                ph = ",".join("?" * len(old_ids))
                conn.execute(f"DELETE FROM agent_diary WHERE id IN ({ph})", old_ids)
                conn.execute(f"DELETE FROM agent_diary_fts WHERE id IN ({ph})", old_ids)
                diary_del = len(old_ids)
        conn.commit()
    return (
        f"Pruned {exp_del} expired + {age_del} stale advisory_memory rows; "
        f"{diary_del} diary rows."
    )


@mcp.tool()
def rule_add(
    description: str, rule_type: str,
    agent: str | None = None, scope: str = "workspace", branch: str | None = None,
) -> str:
    """Add an authoritative rule that agents must follow.

    Args:
        description: Human-readable rule text.
        rule_type: One of never/always/prefer/avoid.
        agent: If set, rule applies only to this agent; None = all agents.
        scope: One of workspace/project/branch/session.
        branch: If set, rule applies only to this branch.
    """
    _chk_rule_type(rule_type)
    _chk_agent(agent)
    _chk_branch(branch)
    _chk_scope(scope)
    with closing(_get_conn()) as conn:
        cur = conn.execute(
            "INSERT INTO rules (agent,scope,branch,rule_type,description) VALUES (?,?,?,?,?)",
            (agent, scope, branch, rule_type, description),
        )
        conn.commit()
        rule_id = cur.lastrowid
    return f"Rule #{rule_id} added [{rule_type}]: {description}"


@mcp.tool()
def rule_list(agent: str | None = None, scope: str = "workspace") -> str:
    """List authoritative rules for agent/scope (includes global rules where agent IS NULL).

    Args:
        agent: If set, include rules for this agent plus global rules.
        scope: Scope to filter by.
    """
    _chk_agent(agent)
    _chk_scope(scope)
    with closing(_get_conn()) as conn:
        rows = conn.execute(
            "SELECT id, rule_type, agent, description, created_at FROM rules "
            "WHERE scope=? AND (agent IS NULL OR agent=?) ORDER BY created_at",
            (scope, agent),
        ).fetchall()
    return _render_rows(
        rows,
        f"No rules for scope {scope!r}.",
        lambda row: f"[#{row['id']} {row['rule_type']}] {row['description']}  (agent={row['agent'] or '*'})",
    )


@mcp.tool()
def rule_remove(rule_id: int) -> str:
    """Delete a rule by its id.

    Args:
        rule_id: Integer id from rule_list.
    """
    with closing(_get_conn()) as conn:
        cur = conn.execute("DELETE FROM rules WHERE id=?", (rule_id,))
        conn.commit()
        deleted = cur.rowcount
    return f"Rule #{rule_id} deleted." if deleted else f"No rule with id {rule_id}."


@mcp.tool()
def diary_add(
    agent: str, entry: str, scope: str = "workspace", tags: str = "",
) -> str:
    """Append a decision/action entry to the agent diary (append-only; FTS-indexed).

    Args:
        agent: Agent identifier.
        entry: The decision or action to record.
        scope: One of workspace/project/branch/session.
        tags: Comma-separated tags (e.g. 'ci,deploy').
    """
    _chk_agent(agent)
    _chk_scope(scope)
    branch = _current_branch(_workspace_root())
    with closing(_get_conn()) as conn:
        with conn:
            cur = conn.execute(
                "INSERT INTO agent_diary (agent,scope,branch,entry,tags) VALUES (?,?,?,?,?)",
                (agent, scope, branch, entry, tags),
            )
            diary_id = cur.lastrowid
            conn.execute(
                "INSERT INTO agent_diary_fts (id,agent,scope,branch,entry,tags) VALUES (?,?,?,?,?,?)",
                (diary_id, agent, scope, branch, entry, tags),
            )
    return f"Diary entry #{diary_id} recorded for {agent!r}."


@mcp.tool()
def diary_get(
    agent: str, scope: str = "workspace", limit: int = 20, tag: str | None = None,
) -> str:
    """Retrieve recent diary entries, newest first.

    Note: entries from all branches are returned. Diary is intentionally
    cross-branch; use tags to distinguish branch-specific entries if needed.

    Args:
        agent: Agent identifier.
        scope: Scope to retrieve from.
        limit: Maximum entries to return.
        tag: If set, filter to entries containing this tag.
    """
    _chk_agent(agent)
    _chk_scope(scope)
    with closing(_get_conn()) as conn:
        if tag:
            rows = conn.execute(
                "SELECT id, entry, tags, recorded_at FROM agent_diary "
                "WHERE agent=? AND scope=? "
                "  AND (',' || tags || ',' LIKE '%,' || ? || ',%') "
                "ORDER BY recorded_at DESC, id DESC LIMIT ?",
                (agent, scope, tag, limit),
            ).fetchall()
        else:
            rows = conn.execute(
                "SELECT id, entry, tags, recorded_at FROM agent_diary "
                "WHERE agent=? AND scope=? ORDER BY recorded_at DESC, id DESC LIMIT ?",
                (agent, scope, limit),
            ).fetchall()
    return _render_rows(
        rows,
        f"No diary entries for {agent!r} in scope {scope!r}.",
        lambda row: f"[#{row['id']} {row['recorded_at']}] {row['entry']}  tags={row['tags']}",
    )


@mcp.tool()
def diary_search(
    agent: str, query: str, scope: str = "workspace",
    limit: int = 20, include_agents: list[str] | None = None,
) -> str:
    """FTS5 full-text search across diary entry and tags.

    Note: searches across all branches. Only entry and tags columns are
    full-text indexed; agent, scope, and branch are filter-only columns.

    Args:
        agent: Primary agent whose diary to search.
        query: FTS5 query (e.g. 'deploy AND ci', '"exact phrase"').
        scope: Scope to search within.
        limit: Maximum results to return.
        include_agents: Optional additional agents to include in the search.
    """
    _chk_agent(agent)
    _chk_scope(scope)
    agents = [agent]
    for a in (include_agents or []):
        _chk_agent(a)
        if a not in agents:
            agents.append(a)
    ph = ",".join("?" * len(agents))
    with closing(_get_conn()) as conn:
        try:
            rows = conn.execute(
                f"SELECT d.id, d.agent, d.entry, d.tags, d.recorded_at "
                f"FROM agent_diary d "
                f"WHERE d.id IN ("
                f"  SELECT id FROM agent_diary_fts "
                f"  WHERE agent_diary_fts MATCH ? AND agent IN ({ph}) AND scope=?"
                f") "
                f"ORDER BY d.recorded_at DESC, d.id DESC LIMIT ?",
                (query, *agents, scope, limit),
            ).fetchall()
        except sqlite3.OperationalError as exc:
            raise RuntimeError(f"FTS5 search error: {exc}") from exc
    return _render_rows(
        rows,
        f"No diary entries matched {query!r} for {agent!r}.",
        lambda row: f"[#{row['id']} {row['recorded_at']} {row['agent']}] {row['entry']}  tags={row['tags']}",
    )

if __name__ == "__main__":  # pragma: no cover
    mcp.run()
