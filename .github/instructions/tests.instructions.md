---
name: Test Files
applyTo: "**/tests/**,**/test_*.py,**/*_test.py"
description: "Conventions for test files in this workspace — framework, fixture approach, and verification discipline"
---

# Test File Instructions

- Testing framework for this workspace: **(not detected)** — run tests with `(not detected)`
- Run the narrowest targeted test for a single changed module during intermediate work.
- Run the full suite at task completion or when shared helpers are touched.
- Fixtures are self-contained in test methods — no external test data files unless the framework requires them.
- Use temporary directories for any test that needs a filesystem workspace; ensure cleanup is automatic.
- Test through public interfaces, not internal implementation details.
- When fixing a bug, write a failing test first, then fix the code.
- Each test class or module covers one logical concern; test method or function names describe the expected behaviour, not the implementation.
- Always — write tests alongside every code change.
