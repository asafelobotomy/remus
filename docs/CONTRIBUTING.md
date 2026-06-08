# Documentation Contributing Guide

This guide defines where to place documentation and how to submit code changes.

## Code Changes

1. Fork the repository and create a feature branch from `main`.
2. Install build dependencies from [docs/setup/BUILD.md](setup/BUILD.md).
3. Build and test locally:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DREMUS_ENABLE_WARNINGS=ON
   cmake --build build -j$(nproc)
   ctest --test-dir build --output-on-failure
   ```
4. Format C++ changes before opening a pull request (use clang-format version from `.clang-format-version`):
   ```bash
   find src tests -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 clang-format -i
   ```
5. Open a pull request using the template and ensure CI checks pass.

Maintainers should require the `CI` and `CodeQL` workflows to pass before merging to `main`.

## Placement Rules

- Put user-facing guides in `docs/` root.
- Put architecture/design content in `docs/architecture/`.
- Put milestone completion/progress items in `docs/archive/milestones/`.
- Put test results and implementation reports in `docs/reports/`.
- Put superseded historical reports in `docs/archive/reports/legacy/`.
- Put setup/build/install material in `docs/setup/`.

## Naming Conventions

- Prefer uppercase, hyphenated report names for milestone/report documents (example: `M10-COMPLETION.md`).
- Prefer lowercase, hyphenated names for evergreen guides in `docs/` root (example: `metadata-providers.md`).
- Use `README.md` for folder indexes only.

## When Moving Existing Docs

1. Move the document to the correct folder.
2. Leave a short stub in the old location that links to the new location.
3. Update folder `README.md` indexes.
4. Update any root-level references (for example, `README.md` or `CHANGELOG.md`) if they point to the old path.

## Quality Checklist

- Links resolve locally.
- The document appears in the relevant folder `README.md` index.
- No duplicated active content across two locations.
- Legacy content is explicitly marked as historical if retained.
