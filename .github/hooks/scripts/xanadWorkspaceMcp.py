#!/usr/bin/env python3
from __future__ import annotations
import json
import os
import shlex
import subprocess
import sys
from pathlib import Path
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).parent))
from _xanad_mcp_source import parse_github_source, resolve_github_release, resolve_github_ref
PROTOCOL_VERSION = "2025-11-25"
SERVER_NAME = "xanadTools"
SERVER_VERSION = "0.1.0"
DEFAULT_CACHE_ROOT = Path.home() / ".xanadAssistant" / "pkg-cache"
WORKSPACE_ROOT_UNAVAILABLE = "The MCP server is not installed in a workspace root."
WORKSPACE_ROOT = Path(__file__).resolve().parents[3]
WORKSPACE_INSTRUCTIONS_PATH = WORKSPACE_ROOT / ".github" / "copilot-instructions.md"
_NEW_LOCKFILE = WORKSPACE_ROOT / ".github" / "xanadAssistant-lock.json"
_LEGACY_LOCKFILE = WORKSPACE_ROOT / ".github" / "xanad-assistant-lock.json"
WORKSPACE_LOCKFILE_PATH = _NEW_LOCKFILE if _NEW_LOCKFILE.exists() else _LEGACY_LOCKFILE
SHELL_METACHARACTERS = ["|", "&", ";", ">", "<", "\n", "\r", "`", "$((", "$(", "${"]
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
def resolve_key_command(label: str) -> str | None:
    return next((entry["command"] for entry in parse_key_commands(WORKSPACE_INSTRUCTIONS_PATH) if entry["label"] == label), None)
def read_lockfile() -> dict | None:
    try:
        return json.loads(WORKSPACE_LOCKFILE_PATH.read_text(encoding="utf-8")) if WORKSPACE_LOCKFILE_PATH.exists() else None
    except json.JSONDecodeError:
        return None
def resolve_lifecycle_package_root(package_root_arg: object | None, source_arg: object | None = None, version_arg: object | None = None, ref_arg: object | None = None) -> tuple[Path | None, str | None]:
    lockfile = read_lockfile()
    package_block = lockfile.get("package") if isinstance(lockfile, dict) else None
    if package_root_arg is not None:
        if not isinstance(package_root_arg, str) or not package_root_arg.strip():
            return None, "packageRoot must be a non-empty string when provided."
        package_root = Path(package_root_arg).expanduser().resolve()
    else:
        package_root_value = package_block.get("packageRoot") if isinstance(package_block, dict) else None
        if isinstance(package_root_value, str) and package_root_value.strip():
            package_root = Path(package_root_value).expanduser().resolve()
        else:
            package_root = None
    if package_root is not None and package_root.exists():
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
def build_unavailable_result(summary: str, **fields: object) -> dict:
    return {"status": "unavailable", "summary": summary, **fields}
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
def _lifecycle_kwargs(arguments: dict) -> dict[str, object | None]: return {"package_root_arg": arguments.get("packageRoot"), "source_arg": arguments.get("source"), "version_arg": arguments.get("version"), "ref_arg": arguments.get("ref")}
LIFECYCLE_MODE_VALUES = ["setup", "update", "repair", "factory-restore"]
LIFECYCLE_SOURCE_PROPERTIES = {
    "packageRoot": {"type": "string"},
    "source": {"type": "string"},
    "version": {"type": "string"},
    "ref": {"type": "string"},
}
def _lifecycle_input_schema(extra_properties: dict[str, dict] | None = None) -> dict:
    properties = dict(LIFECYCLE_SOURCE_PROPERTIES)
    if extra_properties: properties.update(extra_properties)
    return {"type": "object", "properties": properties, "additionalProperties": False}
def _build_lifecycle_handler(cli_command: str, *, fixed_mode: str | None = None, allow_mode: bool = False, mode_as_flag: bool = False, allow_answers: bool = False, allow_non_interactive: bool = False, allow_dry_run: bool = False):
    def handler(arguments: dict) -> dict:
        kwargs: dict[str, object | None] = dict(_lifecycle_kwargs(arguments))
        if allow_mode:
            mode = arguments.get("mode", "setup")
            if mode not in set(LIFECYCLE_MODE_VALUES):
                return build_tool_result(status="unavailable", summary="mode must be one of setup, update, repair, or factory-restore.")
            kwargs["mode"] = mode
            kwargs["mode_as_flag"] = mode_as_flag
        elif fixed_mode is not None:
            kwargs["mode"] = fixed_mode
        if allow_answers:
            kwargs["answers_path"] = arguments.get("answersPath")
        if allow_non_interactive:
            kwargs["non_interactive"] = arguments.get("nonInteractive")
        if allow_dry_run:
            kwargs["dry_run"] = arguments.get("dryRun")
        return run_lifecycle_command(cli_command, **kwargs)
    return handler
EMPTY_INPUT_SCHEMA = {"type": "object", "properties": {}, "additionalProperties": False}
WORKSPACE_RUN_TESTS_INPUT_SCHEMA = {"type": "object", "properties": {"scope": {"type": "string", "enum": ["default", "full"]}, "extraArgs": {"type": "array", "items": {"type": "string"}}}, "additionalProperties": False}
def _tool_spec(title: str, description: str, input_schema: dict, handler) -> dict: return {"title": title, "description": description, "inputSchema": input_schema, "handler": handler}
def run_lifecycle_command(cli_command: str, *, package_root_arg: object | None = None, source_arg: object | None = None, version_arg: object | None = None, ref_arg: object | None = None, mode: str | None = None, mode_as_flag: bool = False, answers_path: object | None = None, non_interactive: object | None = None, dry_run: object | None = None) -> dict:
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
    if answers_path is not None:
        if not (isinstance(answers_path, str) and answers_path.strip() and Path(answers_path).is_file()):
            return build_tool_result(status="unavailable", summary="answersPath must be a non-empty string pointing to an existing file.")
        argv.extend(["--answers", answers_path])
    for name, value, flag in (("nonInteractive", non_interactive, "--non-interactive"), ("dryRun", dry_run, "--dry-run")):
        if value is True:
            argv.append(flag)
        elif value not in {None, False}:
            return build_tool_result(status="unavailable", summary=f"{name} must be a boolean when provided.")
    return run_argv(argv, parse_payload=True)
def tool_workspace_show_key_commands(arguments: dict) -> dict:
    del arguments
    if not workspace_root_valid():
        return build_unavailable_result(WORKSPACE_ROOT_UNAVAILABLE, commands=[])
    commands = parse_key_commands(WORKSPACE_INSTRUCTIONS_PATH)
    if not commands:
        return build_unavailable_result("No key commands were found in .github/copilot-instructions.md.", commands=[])
    return {"status": "ok", "commands": commands, "summary": f"Discovered {len(commands)} key command(s)."}
def tool_workspace_run_tests(arguments: dict) -> dict:
    if not workspace_root_valid():
        return build_unavailable_result(WORKSPACE_ROOT_UNAVAILABLE)
    scope = arguments.get("scope", "default")
    extra_args = arguments.get("extraArgs", [])
    if not isinstance(extra_args, list) or not all(isinstance(arg, str) for arg in extra_args):
        return build_unavailable_result("extraArgs must be a string array.")
    if scope == "full" and extra_args:
        return build_unavailable_result(summary="scope=full runs the declared test command exactly and does not accept extraArgs.")
    command = resolve_key_command("Run tests")
    if command is None:
        return build_unavailable_result("No Run tests command is declared in .github/copilot-instructions.md.")
    reason = reject_shell_metacharacters(command)
    if reason is not None:
        return build_unavailable_result(reason, command=command)
    argv = shlex.split(command) if scope == "full" else shlex.split(command) + extra_args
    return run_argv(argv)
def tool_workspace_run_check_loc(arguments: dict) -> dict:
    del arguments
    if not workspace_root_valid():
        return build_unavailable_result(WORKSPACE_ROOT_UNAVAILABLE)
    command = resolve_key_command("LOC gate")
    if command is None:
        return build_unavailable_result("No LOC gate command is declared in .github/copilot-instructions.md.")
    return run_argv(shlex.split(command))
def tool_workspace_validate_lockfile(arguments: dict) -> dict:
    del arguments
    if not workspace_root_valid():
        return build_unavailable_result(WORKSPACE_ROOT_UNAVAILABLE)
    lockfile = read_lockfile()
    if lockfile is None:
        return build_unavailable_result("No lockfile found at .github/xanadAssistant-lock.json or it contains invalid JSON.")
    required = {"schemaVersion", "package", "manifest", "timestamps", "files"}
    missing = sorted(required - lockfile.keys())
    if missing:
        return {"status": "failed", "summary": f"Lockfile is missing required keys: {missing}.", "missingKeys": missing}
    return {"status": "ok", "summary": "Lockfile is present and structurally valid.", "lockfile": lockfile}
def tool_workspace_show_install_state(arguments: dict) -> dict:
    del arguments
    if not workspace_root_valid():
        return build_unavailable_result(WORKSPACE_ROOT_UNAVAILABLE)
    result = run_lifecycle_command("check")
    payload = result.get("payload") if result.get("status") == "ok" else None
    if payload is None:
        return result
    sub = payload.get("result", {})
    return {"status": "ok", "summary": result["summary"], "installState": sub.get("installState"), "drift": sub.get("drift")}
LIFECYCLE_TOOL_ENTRIES = {
    f"lifecycle_{name}": _tool_spec(
        title,
        f"Run xanadAssistant {command_text} for the current workspace using a local package root.",
        _lifecycle_input_schema(extra_properties),
        _build_lifecycle_handler(cli_command, **handler_kwargs),
    )
    for name, title, command_text, cli_command, extra_properties, handler_kwargs in (
        ("inspect", "Inspect Lifecycle State", "inspect", "inspect", None, {}),
        ("check", "Check Lifecycle State", "check", "check", None, {}),
        ("interview", "Interview For Lifecycle Mode", "interview", "interview", {"mode": {"type": "string", "enum": LIFECYCLE_MODE_VALUES}}, {"allow_mode": True, "mode_as_flag": True}),
        ("plan_setup", "Plan Setup", "plan setup", "plan", {"answersPath": {"type": "string"}, "nonInteractive": {"type": "boolean"}}, {"fixed_mode": "setup", "allow_answers": True, "allow_non_interactive": True}),
        ("apply", "Apply Setup", "apply", "apply", {"answersPath": {"type": "string"}, "nonInteractive": {"type": "boolean"}, "dryRun": {"type": "boolean"}}, {"allow_answers": True, "allow_non_interactive": True, "allow_dry_run": True}),
        ("update", "Update Install", "update", "update", {"answersPath": {"type": "string"}, "nonInteractive": {"type": "boolean"}, "dryRun": {"type": "boolean"}}, {"allow_answers": True, "allow_non_interactive": True, "allow_dry_run": True}),
        ("repair", "Repair Install", "repair", "repair", {"answersPath": {"type": "string"}, "nonInteractive": {"type": "boolean"}}, {"allow_answers": True, "allow_non_interactive": True}),
        ("factory_restore", "Factory Restore", "factory-restore", "factory-restore", {"nonInteractive": {"type": "boolean"}}, {"allow_non_interactive": True}),
    )
}
TOOLS = {
    "workspace_show_key_commands": _tool_spec("Show Key Commands", "Return the commands declared in .github/copilot-instructions.md.", EMPTY_INPUT_SCHEMA, tool_workspace_show_key_commands),
    "workspace_run_tests": _tool_spec("Run Workspace Tests", "Run the workspace test command declared in .github/copilot-instructions.md.", WORKSPACE_RUN_TESTS_INPUT_SCHEMA, tool_workspace_run_tests),
    "workspace_run_check_loc": _tool_spec("Run LOC Gate", "Run the repo-local LOC gate when this workspace defines one.", EMPTY_INPUT_SCHEMA, tool_workspace_run_check_loc),
    "workspace_validate_lockfile": _tool_spec("Validate Lockfile", "Check that .github/xanadAssistant-lock.json exists and contains the required top-level keys.", EMPTY_INPUT_SCHEMA, tool_workspace_validate_lockfile),
    "workspace_show_install_state": _tool_spec("Show Install State", "Return the current installState and drift summary from a lifecycle check without the full check payload.", EMPTY_INPUT_SCHEMA, tool_workspace_show_install_state),
    **LIFECYCLE_TOOL_ENTRIES,
}
def success_response(message_id: int | str, result: dict) -> dict:
    return {"jsonrpc": "2.0", "id": message_id, "result": result}
def error_response(message_id: int | str | None, code: int, message: str) -> dict:
    return {"jsonrpc": "2.0", "id": message_id, "error": {"code": code, "message": message}}
def handle_request(request: dict) -> dict | None:
    message_id = request.get("id")
    method = request.get("method")
    params = request.get("params", {})
    if method == "initialize":
        client_version = params.get("protocolVersion", PROTOCOL_VERSION)
        return success_response(message_id, {"protocolVersion": client_version, "capabilities": {"tools": {}}, "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION}})
    if method == "notifications/initialized":
        return None
    if method == "ping":
        return success_response(message_id, {})
    if method == "tools/list":
        tools = [{"name": name, "title": spec["title"], "description": spec["description"], "inputSchema": spec["inputSchema"]} for name, spec in TOOLS.items()]
        return success_response(message_id, {"tools": tools})
    if method == "tools/call":
        name = params.get("name")
        arguments = params.get("arguments", {})
        if name not in TOOLS:
            return error_response(message_id, -32602, f"Unknown tool: {name}")
        if not isinstance(arguments, dict):
            return error_response(message_id, -32602, "Tool arguments must be an object.")
        result = TOOLS[name]["handler"](arguments)
        return success_response(message_id, {"content": [{"type": "text", "text": json.dumps(result, indent=2, sort_keys=True)}], "structuredContent": result, "isError": result.get("status") == "failed"})
    if method == "prompts/list":
        return success_response(message_id, {"prompts": []})
    if method == "resources/list":
        return success_response(message_id, {"resources": []})
    if method == "resources/templates/list":
        return success_response(message_id, {"resourceTemplates": []})
    if method == "logging/setLevel":
        return success_response(message_id, {})
    if method and method.startswith("notifications/"):
        return None
    if message_id is None:
        return None
    return error_response(message_id, -32601, f"Method not found: {method}")
def read_message(stream) -> dict | None:
    while True:
        line = stream.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            continue
        return json.loads(line.decode("utf-8"))
def write_message(stream, payload: dict) -> None:
    body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    stream.write(body)
    stream.write(b"\n")
    stream.flush()
def main() -> int:  # pragma: no cover
    input_stream = sys.stdin.buffer
    output_stream = sys.stdout.buffer
    while True:
        try:
            request = read_message(input_stream)
            if request is None:
                break
            response = handle_request(request)
        except Exception as exc:  # pragma: no cover - defensive protocol guard
            response = error_response(None, -32603, f"Internal error: {exc}")
        if response is not None:
            write_message(output_stream, response)
    return 0
if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())