# Hasheous offline dumps (local only)

Platform JSON dumps are **not committed**. Download them once per clone:

```bash
scripts/update_hasheous_dumps.sh --all-core   # curated ~25 platforms
# scripts/update_hasheous_dumps.sh --all      # all platforms (large)
```

(`scripts/update_dats.sh --all` runs `--all-core` when this script is present.)

Unit tests use minimal fixtures under `tests/fixtures/hasheous_offline/`.
