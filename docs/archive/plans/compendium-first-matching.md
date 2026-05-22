# Compendium-First Matching with Gap Enrichment

> Date: 2026-05-18 | Status: Planned | Owner: core/metadata

## Goal

Make the compendium the primary, authoritative source for game identity resolution.
All other providers become enrichment-only — they fill in fields that the compendium
does not supply, but they cannot overwrite identity fields (title, system, region,
match method, match score) once compendium has established them.

---

## Background

### Current Waterfall Behaviour

`ProviderOrchestrator::searchWithFallback()` runs all registered local providers
in a single priority-sorted loop. Compendium sits at priority 210, localdatabase
at 200, and GameTDB at 150. Because `mergeMetadata()` uses first-non-empty-wins
for all 19 `GameMetadata` fields, the compendium's result wins for fields it fills —
but only because it happens to have the highest numeric priority, not by design.

Three structural problems exist in the current code:

1. **No explicit compendium-first pass.** If the compendium returns a result,
   the existing waterfall still queries localdatabase and gametdb for the same game.
   This wastes I/O and obscures intent.

2. **`enrichMissingFields()` is never called from `searchWithFallback()`.** The
   gap-fill API exists in `provider_orchestrator_enrich.cpp` but is only used by
   callers that explicitly invoke it. After a compendium hit, bonus fields (artwork,
   rating, external IDs) are never requested from online providers.

3. **Merge guard blocks enrichment-only responses.** In `queryProvider()`, any
   response carrying only artwork, rating, or external IDs — and no identity fields —
   is silently dropped before `mergeMetadata()` is called:

   ```cpp
   if (!result.title.isEmpty() || !result.publisher.isEmpty() || !result.developer.isEmpty())
   ```

---

## Design

### Two-Pass Structure

```text
Pass 1 — Compendium identity
  └─ queryProvider("compendium", ...)
     ├─ title non-empty → identity established
     │   ├─ computeFieldGap(accumulator)          // what is still missing?
     │   ├─ enrichMissingFields(gaps, ...,         // fill gaps from other providers
     │   │     accumulator.title,                  // use canonical title, not raw filename
     │   │     excludeProviders={"compendium"})    // don't re-query compendium
     │   └─ cache + return
     └─ empty → fall through to Pass 2

Pass 2 — Legacy waterfall (compendium excluded)
  ├─ getSortedLocalProviders() minus "compendium"
  ├─ existing local waterfall loop (unchanged)
  └─ existing remote waterfall loop (unchanged)
```

### Identity Protection

`mergeMetadata()` already implements first-non-empty-wins for all 19 `GameMetadata`
fields. Because the compendium runs before any enrichment provider, its title,
system, region, matchScore, and matchMethod are populated first and are never
overwritten by a later provider. **No new locking code is needed.**

### Canonical Title as Search Key

When `enrichMissingFields()` is called after a compendium hit, `accumulator.title`
(the clean canonical game title) is passed as the `name` parameter instead of the
raw ROM filename. This significantly improves name-based lookup quality at online
providers such as ScreenScraper and IGDB.

---

## Implementation Plan

### Phase 1 — Expand gap tracking

*No dependencies. Can start immediately; run parallel with Phase 2.*

#### 1a. `src/core/constants/provider_fields.h` — add `EXTERNAL_IDS` constant

```cpp
inline constexpr const char* EXTERNAL_IDS = "externalIds";
```

The `RATING` and `SCREENSHOTS` constants already exist but are not yet tracked
by `computeFieldGap()`. `EXTERNAL_IDS` is the only missing constant.

`REQUIRED_FIELDS` is **not** changed — rating, screenshots, and external IDs
are bonus fields, not waterfall-stop conditions.

#### 1b. `src/core/constants/provider_fields.h` — update `CAPABILITIES` map

| Provider | Add capabilities |
| --- | --- |
| `screenscraper` | `SCREENSHOTS`, `RATING`, `EXTERNAL_IDS` |
| `igdb` | `SCREENSHOTS`, `RATING`, `EXTERNAL_IDS` |
| `thegamesdb` | `SCREENSHOTS`, `RATING` |
| `hasheous` | `EXTERNAL_IDS` |
| `retroachievements` | `RATING`, `EXTERNAL_IDS` |

#### 1c. `src/metadata/provider_orchestrator_enrich.cpp` — extend `computeFieldGap()`

Add three gap checks after the existing eight:

```cpp
if (m.screenshotUrls.isEmpty()) gap.insert(SCREENSHOTS);
if (m.rating == 0.0f)           gap.insert(RATING);
if (m.externalIds.isEmpty())    gap.insert(EXTERNAL_IDS);
```

---

### Phase 2 — Relax the `queryProvider()` merge guard

*No dependencies. Run parallel with Phase 1.*

**File:** `src/metadata/provider_orchestrator_fallback.cpp` (~line 141)

**Change:**

```cpp
// before
if (!result.title.isEmpty() || !result.publisher.isEmpty() || !result.developer.isEmpty())

// after
if (!result.title.isEmpty() || !result.publisher.isEmpty() || !result.developer.isEmpty()
        || !result.boxArtUrl.isEmpty() || !result.screenshotUrls.isEmpty()
        || result.rating != 0.0f || !result.externalIds.isEmpty())
```

This allows artwork-only, rating-only, or ID-only enrichment responses to pass
through to `mergeMetadata()` rather than being silently discarded.

---

### Phase 3 — Rewrite `searchWithFallback()` and extend `enrichMissingFields()`

*Depends on Phase 1 (correct gap computation).*

#### 3a. Add `excludeProviders` parameter to `enrichMissingFields()`

**Files:** `src/metadata/provider_orchestrator.h` and
`src/metadata/provider_orchestrator_enrich.cpp`

```cpp
// declaration
GameMetadata enrichMissingFields(
    const FieldSet &missing,
    const GameMetadata &existing,
    const QString &hash,
    const QString &name,
    const QString &system,
    const QString &crc32,
    const QString &md5,
    const QString &sha1,
    const QString &serial,
    const QSet<QString> &excludeProviders = {}   // ← new, optional
);
```

At the top of each provider loop body in `enrichMissingFields()`:

```cpp
if (excludeProviders.contains(provider->id())) continue;
```

#### 3b. Rewrite `searchWithFallback()`

**File:** `src/metadata/provider_orchestrator_fallback.cpp`

Replace the existing combined local+remote waterfall with the two-pass structure:

```cpp
// Pass 1 — Compendium identity
if (hasProvider("compendium") && isEnabled("compendium")) {
    queryProvider(accumulator, "compendium", hash, name, system, crc32, md5, sha1, serial);
    if (!accumulator.title.isEmpty()) {
        FieldSet gaps = computeFieldGap(accumulator);
        if (!gaps.isEmpty()) {
            accumulator = enrichMissingFields(
                gaps, accumulator, hash,
                accumulator.title,   // canonical title, not raw filename
                system, crc32, md5, sha1, serial,
                {"compendium"}       // exclude — already ran
            );
        }
        if (!hash.isEmpty() && m_cache)
            m_cache->store(accumulator, hash, system);
        return accumulator;
    }
}

// Pass 2 — Legacy waterfall (unchanged behaviour, compendium excluded)
auto localProviders = getSortedLocalProviders();
localProviders.removeIf([](auto &p) { return p->id() == "compendium"; });
// … existing local + remote waterfall loop …
```

**Key details:**

- Identity check is `!accumulator.title.isEmpty()` — covers hash, serial, and name
  matches from compendium, not just hash-score = 1.0.
- The `identityResolved` lambda (hash-only, score ≥ 1.0) remains for the legacy
  fallback path but is no longer used on the compendium-first path.
- Cache store/look-up logic stays at the same points as today.

---

### Phase 4 — Tests

*Depends on Phases 1–3.*

Add to `tests/test_compendium_provider.cpp` or a new
`tests/test_orchestrator_compendium_primary.cpp`:

| # | Test name | Verifies |
| --- | --- | --- |
| 1 | `TwoPassRouting_CompendiumTitleOnly` | Second provider fills publisher/description; does NOT overwrite title or matchMethod. |
| 2 | `IdentityLock_CompendiumWins` | Compendium sets title "Sonic" / system "Mega Drive"; localdatabase returns different title; accumulator keeps compendium values. |
| 3 | `ArtworkGapFill_RatingMerged` | Compendium returns core metadata, no artwork; ScreenScraper mock returns only boxArtUrl + rating; both are merged (requires Phase 2 guard fix). |
| 4 | `ExternalIdGapFill_HasheousMerged` | Compendium returns result without externalIds; Hasheous mock returns externalIds only; externalIds merged. |
| 5 | `CompendiumMiss_FallsThrough` | Compendium returns empty; localdatabase is tried next, then remote providers — same as current behaviour. |

---

## Verification

```bash
# Build
cmake --build build --target remus-cli -j$(nproc)

# Existing compendium tests still pass
ctest --test-dir build -R compendium

# Existing orchestrator tests still pass
ctest --test-dir build -R orchestrator

# Compendium hit — verify provider source and that online providers only fill gaps
./build/remus-cli --metadata --hash <known_compendium_hash>

# Compendium miss — verify fall-through to existing waterfall
./build/remus-cli --metadata --hash <hash_not_in_compendium>
```

---

## Files Changed

| File | Change |
| --- | --- |
| `src/core/constants/provider_fields.h` | Add `EXTERNAL_IDS` constant; expand `CAPABILITIES` map |
| `src/metadata/provider_orchestrator_enrich.cpp` | Extend `computeFieldGap()`; add `excludeProviders` param to `enrichMissingFields()` |
| `src/metadata/provider_orchestrator.h` | Update `enrichMissingFields()` declaration |
| `src/metadata/provider_orchestrator_fallback.cpp` | Relax merge guard; rewrite `searchWithFallback()` |
| `tests/test_orchestrator_compendium_primary.cpp` | New: 5 unit tests for the above |

---

## Decisions

- `isSufficientlyEnriched()` and `REQUIRED_FIELDS` are **unchanged** — rating,
  screenshots, and external IDs are bonus fields and do not gate waterfall exit.
- No new identity-lock code — `mergeMetadata()` first-non-empty-wins already provides
  the required protection once compendium runs in Pass 1.
- `accumulator.title` replaces the raw ROM filename as the name argument to
  `enrichMissingFields()` so online providers receive a clean canonical search term.
- The `excludeProviders` parameter avoids a redundant second compendium query inside
  the gap-fill pass.
