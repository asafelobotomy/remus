# Observed Issues (Hasheous & Metadata Flow)

Status notes for metadata enrichment and matching edge cases.

## Resolved

- **MetadataProxy `involved_companies` flags** — Hasheous enrichment now resolves developer/publisher when proxy flags are false by following company IDs and checking each company's `developed` / `published` game lists (`hasheous_provider_enrichment.cpp`).
- **CRC32 hash-type detection duplication** — ScreenScraper and Hasheous providers use `HashAlgorithms::detectFromLength()` instead of inline length checks.
- **Empty `System` on IGDB matches** — IGDB provider maps `platforms.slug` back to Remus systems via `SystemResolver`; Hasheous MetadataProxy enrichment uses the same mapping.
- **Fan-translation / patched ROM name fallback** — `deriveMatchingDisplayName()` in `match_utils.cpp` strips patch tags and runs `MatchingEngine::extractGameTitle()` for patched variants so name-based providers can match when hashes diverge.

## Open / partial

- **Rate limiter interval** — Default HTTP spacing reduced from 1000 ms to 500 ms (Hasheous 400 ms). Per-provider tuning and user configuration remain future work if large-library enrichment is still too slow.
