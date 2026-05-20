#!/usr/bin/env python3
"""Filesystem MCP — safe file I/O scoped to an allowed root directory.

Tools
-----
read_file        : Read text content, optionally limited to a line range.
write_file       : Write or overwrite a file with text content.
list_directory   : List directory contents (flat or recursive, with glob filter).
search_files     : Grep files for a pattern (literal or regex).
file_info        : Return size, modification time, and type for a path.
create_directory : Create a directory tree within the allowed root.
move_file        : Rename or move a file within the allowed root.
delete_file      : Delete a single file within the allowed root.

Security model
--------------
Every path argument must resolve — after symlink expansion — to a location
inside ALLOWED_ROOT.  Symlink traversal attacks and path-traversal sequences
are both caught.  No shell execution is used.  Binary files are detected and
read_file rejects them by default.

The allowed root defaults to the workspace root discovered via the nearest
parent containing a .github/ directory; it can be overridden by exporting
FS_ALLOWED_ROOT before starting the server.

Transport: stdio  |  Run: uvx --from "mcp[cli]" mcp run <this-file>
"""
from __future__ import annotations

import fnmatch
import mimetypes
import os
import re
import sys
from datetime import datetime, timezone
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

mcp = FastMCP("xanadFS")

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

_MAX_READ_BYTES = 1 << 20  # 1 MiB soft cap for read_file and search_files
_MAX_SEARCH_RESULTS = 200


def _discover_root(script_path: Path) -> Path:
    for candidate in script_path.resolve().parents:
        if (candidate / ".github").is_dir():
            return candidate
    return script_path.resolve().parents[min(3, len(script_path.resolve().parents) - 1)]


def _get_allowed_root() -> Path:
    env = os.environ.get("FS_ALLOWED_ROOT", "").strip()
    if env:
        p = Path(env).resolve()
        if not p.is_dir():
            raise RuntimeError(f"FS_ALLOWED_ROOT is not a directory: {env!r}")
        return p
    return _discover_root(Path(__file__))


ALLOWED_ROOT: Path = _get_allowed_root()

# ---------------------------------------------------------------------------
# Safety helpers
# ---------------------------------------------------------------------------


def _resolve(path_str: str) -> Path:
    """Expand and resolve *path_str*; raise ValueError if outside ALLOWED_ROOT."""
    if not path_str or "\x00" in path_str:
        raise ValueError(f"Invalid path: {path_str!r}")
    resolved = Path(path_str).expanduser().resolve()
    try:
        resolved.relative_to(ALLOWED_ROOT)
    except ValueError:
        raise ValueError(
            f"Path {path_str!r} is outside the allowed root ({ALLOWED_ROOT})."
        )
    return resolved


def _is_binary(data: bytes) -> bool:
    return b"\x00" in data[:8192]


# ---------------------------------------------------------------------------
# Tools
# ---------------------------------------------------------------------------


@mcp.tool()
def read_file(
    path: str,
    start_line: int | None = None,
    end_line: int | None = None,
    encoding: str = "utf-8",
) -> str:
    """Read a text file, optionally limited to a line range (1-based, inclusive).

    Args:
        path: Path to the file (absolute or relative to the allowed root).
        start_line: First line to return (1-based). None returns from the start.
        end_line: Last line to return (1-based inclusive). None returns to the end.
        encoding: Text encoding to use (default utf-8).
    """
    p = _resolve(path)
    if not p.exists():
        raise FileNotFoundError(f"File not found: {path!r}")
    if not p.is_file():
        raise ValueError(f"Not a file: {path!r}")
    raw = p.read_bytes()
    if len(raw) > _MAX_READ_BYTES:
        raise ValueError(
            f"File exceeds the {_MAX_READ_BYTES // 1024} KiB read limit. "
            "Use start_line / end_line to read a section."
        )
    if _is_binary(raw):
        raise ValueError(
            f"File appears to be binary: {path!r}. "
            "Only text files are supported by read_file."
        )
    text = raw.decode(encoding, errors="replace")
    if start_line is None and end_line is None:
        return text
    lines = text.splitlines(keepends=True)
    sl = (start_line - 1) if start_line is not None else 0
    el = end_line if end_line is not None else len(lines)
    if sl < 0:
        raise ValueError(f"start_line must be ≥ 1 (got {start_line}).")
    return "".join(lines[sl:el])


@mcp.tool()
def write_file(
    path: str,
    content: str,
    encoding: str = "utf-8",
    create_dirs: bool = False,
) -> str:
    """Write text content to a file, creating or overwriting it.

    Args:
        path: Destination path within the allowed root.
        content: Text content to write.
        encoding: Text encoding to use (default utf-8).
        create_dirs: Create missing parent directories when True.
    """
    p = _resolve(path)
    if create_dirs:
        p.parent.mkdir(parents=True, exist_ok=True)
    elif not p.parent.exists():
        raise FileNotFoundError(
            f"Parent directory does not exist: {p.parent}. "
            "Pass create_dirs=True to create it automatically."
        )
    p.write_text(content, encoding=encoding)
    return f"Written {len(content)} characters to {p.relative_to(ALLOWED_ROOT)}"


@mcp.tool()
def list_directory(
    path: str = "",
    recursive: bool = False,
    pattern: str = "*",
) -> str:
    """List the contents of a directory.

    Args:
        path: Directory to list; defaults to the allowed root when empty.
        recursive: List subdirectories recursively when True.
        pattern: Glob pattern to filter entries (e.g. '*.py').
    """
    p = _resolve(path) if path.strip() else ALLOWED_ROOT
    if not p.exists():
        raise FileNotFoundError(f"Directory not found: {path!r}")
    if not p.is_dir():
        raise ValueError(f"Not a directory: {path!r}")
    glob_fn = p.rglob if recursive else p.glob
    entries = sorted(glob_fn(pattern))
    if not entries:
        return "(no entries matching pattern)"
    lines = [
        f"{entry.relative_to(p)}{'/' if entry.is_dir() else ''}"
        for entry in entries
    ]
    return "\n".join(lines)


@mcp.tool()
def search_files(
    path: str = "",
    pattern: str = "",
    include_pattern: str = "*.py *.md *.json *.yaml *.yml *.txt",
    use_regex: bool = False,
    max_results: int = 50,
) -> str:
    """Search for a pattern across files under a directory.

    Args:
        path: Root directory to search; defaults to the allowed root when empty.
        pattern: Text or regex pattern to look for in file contents.
        include_pattern: Space-separated glob patterns limiting which files to read.
        use_regex: Treat pattern as a Python regular expression when True.
        max_results: Maximum number of matching lines to return (capped at 200).
    """
    root = _resolve(path) if path.strip() else ALLOWED_ROOT
    if not root.is_dir():
        raise ValueError(f"Not a directory: {path!r}")
    if not pattern:
        raise ValueError("pattern must not be empty.")
    max_results = min(max_results, _MAX_SEARCH_RESULTS)
    globs = include_pattern.split()
    match_fn = (re.compile(pattern).search if use_regex
                else re.compile(re.escape(pattern)).search)

    results: list[str] = []
    for candidate in sorted(root.rglob("*")):
        if not candidate.is_file():
            continue
        if not any(fnmatch.fnmatch(candidate.name, g) for g in globs):
            continue
        if candidate.stat().st_size > _MAX_READ_BYTES:
            continue
        try:
            text = candidate.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for lineno, line in enumerate(text.splitlines(), start=1):
            if match_fn(line):
                results.append(f"{candidate.relative_to(root)}:{lineno}: {line.rstrip()}")
                if len(results) >= max_results:
                    results.append(f"(capped at {max_results} results — refine your pattern)")
                    return "\n".join(results)
    return "\n".join(results) if results else "(no matches found)"


@mcp.tool()
def file_info(path: str) -> str:
    """Return size, modification time, and type metadata for a path.

    Args:
        path: Absolute or relative path to inspect.
    """
    p = _resolve(path)
    if not p.exists():
        raise FileNotFoundError(f"Path not found: {path!r}")
    stat = p.stat()
    mtime = datetime.fromtimestamp(stat.st_mtime, tz=timezone.utc).isoformat()
    mime, _ = mimetypes.guess_type(str(p))
    lines = [
        f"path:     {p.relative_to(ALLOWED_ROOT)}",
        f"type:     {'directory' if p.is_dir() else 'file'}",
        f"size:     {stat.st_size} bytes",
        f"modified: {mtime}",
    ]
    if mime:
        lines.append(f"mime:     {mime}")
    return "\n".join(lines)


@mcp.tool()
def create_directory(path: str) -> str:
    """Create a directory (and any missing parents) within the allowed root.

    Args:
        path: Directory path to create.
    """
    p = _resolve(path)
    p.mkdir(parents=True, exist_ok=True)
    return f"Directory ready: {p.relative_to(ALLOWED_ROOT)}"


@mcp.tool()
def move_file(source: str, destination: str) -> str:
    """Rename or move a file within the allowed root.

    Args:
        source: Current path of the file.
        destination: Target path; its parent directory must already exist.
    """
    src = _resolve(source)
    dst = _resolve(destination)
    if not src.exists():
        raise FileNotFoundError(f"Source not found: {source!r}")
    if not dst.parent.exists():
        raise FileNotFoundError(
            f"Destination parent does not exist: {dst.parent}. "
            "Create it first with create_directory."
        )
    src.rename(dst)
    return f"Moved {src.relative_to(ALLOWED_ROOT)} → {dst.relative_to(ALLOWED_ROOT)}"


@mcp.tool()
def delete_file(path: str) -> str:
    """Delete a single file within the allowed root.  Directories are not accepted.

    Args:
        path: Path to the file to delete.
    """
    p = _resolve(path)
    if not p.exists():
        raise FileNotFoundError(f"File not found: {path!r}")
    if p.is_dir():
        raise ValueError(
            f"Path is a directory: {path!r}. "
            "delete_file only removes files; use a dedicated directory-removal tool."
        )
    p.unlink()
    return f"Deleted: {p.relative_to(ALLOWED_ROOT)}"


if __name__ == "__main__":
    mcp.run()
