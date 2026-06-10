# Patch / hack DAT catalogues

Community ROM hack and translation catalogues in ClrMamePro `.dat` format.

## Primary source (automated)

`scripts/update_dats.sh` copies libretro-database [`metadat/hacks/`](https://github.com/libretro/libretro-database/tree/master/metadat/hacks)
into `data/patches/hacks/`. Each file includes patched-ROM hashes and often a `patch "…"` URL
pointing at romhacking.net or similar.

## Import into compendium

```bash
bash scripts/import_patch_catalog.sh data/compendium/remus_compendium.db
# or: build/remus-cli --import-patch-catalog --compendium-output data/compendium/remus_compendium.db
```

The full build pipeline runs this automatically after `build_compendium_full.sh`.

## Additional sources

See [docs/reports/COMPENDIUM-DATA-SOURCES.md](../../docs/reports/COMPENDIUM-DATA-SOURCES.md) for
No-Intro non-Redump sections, RAPatches, and other enrichment options.
