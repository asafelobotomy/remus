# Cursor agent terminal — stall prevention

Agent-driven compendium work exposed three failure layers: slow SQL, parallel SQLite access, and Cursor sandbox/bootstrap hangs. Repo scripts mitigate the first two; this doc covers IDE/shell mitigations.

## zsh startup guard

Add near the **top** of `~/.zshrc` (before Oh My Zsh / Powerlevel10k):

```zsh
# Cursor agent: skip heavy interactive init (avoids terminal capture hangs)
if [[ -n "${CURSOR_AGENT:-}" || "${COMPOSER_NO_INTERACTION:-}" == "1" ]]; then
  PROMPT='%n@%m:%~$ '
  return
fi
```

Or, if you use p10k, disable the theme when the agent runs:

```zsh
if [[ -n "$CURSOR_AGENT" ]]; then
  ZSH_THEME=""
fi
```

## Cursor settings (workspace or user)

In `.vscode/settings.json` or Cursor user settings:

```json
"terminal.integrated.shellIntegration.enabled": false
```

This reduces agent vs integrated-terminal capture conflicts on some setups.

## Session hygiene

- Restart Cursor fully every 1–2 hours during long agent sessions (quit and relaunch, not Reload Window only).
- Start a new chat thread for unrelated compendium tasks instead of one mega-session.
- If the agent shell shows no output for >5 minutes, **Cancel** and re-run via `scripts/run_compendium_job.sh` — do not spawn parallel DB jobs.

## Known Cursor issues (external)

- [Background terminal fd3 handshake hang](https://forum.cursor.com/t/backgrounded-terminal-hangs-in-ide/156990)
- [Agent terminal completion detection](https://forum.cursor.com/t/ide-bug-agent-is-waiting-sandbox-response/158675)
- [Long session renderer instability](https://forum.cursor.com/t/renderer-crashes-code-5-during-long-agent-sessions-due-to-sqlite-nested-transaction-bug/152840)

## Compendium-specific commands

```bash
# Fast gate (~1 min)
./scripts/validate_compendium_quick.sh

# Extended checks (~30s, applies migrations 0008/0009)
./scripts/validate_compendium_extended.sh

# Any DB job (serialized)
./scripts/run_compendium_job.sh --db data/compendium/remus_compendium.db -- \
  ./build/remus-cli --enrich-compendium --log-file /tmp/enrich.log ...
```
