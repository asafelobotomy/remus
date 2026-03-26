# Archived GUI / TUI Code

This directory contains the archived Qt GUI, Notcurses TUI, and ncurses interactive CLI
code that was removed from the active build on 2026-03-26. The CLI foundation is the
current focus; GUI/TUI wiring will be restored once the CLI is confirmed working correctly.

## Contents

| Directory | Description |
|-----------|-------------|
| `src/ui/` | Qt6 QML GUI — controllers, models, QML views, resources |
| `src/tui/` | Notcurses-based terminal UI — screens, pipeline, widgets |
| `src/interactive_session.cpp` / `.h` | ncurses-backed interactive CLI session |
| `tests/` | All test files that depended on `remus-ui` or `remus-tui-core` targets |

## Restoring

To re-enable these components, move the directories back and restore the CMake option
blocks (`REMUS_BUILD_GUI`, `REMUS_BUILD_TUI`, `REMUS_BUILD_INTERACTIVE_CLI`) that were
removed from the root `CMakeLists.txt`. The git history preserves the exact CMake
configuration that was in place before archival.
