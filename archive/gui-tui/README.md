# Archived GUI / TUI Code

> **Historical reference only.** This tree is not built, tested, or supported in the active Remus workflow. Do not extend it unless you are explicitly restoring the legacy UI stack.

This directory contains the archived Qt Widgets GUI, Notcurses TUI, and ncurses interactive CLI
code that was removed from the active build on 2026-03-26.

The **active** desktop UI is the Qt Quick application under `src/gui/` (`remus-gui`), built by default alongside `remus-cli`.

## Contents

| Directory | Description |
|-----------|-------------|
| `src/ui/` | Legacy Qt6 QML GUI — controllers, models, QML views, resources |
| `src/tui/` | Notcurses-based terminal UI — screens, pipeline, widgets |
| `src/interactive_session.cpp` / `.h` | ncurses-backed interactive CLI session |
| `tests/` | All test files that depended on `remus-ui` or `remus-tui-core` targets |

## Restoring

To re-enable these legacy components, move the directories back and restore the CMake option
blocks (`REMUS_BUILD_GUI`, `REMUS_BUILD_TUI`, `REMUS_BUILD_INTERACTIVE_CLI`) that were
removed from the root `CMakeLists.txt`. The git history preserves the exact CMake
configuration that was in place before archival.

For new GUI work, prefer extending the active Qt Quick stack in `src/gui/` rather than reviving this archive.
