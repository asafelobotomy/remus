#!/usr/bin/env bash
# Full local audit runner for Remus.
# Handles Arch/CachyOS-specific tool paths; mirrors the CI pipeline.
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR" || exit 1

# ── Colour helpers ──────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'
BOLD='\033[1m'; NC='\033[0m'
pass() { echo -e "  ${GREEN}PASS${NC}  $*"; }
fail() { echo -e "  ${RED}FAIL${NC}  $*"; FAILURES+=("$*"); }
warn() { echo -e "  ${YELLOW}WARN${NC}  $*"; }
info() { echo -e "  ${BLUE}INFO${NC}  $*"; }
section() { echo -e "\n${BOLD}══ $* ══${NC}"; }

FAILURES=()

# ── Tool discovery ──────────────────────────────────────────────────────────
# Arch ships clang-format as /usr/bin/clang-format (not clang-format-22).
CLANG_FORMAT="clang-format"
if command -v clang-format-22 &>/dev/null; then
    CLANG_FORMAT="clang-format-22"
fi

# Arch ships qmllint under /usr/lib/qt6/bin/
if ! command -v qmllint &>/dev/null; then
    for candidate in /usr/lib/qt6/bin/qmllint /usr/bin/qmllint; do
        if [[ -x "$candidate" ]]; then
            qmllint_dir="$(dirname "$candidate")"
            export PATH="${qmllint_dir}:${PATH}"
            break
        fi
    done
fi

# ── 0. Dependency check ─────────────────────────────────────────────────────
section "0. Dependency check"
REQUIRED_TOOLS=(cmake ctest c++ "$CLANG_FORMAT" shellcheck qmllint sqlite3 lcov 7z zip)
OPTIONAL_TOOLS=(chdman clang-tidy ccache)
ALL_GOOD=1
for t in "${REQUIRED_TOOLS[@]}"; do
    if command -v "$t" &>/dev/null; then
        info "$t → $(command -v "$t")"
    else
        warn "$t → MISSING (required)"
        ALL_GOOD=0
    fi
done
for t in "${OPTIONAL_TOOLS[@]}"; do
    if command -v "$t" &>/dev/null; then
        info "$t → $(command -v "$t") [optional]"
    else
        info "$t → missing [optional; some tests may skip]"
    fi
done
if [[ $ALL_GOOD -eq 0 ]]; then
    echo -e "\n${RED}Some required tools are missing. Install them and re-run.${NC}"
    exit 1
fi

# ── 1. Release build ─────────────────────────────────────────────────────────
section "1. Release build"
BUILD_DIR="build"
if cmake -S . -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DREMUS_ENABLE_WARNINGS=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -Wno-dev 2>&1; then
    pass "cmake configure (Release)"
else
    fail "cmake configure (Release)"
fi

if cmake --build "$BUILD_DIR" -j"$(nproc)" 2>&1; then
    pass "cmake build"
else
    fail "cmake build"
fi

# ── 2. Tests ──────────────────────────────────────────────────────────────────
section "2. Test suite"
if [[ -d "$BUILD_DIR" ]]; then
    CTEST_OUT=$(ctest --test-dir "$BUILD_DIR" --output-on-failure 2>&1)
    echo "$CTEST_OUT" | tail -20
    TESTS_PASSED=$(echo "$CTEST_OUT" | grep -oP '\d+(?= tests? passed)' | tail -1 || true)
    TESTS_FAILED=$(echo "$CTEST_OUT" | grep -oP '\d+(?= tests? failed)' | tail -1 || true)
    TESTS_SKIPPED=$(echo "$CTEST_OUT" | grep -oP '\d+(?= tests? skipped)' | tail -1 || true)
    info "Passed: ${TESTS_PASSED:-?}  Failed: ${TESTS_FAILED:-0}  Skipped: ${TESTS_SKIPPED:-0}"
    if [[ "${TESTS_FAILED:-0}" -eq 0 ]]; then
        pass "All tests passed"
    else
        fail "Test failures: $TESTS_FAILED"
    fi
else
    warn "Build dir missing; skipping tests"
fi

# ── 3. clang-format ──────────────────────────────────────────────────────────
section "3. clang-format (format lint)"
CF_VERSION=$("$CLANG_FORMAT" --version 2>/dev/null | head -1)
info "Using: $CF_VERSION"
mapfile -t cpp_files < <(find src tests -type f \( -name '*.cpp' -o -name '*.h' \) | sort)
info "Checking ${#cpp_files[@]} files"
if "$CLANG_FORMAT" --dry-run --Werror "${cpp_files[@]}" 2>&1; then
    pass "clang-format: no drift"
else
    fail "clang-format: files need reformatting"
fi

# ── 4. shellcheck ────────────────────────────────────────────────────────────
section "4. shellcheck"
mapfile -t sh_files < <(find .github/scripts scripts -name '*.sh' -type f | sort)
info "Checking ${#sh_files[@]} shell scripts"
if shellcheck --severity=warning "${sh_files[@]}" 2>&1; then
    pass "shellcheck: no warnings"
else
    fail "shellcheck: warnings found"
fi

# ── 5. qmllint ───────────────────────────────────────────────────────────────
section "5. qmllint"
if bash .github/scripts/run-qmllint.sh 2>&1; then
    pass "qmllint: no hard errors"
else
    fail "qmllint: hard errors found"
fi

# ── 6. Compendium bootstrap validation ──────────────────────────────────────
section "6. Compendium bootstrap"
if bash scripts/setup_compendium_db.sh 2>&1; then
    if bash .github/scripts/validate-compendium-db.sh \
            data/compendium/remus_compendium.db \
            data/compendium/validation/0000_bootstrap_checks.sql 2>&1 \
       && bash .github/scripts/validate-compendium-db.sh \
            data/compendium/remus_compendium.db \
            data/compendium/validation/0004_disc_set_checks.sql 2>&1; then
        pass "Compendium bootstrap validation (0000 + 0004)"
    else
        fail "Compendium bootstrap validation"
    fi
else
    fail "setup_compendium_db.sh failed"
fi

# ── 7. Coverage build ────────────────────────────────────────────────────────
section "7. Coverage"
BUILD_COV="build-coverage"
if cmake --preset coverage -Wno-dev 2>&1 \
   && cmake --build "$BUILD_COV" -j"$(nproc)" 2>&1 \
   && ctest --test-dir "$BUILD_COV" --output-on-failure 2>&1; then
    lcov --capture --directory "$BUILD_COV" \
         --output-file "$BUILD_COV/coverage.info" \
         --rc lcov_branch_coverage=0 2>&1
    lcov --remove "$BUILD_COV/coverage.info" '/usr/*' '*/tests/*' \
         --output-file "$BUILD_COV/coverage.info" 2>&1
    COVERAGE_THRESHOLD=54 bash .github/scripts/check-coverage-threshold.sh \
        "$BUILD_COV/coverage.info" 2>&1 && pass "Coverage ≥ 54%" || fail "Coverage below threshold"
    genhtml "$BUILD_COV/coverage.info" \
            --output-directory "$BUILD_COV/coverage-html" 2>&1 | tail -3
    info "HTML report: $ROOT_DIR/$BUILD_COV/coverage-html/index.html"
else
    fail "Coverage build or test run failed"
fi

# ── 8. clang-tidy spot-check ─────────────────────────────────────────────────
section "8. clang-tidy (informational)"
if command -v clang-tidy &>/dev/null && [[ -f "$BUILD_DIR/compile_commands.json" ]]; then
    mapfile -t tidy_sources < <(find src/core -name '*.cpp' -type f | sort | head -20)
    info "Tidying ${#tidy_sources[@]} core sources"
    clang-tidy -p "$BUILD_DIR" "${tidy_sources[@]}" 2>&1 | tail -30 && \
        pass "clang-tidy: no issues in spot-check" || \
        warn "clang-tidy: issues found (informational, does not block)"
else
    warn "clang-tidy skipped (missing tool or compile_commands.json)"
fi

# ── 9. Sanitizer build ───────────────────────────────────────────────────────
section "9. Sanitizer build (ASan + UBSan)"
BUILD_SAN="build-asan"
if cmake --preset asan -Wno-dev 2>&1 \
   && cmake --build "$BUILD_SAN" -j"$(nproc)" 2>&1; then
    if ASAN_OPTIONS=detect_leaks=0 ctest --test-dir "$BUILD_SAN" --output-on-failure 2>&1; then
        pass "Sanitizer build + tests"
    else
        fail "Sanitizer tests failed"
    fi
else
    fail "Sanitizer build failed"
fi

# ── Summary ──────────────────────────────────────────────────────────────────
section "Summary"
echo ""
if [[ ${#FAILURES[@]} -eq 0 ]]; then
    echo -e "${GREEN}${BOLD}All audit checks passed.${NC}"
else
    echo -e "${RED}${BOLD}${#FAILURES[@]} check(s) failed:${NC}"
    for f in "${FAILURES[@]}"; do
        echo -e "  ${RED}✗${NC} $f"
    done
    exit 1
fi
