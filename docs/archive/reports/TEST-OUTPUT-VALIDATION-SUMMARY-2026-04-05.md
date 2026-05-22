# Test Output Validation Summary — 2026-04-05

This report captures the final findings from the validation runs that were previously staged under `test_output/`.

The heavy artifacts are removed after this report is written. The disposable-output policy remains in [../../test_output/README.md](../../test_output/README.md).

## Confirm the process pipeline behavior

The full pipeline processes ROMs by system batch instead of global mixed stages.

The final corpus validation shows this order:

- Nintendo Entertainment System
- Nintendo GameCube
- Sega Dreamcast
- Sega Genesis / Mega Drive
- Sony PlayStation
- Super Nintendo Entertainment System

The same validation also confirms that PlayStation disc media converts to CHD during bundling.

Key outcome:

- `Tenchu 2 - Birth of the Stealth Assassins (Europe)` converts to CHD during the PlayStation batch.

## Confirm the GameCube RVZ path

After `dolphin-tool` is installed, Remus converts the GameCube bundle payload to RVZ during bundling.

Key outcome:

- `Legend of Zelda, The - The Wind Waker (USA)` matches at `100%` confidence.
- Remus invokes `dolphin-tool convert --format=rvz`.
- Remus writes a bundled archive that contains a root-level `.rvz` payload.
- The final bundled archive size is `837645389` bytes.

## Confirm direct RVZ conversion and verification

Direct conversion through `--convert-rvz` works for the real Wind Waker ISO.

Key outcome:

- Source ISO size: `1459978240` bytes
- RVZ size: `881846208` bytes
- Saved space: about `551.35 MB`
- Verification result: `Problems Found: No`
- Verification hashes:
  - `CRC32: ad21c2ba`
  - `SHA1: 6b5f06c10d50ebb4099cded88217eb71e5bfbb4a`

## Confirm single-file process input support

`--process` now accepts a direct archive path, not only a directory.

Key outcome:

- A direct run against `roms/Legend of Zelda, The - The Wind Waker (USA).7z` reports `Scan complete: 1 files found`.
- The same run reports the effective bundle setting as `Disc: "rvz"`.

## Re-run the critical checks

Use these commands if you want to reproduce the final validation.

```bash
# Full process pipeline
./build/remus-cli --process "$PWD/roms" \
  --db test_output/recheck/process.db \
  --process-output test_output/recheck/final \
  --process-preset es-de \
  --min-confidence 60

# Direct GameCube RVZ conversion
./build/remus-cli --convert-rvz \
  "test_output/recheck/Legend of Zelda, The - The Wind Waker (USA).iso" \
  --output-dir test_output/recheck

# Direct RVZ verification
./build/remus-cli --rvz-verify \
  "test_output/recheck/Legend of Zelda, The - The Wind Waker (USA).rvz"

# Prune transient test artifacts afterward
./scripts/prune_test_output.sh --apply
```

## Keep the folder clean

Treat `test_output/` as disposable workspace state.

Use [../../test_output/README.md](../../test_output/README.md) for the local policy and [../../scripts/prune_test_output.sh](../../scripts/prune_test_output.sh) to remove transient artifacts after a validation pass.
