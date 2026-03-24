# Setup & Build Documentation

Build instructions, dependencies, and environment setup for Remus.

## Contents

- **BUILD.md** - Comprehensive build instructions, dependencies, and compilation steps

## Quick Build

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
./build/remus-cli --help
```

For detailed instructions, see [BUILD.md](BUILD.md).
