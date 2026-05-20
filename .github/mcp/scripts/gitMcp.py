#!/usr/bin/env python3
"""Git MCP server — full git lifecycle over subprocess.

Covers all local and remote operations in a single self-owned server:

Local:  status, diff (unstaged/staged/between refs), add, reset, commit,
        log, show, branch (list/create/delete), checkout, stash, stash-pop,
        tag, rebase
Remote: fetch, pull, push (including --force-with-lease and --set-upstream)

Security model
--------------
All flag-like behaviour (force, rebase, prune, etc.) is expressed as typed
boolean or enum parameters.  Raw user strings are NEVER interpolated into the
git argument list as flags.  The _run helper validates that no caller-supplied
string argument starts with '-' before passing it to subprocess.
Every call site also validates that repo_path resolves to an existing
directory before any subprocess call is made.

Transport: stdio  |  Run: uvx --from "mcp[cli]" mcp run <this-file>
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path
from typing import Literal

try:
    from mcp.server.fastmcp import FastMCP
except ImportError as _exc:  # pragma: no cover
    sys.stderr.write(
        "ERROR: the 'mcp' package is required but not installed.\n"
        "Install it with: pip install 'mcp[cli]'\n"
        f"Details: {_exc}\n"
    )
    sys.exit(1)

mcp = FastMCP("xanadGit")


def _reject_flag_like(arg: str) -> str:
    if arg.startswith("-"):
        raise ValueError(
            f"Argument {arg!r} looks like a flag.  "
            "Pass options through dedicated parameters, not raw strings."
        )
    return arg


def _validate_repo_path(repo_path: str) -> str:
    """Resolve repo_path and verify it is an existing directory.

    Returns the resolved absolute path string used as cwd for subprocess calls.
    Raises ValueError for empty, non-existent, or non-directory paths.
    """
    if not repo_path or not repo_path.strip():
        raise ValueError("repo_path must not be empty.")
    resolved = Path(repo_path).resolve()
    if not resolved.is_dir():
        raise ValueError(
            f"repo_path {repo_path!r} does not exist or is not a directory."
        )
    return str(resolved)


def _run_flags(repo_path: str, base_args: list[str], flags: list[str],
               tail: list[str], timeout: int = 30) -> str:
    """Run git with pre-validated literal flags and caller-supplied tail args.

    Tail args that start with '-' are rejected to prevent flag injection.
    Flags must be hardcoded literals assembled by the calling tool function.
    """
    result = _run_flags_completed(repo_path, base_args, flags, tail, timeout=timeout)
    if result.returncode != 0:
        raise RuntimeError(
            (result.stderr.strip() or result.stdout.strip())
            or f"git exited with code {result.returncode}"
        )
    return result.stdout.strip()


def _run_flags_completed(
    repo_path: str,
    base_args: list[str],
    flags: list[str],
    tail: list[str],
    timeout: int = 30,
) -> subprocess.CompletedProcess[str]:
    """Run git and return the completed process without raising on exit code."""
    repo_path = _validate_repo_path(repo_path)
    for arg in tail:
        _reject_flag_like(arg)
    cmd = ["git", *base_args, *flags, *tail]
    return subprocess.run(
        cmd,
        cwd=repo_path,
        capture_output=True,
        text=True,
        timeout=timeout,
    )


def _mutation_result(
    operation: str,
    result: subprocess.CompletedProcess[str],
    command: list[str],
    **fields: object,
) -> dict[str, object]:
    """Build a structured envelope for mutating git operations."""
    status = "ok" if result.returncode == 0 else "failed"
    envelope: dict[str, object] = {
        "ok": result.returncode == 0,
        "status": status,
        "operation": operation,
        "command": command,
        "exitCode": result.returncode,
        "stdout": result.stdout.strip(),
        "stderr": result.stderr.strip(),
    }
    envelope.update(fields)
    return envelope


def _run(repo_path: str, *args: str, timeout: int = 30) -> str:
    """Convenience wrapper: validate user args then delegate to _run_flags."""
    return _run_flags(repo_path, [_reject_flag_like(arg) for arg in args], [], [], timeout=timeout)


def _validate_user_arg(arg: str) -> str:
    """Return a user-supplied git argument after rejecting flag-like values."""
    return _reject_flag_like(arg)

@mcp.tool()
def git_status(repo_path: str) -> str:
    """Return the working tree status (porcelain v1)."""
    return _run_flags(repo_path, ["status"], ["--porcelain"], [])


@mcp.tool()
def git_diff_unstaged_stat(repo_path: str) -> str:
    """Return unstaged changes as a diff stat summary."""
    return _run_flags(repo_path, ["diff"], ["--stat"], [])


@mcp.tool()
def git_diff_staged_stat(repo_path: str) -> str:
    """Return staged changes as a diff stat summary."""
    return _run_flags(repo_path, ["diff"], ["--cached", "--stat"], [])


@mcp.tool()
def git_diff_unstaged(repo_path: str, context_lines: int = 3) -> str:
    """Return unstaged changes as a unified diff."""
    return _run_flags(repo_path, ["diff"], [f"-U{context_lines}"], [])


@mcp.tool()
def git_diff_staged(repo_path: str, context_lines: int = 3) -> str:
    """Return staged changes as a unified diff."""
    return _run_flags(repo_path, ["diff"], ["--cached", f"-U{context_lines}"], [])


@mcp.tool()
def git_diff(repo_path: str, target: str, context_lines: int = 3) -> str:
    """Return diff between HEAD and target (branch, tag, or commit SHA)."""
    return _run_flags(repo_path, ["diff"], [f"-U{context_lines}"], ["HEAD", target])


@mcp.tool()
def git_log(
    repo_path: str,
    max_count: int = 20,
    branch: str = "",
) -> str:
    """Return commit log (one-line format).

    Args:
        repo_path: Absolute path to the git repository.
        max_count: Maximum number of commits to return.
        branch: Branch or ref to log; defaults to current HEAD.
    """
    flags = [f"--max-count={max_count}", "--oneline", "--decorate"]
    return _run_flags(repo_path, ["log"], flags, [branch] if branch else [])


@mcp.tool()
def git_show(repo_path: str, revision: str) -> str:
    """Show contents of a commit or object (e.g. HEAD, a SHA, or tag)."""
    return _run(repo_path, "show", revision)


@mcp.tool()
def git_branch_list(
    repo_path: str,
    scope: Literal["local", "remote", "all"] = "local",
) -> str:
    """List branches.

    Args:
        repo_path: Absolute path to the git repository.
        scope: 'local' (default), 'remote', or 'all'.
    """
    flag_map = {"local": [], "remote": ["--remotes"], "all": ["--all"]}
    return _run_flags(repo_path, ["branch"], flag_map[scope], [])


@mcp.tool()
def git_create_branch(
    repo_path: str,
    branch_name: str,
    base_branch: str = "",
) -> str:
    """Create and switch to a new branch.

    Args:
        repo_path: Absolute path to the git repository.
        branch_name: Name for the new branch.
        base_branch: Start point (branch, tag, or SHA); defaults to HEAD.
    """
    return _run_flags(repo_path, ["checkout"], ["-b"],
                       [branch_name, base_branch] if base_branch else [branch_name])


@mcp.tool()
def git_checkout(repo_path: str, branch_name: str) -> str:
    """Switch to an existing branch or restore a file from HEAD."""
    return _run(repo_path, "checkout", branch_name)


@mcp.tool()
def git_delete_branch(
    repo_path: str,
    branch_name: str,
    force: bool = False,
) -> str:
    """Delete a local branch.

    Args:
        repo_path: Absolute path to the git repository.
        branch_name: Branch to delete.
        force: When True uses -D (force-delete unmerged); default is safe -d.
    """
    return _run_flags(repo_path, ["branch"], ["-D" if force else "-d"], [branch_name])


@mcp.tool()
def git_add(repo_path: str, files: list[str]) -> str:
    """Stage files for commit.

    Args:
        repo_path: Absolute path to the git repository.
        files: List of file paths to stage (relative to repo_path).
    """
    return _run_flags(repo_path, ["add", "--"], [], files)


@mcp.tool()
def git_reset(repo_path: str, files: list[str] | None = None) -> str:
    """Unstage staged changes.

    Args:
        repo_path: Absolute path to the git repository.
        files: Optional file paths to unstage. When omitted, unstages all
            staged changes with a mixed reset to HEAD.
    """
    if files:
        return _run_flags(repo_path, ["reset"], ["HEAD", "--"], files)
    return _run_flags(repo_path, ["reset"], ["HEAD"], [])


@mcp.tool()
def git_commit(repo_path: str, message: str) -> dict[str, object]:
    """Record staged changes as a new commit.

    The message string is passed directly to subprocess (not via a shell), so
    embedded newlines (``\\n``) are preserved as-is — multi-paragraph messages
    work without a temp file.

    Args:
        repo_path: Absolute path to the git repository.
        message: Commit message. Use a blank line between subject and body
                 for multi-paragraph messages (e.g. "subject\\n\\nbody").
    """
    if not message.strip(): raise ValueError("Commit message must not be empty.")
    command = ["git", "commit", "-m", message]
    result = _run_flags_completed(repo_path, ["commit"], ["-m", message], [])
    return _mutation_result(
        "git_commit",
        result,
        command,
        message=message,
        summary=f"git commit {'succeeded' if result.returncode == 0 else 'failed'}",
    )


@mcp.tool()
def git_stash(repo_path: str, message: str = "") -> str:
    """Stash working directory and index changes.

    Args:
        repo_path: Absolute path to the git repository.
        message: Optional description for the stash entry.
    """
    return _run_flags(repo_path, ["stash", "push"],
                       ["-m", message] if message else [], [])


@mcp.tool()
def git_stash_pop(repo_path: str) -> str:
    """Apply and remove the most recent stash entry."""
    return _run(repo_path, "stash", "pop")


@mcp.tool()
def git_stash_apply(repo_path: str, stash_ref: str = "") -> dict[str, object]:
    """Apply a stash entry without dropping it.

    Args:
        repo_path: Absolute path to the git repository.
        stash_ref: Optional stash reference such as ``stash@{1}``; defaults to
            the most recent stash when omitted.
    """
    tail = [stash_ref] if stash_ref else []
    command = ["git", "stash", "apply", *tail]
    result = _run_flags_completed(repo_path, ["stash", "apply"], [], tail)
    return _mutation_result(
        "git_stash_apply",
        result,
        command,
        stashRef=stash_ref,
        summary=f"git stash apply {'succeeded' if result.returncode == 0 else 'failed'}",
    )


@mcp.tool()
def git_stash_drop(repo_path: str, stash_ref: str = "") -> dict[str, object]:
    """Drop a stash entry.

    Args:
        repo_path: Absolute path to the git repository.
        stash_ref: Optional stash reference such as ``stash@{1}``; defaults to
            the most recent stash when omitted.
    """
    tail = [stash_ref] if stash_ref else []
    command = ["git", "stash", "drop", *tail]
    result = _run_flags_completed(repo_path, ["stash", "drop"], [], tail)
    return _mutation_result(
        "git_stash_drop",
        result,
        command,
        stashRef=stash_ref,
        summary=f"git stash drop {'succeeded' if result.returncode == 0 else 'failed'}",
    )


@mcp.tool()
def git_stash_list(repo_path: str) -> str:
    """List all stash entries."""
    return _run(repo_path, "stash", "list")


@mcp.tool()
def git_tag(
    repo_path: str,
    name: str,
    message: str = "",
    ref: str = "HEAD",
) -> str:
    """Create a tag at ref.

    Args:
        repo_path: Absolute path to the git repository.
        name: Tag name (e.g. 'v1.2.3').
        message: When provided, creates an annotated tag; otherwise lightweight.
        ref: Commit, branch, or tag to tag; defaults to HEAD.
    """
    name = _validate_user_arg(name)
    ref = _validate_user_arg(ref)
    return (_run_flags(repo_path, ["tag"], ["-a", name, "-m", message], [ref])
            if message else _run(repo_path, "tag", name, ref))


@mcp.tool()
def git_tag_list(repo_path: str) -> str:
    """List all tags."""
    return _run_flags(repo_path, ["tag"], ["--list"], [])


@mcp.tool()
def git_push_tag(
    repo_path: str,
    tag_name: str,
    remote: str = "origin",
) -> str:
    """Push exactly one tag to a remote.

    Args:
        repo_path: Absolute path to the git repository.
        tag_name: Tag name to push (e.g. 'v1.2.3').
        remote: Remote name; defaults to 'origin'.
    """
    tag_name = _validate_user_arg(tag_name)
    refspec = f"refs/tags/{tag_name}:refs/tags/{tag_name}"
    return _run_flags(repo_path, ["push"], [], [remote, refspec])


@mcp.tool()
def git_rebase(
    repo_path: str,
    onto: str = "",
    action: Literal["start", "continue", "abort", "skip"] = "start",
) -> dict[str, object]:
    """Rebase the current branch.

    Args:
        repo_path: Absolute path to the git repository.
        onto: Branch or commit to rebase onto (required when action='start').
        action: 'start' (default), 'continue', 'abort', or 'skip'.
    """
    if action == "start":
        if not onto: raise ValueError("'onto' is required when action='start'.")
        command = ["git", "rebase", onto]
        result = _run_flags_completed(repo_path, ["rebase"], [], [onto])
    else:
        command = ["git", "rebase", f"--{action}"]
        result = _run_flags_completed(repo_path, ["rebase"], [f"--{action}"], [])
    return _mutation_result(
        "git_rebase",
        result,
        command,
        action=action,
        onto=onto,
        summary=f"git rebase {action} {'succeeded' if result.returncode == 0 else 'failed'}",
    )


@mcp.tool()
def git_fetch(
    repo_path: str,
    remote: str = "origin",
    prune: bool = False,
) -> str:  # pragma: no cover
    """Fetch from a remote without merging.

    Args:
        repo_path: Absolute path to the git repository.
        remote: Remote name; defaults to 'origin'.
        prune: When True, removes remote-tracking refs that no longer exist.
    """
    return _run_flags(repo_path, ["fetch"], ["--prune"] if prune else [], [remote])


@mcp.tool()
def git_pull(
    repo_path: str,
    remote: str = "origin",
    branch: str = "",
    rebase: bool = False,
) -> dict[str, object]:  # pragma: no cover
    """Fetch and integrate changes from a remote branch.

    Args:
        repo_path: Absolute path to the git repository.
        remote: Remote name; defaults to 'origin'.  Ignored when branch is
            omitted — git uses the branch's configured upstream instead.
        branch: Remote branch to pull.  When omitted, git uses the current
            branch's configured upstream (i.e. ``git pull`` with no args).
        rebase: When True, rebases local commits on top of the fetched branch.
    """
    flags = ["--rebase"] if rebase else []
    # When no explicit branch is given, omit remote too so that git resolves
    # the tracking upstream from config rather than using FETCH_HEAD.
    tail = [remote, branch] if branch else []
    command = ["git", "pull", *flags, *tail]
    result = _run_flags_completed(repo_path, ["pull"], flags, tail)
    branch_summary = f"{remote}/{branch}" if branch else "configured upstream"
    return _mutation_result(
        "git_pull",
        result,
        command,
        remote=remote,
        branch=branch,
        rebase=rebase,
        summary=f"git pull from {branch_summary} {'succeeded' if result.returncode == 0 else 'failed'}",
    )


@mcp.tool()
def git_push(
    repo_path: str,
    remote: str = "origin",
    branch: str = "",
    force_with_lease: bool = False,
    force: bool = False,
    set_upstream: bool = False,
    tags: bool = False,
) -> dict[str, object]:  # pragma: no cover
    """Push commits to a remote repository.

    Args:
        repo_path: Absolute path to the git repository.
        remote: Remote name; defaults to 'origin'.
        branch: Branch to push.  When omitted, git resolves the target via
            push.default (typically the current branch's tracking ref).
        force_with_lease: Safer force-push — fails if the remote has new
            commits since the last fetch.  Preferred over force=True.
        force: Unconditional force-push.  Ignored when force_with_lease=True.
        set_upstream: Pass -u to set the tracking branch on first push.
        tags: Push all local tags alongside commits.
    """
    flags: list[str] = []
    if set_upstream: flags.append("-u")
    if force_with_lease: flags.append("--force-with-lease")
    elif force: flags.append("--force")
    if tags: flags.append("--tags")
    tail = [remote] + ([branch] if branch else [])
    command = ["git", "push", *flags, *tail]
    result = _run_flags_completed(repo_path, ["push"], flags, tail)
    branch_summary = branch or "current tracking branch"
    return _mutation_result(
        "git_push",
        result,
        command,
        remote=remote,
        branch=branch,
        forceWithLease=force_with_lease,
        force=force and not force_with_lease,
        setUpstream=set_upstream,
        tags=tags,
        summary=f"git push to {remote} {branch_summary} {'succeeded' if result.returncode == 0 else 'failed'}",
    )


if __name__ == "__main__":  # pragma: no cover
    mcp.run()
