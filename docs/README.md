# Remus Documentation

Documentation index for Remus.

## Start Here

- [../README.md](../README.md) - Project overview
- [setup/BUILD.md](setup/BUILD.md) - Build and run instructions
- [quick-reference.md](quick-reference.md) - CLI quick reference

## User Guides

- [examples.md](examples.md) - End-to-end usage examples
- [emulator-frontend-compatibility.md](emulator-frontend-compatibility.md) - Frontend export compatibility
- [verification-and-patching.md](verification-and-patching.md) - Compendium-backed verification and ROM patching
- [naming-standards.md](naming-standards.md) - Naming conventions
- [chd-conversion.md](chd-conversion.md) - CHD conversion workflow
- [format-matrix.md](format-matrix.md) - Recommended canonical formats, accepted inputs, and tool backends by system
- [guides/TEST-DATA-POLICY.md](guides/TEST-DATA-POLICY.md) - Canonical `roms/` and `test_output/` policy

## Technical Reference

- [architecture/README.md](architecture/README.md) - Architecture and design docs
- [data-model.md](data-model.md) - SQLite data model
- [requirements.md](requirements.md) - Requirements and scope
- [plan.md](plan.md) - Roadmap and milestone plan
- [plans/archive-format-backlog.md](plans/archive-format-backlog.md) - Concrete backlog for CSO, WBFS normalization, and PBP export
- [adr/adr-0002-use-system-specific-canonical-archive-formats.md](adr/adr-0002-use-system-specific-canonical-archive-formats.md) - Locked archive policy for future format work
- [metadata-providers.md](metadata-providers.md) - Metadata provider details
- [cli/README.md](cli/README.md) - CLI implementation docs

## Testing and Reports

- [guides/TESTING-GUIDE.md](guides/TESTING-GUIDE.md) - Testing guide
- [reports/README.md](reports/README.md) - Test and implementation reports
- [archive/milestones/README.md](archive/milestones/README.md) - Milestone completion docs *(archived)*

## Historical Material

- [archive/README.md](archive/README.md) - Archived design/theming docs
- [archive/reports/legacy/](archive/reports/legacy/) - Legacy implementation and audit reports *(archived)*

## Documentation Placement Rules

1. User-facing guides: `docs/` root
2. Architecture/design docs: `docs/architecture/`
3. Milestone completion docs: `docs/archive/milestones/` *(archived)*
4. Test and implementation reports: `docs/reports/`
5. Historical/legacy documents: `docs/archive/` or `docs/archive/reports/legacy/`

See also: [CONTRIBUTING.md](CONTRIBUTING.md) for documentation contribution workflow.
