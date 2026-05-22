# P1 Issue #5 - System Name Resolution

**Date:** 2026-02-05  
**Status:** ✅ COMPLETED  
**Priority:** High (Quick Win)  
**Impact:** MEDIUM - Improves UX in match review interface

## Problem

The `getSystemName()` method in `MatchController` was a stub that always returned "Unknown" instead of querying the database for actual system names. This caused the match review UI to display "Unknown" for all systems instead of proper names like "NES", "PlayStation", "Genesis", etc.

## Solution

### Files Modified
1. **src/ui/controllers/match_controller.cpp**
   - Implemented database query in `getSystemName()` (lines 88-99)
   - Added `QSqlQuery` and `QSqlError` includes

### Implementation
```cpp
QString MatchController::getSystemName(int systemId) const
{
    QSqlQuery query(m_db->database());
    query.prepare("SELECT name FROM systems WHERE id = ?");
    query.addBindValue(systemId);
    
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    
    qWarning() << "Failed to find system name for ID:" << systemId;
    return "Unknown";
}
```

### Database Schema Used
Queries the `systems` table created during database initialization:
```sql
CREATE TABLE systems (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,          -- "NES", "SNES", "PlayStation"
    display_name TEXT NOT NULL,
    manufacturer TEXT,
    extensions TEXT NOT NULL,
    preferred_hash TEXT NOT NULL,
    ...
);
```

## Verification

### New Test Created
**File:** `tests/test_system_name_resolution.cpp`

**Test Coverage:**
- Insert test system ("NES") into database
- Query system name using the same SQL as `getSystemName()`
- Verify retrieved name matches inserted name
- Validate query executes without errors

### Test Results
```
Test project /home/solon/Documents/remus/build
    Start 1: ConstantsTest
1/3 Test #1: ConstantsTest ....................   Passed    0.01 sec
    Start 2: CacheDeserializationTest
2/3 Test #2: CacheDeserializationTest .........   Passed    0.03 sec
    Start 3: SystemNameResolutionTest
3/3 Test #3: SystemNameResolutionTest .........   Passed    0.02 sec

100% tests passed, 0 tests failed out of 3
```

✅ All tests passing

## Impact

### Before Fix
```
Match Review UI:
  File: Super Mario Bros.nes
  System: Unknown          ← Always "Unknown"
  Match: Super Mario Bros
```

### After Fix
```
Match Review UI:
  File: Super Mario Bros.nes
  System: NES              ← Correct system name from database
  Match: Super Mario Bros
```

### User Experience Improvements
- **Clarity:** Users can instantly see which system each ROM belongs to
- **Confidence:** Proper system names increase trust in metadata matching
- **Organization:** Easier to identify mismatched systems at a glance
- **Debugging:** Developers can see actual system IDs being used

### Usage Context
The `getSystemName()` method is called from two key places in match workflow:

1. **Hash-based matching** (line 45):
   ```cpp
   QString systemName = getSystemName(file.systemId);
   GameMetadata metadata = m_orchestrator->getByHashWithFallback(hash, systemName);
   ```

2. **Name-based matching** (line 58):
   ```cpp
   QString systemName = getSystemName(file.systemId);
   GameMetadata metadata = m_orchestrator->searchWithFallback("", cleanName, systemName);
   ```

Both paths now receive correct system names, improving metadata provider queries.

## Code Quality

### Error Handling
- Returns "Unknown" gracefully if system ID not found
- Logs warning message for debugging: `"Failed to find system name for ID: X"`
- No crashes or exceptions on invalid IDs

### Performance
- Simple indexed SELECT query (PRIMARY KEY lookup)
- O(1) complexity for database lookup
- Minimal overhead added to matching workflow
- Result could be cached if performance becomes an issue

### Maintainability
- Follows existing pattern used throughout codebase
- Simple, readable SQL query
- Self-documenting with clear variable names
- Minimal code complexity (8 lines)

## Next Steps

With P0 and first P1 item complete, recommended next fixes:
1. **#6 - Levenshtein matching** (3-4 hrs) - Improves metadata matching accuracy
2. **#8 - ScreenScraper artwork** (2-3 hrs) - Completes metadata provider layer
3. **#2 - Undo operations** (6-8 hrs) - High-value UX improvement

## Related Changes

This fix complements other database query patterns in the codebase:
- Similar to how `FileListModel` queries file metadata
- Follows pattern established in `Database::getAllFiles()`
- Could be extended to return `display_name` for prettier UI labels

## Related Documentation

- [docs/NEXT-STEPS.md](NEXT-STEPS.md) - Updated to mark #5 complete
- [docs/data-model.md](data-model.md) - Systems table schema
- [src/core/database.cpp](../src/core/database.cpp) - Systems table creation

---

**Estimated Effort:** 30 minutes  
**Actual Effort:** ~25 minutes (including testing)  
**Complexity:** Low (simple database query)
