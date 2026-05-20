# Setup Prompt

Use this prompt when the user asks to install or refresh xanadAssistant in the
active repository.

Target workspace: remus (display name)
Selected profile: balanced

Use `.` as the workspace path in CLI examples below when running from the target repository root.

## Workflow

If the workspace already has the `xanadTools` MCP server connected and the
server can resolve a local xanadAssistant package root or a supported remote
source, prefer the MCP
lifecycle tools over shelling out directly:

- `lifecycle_inspect`
- `lifecycle_interview`
- `lifecycle_plan_setup`
- `lifecycle_setup`
- `lifecycle_check`

For a first-time local install, pass the checkout path as `packageRoot` to the
MCP tool call. For a remote install, pass `source` plus `version` or `ref`.
If the MCP server is unavailable or the workspace cannot resolve the requested
package source, fall back to the CLI commands below.

### 1. Inspect current state

```sh
python3 xanadAssistant.py inspect \
  --workspace . \
  --package-root <path-to-xanadAssistant-checkout> \
  --ui agent --json-lines
```

Review `installState`, `manifestSummary`, and any warnings. If the user's
request does not specify a workspace path or more than one workspace is open,
ask the user to confirm the target path before proceeding.

When `mcp.enabled` is true, the plan should also install the local MCP scripts.
Verify that the plan output includes entries under `.github/mcp/scripts/` plus
`.vscode/mcp.json`; at minimum, `xanadWorkspaceMcp.py`, `memoryMcp.py`, and
`sequentialThinkingMcp.py` should be present. If any are absent when
`mcp.enabled` is true, surface the discrepancy to the user before proceeding.

If the warnings include `package_name_mismatch` or `successor_cleanup_required`,
treat the workspace as a predecessor `copilot-instructions-template` install.
Use `plan repair` plus `repair` so xanadAssistant can archive predecessor-owned
files and adopt the workspace cleanly.

### 2. Clarify answers if needed

If `installState` is `not-installed` or if the user wants to change pack or
profile selection, run the interview:

```sh
python3 xanadAssistant.py interview \
  --workspace . \
  --package-root <path-to-xanadAssistant-checkout> \
  --mode setup --json-lines
```

Parse `result.questions`. For each question, present the `prompt` and `default`
to the user and ask whether they want to override. Create the answers directory
and write the collected answers to `.xanadAssistant/tmp/setup-answers.json`.
Include only keys the user explicitly overrides — omitted keys are filled from
their declared `default` values by the lifecycle engine.

Each question also carries a `batch` field:

- `setup` — always first; ask `setup.depth` before anything else
- `simple` — always shown
- `advanced` — shown when `setup.depth` is `advanced` or `full`
- `full` — shown only when `setup.depth` is `full`
- `agent` — emitted later by `plan setup` for locally installed configurable agents

Use the user's `setup.depth` answer to decide which later questions to show.

**Answer file format:**

```json
{
  "setup.depth": "simple",
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

`packs.selected` accepts one or more pack names as an array. If the user selects
multiple packs, the plan step will surface any token conflicts that need
resolution before proceeding.

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

Pass `--resolutions .xanadAssistant/tmp/conflict-resolutions.json` to
`plan setup` below.  If `existingFiles` is empty, skip this step.

### 3. Generate a plan

```sh
python3 xanadAssistant.py plan setup \
  --workspace . \
  --package-root <path-to-xanadAssistant-checkout> \
  --answers .xanadAssistant/tmp/setup-answers.json \
  --plan-out .xanadAssistant/tmp/setup-plan.json \
  --non-interactive --json-lines
```

For predecessor-managed installs, replace `plan setup`
with `plan repair`.

Inspect `result.questions` in the plan payload before proceeding. If it includes
any entries with `batch: "agent"`, present those follow-up questions to the user.
If the user wants to override any of those agent defaults, add the answers to
`.xanadAssistant/tmp/setup-answers.json` and rerun `plan setup` so the serialized
plan captures the final agent-customization answers. If the user accepts the
defaults, you can proceed without changing the answers file.

If `approvalRequired` is true in the plan payload, summarise the planned writes
and retired files for the user and ask for approval before proceeding.

To preview what would be written without making any changes, pass `--dry-run` to
the `plan` command and inspect the serialized plan instead of running `setup`.

### 4. Setup

Once the user approves (or `approvalRequired` is false):

```sh
python3 xanadAssistant.py setup \
  --workspace . \
  --package-root <path-to-xanadAssistant-checkout> \
  --plan .xanadAssistant/tmp/setup-plan.json \
  --ui agent --json-lines
```

For predecessor-managed installs, replace `setup` with
`repair`.

Check the `validation.status` in the setup result. If it is not `passed`,
report the error and the `backupPath` to the user.

### 5. Confirm and clean up

Show the user the Receipt phase output and the path to the generated
`.github/copilot-version.md` summary.

When MCP is available, prefer `lifecycle_check` for the final confirmation step.

After a successful setup, remove the temporary answers file:

```sh
rm -rf .xanadAssistant/tmp
```
