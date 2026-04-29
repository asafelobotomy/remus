# Memory Strategy — Remus

- Use project-scoped memory for conventions discovered in this codebase.
- Use session transcripts for recent context; do not rely on long-term memory for facts that are in source files.
- Always prefer reading the source file over recalling a cached summary of it.
- The `src/core/` layer is pure logic — no Qt UI dependencies. Keep it that way.
- The `remus-constants` library owns all magic numbers — never hardcode new ones.
- SQLite cache (30-day TTL) sits between providers and the service layer; invalidation is by age, not by demand.

- `ArchiveInfo` (in `src/core/archive_extractor.h`) uses `QStringList contents` to list archive members — there is no `entries` field or `ArchiveEntry` struct. Use `contents.contains(...)` to check for a specific member.
- `DiscOutputFormat` enum (in `src/core/rom_bundler.h`) has four values: `Original`, `Chd`, `Rvz`, `Cso`. CSO is triggered automatically by `ConversionPlanner` for PSP; it is not exposed as a CLI option.
- The process pipeline uses a shared `QTemporaryDir processArtworkTemp` (stored as `ctx.processArtworkCacheDir`) to cache downloaded artwork across per-system batches, avoiding duplicate provider round-trips.

- Generated export files (`*-export.csv`, `*-export.json`) and temp pipeline dirs (`temp_*/`, `current_run_dir.txt`) are gitignored — never commit them.
- `dist/` is gitignored (release tarballs). `test_output/` is gitignored except `README.md`. `logs/` is gitignored entirely.

- XML export (`--export emustation` / `--export launchbox`) does NOT escape special characters — known issue. Use `QString::toHtmlEscaped()` on all user-supplied fields before writing XML. Affects: game title, description, genre, region, path in `cli_commands_export.cpp`.
- CSV export (`--export csv`) only escapes the title column; other string fields (path, system, region) are unquoted — known issue.
- `--bundle-format` help text in `main.cpp` says "default: zip" but actual default is "7z" — stale help text, known issue.

- [2026-04] Template `security.agent.md` removed upstream (merged into `audit.agent.md` + `security-audit` skill). The installed file at `.github/agents/security.agent.md` is unmaintained by upstream. Keep per §8 but note it will diverge.
- [2026-04] When an agent file returns 404 during template update: keep installed version, do NOT delete — §8 forbids deletion without explicit user instruction.

*(Updated as the memory system is used.)*
