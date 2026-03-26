# Frontend Status

Remus is developed as a CLI-only product. GUI and TUI code has been physically archived.

## Archived code

As of 2026-03-26, the Qt GUI (`src/ui/`), Notcurses TUI (`src/tui/`), and ncurses
interactive CLI session have been moved to `archive/gui-tui/`. The corresponding test
files are in `archive/gui-tui/tests/`.

The CMake options `REMUS_BUILD_GUI`, `REMUS_BUILD_TUI`, and `REMUS_BUILD_INTERACTIVE_CLI`
have been removed from the build. To restore them, move the archived code back and
re-add the CMake option blocks (preserved in git history).

## Build policy

The CLI-only build is the only supported path:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

## Improve the CLI instead

When a feature would otherwise require browsing or inspection in a frontend, prefer adding a CLI command or richer CLI output.

Current examples:

- Use `--mod-systems` to inspect catalog coverage.
- Use `--mod-system <name>` to browse mods by platform.
- Use `--mod-show <id>` to inspect a mod before installation.