from __future__ import annotations

from contextlib import closing
import json
import math
from pathlib import Path
import sqlite3
from typing import Any


def get_conn(root: str) -> sqlite3.Connection:
    path = Path(root) / ".github" / "xanadAssistant" / "memory" / "memory.db"
    path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(path))
    conn.row_factory = sqlite3.Row
    init_db(conn)
    return conn


def chk_scope(value: str, valid_scopes: set[str]) -> None:
    if value not in valid_scopes:
        raise ValueError(f"scope must be one of {sorted(valid_scopes)}; got {value!r}")


def chk_rule_type(value: str, valid_rule_types: set[str]) -> None:
    if value not in valid_rule_types:
        raise ValueError(f"rule_type must be one of {sorted(valid_rule_types)}; got {value!r}")


def chk_agent(value: str | None, agent_re) -> None:
    if value is not None and not agent_re.match(value):
        raise ValueError(f"agent must be alphanumeric+hyphens; got {value!r}")


def chk_branch(value: str | None, branch_re) -> None:
    if value is not None and not branch_re.match(value):
        raise ValueError(f"branch must be alphanumeric with ._/-; got {value!r}")


def chk_confidence(value: float) -> float:
    try:
        confidence = float(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"confidence must be numeric; got {value!r}") from exc
    if not math.isfinite(confidence) or not 0.0 <= confidence <= 1.0:
        raise ValueError(f"confidence must be between 0.0 and 1.0; got {value!r}")
    return confidence


def init_db(conn: sqlite3.Connection) -> None:
    conn.executescript(
        """
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
        """
    )
    conn.commit()
    migrate_fts_if_needed(conn)


def migrate_fts_if_needed(conn: sqlite3.Connection) -> None:
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


def rows(rows_in: list[sqlite3.Row]) -> list[dict[str, Any]]:
    return [dict(zip(row.keys(), tuple(row))) for row in rows_in]


def row(row_in: sqlite3.Row) -> dict[str, Any]:
    return dict(zip(row_in.keys(), tuple(row_in)))


def render_rows(rows_in: list[sqlite3.Row], empty_message: str, render_row) -> str:
    if not rows_in:
        return empty_message
    return "\n".join(render_row(row_in) for row_in in rows_in)


def prune_extra(agent: str | None, scope: str | None) -> tuple[str, list[Any]]:
    where = ""
    params: list[Any] = []
    if agent:
        where += " AND agent=?"
        params.append(agent)
    if scope:
        where += " AND scope=?"
        params.append(scope)
    return where, params


def active_fact_where() -> str:
    return (
        "  AND invalidated_at IS NULL "
        "  AND (expires_at IS NULL OR expires_at > strftime('%Y-%m-%dT%H:%M:%SZ','now')) "
        "  AND (valid_from IS NULL OR valid_from <= strftime('%Y-%m-%dT%H:%M:%SZ','now')) "
        "  AND (valid_until IS NULL OR valid_until > strftime('%Y-%m-%dT%H:%M:%SZ','now'))"
    )


def memory_dump(agent: str, root: str, branch: str) -> str:
    with closing(get_conn(root)) as conn:
        rules = rows(
            conn.execute(
                "SELECT id, rule_type, description, scope, branch, created_at FROM rules "
                "WHERE (agent IS NULL OR agent=?) "
                "  AND (branch IS NULL OR branch='' OR branch=?) ORDER BY created_at",
                (agent, branch),
            ).fetchall()
        )
        facts = rows(
            conn.execute(
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
            ).fetchall()
        )
    return json.dumps({"rules": rules, "facts": facts})


def memory_prune(agent: str | None, scope: str | None, max_age_days: float | None, root: str) -> str:
    extra, extra_params = prune_extra(agent, scope)
    with closing(get_conn(root)) as conn:
        expired = conn.execute(
            "DELETE FROM advisory_memory WHERE expires_at IS NOT NULL "
            f"AND expires_at <= strftime('%Y-%m-%dT%H:%M:%SZ','now'){extra}",
            extra_params,
        )
        expired_deleted = expired.rowcount
        age_deleted = 0
        diary_deleted = 0
        if max_age_days is not None:
            cutoff = max(0, round(float(max_age_days)))
            stale = conn.execute(
                f"DELETE FROM advisory_memory WHERE updated_at <= "
                f"strftime('%Y-%m-%dT%H:%M:%SZ',datetime('now','-{cutoff} days'))"
                f"{extra}",
                extra_params,
            )
            age_deleted = stale.rowcount
            old_ids = [
                row_in[0]
                for row_in in conn.execute(
                    f"SELECT id FROM agent_diary WHERE recorded_at <= "
                    f"strftime('%Y-%m-%dT%H:%M:%SZ',datetime('now','-{cutoff} days'))"
                    f"{extra}",
                    extra_params,
                ).fetchall()
            ]
            if old_ids:
                placeholders = ",".join("?" * len(old_ids))
                conn.execute(f"DELETE FROM agent_diary WHERE id IN ({placeholders})", old_ids)
                conn.execute(f"DELETE FROM agent_diary_fts WHERE id IN ({placeholders})", old_ids)
                diary_deleted = len(old_ids)
        conn.commit()
    return (
        f"Pruned {expired_deleted} expired + {age_deleted} stale advisory_memory rows; "
        f"{diary_deleted} diary rows."
    )


def rule_add(description: str, rule_type: str, agent: str | None, scope: str, branch: str | None, root: str) -> str:
    with closing(get_conn(root)) as conn:
        cur = conn.execute(
            "INSERT INTO rules (agent,scope,branch,rule_type,description) VALUES (?,?,?,?,?)",
            (agent, scope, branch, rule_type, description),
        )
        conn.commit()
        rule_id = cur.lastrowid
    return f"Rule #{rule_id} added [{rule_type}]: {description}"


def rule_list(agent: str | None, scope: str, root: str) -> str:
    with closing(get_conn(root)) as conn:
        rows_in = conn.execute(
            "SELECT id, rule_type, agent, description, created_at FROM rules "
            "WHERE scope=? AND (agent IS NULL OR agent=?) ORDER BY created_at",
            (scope, agent),
        ).fetchall()
    return render_rows(
        rows_in,
        f"No rules for scope {scope!r}.",
        lambda row_in: f"[#{row_in['id']} {row_in['rule_type']}] {row_in['description']}  (agent={row_in['agent'] or '*'})",
    )


def rule_remove(rule_id: int, root: str) -> str:
    with closing(get_conn(root)) as conn:
        cur = conn.execute("DELETE FROM rules WHERE id=?", (rule_id,))
        conn.commit()
        deleted = cur.rowcount
    return f"Rule #{rule_id} deleted." if deleted else f"No rule with id {rule_id}."


def diary_add(agent: str, entry: str, scope: str, tags: str, branch: str, root: str) -> str:
    with closing(get_conn(root)) as conn:
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


def diary_get(agent: str, scope: str, limit: int, tag: str | None, root: str) -> str:
    with closing(get_conn(root)) as conn:
        if tag:
            rows_in = conn.execute(
                "SELECT id, entry, tags, recorded_at FROM agent_diary "
                "WHERE agent=? AND scope=? "
                "  AND (',' || tags || ',' LIKE '%,' || ? || ',%') "
                "ORDER BY recorded_at DESC, id DESC LIMIT ?",
                (agent, scope, tag, limit),
            ).fetchall()
        else:
            rows_in = conn.execute(
                "SELECT id, entry, tags, recorded_at FROM agent_diary "
                "WHERE agent=? AND scope=? ORDER BY recorded_at DESC, id DESC LIMIT ?",
                (agent, scope, limit),
            ).fetchall()
    return render_rows(
        rows_in,
        f"No diary entries for {agent!r} in scope {scope!r}.",
        lambda row_in: f"[#{row_in['id']} {row_in['recorded_at']}] {row_in['entry']}  tags={row_in['tags']}",
    )


def diary_search(agent: str, query: str, scope: str, limit: int, include_agents: list[str] | None, root: str) -> str:
    agents = [agent]
    for extra_agent in include_agents or []:
        if extra_agent not in agents:
            agents.append(extra_agent)
    placeholders = ",".join("?" * len(agents))
    with closing(get_conn(root)) as conn:
        try:
            rows_in = conn.execute(
                f"SELECT d.id, d.agent, d.entry, d.tags, d.recorded_at "
                f"FROM agent_diary d "
                f"WHERE d.id IN ("
                f"  SELECT id FROM agent_diary_fts "
                f"  WHERE agent_diary_fts MATCH ? AND agent IN ({placeholders}) AND scope=?"
                f") "
                f"ORDER BY d.recorded_at DESC, d.id DESC LIMIT ?",
                (query, *agents, scope, limit),
            ).fetchall()
        except sqlite3.OperationalError as exc:
            raise RuntimeError(f"FTS5 search error: {exc}") from exc
    return render_rows(
        rows_in,
        f"No diary entries matched {query!r} for {agent!r}.",
        lambda row_in: f"[#{row_in['id']} {row_in['recorded_at']} {row_in['agent']}] {row_in['entry']}  tags={row_in['tags']}",
    )