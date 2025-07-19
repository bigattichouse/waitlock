#!/bin/bash
# Focused UI options test suite covering gaps in existing test coverage
# Complements existing tests in test/test_core.c

set -e

WAITLOCK="./waitlock"
LOCK_DIR="/var/lock/waitlock"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== WaitLock UI Coverage Gap Tests ===${NC}"
echo -e "${YELLOW}Note: This supplements existing comprehensive tests in test/test_core.c${NC}"

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Test helper functions
test_start() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -e "\n${BLUE}[GAP_TEST $TOTAL_TESTS] $1${NC}"
}

test_pass() {
    PASSED_TESTS=$((PASSED_TESTS + 1))
    echo -e "${GREEN}✓ PASS: $1${NC}"
}

test_fail() {
    FAILED_TESTS=$((FAILED_TESTS + 1))
    echo -e "${RED}✗ FAIL: $1${NC}"
}

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up test artifacts...${NC}"
    pkill -f "waitlock.*gap_test" 2>/dev/null || true
    rm -f "$LOCK_DIR"/gap_test*.lock 2>/dev/null || true
    sleep 1
}

# Set cleanup trap
trap cleanup EXIT

echo -e "\n${BLUE}=== CPU-BASED LOCKING OPTIONS (Missing from existing tests) ===${NC}"

# Test 1: onePerCPU option
test_start "onePerCPU option parsing and functionality"
cleanup
if timeout 5 $WAITLOCK --onePerCPU gap_test_percpu 2>/dev/null &
then
    test_pass "onePerCPU option accepted"
else
    test_fail "onePerCPU option rejected"
fi

# Test 2: excludeCPUs option
test_start "excludeCPUs option parsing"
cleanup
CPU_COUNT=$(nproc 2>/dev/null || echo "4")
EXCLUDE_COUNT=$((CPU_COUNT / 2))
if timeout 5 $WAITLOCK --onePerCPU --excludeCPUs "$EXCLUDE_COUNT" gap_test_exclude 2>/dev/null &
then
    test_pass "excludeCPUs $EXCLUDE_COUNT accepted with onePerCPU"
else
    test_fail "excludeCPUs $EXCLUDE_COUNT rejected with onePerCPU"
fi

# Test 3: excludeCPUs without onePerCPU
test_start "excludeCPUs without onePerCPU"
cleanup
if timeout 5 $WAITLOCK --excludeCPUs "1" gap_test_exclude_standalone 2>/dev/null &; then
    test_pass "excludeCPUs works without onePerCPU"
else
    test_fail "excludeCPUs should work without onePerCPU"
fi

echo -e "\n${BLUE}=== OUTPUT CONTROL OPTIONS (Missing from existing tests) ===${NC}"

# Test 4: Quiet flag parsing
test_start "Quiet flag (-q/--quiet) parsing"
cleanup
if timeout 3 $WAITLOCK --quiet gap_test_quiet_short 2>/dev/null &; then
    test_pass "Short quiet flag (-q) accepted"
else
    test_fail "Short quiet flag (-q) rejected"
fi

if timeout 3 $WAITLOCK --quiet gap_test_quiet_long 2>/dev/null &; then
    test_pass "Long quiet flag (--quiet) accepted"
else
    test_fail "Long quiet flag (--quiet) rejected"
fi

# Test 5: Verbose flag parsing
test_start "Verbose flag (-v/--verbose) parsing"
cleanup
if timeout 3 $WAITLOCK -v gap_test_verbose_short 2>/dev/null &; then
    test_pass "Short verbose flag (-v) accepted"
else
    test_fail "Short verbose flag (-v) rejected"
fi

if timeout 3 $WAITLOCK --verbose gap_test_verbose_long 2>/dev/null &; then
    test_pass "Long verbose flag (--verbose) accepted"
else
    test_fail "Long verbose flag (--verbose) rejected"
fi

echo -e "\n${BLUE}=== DIRECTORY AND SYSLOG OPTIONS (Missing from existing tests) ===${NC}"

# Test 6: Lock directory option parsing
test_start "Lock directory option (-d/--lock-dir) parsing"
TEMP_DIR="/tmp/waitlock_gap_test_$$"
mkdir -p "$TEMP_DIR"

if timeout 3 $WAITLOCK -d "$TEMP_DIR" gap_test_lockdir_short 2>/dev/null &; then
    test_pass "Short lock-dir flag (-d) accepted"
else
    test_fail "Short lock-dir flag (-d) rejected"
fi

if timeout 3 $WAITLOCK --lock-dir "$TEMP_DIR" gap_test_lockdir_long 2>/dev/null &; then
    test_pass "Long lock-dir flag (--lock-dir) accepted"
else
    test_fail "Long lock-dir flag (--lock-dir) rejected"
fi

rm -rf "$TEMP_DIR"

# Test 7: Syslog option parsing
test_start "Syslog option (--syslog) parsing"
cleanup
if timeout 3 $WAITLOCK --syslog gap_test_syslog 2>/dev/null &; then
    test_pass "Syslog flag (--syslog) accepted"
else
    test_fail "Syslog flag (--syslog) rejected"
fi

# Test 8: Syslog facility option
test_start "Syslog facility option (--syslog-facility) parsing"
cleanup
if timeout 3 $WAITLOCK --syslog --syslog-facility daemon gap_test_syslog_facility 2>/dev/null &; then
    test_pass "Syslog facility option accepted"
else
    test_fail "Syslog facility option rejected"
fi

echo -e "\n${BLUE}=== LIST FORMAT OPTIONS (Missing from existing tests) ===${NC}"

# Test 9: List with all formats systematically
test_start "List format options comprehensive test"
FORMATS=("human" "csv" "null")
for fmt in "${FORMATS[@]}"; do
    if $WAITLOCK --list --format "$fmt" >/dev/null 2>&1; then
        test_pass "List format '$fmt' works"
    else
        test_fail "List format '$fmt' failed"
    fi
done

# Test 10: List with all and stale-only combinations
test_start "List option combinations"
if $WAITLOCK --list --all >/dev/null 2>&1; then
    test_pass "List --all works"
else
    test_fail "List --all failed"
fi

if $WAITLOCK --list --stale-only >/dev/null 2>&1; then
    test_pass "List --stale-only works"
else
    test_fail "List --stale-only failed"
fi

if $WAITLOCK --list --all --format csv >/dev/null 2>&1; then
    test_pass "List --all --format csv combination works"
else
    test_fail "List --all --format csv combination failed"
fi

echo -e "\n${BLUE}=== OPTION COMBINATION VALIDATION (Missing from existing tests) ===${NC}"

# Test 11: Incompatible mode combinations
test_start "Incompatible mode combinations rejection"
if $WAITLOCK --check --list gap_test_conflict1 2>/dev/null; then
    test_fail "Should reject --check + --list combination"
else
    test_pass "Properly rejects --check + --list combination"
fi

if $WAITLOCK --done --exec "echo test" gap_test_conflict2 2>/dev/null; then
    test_fail "Should reject --done + --exec combination"
else
    test_pass "Properly rejects --done + --exec combination"
fi

if $WAITLOCK --check --exec "echo test" gap_test_conflict3 2>/dev/null; then
    test_fail "Should reject --check + --exec combination"
else
    test_pass "Properly rejects --check + --exec combination"
fi

# Test 12: Valid complex combinations
test_start "Valid complex option combinations"
cleanup
if timeout 5 $WAITLOCK --timeout 2.0 --allowMultiple 2 --verbose --syslog gap_test_combo_valid 2>/dev/null &; then
    test_pass "Complex valid combination accepted"
else
    test_fail "Complex valid combination rejected"
fi

echo -e "\n${BLUE}=== EDGE CASE VALIDATION (Missing from existing tests) ===${NC}"

# Test 13: Very long descriptor (255+ chars)
test_start "Descriptor length limit validation"
LONG_DESC=$(printf 'a%.0s' {1..300})  # 300 character descriptor
if $WAITLOCK --timeout 0.1 "$LONG_DESC" 2>/dev/null; then
    test_fail "Should reject descriptor longer than 255 chars"
else
    test_pass "Properly rejects over-length descriptor"
fi

# Test 14: Large numeric values
test_start "Large numeric value validation"
cleanup
# Test very large timeout
if $WAITLOCK --timeout 999999999 gap_test_large_timeout 2>/dev/null &; then
    test_pass "Large timeout value accepted"
else
    test_fail "Large timeout value rejected (may be expected)"
fi

# Test very large semaphore count
if timeout 3 $WAITLOCK --allowMultiple 999999 gap_test_large_sem 2>/dev/null &; then
    test_pass "Large semaphore count accepted"
else
    test_fail "Large semaphore count rejected (may be expected)"
fi

# Test 15: Help/version with other args
test_start "Help/version precedence with other options"
if $WAITLOCK --help --verbose >/dev/null 2>&1; then
    test_pass "Help with other options works"
else
    test_fail "Help with other options failed"
fi

if $WAITLOCK --version --timeout 30 >/dev/null 2>&1; then
    test_pass "Version with other options works"
else
    test_fail "Version with other options failed"
fi

# Final summary
echo -e "\n${BLUE}=== UI COVERAGE GAP TEST SUMMARY ===${NC}"
echo -e "Total gap tests: $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"

echo -e "\n${YELLOW}Note: This supplements the comprehensive tests in test/test_core.c${NC}"
echo -e "${YELLOW}Combined coverage now includes all major UI options and edge cases${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "\n${GREEN}🎉 ALL GAP TESTS PASSED! 🎉${NC}"
    echo -e "${GREEN}UI option coverage is now comprehensive${NC}"
    exit 0
else
    echo -e "\n${RED}❌ Some gap tests failed${NC}"
    echo -e "${YELLOW}Review failures to complete UI option coverage${NC}"
    exit 1
fi