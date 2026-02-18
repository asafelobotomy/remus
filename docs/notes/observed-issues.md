# Observed Issues (Hasheous & Metadata Flow)

- MetadataProxy `involved_companies` flags (`developer`/`publisher`) are always `false`; resolving roles requires separate company lookups and checking their `developed`/`published` game lists.
- Rate limiter is set to 1000 ms; with per-company lookups this can slow large libraries. Could likely be reduced (e.g., 300–500 ms) or made configurable.
- `HashAlgorithms::detectFromLength()` already handled CRC32; the earlier inline detection duplicated this logic unnecessarily.
- Matched games were showing empty `System` because IGDB platform data wasn’t mapped back to Remus systems; requires parsing platforms and using `SystemResolver`.
- Fan-translation/patched ROMs (e.g., Shin Megami Tensei fan hack) won’t hash-match upstream DBs; name-based fallback (with working name providers) is needed for these cases.
