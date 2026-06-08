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

### Required CI checks

Maintainers should configure [branch protection](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches) on `main` to require these status checks before merge:

| Check name | Workflow | Notes |
|------------|----------|-------|
| `build (Debug)` | CI | Primary build + test matrix |
| `build (Release)` | CI | Release configuration smoke build |
| `lint` | CI | clang-format (pinned version) |
| `coverage` | CI | lcov report generation |
| `sanitizer` | CI | ASan + UBSan test pass |
| `Analyze` | CodeQL | C++ static analysis (job display name: `Analyze (C/C++)`) |

Also require at least one approving review, **code owner review** on protected paths (see `.github/CODEOWNERS`), and disallow force-pushes to `main`.

Releases are **manual only**: run the **Release** workflow via *Actions → Release → Run workflow*.

### Release workflow (maintainers)

1. Bump `APP_VERSION` in `src/core/constants/api.h` and update `CHANGELOG.md` on a PR merged to `main`.
2. Run **Release** → *Run workflow* (leave version empty to use `api.h`, or pass e.g. `v0.10.2`).
3. The job creates the tag (if missing), runs tests, and publishes tar.gz + AppImage artifacts.
4. Use **force: true** only to republish when a tag already exists (bad artifact rebuild). Do not use force for routine releases.

Configure a **`release` environment** in GitHub repository settings with required reviewers so the publish job waits for approval before uploading assets.

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
