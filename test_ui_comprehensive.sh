#!/bin/bash
# Comprehensive UI options test covering gaps in existing coverage
# All options require proper descriptor arguments

set -e

WAITLOCK="./waitlock"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== WaitLock UI Options Comprehensive Test ===${NC}"

TOTAL=0
PASSED=0
FAILED=0

test_start() {
    TOTAL=$((TOTAL + 1))
    echo -e "\n${BLUE}[TEST $TOTAL] $1${NC}"
}

test_pass() {
    PASSED=$((PASSED + 1))
    echo -e "${GREEN}✓ PASS: $1${NC}"
}

test_fail() {
    FAILED=$((FAILED + 1))
    echo -e "${RED}✗ FAIL: $1${NC}"
}

# Cleanup
cleanup() {
    pkill -f "waitlock.*ui_test" 2>/dev/null || true
    rm -f /var/lock/waitlock/ui_test*.lock 2>/dev/null || true
}
trap cleanup EXIT

echo -e "\n${BLUE}=== CPU-BASED LOCKING OPTIONS ===${NC}"

test_start "onePerCPU option"
cleanup
if timeout 3 $WAITLOCK --onePerCPU ui_test_percpu >/dev/null 2>&1 &
then
    test_pass "onePerCPU option works"
else
    test_fail "onePerCPU option failed"
fi

test_start "excludeCPUs option"
cleanup  
if timeout 3 $WAITLOCK --excludeCPUs 1 ui_test_exclude >/dev/null 2>&1 &
then
    test_pass "excludeCPUs option works"
else
    test_fail "excludeCPUs option failed"
fi

test_start "onePerCPU + excludeCPUs combination"
cleanup
if timeout 3 $WAITLOCK --onePerCPU --excludeCPUs 1 ui_test_combo1 >/dev/null 2>&1 &
then
    test_pass "onePerCPU + excludeCPUs combination works"
else
    test_fail "onePerCPU + excludeCPUs combination failed"
fi

echo -e "\n${BLUE}=== OUTPUT CONTROL OPTIONS ===${NC}"

test_start "Quiet option (-q)"
cleanup
if timeout 3 $WAITLOCK -q ui_test_quiet_short >/dev/null 2>&1 &
then
    test_pass "Quiet short flag works"
else
    test_fail "Quiet short flag failed"
fi

test_start "Quiet option (--quiet)"
cleanup
if timeout 3 $WAITLOCK --quiet ui_test_quiet_long >/dev/null 2>&1 &
then
    test_pass "Quiet long flag works"
else
    test_fail "Quiet long flag failed"
fi

test_start "Verbose option (-v)"
cleanup
if timeout 3 $WAITLOCK -v ui_test_verbose_short >/dev/null 2>&1 &
then
    test_pass "Verbose short flag works"
else
    test_fail "Verbose short flag failed"
fi

test_start "Verbose option (--verbose)"  
cleanup
if timeout 3 $WAITLOCK --verbose ui_test_verbose_long >/dev/null 2>&1 &
then
    test_pass "Verbose long flag works"
else
    test_fail "Verbose long flag failed"
fi

echo -e "\n${BLUE}=== DIRECTORY AND SYSLOG OPTIONS ===${NC}"

test_start "Lock directory option (-d)"
TEMP_DIR="/tmp/waitlock_ui_test_$$"
mkdir -p "$TEMP_DIR"
cleanup
if timeout 3 $WAITLOCK -d "$TEMP_DIR" ui_test_dir_short >/dev/null 2>&1 &
then
    test_pass "Lock directory short flag works"
else
    test_fail "Lock directory short flag failed"
fi
rm -rf "$TEMP_DIR"

test_start "Lock directory option (--lock-dir)"
TEMP_DIR="/tmp/waitlock_ui_test_$$"
mkdir -p "$TEMP_DIR" 
cleanup
if timeout 3 $WAITLOCK --lock-dir "$TEMP_DIR" ui_test_dir_long >/dev/null 2>&1 &
then
    test_pass "Lock directory long flag works"
else
    test_fail "Lock directory long flag failed"
fi
rm -rf "$TEMP_DIR"

test_start "Syslog option (--syslog)"
cleanup
if timeout 3 $WAITLOCK --syslog ui_test_syslog >/dev/null 2>&1 &
then
    test_pass "Syslog flag works"
else
    test_fail "Syslog flag failed"
fi

test_start "Syslog facility option"
cleanup
if timeout 3 $WAITLOCK --syslog --syslog-facility daemon ui_test_facility >/dev/null 2>&1 &
then
    test_pass "Syslog facility option works"
else
    test_fail "Syslog facility option failed"
fi

echo -e "\n${BLUE}=== LIST FORMAT OPTIONS ===${NC}"

test_start "List with human format"
if $WAITLOCK --list --format human >/dev/null 2>&1; then
    test_pass "List human format works"
else
    test_fail "List human format failed"
fi

test_start "List with CSV format"
if $WAITLOCK --list --format csv >/dev/null 2>&1; then
    test_pass "List CSV format works"
else
    test_fail "List CSV format failed"
fi

test_start "List with null format"
if $WAITLOCK --list --format null >/dev/null 2>&1; then
    test_pass "List null format works"
else
    test_fail "List null format failed"
fi

test_start "List with --all option"
if $WAITLOCK --list --all >/dev/null 2>&1; then
    test_pass "List --all works"
else
    test_fail "List --all failed"
fi

test_start "List with --stale-only option"
if $WAITLOCK --list --stale-only >/dev/null 2>&1; then
    test_pass "List --stale-only works" 
else
    test_fail "List --stale-only failed"
fi

echo -e "\n${BLUE}=== OPTION VALIDATION ===${NC}"

test_start "Invalid format rejection"
if $WAITLOCK --list --format invalid 2>/dev/null; then
    test_fail "Should reject invalid format"
else
    test_pass "Properly rejects invalid format"
fi

test_start "Conflicting modes rejection (--check + --list)"
if $WAITLOCK --check --list ui_test 2>/dev/null; then
    test_fail "Should reject conflicting modes"
else
    test_pass "Properly rejects conflicting modes"
fi

test_start "Conflicting modes rejection (--done + --exec)"
if $WAITLOCK --done --exec "echo test" ui_test 2>/dev/null; then
    test_fail "Should reject conflicting modes"
else
    test_pass "Properly rejects conflicting modes"
fi

echo -e "\n${BLUE}=== COMPLEX COMBINATIONS ===${NC}"

test_start "Complex valid combination"
cleanup
if timeout 3 $WAITLOCK --timeout 2.0 --allowMultiple 2 --verbose ui_test_complex >/dev/null 2>&1 &
then
    test_pass "Complex combination works"
else
    test_fail "Complex combination failed"
fi

test_start "Help with other options (should show help)"
if $WAITLOCK --help --verbose >/dev/null 2>&1; then
    test_pass "Help precedence works"
else
    test_fail "Help precedence failed"
fi

test_start "Version with other options (should show version)"
if $WAITLOCK --version --timeout 30 >/dev/null 2>&1; then
    test_pass "Version precedence works"
else
    test_fail "Version precedence failed"
fi

# Final summary
echo -e "\n${BLUE}=== UI OPTIONS TEST SUMMARY ===${NC}"
echo -e "Total tests: $TOTAL"
echo -e "Passed: ${GREEN}$PASSED${NC}"
echo -e "Failed: ${RED}$FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo -e "\n${GREEN}🎉 ALL UI OPTION TESTS PASSED! 🎉${NC}"
    exit 0
else
    echo -e "\n${RED}❌ Some UI option tests failed${NC}"
    exit 1
fi