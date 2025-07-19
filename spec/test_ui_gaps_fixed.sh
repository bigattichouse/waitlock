#!/bin/bash
# Focused UI options test suite covering gaps in existing test coverage

set -e

WAITLOCK="./waitlock"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== WaitLock UI Coverage Gap Tests ===${NC}"

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

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

echo -e "\n${BLUE}=== CPU-BASED LOCKING OPTIONS ===${NC}"

# Test 1: onePerCPU option
test_start "onePerCPU option parsing"
if timeout 3 $WAITLOCK --onePerCPU gap_test_percpu >/dev/null 2>&1; then
    test_pass "onePerCPU option accepted"
else
    test_fail "onePerCPU option rejected"
fi

# Test 2: excludeCPUs option  
test_start "excludeCPUs option parsing"
if timeout 3 $WAITLOCK --excludeCPUs 1 gap_test_exclude >/dev/null 2>&1; then
    test_pass "excludeCPUs option accepted"
else
    test_fail "excludeCPUs option rejected"
fi

echo -e "\n${BLUE}=== OUTPUT CONTROL OPTIONS ===${NC}"

# Test 3: Quiet flag
test_start "Quiet flag parsing"
if timeout 3 $WAITLOCK -q gap_test_quiet >/dev/null 2>&1; then
    test_pass "Quiet flag (-q) accepted"
else
    test_fail "Quiet flag (-q) rejected"
fi

if timeout 3 $WAITLOCK --quiet gap_test_quiet2 >/dev/null 2>&1; then
    test_pass "Quiet flag (--quiet) accepted"
else
    test_fail "Quiet flag (--quiet) rejected"
fi

# Test 4: Verbose flag
test_start "Verbose flag parsing"
if timeout 3 $WAITLOCK -v gap_test_verbose >/dev/null 2>&1; then
    test_pass "Verbose flag (-v) accepted"
else
    test_fail "Verbose flag (-v) rejected"
fi

if timeout 3 $WAITLOCK --verbose gap_test_verbose2 >/dev/null 2>&1; then
    test_pass "Verbose flag (--verbose) accepted"
else
    test_fail "Verbose flag (--verbose) rejected"
fi

echo -e "\n${BLUE}=== DIRECTORY OPTIONS ===${NC}"

# Test 5: Lock directory option
test_start "Lock directory option parsing"
TEMP_DIR="/tmp/waitlock_test_$$"
mkdir -p "$TEMP_DIR"

if timeout 3 $WAITLOCK -d "$TEMP_DIR" gap_test_dir >/dev/null 2>&1; then
    test_pass "Lock directory (-d) accepted"
else
    test_fail "Lock directory (-d) rejected"
fi

if timeout 3 $WAITLOCK --lock-dir "$TEMP_DIR" gap_test_dir2 >/dev/null 2>&1; then
    test_pass "Lock directory (--lock-dir) accepted"
else
    test_fail "Lock directory (--lock-dir) rejected"
fi

rm -rf "$TEMP_DIR"

echo -e "\n${BLUE}=== SYSLOG OPTIONS ===${NC}"

# Test 6: Syslog options
test_start "Syslog option parsing"
if timeout 3 $WAITLOCK --syslog gap_test_syslog >/dev/null 2>&1; then
    test_pass "Syslog flag accepted"
else
    test_fail "Syslog flag rejected"
fi

if timeout 3 $WAITLOCK --syslog --syslog-facility daemon gap_test_facility >/dev/null 2>&1; then
    test_pass "Syslog facility option accepted"
else
    test_fail "Syslog facility option rejected"
fi

echo -e "\n${BLUE}=== OPTION COMBINATIONS ===${NC}"

# Test 7: Incompatible combinations
test_start "Incompatible option combinations"
if $WAITLOCK --check --list gap_test_conflict 2>/dev/null; then
    test_fail "Should reject --check + --list"
else
    test_pass "Properly rejects --check + --list"
fi

if $WAITLOCK --done --exec "echo test" gap_test_conflict2 2>/dev/null; then
    test_fail "Should reject --done + --exec"
else
    test_pass "Properly rejects --done + --exec"
fi

# Test 8: Valid combinations
test_start "Valid option combinations"
if timeout 3 $WAITLOCK --timeout 1.0 --verbose gap_test_combo >/dev/null 2>&1; then
    test_pass "Valid combination accepted"
else
    test_fail "Valid combination rejected"
fi

echo -e "\n${BLUE}=== EDGE CASES ===${NC}"

# Test 9: Long descriptor
test_start "Long descriptor validation"
LONG_DESC=$(printf 'a%.0s' {1..300})
if $WAITLOCK --timeout 0.1 "$LONG_DESC" 2>/dev/null; then
    test_fail "Should reject long descriptor"
else
    test_pass "Properly rejects long descriptor"
fi

# Test 10: Help/version precedence
test_start "Help/version with other options"
if $WAITLOCK --help --verbose >/dev/null 2>&1; then
    test_pass "Help with options works"
else
    test_fail "Help with options failed"
fi

if $WAITLOCK --version --timeout 30 >/dev/null 2>&1; then
    test_pass "Version with options works"  
else
    test_fail "Version with options failed"
fi

# Summary
echo -e "\n${BLUE}=== GAP TEST SUMMARY ===${NC}"
echo -e "Total tests: $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "\n${GREEN}🎉 ALL GAP TESTS PASSED! 🎉${NC}"
    exit 0
else
    echo -e "\n${RED}❌ Some gap tests failed${NC}"
    exit 1
fi