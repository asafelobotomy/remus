---
name: Test Files
applyTo: "**/tests/**,**/test_*.py,**/*_test.py"
description: "Conventions for test files in this workspace — framework, fixture approach, and verification discipline"
---

# Test File Instructions

## Execution scope

- Testing framework for this workspace: **CTest / Qt Test (QTest)** — run tests with `cmake --build build --target run_tests -- -j$(nproc)` then `cd build && ctest --output-on-failure`
- Run the single test module or test class that directly covers the changed code during intermediate work.
- Run the full suite at task completion or when any file imported by more than one test module is modified.
- Write tests alongside every code change.

## Authoring conventions

- Fixtures are self-contained in test methods — no external test data files unless the framework requires them.
- Use temporary directories for any test that needs a filesystem workspace; ensure cleanup is automatic.
- Test through public interfaces, not internal implementation details.
- Prefer real implementations over mocks; use framework-provided mocking only for I/O, network calls, or non-deterministic system calls.
- When fixing a bug, write a failing test first, then fix the code.
- Each test class or module covers one logical concern; test method or function names describe the expected behaviour, not the implementation.
