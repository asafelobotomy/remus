# Setup Prompt

Use this prompt when the user asks to install or refresh xanadAssistant in the
active repository.

Target workspace: remus
Selected profile: balanced

## Workflow

If the workspace already has the `xanadTools` MCP server connected and the
server can resolve a local xanadAssistant package root or a supported remote
source, prefer the MCP
lifecycle tools over shelling out directly:

- `lifecycle_inspect`
- `lifecycle_interview`
- `lifecycle_plan_setup`
- `lifecycle_apply`
- `lifecycle_check`

For a first-time local install, pass the checkout path as `packageRoot` to the
MCP tool call. For a remote install, pass `source` plus `version` or `ref`.
If the MCP server is unavailable or the workspace cannot resolve the requested
package source, fall back to the CLI commands below.

### 1. Inspect current state

```
python3 xanadAssistant.py inspect \
  --workspace remus \
  --package-root <path-to-xanadAssistant-checkout> \
  --ui agent --json-lines
```

Review `installState`, `manifestSummary`, and any warnings. Ask the user to
confirm the target workspace path if it is not clear from context.

When `mcp.enabled` is true, the plan should also install the local hook scripts.
Expect `.github/hooks/scripts/xanadWorkspaceMcp.py`,
`.github/hooks/scripts/mcpSequentialThinkingServer.py`, and `.vscode/mcp.json`
to appear in the planned writes.

If the warnings include `package_name_mismatch` or `successor_cleanup_required`,
treat the workspace as a predecessor `copilot-instructions-template` install.
Use `plan repair` plus `repair` so xanadAssistant can archive predecessor-owned
files and adopt the workspace cleanly.

### 2. Clarify answers if needed

If `installState` is `not-installed` or if the user wants to change pack or
profile selection, run the interview:

```
python3 xanadAssistant.py interview \
  --workspace remus \
  --package-root <path-to-xanadAssistant-checkout> \
  --mode setup --json-lines
```

Parse `result.questions`. For each question, present the `prompt` and `default`
to the user and ask whether they want to override. Create the answers directory
and write the collected answers to `.xanadAssistant/tmp/setup-answers.json`.
Include only keys the user explicitly overrides — omitted keys are filled from
their declared `default` values by the lifecycle engine.

**Answer file format:**

```json
{
  "profile.selected": "balanced",
  "packs.selected": [],
  "ownership.agents": "plugin-backed-copilot-format",
  "ownership.skills": "plugin-backed-copilot-format",
  "response.style": "balanced",
  "autonomy.level": "ask-first",
  "agent.persona": "professional",
  "testing.philosophy": "always",
  "mcp.enabled": true,
  "mcp.servers": []
}
```

If the user accepts all defaults, use `--non-interactive` and omit `--answers`.

### 2.5. Resolve pre-existing files (if any)

After the interview, inspect `result.existingFiles` in the interview output.
If the array is non-empty, present each entry to the user:

- `type: "collision"` — a file that xanadAssistant would overwrite.
  Available decisions: `keep` (preserve the user's file), `replace` (xanadAssistant wins),
  `merge` (if `mergeSupported` is true).
- `type: "unmanaged"` — a file in a managed directory that xanadAssistant does not own.
  Available decisions: `keep`, `remove`.

Collect the user's per-file decisions and write them to
`.xanadAssistant/tmp/conflict-resolutions.json` as a flat JSON object
mapping relative path → decision string:

```json
{
  ".github/prompts/my-prompt.md": "keep",
  ".github/agents/old-agent.md": "remove"
}
```

Pass `--resolutions .xanadAssistant/tmp/conflict-resolutions.json` to both
`plan setup` and `apply` below.  If `existingFiles` is empty, skip this step.

### 3. Generate a plan

```
python3 xanadAssistant.py plan setup \
  --workspace remus \
  --package-root <path-to-xanadAssistant-checkout> \
  --answers .xanadAssistant/tmp/setup-answers.json \
  --non-interactive --json-lines
```

For predecessor-managed installs, replace `plan setup`
with `plan repair`.

If `approvalRequired` is true in the plan payload, summarise the planned writes
and retired files for the user and ask for approval before proceeding.

To preview what would be written without making any changes, pass `--dry-run` to
the `apply` command instead of running `plan` first.

### 4. Apply

Once the user approves (or `approvalRequired` is false):

```
python3 xanadAssistant.py apply \
  --workspace remus \
  --package-root <path-to-xanadAssistant-checkout> \
  --answers .xanadAssistant/tmp/setup-answers.json \
  --non-interactive --ui agent --json-lines
```

For predecessor-managed installs, replace `apply` with
`repair`.

Check the `validation.status` in the apply result. If it is not `passed`,
report the error and the `backupPath` to the user.

### 5. Confirm and clean up

Show the user the Receipt phase output and the path to the generated
`.github/copilot-version.md` summary.

When MCP is available, prefer `lifecycle_check` for the final confirmation step.

After a successful apply, remove the temporary answers file:

```sh
rm -rf .xanadAssistant/tmp
```
