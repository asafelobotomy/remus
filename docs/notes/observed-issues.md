# Observed Issues (Hasheous & Metadata Flow)

Status notes for metadata enrichment and matching edge cases.

## Resolved

- **MetadataProxy `involved_companies` flags** — Hasheous enrichment now resolves developer/publisher when proxy flags are false by following company IDs and checking each company's `developed` / `published` game lists (`hasheous_provider_enrichment.cpp`).
- **CRC32 hash-type detection duplication** — ScreenScraper and Hasheous providers use `HashAlgorithms::detectFromLength()` instead of inline length checks.
- **Empty `System` on IGDB matches** — IGDB provider maps `platforms.slug` back to Remus systems via `SystemResolver`; Hasheous MetadataProxy enrichment uses the same mapping.
- **Fan-translation / patched ROM name fallback** — `deriveMatchingDisplayName()` in `match_utils.cpp` strips patch tags and runs `MatchingEngine::extractGameTitle()` for patched variants so name-based providers can match when hashes diverge.

## Open / partial

- **Rate limiter interval** — Per-provider and global HTTP spacing is configurable in GUI Settings (`metadata/rate_limit/*` keys) and via QSettings; defaults remain in `src/core/constants/network.h`.
