#!/usr/bin/env python3
"""GitHub MCP via the GitHub REST API.

Requires `GITHUB_TOKEN` or `GH_TOKEN` and exposes repository, issue, pull
request, release, Actions, code-search, and file-content tools over stdio.
Run with: `uvx --from "mcp[cli]" mcp run <this-file>`.
"""
from __future__ import annotations

import base64
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from typing import Any

try:
    from mcp.server.fastmcp import FastMCP
except ImportError as _exc:  # pragma: no cover
    sys.stderr.write(
        "ERROR: the 'mcp' package is required but not installed.\n"
        "Install it with: pip install 'mcp[cli]'\n"
        f"Details: {_exc}\n"
    )
    sys.exit(1)

mcp = FastMCP("xanadGitHub")

_API = "https://api.github.com"
_SAFE_NAME = re.compile(r"^[A-Za-z0-9._-]+$")
_ALLOWED_STATES = {"open", "closed", "all"}

_REPO_FIELDS = ("full_name", "description", "default_branch", "stargazers_count", "forks_count", "open_issues_count", "license", "visibility", "html_url", "pushed_at")
_ISSUE_FIELDS = ("number", "title", "state", "body", "html_url", "user", "labels", "assignees", "created_at", "updated_at")
_PULL_FIELDS = ("number", "title", "state", "body", "html_url", "draft", "head", "base", "user", "mergeable", "created_at", "updated_at")


def _validate_owner_repo(owner: str, repo: str) -> None:
    """Raise ValueError if owner or repo contain non-safe characters."""
    if not _SAFE_NAME.match(owner):
        raise ValueError(f"Invalid owner name: {owner!r}")
    if not _SAFE_NAME.match(repo):
        raise ValueError(f"Invalid repo name: {repo!r}")


def _normalize_per_page(per_page: int, maximum: int) -> int:
    """Clamp GitHub list sizes to the documented inclusive range."""
    return max(1, min(maximum, per_page))


def _validate_state(state: str) -> None:
    """Raise ValueError when a list endpoint receives an unsupported state."""
    if state not in _ALLOWED_STATES:
        allowed = ", ".join(sorted(_ALLOWED_STATES))
        raise ValueError(f"Invalid state: {state!r}. Expected one of: {allowed}.")


def _validate_workflow_id(workflow_id: str | int) -> str:
    """Return a validated workflow id or filename suitable for a path segment."""
    from pathlib import PurePosixPath

    value = str(workflow_id).strip()
    if not value:
        return ""
    path = PurePosixPath(value)
    if len(path.parts) == 3 and path.parts[:2] == (".github", "workflows"):
        value = path.name
    if not _SAFE_NAME.match(value):
        raise ValueError(f"Invalid workflow_id: {workflow_id!r}")
    return value


def _dump_fields(payload: dict[str, Any], fields: tuple[str, ...]) -> str:
    return json.dumps({field: payload.get(field) for field in fields}, indent=2)


def _render_lines(items: list[Any], empty_message: str, render_item) -> str:
    if not items:
        return empty_message
    return "\n".join(render_item(item) for item in items)


def _gh_cli_token() -> str:
    """Return the token from the GitHub CLI (``gh auth token``), or '' if unavailable."""
    try:
        result = subprocess.run(
            ["gh", "auth", "token"],
            capture_output=True, text=True, timeout=5,
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
        pass
    return ""


def _token() -> str:
    tok = (
        os.environ.get("GITHUB_TOKEN", "").strip()
        or os.environ.get("GH_TOKEN", "").strip()
    )
    if not tok:
        tok = _gh_cli_token()
    if not tok:
        raise RuntimeError(
            "No GitHub token found. Authenticate with one of:\n"
            "  1. gh auth login  (GitHub CLI)\n"
            "  2. export GITHUB_TOKEN=<pat> in your shell profile\n"
            "Then restart VS Code."
        )
    return tok


def _decode_json_response(raw_body: bytes, path: str) -> Any:
    """Decode a GitHub API JSON response or raise a structured RuntimeError."""
    if not raw_body.strip():
        raise RuntimeError(f"GitHub API returned an empty response for {path}.")
    try:
        return json.loads(raw_body)
    except json.JSONDecodeError as exc:
        preview = raw_body.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"GitHub API returned a non-JSON response for {path}: {preview[:200]}"
        ) from exc


def _format_http_error(path: str, code: int, body_text: str) -> str:
    """Return a GitHub-specific error message with actionable hints."""
    details = body_text.strip() or "request failed"
    message = f"GitHub API {code}: {details}"
    hints: list[str] = []
    if code == 404:
        if "/pulls/" in path:
            hints.append(
                "If this number refers to an issue rather than a pull request, use get_issue instead."
            )
        if path.startswith("/repos/"):
            hints.append(
                "GitHub also returns 404 when the repository or item does not exist or your token cannot access it."
            )
    if hints:
        return f"{message} {' '.join(hints)}"
    return message


def _req(method: str, path: str, body: dict | None = None,
         params: dict | None = None) -> Any:
    url = f"{_API}{path}"
    if params:
        filtered = {k: v for k, v in params.items() if v is not None}
        if filtered:
            url += "?" + urllib.parse.urlencode(filtered)
    data = json.dumps(body).encode() if body else None
    req = urllib.request.Request(
        url, data=data, method=method,
        headers={
            "Authorization": f"Bearer {_token()}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "Content-Type": "application/json",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            return _decode_json_response(resp.read(), path)
    except urllib.error.HTTPError as exc:
        body_text = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(_format_http_error(path, exc.code, body_text)) from exc


def _get(path: str, params: dict | None = None) -> Any:
    return _req("GET", path, params=params)


def _post(path: str, body: dict) -> Any:
    return _req("POST", path, body=body)


@mcp.tool()
def get_repo(owner: str, repo: str) -> str:
    """Return key metadata for a GitHub repository."""
    _validate_owner_repo(owner, repo)
    return _dump_fields(_get(f"/repos/{owner}/{repo}"), _REPO_FIELDS)


@mcp.tool()
def get_file_contents(owner: str, repo: str, path: str, ref: str = "") -> str:
    """Return the decoded text content of a file from a GitHub repository.

    Args:
        owner: Repository owner (user or org).
        repo: Repository name.
        path: File path within the repository (e.g. 'src/main.py').
        ref: Branch, tag, or commit SHA (defaults to the default branch).
    """
    _validate_owner_repo(owner, repo)
    from pathlib import PurePosixPath
    if ".." in PurePosixPath(path).parts:
        raise ValueError(f"Path must not contain '..' components: {path!r}")
    safe_path = urllib.parse.quote(path, safe="/")
    params = {"ref": ref} if ref else None
    d = _get(f"/repos/{owner}/{repo}/contents/{safe_path}", params=params)
    if d.get("encoding") == "base64":
        encoded_content = d.get("content")
        if not encoded_content:
            return "(content unavailable)"
        return base64.b64decode(encoded_content).decode("utf-8", errors="replace")
    return d.get("content", "(binary or unsupported encoding)")


@mcp.tool()
def search_code(query: str, per_page: int = 10) -> str:
    """Search code on GitHub.

    Args:
        query: GitHub code search query (e.g. 'repo:owner/repo someFunction').
        per_page: Number of results, 1–30 (default 10).
    """
    d = _get("/search/code", {"q": query, "per_page": _normalize_per_page(per_page, 30)})
    items = d.get("items", [])
    if not items:
        return f"No code results for: {query!r}"
    lines = [f"Found {d.get('total_count', '?')} result(s):"]
    for item in items:
        lines.append(f"  {item['repository']['full_name']}: {item['path']} ({item['html_url']})")
    return "\n".join(lines)


@mcp.tool()
def list_issues(owner: str, repo: str, state: str = "open",
                per_page: int = 20) -> str:
    """List issues for a repository (excludes pull requests).

    Args:
        owner: Repository owner.
        repo: Repository name.
        state: 'open' (default), 'closed', or 'all'.
        per_page: Results per page, 1–50 (default 20).
    """
    _validate_owner_repo(owner, repo)
    _validate_state(state)
    items = _get(f"/repos/{owner}/{repo}/issues",
                 {"state": state, "per_page": _normalize_per_page(per_page, 50)})
    issues = [i for i in items if "pull_request" not in i]
    return _render_lines(
        issues,
        f"No {state} issues in {owner}/{repo}.",
        lambda issue: (
            f"#{issue['number']} [{issue['state']}] {issue['title']} ({issue['html_url']})"
        ),
    )


@mcp.tool()
def get_issue(owner: str, repo: str, issue_number: int) -> str:
    """Get full details of a specific issue.

    Args:
        owner: Repository owner.
        repo: Repository name.
        issue_number: Issue number.
    """
    _validate_owner_repo(owner, repo)
    return _dump_fields(_get(f"/repos/{owner}/{repo}/issues/{issue_number}"), _ISSUE_FIELDS)


@mcp.tool()
def create_issue_comment(owner: str, repo: str,
                         issue_number: int, body: str) -> str:
    """Post a comment on an issue or pull request.

    Args:
        owner: Repository owner.
        repo: Repository name.
        issue_number: Issue or PR number.
        body: Comment text (Markdown supported).
    """
    _validate_owner_repo(owner, repo)
    r = _post(f"/repos/{owner}/{repo}/issues/{issue_number}/comments", {"body": body})
    return f"Comment created: {r.get('html_url')}"


@mcp.tool()
def list_pull_requests(owner: str, repo: str, state: str = "open",
                       per_page: int = 20) -> str:
    """List pull requests for a repository.

    Args:
        owner: Repository owner.
        repo: Repository name.
        state: 'open' (default), 'closed', or 'all'.
        per_page: Results per page, 1–50 (default 20).
    """
    _validate_owner_repo(owner, repo)
    _validate_state(state)
    items = _get(f"/repos/{owner}/{repo}/pulls",
                 {"state": state, "per_page": _normalize_per_page(per_page, 50)})
    return _render_lines(
        items,
        f"No {state} pull requests in {owner}/{repo}.",
        lambda pull: (
            f"#{pull['number']} [{pull['state']}] {pull['title']} "
            f"({pull['head']['ref']}→{pull['base']['ref']}) {pull['html_url']}"
        ),
    )


@mcp.tool()
def get_pull_request(owner: str, repo: str, pull_number: int) -> str:
    """Get full details of a specific pull request.

    Args:
        owner: Repository owner.
        repo: Repository name.
        pull_number: Pull request number. If you are not sure the number
                     belongs to a PR, use get_issue first because GitHub
                     returns 404 for issue numbers that are not pull requests.
    """
    _validate_owner_repo(owner, repo)
    return _dump_fields(_get(f"/repos/{owner}/{repo}/pulls/{pull_number}"), _PULL_FIELDS)


@mcp.tool()
def create_pull_request(owner: str, repo: str, title: str, head: str,
                        base: str, body: str = "",
                        draft: bool = False) -> str:
    """Create a pull request.

    Args:
        owner: Repository owner.
        repo: Repository name.
        title: PR title.
        head: Branch containing the changes (e.g. 'my-feature').
        base: Branch to merge into (e.g. 'main').
        body: PR description body (Markdown).
        draft: When True, creates as a draft PR.
    """
    _validate_owner_repo(owner, repo)
    r = _post(f"/repos/{owner}/{repo}/pulls",
              {"title": title, "head": head, "base": base,
               "body": body, "draft": draft})
    return f"Pull request created: #{r['number']} {r['html_url']}"


@mcp.tool()
def list_releases(owner: str, repo: str, per_page: int = 10) -> str:
    """List releases for a repository.

    Args:
        owner: Repository owner.
        repo: Repository name.
        per_page: Results per page, 1–30 (default 10).
    """
    _validate_owner_repo(owner, repo)
    items = _get(
        f"/repos/{owner}/{repo}/releases",
        {"per_page": _normalize_per_page(per_page, 30)},
    )
    return _render_lines(
        items,
        f"No releases found for {owner}/{repo}.",
        lambda release: (
            f"{release['tag_name']} — {release['name']} "
            f"({'draft' if release['draft'] else 'pre-release' if release['prerelease'] else 'stable'}) "
            f"{release.get('published_at', release.get('created_at', '?'))}"
        ),
    )


@mcp.tool()
def list_workflow_runs(owner: str, repo: str, workflow_id: str = "",
                       status: str = "", per_page: int = 10) -> str:
    """List recent workflow runs for a repository.

    Args:
        owner: Repository owner.
        repo: Repository name.
        workflow_id: Workflow filename (e.g. 'ci.yml'),
                 '.github/workflows/<file>' path, or numeric ID.
                     Omit to list runs across all workflows.
        status: Filter — 'completed', 'in_progress', 'queued', 'waiting',
                'requested', or 'pending'. Omit for all statuses.
        per_page: Results per page, 1–50 (default 10).
    """
    _validate_owner_repo(owner, repo)
    workflow_name = _validate_workflow_id(workflow_id)
    path = f"/repos/{owner}/{repo}/actions"
    path += f"/workflows/{workflow_name}/runs" if workflow_name else "/runs"
    params: dict = {"per_page": _normalize_per_page(per_page, 50)}
    if status:
        params["status"] = status
    d = _get(path, params)
    runs = d.get("workflow_runs", [])
    return _render_lines(
        runs,
        "No workflow runs found.",
        lambda run: (
            f"#{run['id']} {run['name']} [{run['status']}/{run.get('conclusion', '—')}] "
            f"{run['head_branch']} {run['created_at']} {run['html_url']}"
        ),
    )


if __name__ == "__main__":  # pragma: no cover
    mcp.run()
