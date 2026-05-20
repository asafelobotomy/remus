#!/usr/bin/env python3
from __future__ import annotations
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path
from pydantic import BaseModel, ConfigDict
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).parent))
from _xanad_mcp_source import parse_github_source, resolve_github_release, resolve_github_ref
from mcp.server.fastmcp import FastMCP
DEFAULT_CACHE_ROOT = Path.home() / ".xanadAssistant" / "pkg-cache"
WORKSPACE_ROOT_UNAVAILABLE = "The MCP server is not installed in a workspace root."


def discover_workspace_root(script_path: Path) -> Path:
    resolved = script_path.resolve()
    for candidate in resolved.parents:
        if (candidate / ".github").is_dir():
            return candidate
    fallback_index = min(3, len(resolved.parents) - 1)
    return resolved.parents[fallback_index]


WORKSPACE_ROOT = discover_workspace_root(Path(__file__))
WORKSPACE_INSTRUCTIONS_PATH = WORKSPACE_ROOT / ".github" / "copilot-instructions.md"
_NEW_LOCKFILE = WORKSPACE_ROOT / ".github" / "xanadAssistant-lock.json"
_LEGACY_LOCKFILE = WORKSPACE_ROOT / ".github" / "xanad-assistant-lock.json"
WORKSPACE_LOCKFILE_PATH = _NEW_LOCKFILE if _NEW_LOCKFILE.exists() else _LEGACY_LOCKFILE
SHELL_METACHARACTERS = ["|", "&", ";", ">", "<", "\n", "\r", "`", "$((", "$(", "${"]
UNRESOLVED_COMMAND_VALUES = frozenset({"(not detected)", "not detected"})
DEFAULT_KEY_COMMANDS = {"LOC gate": "python3 scripts/check_loc.py"}
_ALLOWED_RUNNER_EXECUTABLES = frozenset({
    "python", "python3", "pytest", "py.test",
    "npm", "npx", "yarn", "pnpm", "cargo", "go",
})
def _check_executable_allowed(command: str) -> str | None:
    """Return a rejection reason if the command's executable is not in the runner allowlist."""
    try:
        argv = shlex.split(command)
    except ValueError:
        return "command could not be parsed"
    if not argv:
        return "command is empty"
    executable = Path(argv[0]).name
    if executable in _ALLOWED_RUNNER_EXECUTABLES:
        return None
    # Allow version-suffixed Python interpreters (e.g. python3.11, python3.12)
    prefix, _, suffix = executable.partition(".")
    if prefix in {"python", "python3"} and suffix.isdigit():
        return None
    return f"executable {executable!r} is not in the permitted runner allowlist"
def workspace_root_valid() -> bool: return (WORKSPACE_ROOT / ".github").is_dir()
def parse_key_commands(instructions_path: Path) -> list[dict[str, str]]:
    if not instructions_path.exists():
        return []
    lines = instructions_path.read_text(encoding="utf-8").splitlines()
    in_key_commands = False
    commands: list[dict[str, str]] = []
    for line in lines:
        if line.strip() == "## Key Commands":
            in_key_commands = True
            continue
        if in_key_commands and line.startswith("## "):
            break
        if not in_key_commands or not line.startswith("|"):
            continue
        columns = [part.strip() for part in line.strip().strip("|").split("|")]
        if len(columns) != 2 or columns[0] == "Task" or set(columns[0]) == {"-"}:
            continue
        commands.append({"label": columns[0], "command": columns[1].strip("`")})
    return commands
def is_unresolved_command(command: str | None) -> bool:
    return command is None or not command.strip() or command.strip().lower() in UNRESOLVED_COMMAND_VALUES
def resolve_key_command(label: str) -> str | None:
    command = next((entry["command"] for entry in parse_key_commands(WORKSPACE_INSTRUCTIONS_PATH) if entry["label"] == label), None)
    if not is_unresolved_command(command):
        return command
    fallback = DEFAULT_KEY_COMMANDS.get(label)
    return None if is_unresolved_command(fallback) else fallback
def prefer_workspace_python(command: str) -> str:
    try:
        argv = shlex.split(command)
    except ValueError:
        return command
    if not argv:
        return command
    executable = Path(argv[0]).name
    if executable not in {"python", "python3"}:
        prefix, _, suffix = executable.partition(".")
        if prefix not in {"python", "python3"} or not suffix.isdigit():
            return command
    workspace_python = WORKSPACE_ROOT / ".venv" / "bin" / "python"
    if not workspace_python.is_file():
        return command
    argv[0] = str(workspace_python)
    return shlex.join(argv)
def read_lockfile() -> dict | None:
    if not WORKSPACE_LOCKFILE_PATH.exists():
        return None
    try:
        return json.loads(WORKSPACE_LOCKFILE_PATH.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError, ValueError):
        return None
def resolve_lifecycle_package_root(package_root_arg: object | None, source_arg: object | None = None, version_arg: object | None = None, ref_arg: object | None = None) -> tuple[Path | None, str | None]:
    lockfile = read_lockfile()
    package_block = lockfile.get("package") if isinstance(lockfile, dict) else None
    if package_root_arg is not None:
        if not isinstance(package_root_arg, str) or not package_root_arg.strip():
            return None, "packageRoot must be a non-empty string when provided."
        package_root = Path(package_root_arg).expanduser().resolve()
        if not package_root.exists():
            return None, f"Explicit packageRoot does not exist: {package_root_arg}"
        if not package_root.is_dir():
            return None, f"Explicit packageRoot is not a directory: {package_root_arg}"
    else:
        package_root_value = package_block.get("packageRoot") if isinstance(package_block, dict) else None
        if isinstance(package_root_value, str) and package_root_value.strip():
            package_root = Path(package_root_value).expanduser().resolve()
        else:
            package_root = None
    if package_root is not None and package_root.exists():
        if not package_root.is_dir():
            return None, f"packageRoot resolved from lockfile is not a directory: {package_root}"
        return package_root, None
    resolved_source = source_arg
    resolved_version = version_arg
    resolved_ref = ref_arg
    if resolved_source is None and isinstance(package_block, dict):
        resolved_source = package_block.get("source")
        if resolved_version is None:
            resolved_version = package_block.get("version") or package_block.get("release")
        if resolved_ref is None:
            resolved_ref = package_block.get("ref")
    if resolved_source is None:
        return None, "Lifecycle MCP requires an explicit packageRoot, an installed lockfile with package.packageRoot, or a GitHub source contract."
    for name, value in (("source", resolved_source), ("version", resolved_version), ("ref", resolved_ref)):
        if value is not None and (not isinstance(value, str) or not value.strip()):
            return None, f"{name} must be a non-empty string when provided."
    try:
        owner, repo = parse_github_source(resolved_source)
        cache_root = Path(os.environ.get("XANAD_PKG_CACHE", DEFAULT_CACHE_ROOT)).expanduser().resolve()
        if isinstance(resolved_version, str) and resolved_version.strip():
            return resolve_github_release(owner, repo, resolved_version, cache_root), None
        resolved_ref_value = resolved_ref if isinstance(resolved_ref, str) and resolved_ref.strip() else "main"
        return resolve_github_ref(owner, repo, resolved_ref_value, cache_root), None
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        return None, f"Failed to resolve remote lifecycle source: {exc}"
def resolve_lifecycle_cli(package_root: Path) -> tuple[list[str] | None, str | None]:
    for path in (package_root / "xanadAssistant.py", package_root / "scripts" / "lifecycle" / "xanadAssistant.py"):
        if path.exists():
            return [sys.executable, str(path)], None
    return None, f"No xanadAssistant CLI entrypoint was found under {package_root}"
def reject_shell_metacharacters(command: str) -> str | None:
    for token in SHELL_METACHARACTERS:
        if token in command:
            return f"command contains unsupported shell syntax: {token}"
    return None
def tail_text(text: str, *, max_lines: int = 20, max_chars: int = 4000) -> str | None:
    if not text:
        return None
    tail = "\n".join(text.splitlines()[-max_lines:])
    if len(tail) > max_chars:
        tail = tail[-max_chars:]
    return tail
def build_tool_result(*, status: str, summary: str, command: str | None = None, exit_code: int | None = None, stdout: str = "", stderr: str = "") -> dict:
    result = {"status": status, "summary": summary}
    if command is not None:
        result["command"] = command
    if exit_code is not None:
        result["exitCode"] = exit_code
    for key, value in (("stdoutTail", tail_text(stdout)), ("stderrTail", tail_text(stderr))):
        if value is not None:
            result[key] = value
    return result
def _parse_json_payload(stdout: str) -> dict | None:
    if not (stdout_text := stdout.strip()):
        return None
    try:
        payload = json.loads(stdout_text)
    except json.JSONDecodeError:
        return None
    return payload if isinstance(payload, dict) else None
def run_argv(argv: list[str], *, parse_payload: bool = False) -> dict:
    completed = subprocess.run(argv, cwd=WORKSPACE_ROOT, capture_output=True, text=True, check=False)
    payload = _parse_json_payload(completed.stdout) if parse_payload else None
    if payload is not None and payload.get("status") != "error":
        status = "ok"
        summary = f"Lifecycle command {payload.get('command', 'unknown')} completed."
    else:
        status = "failed" if completed.returncode != 0 or (payload is not None and payload.get("status") == "error") else "ok"
        if parse_payload:
            summary = "Lifecycle command completed successfully." if status == "ok" else "Lifecycle command failed."
        else:
            summary = "Command completed successfully." if status == "ok" else "Command failed."
    result = build_tool_result(status=status, summary=summary, command=shlex.join(argv), exit_code=completed.returncode, stdout=completed.stdout, stderr=completed.stderr)
    if payload is not None:
        result["payload"] = payload
    return result


def _resolve_workspace_file(path_value: object, argument_name: str) -> tuple[Path | None, dict | None]:
    if not (isinstance(path_value, str) and path_value.strip()):
        return None, build_tool_result(
            status="unavailable",
            summary=f"{argument_name} must be a non-empty string pointing to an existing file.",
        )
    candidate = Path(path_value).expanduser()
    if not candidate.is_absolute():
        candidate = WORKSPACE_ROOT / candidate
    resolved = candidate.resolve()
    if not resolved.is_file():
        return None, build_tool_result(
            status="unavailable",
            summary=f"{argument_name} must be a non-empty string pointing to an existing file.",
        )
    if not resolved.is_relative_to(WORKSPACE_ROOT):
        return None, build_tool_result(
            status="unavailable",
            summary=f"{argument_name} must be a path within the workspace root.",
        )
    return resolved, None


def run_lifecycle_command(cli_command: str, *, package_root_arg: object | None = None, source_arg: object | None = None, version_arg: object | None = None, ref_arg: object | None = None, mode: str | None = None, mode_as_flag: bool = False, answers_path: object | None = None, non_interactive: object | None = None, dry_run: object | None = None, plan_path: object | None = None) -> dict:
    if not workspace_root_valid():
        return build_tool_result(status="unavailable", summary="The MCP server is not installed in a workspace root.")
    package_root, reason = resolve_lifecycle_package_root(package_root_arg, source_arg, version_arg, ref_arg)
    if reason is not None or package_root is None:
        return build_tool_result(status="unavailable", summary=reason or "Lifecycle package root could not be resolved.")
    cli_prefix, cli_reason = resolve_lifecycle_cli(package_root)
    if cli_reason is not None or cli_prefix is None:
        return build_tool_result(status="unavailable", summary=cli_reason or "Lifecycle CLI is unavailable.")
    argv = list(cli_prefix) + [cli_command]
    if mode is not None:
        argv.extend(["--mode", mode] if mode_as_flag else [mode])
    argv.extend(["--workspace", str(WORKSPACE_ROOT), "--package-root", str(package_root), "--json"])
    if plan_path is not None:
        resolved_plan, error = _resolve_workspace_file(plan_path, "planPath")
        if error is not None or resolved_plan is None:
            return error or build_tool_result(status="unavailable", summary="planPath must be a non-empty string pointing to an existing file.")
        argv.extend(["--plan", str(resolved_plan)])
    if answers_path is not None:
        resolved_answers, error = _resolve_workspace_file(answers_path, "answersPath")
        if error is not None or resolved_answers is None:
            return error or build_tool_result(status="unavailable", summary="answersPath must be a non-empty string pointing to an existing file.")
        argv.extend(["--answers", str(resolved_answers)])
    for name, value, flag in (("nonInteractive", non_interactive, "--non-interactive"), ("dryRun", dry_run, "--dry-run")):
        if value is True:
            argv.append(flag)
        elif value not in {None, False}:
            return build_tool_result(status="unavailable", summary=f"{name} must be a boolean when provided.")
    return run_argv(argv, parse_payload=True)
def tool_workspace_show_key_commands(arguments: dict) -> dict:
    del arguments
    if not workspace_root_valid():
        return {"status": "unavailable", "summary": WORKSPACE_ROOT_UNAVAILABLE, "commands": []}
    commands = parse_key_commands(WORKSPACE_INSTRUCTIONS_PATH)
    if not commands:
        return {"status": "unavailable", "summary": "No key commands were found in .github/copilot-instructions.md.", "commands": []}
    return {"status": "ok", "commands": commands, "summary": f"Discovered {len(commands)} key command(s)."}
def tool_workspace_run_tests(arguments: dict) -> dict:
    if not workspace_root_valid():
        return build_tool_result(status="unavailable", summary=WORKSPACE_ROOT_UNAVAILABLE)
    scope = arguments.get("scope", "default")
    extra_args = arguments.get("extraArgs", [])
    if not isinstance(extra_args, list) or not all(isinstance(arg, str) for arg in extra_args):
        return build_tool_result(status="unavailable", summary="extraArgs must be a string array.")
    if scope == "full" and extra_args:
        return build_tool_result(status="unavailable", summary="scope=full runs the declared test command exactly and does not accept extraArgs.")
    command = resolve_key_command("Run tests")
    if command is None:
        return build_tool_result(status="unavailable", summary="No Run tests command is declared in .github/copilot-instructions.md.")
    command = prefer_workspace_python(command)
    reason = reject_shell_metacharacters(command)
    if reason is not None:
        return {"status": "unavailable", "summary": reason, "command": command}
    reason = _check_executable_allowed(command)
    if reason is not None:
        return build_tool_result(status="unavailable", summary=reason)
    argv = shlex.split(command) if scope == "full" else shlex.split(command) + extra_args
    return run_argv(argv)
def tool_workspace_run_check_loc(arguments: dict) -> dict:
    del arguments
    if not workspace_root_valid():
        return build_tool_result(status="unavailable", summary=WORKSPACE_ROOT_UNAVAILABLE)
    command = resolve_key_command("LOC gate")
    if command is None:
        return build_tool_result(status="unavailable", summary="No LOC gate command is declared in .github/copilot-instructions.md.")
    command = prefer_workspace_python(command)
    reason = reject_shell_metacharacters(command)
    if reason is not None:
        return {"status": "unavailable", "summary": reason, "command": command}
    reason = _check_executable_allowed(command)
    if reason is not None:
        return build_tool_result(status="unavailable", summary=reason)
    return run_argv(shlex.split(command))
def tool_workspace_validate_lockfile(arguments: dict) -> dict:
    del arguments
    if not workspace_root_valid():
        return build_tool_result(status="unavailable", summary=WORKSPACE_ROOT_UNAVAILABLE)
    lockfile = read_lockfile()
    if lockfile is None:
        return build_tool_result(status="unavailable", summary="No lockfile found at .github/xanadAssistant-lock.json or it contains invalid JSON.")
    required = {"schemaVersion", "package", "manifest", "timestamps", "files"}
    missing = sorted(required - lockfile.keys())
    if missing:
        return {"status": "failed", "summary": f"Lockfile is missing required keys: {missing}.", "missingKeys": missing}
    return {"status": "ok", "summary": "Lockfile is present and structurally valid.", "lockfile": lockfile}
def tool_workspace_show_install_state(arguments: dict) -> dict:
    del arguments
    if not workspace_root_valid():
        return build_tool_result(status="unavailable", summary=WORKSPACE_ROOT_UNAVAILABLE)
    result = run_lifecycle_command("check")
    payload = result.get("payload") if result.get("status") == "ok" else None
    if payload is None:
        return result
    sub = payload.get("result", {})
    return {"status": "ok", "summary": result["summary"], "installState": sub.get("installState"), "drift": payload.get("status")}
class ToolResult(BaseModel):
    model_config = ConfigDict(extra="allow")
def _tool_result(payload: dict) -> ToolResult: return ToolResult.model_validate(payload)
def _lifecycle_tool(command: str, **kwargs: object) -> ToolResult: return _tool_result(run_lifecycle_command(command, **kwargs))

mcp = FastMCP("xanadTools")

@mcp.tool()
def workspace_show_key_commands() -> ToolResult:
    """List the workspace key commands declared in .github/copilot-instructions.md."""
    return _tool_result(tool_workspace_show_key_commands({}))

@mcp.tool()
def workspace_run_tests(scope: str = "default", extraArgs: list[str] | None = None) -> ToolResult:
    """Run the workspace's declared test command, optionally appending extra arguments."""
    return _tool_result(tool_workspace_run_tests({"scope": scope, "extraArgs": extraArgs or []}))

@mcp.tool()
def workspace_run_check_loc() -> ToolResult:
    """Run the workspace LOC gate command using the declared or default check_loc entry."""
    return _tool_result(tool_workspace_run_check_loc({}))

@mcp.tool()
def workspace_validate_lockfile() -> ToolResult:
    """Validate that the xanadAssistant workspace lockfile is present and structurally sound."""
    return _tool_result(tool_workspace_validate_lockfile({}))

@mcp.tool()
def workspace_show_install_state() -> ToolResult:
    """Show the current install and drift state for this workspace's managed surfaces."""
    return _tool_result(tool_workspace_show_install_state({}))

_LIFECYCLE_MODES, _INVALID_MODE_MSG = frozenset(["setup", "update", "repair", "factory-restore"]), "mode must be one of setup, update, repair, or factory-restore."

@mcp.tool()
def lifecycle_inspect(packageRoot: str | None = None, source: str | None = None, version: str | None = None, ref: str | None = None) -> ToolResult:
    """Inspect the target workspace and report install state, findings, and repair reasons."""
    return _lifecycle_tool("inspect", package_root_arg=packageRoot, source_arg=source, version_arg=version, ref_arg=ref)

@mcp.tool()
def lifecycle_check(packageRoot: str | None = None, source: str | None = None, version: str | None = None, ref: str | None = None) -> ToolResult:
    """Check managed files in the target workspace and classify drift or damage."""
    return _lifecycle_tool("check", package_root_arg=packageRoot, source_arg=source, version_arg=version, ref_arg=ref)

@mcp.tool()
def lifecycle_interview(packageRoot: str | None = None, source: str | None = None, version: str | None = None, ref: str | None = None, mode: str = "setup") -> ToolResult:
    """Emit the structured interview questions needed for a lifecycle mode."""
    if mode not in _LIFECYCLE_MODES:
        return _tool_result(build_tool_result(status="unavailable", summary=_INVALID_MODE_MSG))
    return _lifecycle_tool("interview", package_root_arg=packageRoot, source_arg=source, version_arg=version, ref_arg=ref, mode=mode, mode_as_flag=True)

@mcp.tool()
def lifecycle_plan_setup(packageRoot: str | None = None, source: str | None = None, version: str | None = None, ref: str | None = None, answersPath: str | None = None, nonInteractive: bool | None = None) -> ToolResult:
    """Plan a first-time setup without writing files to the target workspace."""
    return _lifecycle_tool("plan", package_root_arg=packageRoot, source_arg=source, version_arg=version, ref_arg=ref, mode="setup", answers_path=answersPath, non_interactive=nonInteractive)

@mcp.tool()
def lifecycle_setup(packageRoot: str | None = None, source: str | None = None, version: str | None = None, ref: str | None = None, planPath: str | None = None, dryRun: bool | None = None) -> ToolResult:
    """Apply a serialized setup plan to install xanadAssistant into the target workspace."""
    return _lifecycle_tool("setup", package_root_arg=packageRoot, source_arg=source, version_arg=version, ref_arg=ref, plan_path=planPath, dry_run=dryRun)

@mcp.tool()
def lifecycle_apply(packageRoot: str | None = None, source: str | None = None, version: str | None = None, ref: str | None = None, planPath: str | None = None, dryRun: bool | None = None) -> ToolResult:
    """Report that lifecycle_apply is retired and direct callers to the supported replacements."""
    del packageRoot, source, version, ref, planPath, dryRun
    return _tool_result(
        build_tool_result(
            status="unavailable",
            summary="lifecycle_apply is retired. Use lifecycle_setup for serialized setup plans, or use lifecycle_update, lifecycle_repair, or lifecycle_factory_restore for those modes.",
            command="lifecycle_apply",
        )
    )

@mcp.tool()
def lifecycle_update(packageRoot: str | None = None, source: str | None = None, version: str | None = None, ref: str | None = None, answersPath: str | None = None, nonInteractive: bool | None = None, dryRun: bool | None = None) -> ToolResult:
    """Update an installed workspace to the current package state."""
    return _lifecycle_tool("update", package_root_arg=packageRoot, source_arg=source, version_arg=version, ref_arg=ref, answers_path=answersPath, non_interactive=nonInteractive, dry_run=dryRun)

@mcp.tool()
def lifecycle_repair(packageRoot: str | None = None, source: str | None = None, version: str | None = None, ref: str | None = None, answersPath: str | None = None, nonInteractive: bool | None = None) -> ToolResult:
    """Repair a damaged or drifted xanadAssistant install in the target workspace."""
    return _lifecycle_tool("repair", package_root_arg=packageRoot, source_arg=source, version_arg=version, ref_arg=ref, answers_path=answersPath, non_interactive=nonInteractive)

@mcp.tool()
def lifecycle_factory_restore(packageRoot: str | None = None, source: str | None = None, version: str | None = None, ref: str | None = None, nonInteractive: bool | None = None) -> ToolResult:
    """Reinstall the managed surfaces from scratch using the workspace's recorded answers."""
    return _lifecycle_tool("factory-restore", package_root_arg=packageRoot, source_arg=source, version_arg=version, ref_arg=ref, non_interactive=nonInteractive)


if __name__ == "__main__":  # pragma: no cover
    mcp.run()