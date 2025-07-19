#!/bin/bash
# Comprehensive UI options test suite for waitlock
# Tests all command-line options and their combinations

set -e

WAITLOCK="./waitlock"
LOCK_DIR="/var/lock/waitlock"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== WaitLock UI Options Test Suite ===${NC}"

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Test helper functions
test_start() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -e "\n${BLUE}[UI_TEST $TOTAL_TESTS] $1${NC}"
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
    pkill -f "waitlock.*ui_test" 2>/dev/null || true
    rm -f "$LOCK_DIR"/ui_test*.lock 2>/dev/null || true
    sleep 1
}

# Set cleanup trap
trap cleanup EXIT

echo -e "\n${BLUE}=== BASIC OPTION TESTS ===${NC}"

# Test 1: Help option
test_start "Help option (--help)"
if $WAITLOCK --help >/dev/null 2>&1; then
    test_pass "Help option works"
else
    test_fail "Help option failed"
fi

# Test 2: Version option
test_start "Version option (--version)"
if $WAITLOCK --version >/dev/null 2>&1; then
    test_pass "Version option works"
else
    test_fail "Version option failed"
fi

# Test 3: Invalid option
test_start "Invalid option handling"
if $WAITLOCK --invalid-option 2>/dev/null; then
    test_fail "Should reject invalid options"
else
    test_pass "Properly rejects invalid options"
fi

echo -e "\n${BLUE}=== TIMEOUT OPTION TESTS ===${NC}"

# Test 4: Timeout with valid values
test_start "Timeout option with valid values"
cleanup
VALUES=("0" "0.5" "1.0" "5" "10.5")
for val in "${VALUES[@]}"; do
    if timeout 3 $WAITLOCK --timeout "$val" ui_test_timeout_valid &>/dev/null; then
        test_pass "Timeout $val accepted"
    else
        test_fail "Timeout $val rejected"
    fi
done

# Test 5: Timeout with invalid values
test_start "Timeout option with invalid values"
INVALID_VALUES=("-1" "abc" "1.2.3" "")
for val in "${INVALID_VALUES[@]}"; do
    if $WAITLOCK --timeout "$val" ui_test_timeout_invalid 2>/dev/null; then
        test_fail "Should reject timeout value: $val"
    else
        test_pass "Properly rejects invalid timeout: $val"
    fi
done

echo -e "\n${BLUE}=== SEMAPHORE OPTION TESTS ===${NC}"

# Test 6: allowMultiple with valid values
test_start "allowMultiple option with valid values"
cleanup
VALUES=("1" "2" "5" "10")
for val in "${VALUES[@]}"; do
    if timeout 3 $WAITLOCK --allowMultiple "$val" ui_test_multi_valid &>/dev/null; then
        test_pass "allowMultiple $val accepted"
    else
        test_fail "allowMultiple $val rejected"
    fi
done

# Test 7: allowMultiple with invalid values
test_start "allowMultiple option with invalid values"
INVALID_VALUES=("0" "-1" "abc" "1.5" "")
for val in "${INVALID_VALUES[@]}"; do
    if $WAITLOCK --allowMultiple "$val" ui_test_multi_invalid 2>/dev/null; then
        test_fail "Should reject allowMultiple value: $val"
    else
        test_pass "Properly rejects invalid allowMultiple: $val"
    fi
done

echo -e "\n${BLUE}=== CPU-BASED LOCKING TESTS ===${NC}"

# Test 8: onePerCPU option
test_start "onePerCPU option"
cleanup
if timeout 3 $WAITLOCK --onePerCPU ui_test_percpu &>/dev/null; then
    test_pass "onePerCPU option accepted"
else
    test_fail "onePerCPU option rejected"
fi

# Test 9: excludeCPUs option
test_start "excludeCPUs option"
cleanup
VALUES=("0" "1" "2")
for val in "${VALUES[@]}"; do
    if timeout 3 $WAITLOCK --onePerCPU --excludeCPUs "$val" ui_test_exclude &>/dev/null; then
        test_pass "excludeCPUs $val accepted (with onePerCPU)"
    else
        test_fail "excludeCPUs $val rejected (with onePerCPU)"
    fi
done

# Test 10: excludeCPUs without onePerCPU (should work)
test_start "excludeCPUs without onePerCPU"
cleanup
if timeout 3 $WAITLOCK --excludeCPUs "1" ui_test_exclude_no_percpu &>/dev/null; then
    test_pass "excludeCPUs works without onePerCPU"
else
    test_fail "excludeCPUs should work without onePerCPU"
fi

echo -e "\n${BLUE}=== MODE OPTION TESTS ===${NC}"

# Test 11: Check mode
test_start "Check mode (--check)"
cleanup
# First test on non-existent lock
if $WAITLOCK --check ui_test_check_nonexistent >/dev/null 2>&1; then
    test_pass "Check mode works on non-existent lock"
else
    test_fail "Check mode failed on non-existent lock"
fi

# Test 12: List mode
test_start "List mode (--list)"
if $WAITLOCK --list >/dev/null 2>&1; then
    test_pass "List mode works"
else
    test_fail "List mode failed"
fi

# Test 13: List with format options
test_start "List mode with format options"
FORMATS=("human" "csv" "null")
for fmt in "${FORMATS[@]}"; do
    if $WAITLOCK --list --format "$fmt" >/dev/null 2>&1; then
        test_pass "List format $fmt accepted"
    else
        test_fail "List format $fmt rejected"
    fi
done

# Test 14: Invalid format
test_start "List mode with invalid format"
if $WAITLOCK --list --format "invalid" 2>/dev/null; then
    test_fail "Should reject invalid format"
else
    test_pass "Properly rejects invalid format"
fi

# Test 15: List with all option
test_start "List mode with --all option"
if $WAITLOCK --list --all >/dev/null 2>&1; then
    test_pass "List --all option works"
else
    test_fail "List --all option failed"
fi

# Test 16: List with stale-only option  
test_start "List mode with --stale-only option"
if $WAITLOCK --list --stale-only >/dev/null 2>&1; then
    test_pass "List --stale-only option works"
else
    test_fail "List --stale-only option failed"
fi

echo -e "\n${BLUE}=== EXEC MODE TESTS ===${NC}"

# Test 17: Exec mode with simple command
test_start "Exec mode with simple command"
cleanup
if timeout 5 $WAITLOCK --exec "echo test" ui_test_exec >/dev/null 2>&1; then
    test_pass "Exec mode with echo works"
else
    test_fail "Exec mode with echo failed"
fi

# Test 18: Exec mode with complex command
test_start "Exec mode with complex command"
cleanup
if timeout 5 $WAITLOCK --exec "ls /tmp | head -1" ui_test_exec_complex >/dev/null 2>&1; then
    test_pass "Exec mode with complex command works"
else
    test_fail "Exec mode with complex command failed"
fi

# Test 19: Exec mode with nonexistent command
test_start "Exec mode with nonexistent command"
cleanup
if $WAITLOCK --exec "nonexistent_command_12345" ui_test_exec_bad 2>/dev/null; then
    test_fail "Should reject nonexistent command"
else
    test_pass "Properly handles nonexistent command"
fi

echo -e "\n${BLUE}=== OUTPUT CONTROL TESTS ===${NC}"

# Test 20: Quiet mode
test_start "Quiet mode (--quiet)"
cleanup
OUTPUT=$(timeout 3 $WAITLOCK --quiet ui_test_quiet 2>&1 &)
if [ -z "$OUTPUT" ]; then
    test_pass "Quiet mode suppresses output"
else
    test_fail "Quiet mode should suppress output"
fi

# Test 21: Verbose mode  
test_start "Verbose mode (--verbose)"
cleanup
if timeout 3 $WAITLOCK --verbose ui_test_verbose &>/dev/null; then
    test_pass "Verbose mode accepted"
else
    test_fail "Verbose mode rejected"
fi

echo -e "\n${BLUE}=== DIRECTORY OPTION TESTS ===${NC}"

# Test 22: Custom lock directory
test_start "Custom lock directory (--lock-dir)"
TEMP_DIR="/tmp/waitlock_ui_test_$$"
mkdir -p "$TEMP_DIR"
if timeout 3 $WAITLOCK --lock-dir "$TEMP_DIR" ui_test_lockdir &>/dev/null; then
    test_pass "Custom lock directory accepted"
else
    test_fail "Custom lock directory rejected"
fi
rm -rf "$TEMP_DIR"

# Test 23: Invalid lock directory
test_start "Invalid lock directory"
if $WAITLOCK --lock-dir "/nonexistent/invalid/path" ui_test_bad_lockdir 2>/dev/null; then
    test_fail "Should reject invalid lock directory"
else
    test_pass "Properly rejects invalid lock directory"
fi

echo -e "\n${BLUE}=== SYSLOG OPTION TESTS ===${NC}"

# Test 24: Syslog option
test_start "Syslog option (--syslog)"
cleanup
if timeout 3 $WAITLOCK --syslog ui_test_syslog &>/dev/null; then
    test_pass "Syslog option accepted"
else
    test_fail "Syslog option rejected"
fi

# Test 25: Syslog facility options
test_start "Syslog facility options"
FACILITIES=("daemon" "local0" "local1" "local7")
for facility in "${FACILITIES[@]}"; do
    if timeout 3 $WAITLOCK --syslog --syslog-facility "$facility" ui_test_facility &>/dev/null; then
        test_pass "Syslog facility $facility accepted"
    else
        test_fail "Syslog facility $facility rejected"
    fi
done

echo -e "\n${BLUE}=== OPTION COMBINATION TESTS ===${NC}"

# Test 26: Multiple compatible options
test_start "Multiple compatible options"
cleanup
if timeout 5 $WAITLOCK --timeout 2.0 --allowMultiple 2 --verbose ui_test_combo1 &>/dev/null; then
    test_pass "Compatible option combination works"
else
    test_fail "Compatible option combination failed"
fi

# Test 27: Conflicting modes (check + exec)
test_start "Conflicting modes (check + exec)"
if $WAITLOCK --check --exec "echo test" ui_test_conflict 2>/dev/null; then
    test_fail "Should reject conflicting modes"
else
    test_pass "Properly rejects conflicting modes"
fi

# Test 28: stdin descriptor input
test_start "Stdin descriptor input"
cleanup
if echo "ui_test_stdin" | timeout 3 $WAITLOCK --timeout 1.0 &>/dev/null; then
    test_pass "Stdin descriptor input works"
else
    test_fail "Stdin descriptor input failed"
fi

echo -e "\n${BLUE}=== DESCRIPTOR VALIDATION TESTS ===${NC}"

# Test 29: Valid descriptors
test_start "Valid descriptor formats"
VALID_DESCRIPTORS=("test123" "test_name" "test-name" "test.name" "a" "123")
for desc in "${VALID_DESCRIPTORS[@]}"; do
    if timeout 3 $WAITLOCK --timeout 0.1 "$desc" &>/dev/null; then
        test_pass "Valid descriptor '$desc' accepted"
    else
        test_fail "Valid descriptor '$desc' rejected"
    fi
done

# Test 30: Invalid descriptors
test_start "Invalid descriptor formats"
INVALID_DESCRIPTORS=("test@name" "test name" "test/name" "test:name" "")
for desc in "${INVALID_DESCRIPTORS[@]}"; do
    if $WAITLOCK --timeout 0.1 "$desc" 2>/dev/null; then
        test_fail "Should reject invalid descriptor '$desc'"
    else
        test_pass "Properly rejects invalid descriptor '$desc'"
    fi
done

# Test 31: Long descriptor
test_start "Long descriptor handling"
LONG_DESC=$(printf 'a%.0s' {1..300})  # 300 character descriptor
if $WAITLOCK --timeout 0.1 "$LONG_DESC" 2>/dev/null; then
    test_fail "Should reject too-long descriptor"
else
    test_pass "Properly rejects too-long descriptor"
fi

# Final summary
echo -e "\n${BLUE}=== UI OPTIONS TEST SUMMARY ===${NC}"
echo -e "Total tests: $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "\n${GREEN}🎉 ALL UI OPTION TESTS PASSED! 🎉${NC}"
    exit 0
else
    echo -e "\n${RED}❌ Some UI option tests failed${NC}"
    exit 1
fi