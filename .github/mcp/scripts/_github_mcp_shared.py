from __future__ import annotations

import json
import os
import re
import subprocess
import urllib.error
import urllib.parse
import urllib.request
from pathlib import PurePosixPath
from typing import Any

_API = "https://api.github.com"
_SAFE_NAME = re.compile(r"^[A-Za-z0-9._-]+$")
_ALLOWED_STATES = {"open", "closed", "all"}

REPO_FIELDS = (
    "full_name",
    "description",
    "default_branch",
    "stargazers_count",
    "forks_count",
    "open_issues_count",
    "license",
    "visibility",
    "html_url",
    "pushed_at",
)
ISSUE_FIELDS = (
    "number",
    "title",
    "state",
    "body",
    "html_url",
    "user",
    "labels",
    "assignees",
    "created_at",
    "updated_at",
)
PULL_FIELDS = (
    "number",
    "title",
    "state",
    "body",
    "html_url",
    "draft",
    "head",
    "base",
    "user",
    "mergeable",
    "created_at",
    "updated_at",
)


def validate_owner_repo(owner: str, repo: str) -> None:
    if not _SAFE_NAME.match(owner):
        raise ValueError(f"Invalid owner name: {owner!r}")
    if not _SAFE_NAME.match(repo):
        raise ValueError(f"Invalid repo name: {repo!r}")


def normalize_per_page(per_page: int, maximum: int) -> int:
    return max(1, min(maximum, per_page))


def validate_state(state: str) -> None:
    if state not in _ALLOWED_STATES:
        allowed = ", ".join(sorted(_ALLOWED_STATES))
        raise ValueError(f"Invalid state: {state!r}. Expected one of: {allowed}.")


def validate_workflow_id(workflow_id: str | int) -> str:
    value = str(workflow_id).strip()
    if not value:
        return ""
    path = PurePosixPath(value)
    if len(path.parts) == 3 and path.parts[:2] == (".github", "workflows"):
        value = path.name
    if not _SAFE_NAME.match(value):
        raise ValueError(f"Invalid workflow_id: {workflow_id!r}")
    return value


def dump_fields(payload: dict[str, Any], fields: tuple[str, ...]) -> str:
    return json.dumps({field: payload.get(field) for field in fields}, indent=2)


def render_lines(items: list[Any], empty_message: str, render_item) -> str:
    if not items:
        return empty_message
    return "\n".join(render_item(item) for item in items)


def gh_cli_token() -> str:
    try:
        result = subprocess.run(
            ["gh", "auth", "token"],
            capture_output=True,
            text=True,
            timeout=5,
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
        pass
    return ""


def token() -> str:
    tok = os.environ.get("GITHUB_TOKEN", "").strip() or os.environ.get("GH_TOKEN", "").strip()
    if not tok:
        tok = gh_cli_token()
    if not tok:
        raise RuntimeError(
            "No GitHub token found. Authenticate with one of:\n"
            "  1. gh auth login  (GitHub CLI)\n"
            "  2. export GITHUB_TOKEN=<pat> in your shell profile\n"
            "Then restart VS Code."
        )
    return tok


def decode_json_response(raw_body: bytes, path: str) -> Any:
    if not raw_body.strip():
        raise RuntimeError(f"GitHub API returned an empty response for {path}.")
    try:
        return json.loads(raw_body)
    except json.JSONDecodeError as exc:
        preview = raw_body.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"GitHub API returned a non-JSON response for {path}: {preview[:200]}"
        ) from exc


def format_http_error(path: str, code: int, body_text: str) -> str:
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


def req(
    method: str,
    path: str,
    body: dict | None = None,
    params: dict | None = None,
    *,
    token_provider=token,
) -> Any:
    url = f"{_API}{path}"
    if params:
        filtered = {k: v for k, v in params.items() if v is not None}
        if filtered:
            url += "?" + urllib.parse.urlencode(filtered)
    data = json.dumps(body).encode() if body else None
    request = urllib.request.Request(
        url,
        data=data,
        method=method,
        headers={
            "Authorization": f"Bearer {token_provider()}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "Content-Type": "application/json",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return decode_json_response(response.read(), path)
    except urllib.error.HTTPError as exc:
        body_text = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(format_http_error(path, exc.code, body_text)) from exc


def get(path: str, params: dict | None = None, *, token_provider=token) -> Any:
    return req("GET", path, params=params, token_provider=token_provider)


def post(path: str, body: dict, *, token_provider=token) -> Any:
    return req("POST", path, body=body, token_provider=token_provider)