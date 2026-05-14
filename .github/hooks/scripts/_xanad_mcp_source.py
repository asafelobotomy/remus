"""GitHub source resolution helpers for xanadWorkspaceMcp.py.

Installed alongside xanadWorkspaceMcp.py in .github/hooks/scripts/.
"""
from __future__ import annotations
import re
import subprocess
from pathlib import Path

SAFE_GITHUB_NAME = re.compile(r"^[A-Za-z0-9._-]+$")


def parse_github_source(source: str) -> tuple[str, str]:
    if not source.startswith("github:"):
        raise ValueError(f"Unsupported source scheme: {source!r}.")
    owner, sep, repo = source[len("github:"):].partition("/")
    if not sep or not owner or not repo or "/" in repo:
        raise ValueError(f"Invalid GitHub source: {source!r}.")
    if not SAFE_GITHUB_NAME.match(owner) or not SAFE_GITHUB_NAME.match(repo):
        raise ValueError(f"GitHub owner or repo contains invalid characters in: {source!r}.")
    return owner, repo


def resolve_github_release(owner: str, repo: str, version: str, cache_root: Path) -> Path:  # pragma: no cover
    import tarfile as _tarfile
    import tempfile as _tempfile
    import urllib.request as _urllib_request
    safe_version = re.sub(r"[^A-Za-z0-9._-]", "-", version)
    cache_dir = cache_root / "github" / f"{owner}-{repo}" / f"release-{safe_version}"
    sentinel = cache_dir / ".complete"
    if sentinel.exists():
        return cache_dir
    url = f"https://github.com/{owner}/{repo}/archive/refs/tags/{version}.tar.gz"
    cache_dir.mkdir(parents=True, exist_ok=True)
    tmp_path: Path | None = None
    try:
        with _tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False) as tmp:
            tmp_path = Path(tmp.name)
        req = _urllib_request.Request(url, headers={"User-Agent": "xanadAssistant-mcp/0.1"})
        with _urllib_request.urlopen(req, timeout=60) as response:
            tmp_path.write_bytes(response.read())
        with _tarfile.open(tmp_path, "r:gz") as tar:
            for member in tar.getmembers():
                parts = Path(member.name).parts
                if len(parts) < 2:
                    continue
                rel_path = Path(*parts[1:])
                if ".." in rel_path.parts or rel_path.is_absolute():
                    continue
                dest = cache_dir / rel_path
                if member.isdir():
                    dest.mkdir(parents=True, exist_ok=True)
                elif member.isfile():
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    if (file_obj := tar.extractfile(member)) is not None:
                        dest.write_bytes(file_obj.read())
        sentinel.write_text("ok\n", encoding="utf-8")
    finally:
        if tmp_path is not None and tmp_path.exists():
            tmp_path.unlink(missing_ok=True)
    return cache_dir


def resolve_github_ref(owner: str, repo: str, ref: str, cache_root: Path) -> Path:  # pragma: no cover
    if not re.match(r"^[A-Za-z0-9._/-]+$", ref):
        raise ValueError(f"ref contains invalid characters: {ref!r}")
    safe_ref = re.sub(r"[^A-Za-z0-9._-]", "-", ref)
    cache_dir = cache_root / "github" / f"{owner}-{repo}" / f"ref-{safe_ref}"
    clone_url = f"https://github.com/{owner}/{repo}.git"
    if (cache_dir / ".git").exists():
        for argv in (["git", "-C", str(cache_dir), "fetch", "--depth", "1", "origin", ref], ["git", "-C", str(cache_dir), "checkout", "FETCH_HEAD"]):
            subprocess.run(argv, check=True, capture_output=True)
        return cache_dir
    cache_dir.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "clone", "--depth", "1", "--branch", ref, clone_url, str(cache_dir)], check=True, capture_output=True)
    return cache_dir
