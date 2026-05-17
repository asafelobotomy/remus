# Bootstrap xanadAssistant

Use this prompt when a user asks to install xanadAssistant from scratch into
the current workspace using the bootstrap runner.

This is a post-install convenience surface. For a brand-new workspace with no
prior xanadAssistant install, use [INSTALL.md](https://github.com/asafelobotomy/xanadassistant/blob/main/INSTALL.md)
to obtain `xanadBootstrap.py` first.

## Workflow

### 1 — Inspect

```sh
python3 xanadBootstrap.py inspect --workspace . --json
```

Confirm `installState` is `not-installed` before continuing.

### 2 — Interview

```sh
python3 xanadBootstrap.py interview --workspace . --mode setup --json
```

Parse `result.questions`. For each question, present the `prompt` and `default`
to the user and ask whether they want to override. Write the collected answers
to `.xanadAssistant/tmp/setup-answers.json`.

Each question also carries a `batch` field:

- `setup` — always first; ask `setup.depth` before anything else
- `simple` — always shown
- `advanced` — shown when `setup.depth` is `advanced` or `full`
- `full` — shown only when `setup.depth` is `full`

Use the user's `setup.depth` answer to decide which later questions to show.

**Answer file format** — include only keys the user explicitly overrides:

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

Any key omitted from the file is resolved to its declared `default` by the
lifecycle engine. Create the directory first:

`packs.selected` accepts one or more pack names as an array. If the user selects
multiple packs, the plan step will surface any token conflicts that need
resolution before proceeding.

```sh
mkdir -p .xanadAssistant/tmp
```

### 2.5 — Resolve pre-existing files (if any)

Inspect `result.existingFiles` from the interview response.
If the array is non-empty, present each entry to the user:

- `type: "collision"` — xanadAssistant would overwrite this file.
  Decisions: `keep`, `replace`, or `merge` (if `mergeSupported` is true).
- `type: "unmanaged"` — a file in a managed directory not owned by xanadAssistant.
  Decisions: `keep` or `remove`.

Collect decisions and write to `.xanadAssistant/tmp/conflict-resolutions.json`:

```json
{
  ".github/prompts/my-prompt.md": "keep",
  ".github/agents/old-agent.md": "remove"
}
```

Pass `--resolutions .xanadAssistant/tmp/conflict-resolutions.json` to
`plan setup` below.  Skip this step if `existingFiles` is empty.

### 3 — Plan

```sh
python3 xanadBootstrap.py plan setup \
  --workspace . \
  --answers .xanadAssistant/tmp/setup-answers.json \
  --plan-out .xanadAssistant/tmp/setup-plan.json \
  --non-interactive --json
```

If `approvalRequired` is `true`, summarise the planned writes and confirm with
the user before proceeding.

### 4 — Apply

```sh
python3 xanadBootstrap.py apply \
  --workspace . \
  --plan .xanadAssistant/tmp/setup-plan.json \
  --json
```

Check `validation.status`. If it is not `passed`, report the error and
`backupPath` to the user.

### 5 — Clean up

```sh
rm xanadBootstrap.py
rm -rf .xanadAssistant/tmp
```

The install is complete. Use `@xanadLifecycle` for all future lifecycle
operations (update, repair, factory-restore).
