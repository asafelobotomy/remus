# Frontend Status

Remus is currently developed as a CLI-first product.

## Treat GUI and TUI as archived

The Qt GUI in `src/ui/` and the notcurses TUI in `src/tui/` remain in the repository for reference and possible future extraction.

They are archived for day-to-day development:

- They are off by default in CMake.
- They are not part of the default build flow.
- They are not part of default CI coverage.
- New feature work should land in the CLI unless there is an explicit decision to revive a frontend.

## Build policy

Use the CLI-only build as the supported path:

```bash
cmake -S . -B build \
  -DREMUS_BUILD_GUI=OFF \
  -DREMUS_BUILD_TUI=OFF \
  -DREMUS_BUILD_INTERACTIVE_CLI=OFF
cmake --build build -j$(nproc)
```

The archived frontend flags still exist for exploratory work:

- `REMUS_BUILD_GUI`
- `REMUS_BUILD_TUI`
- `REMUS_BUILD_INTERACTIVE_CLI`

Treat those builds as opt-in and unsupported for the main delivery cycle.

## Improve the CLI instead

When a feature would otherwise require browsing or inspection in a frontend, prefer adding a CLI command or richer CLI output.

Current examples:

- Use `--mod-systems` to inspect catalog coverage.
- Use `--mod-system <name>` to browse mods by platform.
- Use `--mod-show <id>` to inspect a mod before installation.