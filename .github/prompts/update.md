# Update xanadAssistant

Use this prompt when the user asks to update xanadAssistant or pull the latest
agents, skills, hooks, and prompts into the active repository.

Target workspace: remus (display name)
Selected profile: balanced

Use `.` as the workspace path in CLI examples below when running from the target repository root.

## Workflow

If the workspace `xanadTools` MCP server is connected and can resolve a local
package root or a supported remote source, prefer the MCP lifecycle tools:

- `lifecycle_inspect`
- `lifecycle_check`
- `lifecycle_update`

Fall back to the CLI commands below if MCP is unavailable.

### 1. Inspect current state

```
python3 xanadAssistant.py inspect \
  --workspace . \
  --package-root <path-to-xanadAssistant-checkout> \
  --ui agent --json-lines
```

Review `installState`. If it is `not-installed`, halt and inform the user that
xanadAssistant is not installed; offer to run `/setup` or `/bootstrap` instead.

### 2. Re-interview (optional)

Skip this step if the user wants to keep existing answers and just pull the
latest package files.

If the user wants to change pack selection, profile, or personalisation during
the update:

```
python3 xanadAssistant.py interview \
  --workspace . \
  --package-root <path-to-xanadAssistant-checkout> \
  --mode update --json-lines
```

Collect answers and write only the overridden keys to
`.xanadAssistant/tmp/update-answers.json`.

### 3. Update

```
python3 xanadAssistant.py update \
  --workspace . \
  --package-root <path-to-xanadAssistant-checkout> \
  --non-interactive --ui agent --json-lines
```

### 4. Confirm result

Check `validation.status` in the update result.

- If `passed`: show the user the summary from `.github/copilot-version.md` and
  list the files that were updated or retired.
- If not `passed`: report the error and `backupPath` to the user so they can
  restore the previous state if needed.

If Step 2 ran (re-interview), remove the temporary directory:

```sh
rm -rf .xanadAssistant/tmp
```

If a re-interview was run, add
`--answers .xanadAssistant/tmp/update-answers.json`.

Check `validation.status`. If it is not `passed`, report the error and
`backupPath` to the user.

### 4. Confirm

Show the Receipt phase output and the path to the updated
`.github/copilot-version.md` summary.

When MCP is available, prefer `lifecycle_check` for the final confirmation.

Clean up any temporary files:

```sh
rm -rf .xanadAssistant/tmp
```
