# CLI exit codes

`remus-cli` uses the following exit codes:

| Code | Meaning |
|------|---------|
| `0` | Success |
| `1` | Operational failure (database error, verification mismatch, missing input, etc.) |
| `2` | Usage error (conflicting primary actions) |

## Verification

`--verify` exits `1` when the verification summary reports mismatched or corrupt files, or when any scanned file is not found in the loaded DAT. Files that lack hashes are reported as warnings but do not fail the command.

`--verify-set` exits `1` when any disc set is incomplete (missing discs, track gaps, or warnings).

## Primary actions

Only one primary action is allowed per invocation, except for scan pipeline companions:

`--scan` may be combined with `--hash`, `--match`, `--enrich`, `--bundle`, `--organize`, `--generate-m3u`, and `--download-artwork`.

`--process` and `--library` cannot be combined with any other primary action.

Compendium build modifiers such as `--force-full-rebuild` are modifiers only; they must accompany a compendium command such as `--build-compendium`.
