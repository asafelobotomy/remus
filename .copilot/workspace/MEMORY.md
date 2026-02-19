# Memory Strategy — Remus

- Use project-scoped memory for conventions discovered in this codebase.
- Use session transcripts for recent context; do not rely on long-term memory for facts that are in source files.
- Always prefer reading the source file over recalling a cached summary of it.
- The `src/core/` layer is pure logic — no Qt UI dependencies. Keep it that way.
- The `remus-constants` library owns all magic numbers — never hardcode new ones.
- SQLite cache (30-day TTL) sits between providers and the service layer; invalidation is by age, not by demand.

*(Updated as the memory system is used.)*
