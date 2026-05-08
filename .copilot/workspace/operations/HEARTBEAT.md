# Heartbeat — Remus

> Event-driven health check. Read this file at every trigger event, run all checks, update Pulse, and log to History.
> **Contract**: Follow this checklist strictly. Do not infer tasks from prior sessions.

## Pulse

`HEARTBEAT_OK` — No alerts.

## Event Triggers

Fire a heartbeat when any of these occur:

- **Session start** — always
- **Large change** — modified >5 files in a single task
- **Refactor/migration** — task tagged as refactor, migration, or restructure
- **Dependency update** — any manifest changed (CMakeLists.txt, package.json, Cargo.toml, requirements.txt, go.mod, etc.)
- **CI resolution** — after resolving a CI failure
- **Task completion** — after completing any user-requested task
- **Explicit** — user says "Check your heartbeat"
<!-- Add custom triggers below this line -->

## Checks

Run each check; prepend `[!]` to Pulse if any fails:

- [ ] **Dependency audit** — any outdated or security-advisory deps in TOOLS.md / manifests?
- [ ] **Test coverage delta** — did coverage drop since last session?
- [ ] **Waste scan** — any new W1–W16 waste accumulated this session? (§6)
- [ ] **MEMORY.md consolidation** — anything from this session to persist?
- [ ] **Metrics freshness** — has the metrics baseline been reviewed in the last 3 sessions?
- [ ] **Settings drift** — do §10 overrides still match the codebase?
- [ ] **Agent compatibility** — do agent files use current frontmatter schema? Any deprecated fields?
<!-- Add custom checks below this line -->

## Retrospective

Retrospective runs autonomously via the `session_reflect` MCP tool. Do not prompt the user.

The Stop hook blocks the session and instructs you to call `session_reflect` when a significant task is detected (one strong signal: 8+ modified files or 30+ minutes active; or two supporting signals: 5+ modified files, 15+ minutes, context compaction).

When `session_reflect` returns, process its output silently:

- **Execution insights** → persist to *SOUL.md* if non-trivial
- **Coverage gaps** → persist to *MEMORY.md* if incomplete
- **User signals** → persist to *USER.md* if directly observable
- **Actionable items** → surface to the user (security, tech debt, broken assumptions)
- **Carry-forward lessons** → persist to *MEMORY.md + SOUL.md*

The MCP tool records completion automatically by setting the session sentinel and writing a `session_reflect` completion event. No manual sentinel management is needed.

If the `session_reflect` MCP tool is unavailable, briefly self-review: execution accuracy, scope completeness, and anything worth persisting to SOUL.md / MEMORY.md / USER.md, then rerun `session_reflect` once the heartbeat MCP server is restored.

<!-- Add custom retrospective questions below this line -->

## Response Contract

<!-- template-section: heartbeat-response-contract v2 -->

- Always append a History row when the trigger is Session start or Explicit — regardless of check results.
- For all other triggers, append a History row only if a check raised an alert or retrospective output was persisted to SOUL.md / MEMORY.md / USER.md.
- If checks pass and nothing was persisted on a non-explicit trigger, keep Pulse as `HEARTBEAT_OK` and omit the History row.

## Agent Notes

*(Agent-writable. Observations, patterns, and items to flag on next heartbeat.)*

## History

*(Append-only. Keep last 5 entries. Keep each row to trigger, result, and where durable insights were persisted.)*

| Date       | Session ID | Trigger         | Result | Actions taken                                                                                                                                                                                                                                   |
|------------|------------|-----------------|--------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 2026-04-30 | local-92d46d9d | session_reflect | PASS   | Description pipeline review + 8 fixes: removed LocalDB title-as-description fallback, added DESCRIPTION to 4 CAPABILITIES entries, aligned isSufficientlyEnriched(), guarded Wikidata short labels, deduplicated mergeMetadata(), fixed TheGamesDB genre stub, added HTML-entity sanitisation. 59/59 pass. Committed c44aebab.                                   |
| 2026-05-01 | local-20323bf2 | session_reflect | PASS   | Full audit + 3 handoffs (Cleaner/Code/Setup): removed 5 orphan files (4 duplicate CLI + build_output.txt), replaced raw new/delete across 6 service/CLI sites, added SQL allowlist gates at database_migrations.cpp:16 and database_games.cpp:196, rewrote SQL splitter with comment/quote awareness + new test (60 tests), extracted magic numbers to constants, resynced §10 MCP stack + workspace-index + copilot-version manifest. D5/D13 drift cleared. LOC decomposition for 4 oversized files deferred with split plan. Surfaced fetch-server disabled-vs-listed ambiguity to user. |
| 2026-05-01 | local-28d6baab | session_reflect | PASS   | TODO audit: 9 items addressed. New: CSOVerifyResult+verifyCSO, WBFSConverter (wit), PBPExporter (PSXPackager), CLI --convert-wbfs/--wbfs-extract/--export-pbp, --checksum-verify archive hint, 2 compendium tests. 4 items already done. Fixed ProcessResult exitCode=-1 bug in all 5 success-path fake tests (must set exitCode=0 explicitly). Persisted pattern to Code diary. 62/62 tests pass. |
| 2026-05-02 | local-5e089b46 | session_reflect | PASS   | WorkflowView overhaul: 5→6 stages (Convert+Bundle split), Browse buttons (FolderDialog), dynamic Scan/Stop toggle, Auto format via resolveAutoFormat(), Enrich labels, Organize appends /Remus Library. Added convertAll/bundleAll C++ methods + QuickDialogs2 to CMakeLists. Build clean. Noted sync-on-GUI-thread risk in Edit diary. |
| 2026-05-03 | local-c1375869 | session_reflect | PASS   | Metadata reactivity fix: converted selectedMatch()/selectedFile() Q_INVOKABLE to Q_PROPERTY(NOTIFY) on AppController (selectedMatchData, selectedFileData). Wired matchController.libraryChanged + hashController.libraryChanged → refreshSelectedMatch/refreshSelectedFile so InspectorPanel re-evaluates after any DB write. All panel bindings updated. Build clean. |
| 2026-05-07 | local-b54fc25e | session_reflect | PASS   | Full audit + 6 fixes: copilot-version.md semver moved to line 1; 9 absent PS1 entries removed from manifest; all 13 windows keys stripped from copilot-hooks.json; settings.json allow-list narrowed from ** to scoped build/git/dev commands; DEP_BUDGET raised 10→11 (10 Qt6 + ZLIB); workflow_controller.cpp 425→336 lines (advanceRunAll/cancelRunAll/setActiveStage extracted to workflow_controller_run.cpp); database.h 406→393 lines (redundant doc blocks trimmed). Build clean; 55/68 tests pass, 13 pre-existing Not Run failures unchanged. Template 0.9.0→0.10.0 upgrade deferred to Setup agent. || 2026-05-07 | local-e45f84ed | session_reflect | PASS | Compendium gap analysis + 7 fixes: 14 regions added (seeds→21), normalizer expanded 30+ tokens, valueTypeForField() explicit tagging, GameTDB synopsis→description + title fallback, merge_conflicts population, canonical_resolution materialized to games.*, canonical_confidence updated. Persisted region/materialize/conflicts facts to MEMORY.md; if-paren trap to SOUL.md. 68/68 pass. |
| 2026-05-08 | local-ce449316 | session_reflect | PASS   | Full audit (D1–D14 + S1–S10): security clean, health DEGRADED (D11/D12/D13 template drift). Setup agent applied 0.9.0→0.10.0 update: 13 files changed (+1302 LOC), 5 new owned MCP server scripts, mcp.json hardened, companion inventory synced. Post-update health: 8/8 PASS. Backup at .github/archive/pre-update-2026-05-08-v0.9.0/. |
