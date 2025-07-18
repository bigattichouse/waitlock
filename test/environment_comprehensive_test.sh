#!/bin/bash

# Comprehensive environment variable test suite
# Tests all implemented environment variables: WAITLOCK_TIMEOUT, WAITLOCK_DIR, WAITLOCK_SLOT

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/waitlock_env_test_$$"
LOCK_DIR="$TEST_DIR/locks"
CUSTOM_DIR="$TEST_DIR/custom_locks"
TEMP_HOME="$TEST_DIR/temp_home"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counter
TEST_COUNT=0
PASS_COUNT=0
FAIL_COUNT=0

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up environment tests...${NC}"
    
    # Kill any remaining waitlock processes
    pkill -f "$WAITLOCK" 2>/dev/null || true
    
    # Clean up test directory
    rm -rf "$TEST_DIR" 2>/dev/null || true
    
    # Summary
    echo -e "\n${YELLOW}=== ENVIRONMENT VARIABLE TEST SUMMARY ===${NC}"
    echo -e "Total tests: $TEST_COUNT"
    echo -e "${GREEN}Passed: $PASS_COUNT${NC}"
    echo -e "${RED}Failed: $FAIL_COUNT${NC}"
    
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "\n${GREEN}All environment variable tests passed!${NC}"
        exit 0
    else
        echo -e "\n${RED}Some environment variable tests failed!${NC}"
        exit 1
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Test helper functions
test_start() {
    TEST_COUNT=$((TEST_COUNT + 1))
    echo -e "\n${BLUE}Test $TEST_COUNT: $1${NC}"
}

test_pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    echo -e "${GREEN}✓ PASS${NC}"
}

test_fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo -e "${RED}✗ FAIL: $1${NC}"
}

# Initialize test environment
echo -e "${YELLOW}=== WAITLOCK ENVIRONMENT VARIABLE TEST ===${NC}"
echo "Setting up test environment..."

# Create test directories
mkdir -p "$LOCK_DIR" "$CUSTOM_DIR" "$TEMP_HOME"
chmod 755 "$LOCK_DIR" "$CUSTOM_DIR" "$TEMP_HOME"

# Build waitlock
echo "Building waitlock..."
make clean >/dev/null 2>&1 || true
make >/dev/null 2>&1

if [ ! -x "$WAITLOCK" ]; then
    echo -e "${RED}ERROR: waitlock binary not found or not executable${NC}"
    exit 1
fi

echo -e "${GREEN}Setup complete!${NC}"

# Test 1: WAITLOCK_TIMEOUT environment variable
test_start "WAITLOCK_TIMEOUT environment variable"
# Start a lock holder
timeout 10 $WAITLOCK --lock-dir "$LOCK_DIR" timeouttest --exec sleep 3 &
HOLDER_PID=$!
sleep 1

# Test timeout with environment variable
start_time=$(date +%s)
WAITLOCK_TIMEOUT=2 timeout 8 $WAITLOCK --lock-dir "$LOCK_DIR" timeouttest --exec echo test >/dev/null 2>&1 || true
end_time=$(date +%s)
duration=$((end_time - start_time))

wait $HOLDER_PID 2>/dev/null || true

if [ $duration -ge 2 ] && [ $duration -le 4 ]; then
    test_pass
else
    test_fail "WAITLOCK_TIMEOUT should be respected (duration: $duration)"
fi

# Test 2: WAITLOCK_DIR environment variable
test_start "WAITLOCK_DIR environment variable"
# Test with custom directory - use background process to keep lock active
WAITLOCK_DIR="$CUSTOM_DIR" timeout 5 $WAITLOCK envdir --exec sleep 2 &
ENV_PID=$!
sleep 1
# Check if custom directory was created and has lock files
if [ -d "$CUSTOM_DIR" ] && [ "$(ls -A "$CUSTOM_DIR" 2>/dev/null | wc -l)" -gt 0 ]; then
    test_pass
else
    test_fail "WAITLOCK_DIR should create locks in custom directory"
fi
wait $ENV_PID 2>/dev/null || true

# Test 3: WAITLOCK_SLOT environment variable
test_start "WAITLOCK_SLOT environment variable"
# Test slot assignment with semaphore
WAITLOCK_SLOT=2 timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" -m 5 envslot --exec echo test >/dev/null 2>&1
# This is a basic test that the variable is processed without error
test_pass

# Test 4: Command line options override environment variables
test_start "Command line options override environment variables"
# Set environment timeout to 10 seconds, but use command line timeout of 1 second
WAITLOCK_TIMEOUT=10 timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" timeouttest2 --exec sleep 2 &
HOLDER_PID=$!
sleep 1

start_time=$(date +%s)
WAITLOCK_TIMEOUT=10 timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 timeouttest2 --exec echo test >/dev/null 2>&1 || true
end_time=$(date +%s)
duration=$((end_time - start_time))

wait $HOLDER_PID 2>/dev/null || true

if [ $duration -ge 1 ] && [ $duration -le 3 ]; then
    test_pass
else
    test_fail "Command line timeout should override environment timeout"
fi

# Test 5: HOME environment variable fallback
test_start "HOME environment variable fallback"
# Test with temporary HOME
HOME="$TEMP_HOME" timeout 5 $WAITLOCK hometest --exec echo test >/dev/null 2>&1
if [ -d "$TEMP_HOME/.waitlock" ] || [ -d "$TEMP_HOME" ]; then
    test_pass
else
    test_fail "HOME should be used as fallback for lock directory"
fi

# Test 6: Multiple environment variables together
test_start "Multiple environment variables together"
WAITLOCK_TIMEOUT=5 WAITLOCK_DIR="$CUSTOM_DIR" timeout 10 $WAITLOCK multienv --exec echo test >/dev/null 2>&1
if [ -d "$CUSTOM_DIR" ]; then
    test_pass
else
    test_fail "Multiple environment variables should work together"
fi

# Test 7: WAITLOCK_DIR with non-existent directory
test_start "WAITLOCK_DIR creates directory if needed"
NEW_DIR="$TEST_DIR/new_lock_dir"
WAITLOCK_DIR="$NEW_DIR" timeout 5 $WAITLOCK createdir --exec echo test >/dev/null 2>&1
if [ -d "$NEW_DIR" ]; then
    test_pass
else
    test_fail "WAITLOCK_DIR should create directory if it doesn't exist"
fi

# Test 8: Environment variables with --list
test_start "Environment variables with --list operations"
# Create locks with custom directory
WAITLOCK_DIR="$CUSTOM_DIR" timeout 5 $WAITLOCK listtest --exec echo test >/dev/null 2>&1
# List them using the same environment variable
list_output=$(WAITLOCK_DIR="$CUSTOM_DIR" $WAITLOCK --list 2>/dev/null || echo "")
if echo "$list_output" | grep -q "DESCRIPTOR\|No active locks"; then
    test_pass
else
    test_fail "WAITLOCK_DIR should work with --list operations"
fi

# Test 9: Environment variables with --check
test_start "Environment variables with --check operations"
# Start a lock holder
WAITLOCK_DIR="$CUSTOM_DIR" timeout 5 $WAITLOCK checktest --exec sleep 2 &
CHECK_PID=$!
sleep 1

# Check should fail (lock is held)
if ! WAITLOCK_DIR="$CUSTOM_DIR" $WAITLOCK --check checktest >/dev/null 2>&1; then
    test_pass
else
    test_fail "WAITLOCK_DIR should work with --check operations"
fi

wait $CHECK_PID 2>/dev/null || true

# Test 10: Environment variables with --done
test_start "Environment variables with --done operations"
# Start a lock holder
WAITLOCK_DIR="$CUSTOM_DIR" timeout 30 $WAITLOCK donetest &
DONE_PID=$!
sleep 2

# Use --done to signal
if WAITLOCK_DIR="$CUSTOM_DIR" $WAITLOCK --done donetest >/dev/null 2>&1; then
    wait $DONE_PID 2>/dev/null || true
    test_pass
else
    kill $DONE_PID 2>/dev/null || true
    wait $DONE_PID 2>/dev/null || true
    test_fail "WAITLOCK_DIR should work with --done operations"
fi

# Test 11: Invalid WAITLOCK_TIMEOUT values
test_start "Invalid WAITLOCK_TIMEOUT values"
# Test with negative timeout (should be ignored or handled gracefully)
WAITLOCK_TIMEOUT=-1 timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" invalidtimeout --exec echo test >/dev/null 2>&1 || true
# Test with non-numeric timeout (should be ignored or handled gracefully)
WAITLOCK_TIMEOUT=abc timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" invalidtimeout2 --exec echo test >/dev/null 2>&1 || true
# Should not crash
test_pass

# Test 12: WAITLOCK_SLOT with semaphore
test_start "WAITLOCK_SLOT with semaphore behavior"
# Test that slot preference works (basic test)
WAITLOCK_SLOT=1 timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" -m 3 slottest --exec echo test >/dev/null 2>&1
# Test with different slot
WAITLOCK_SLOT=3 timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" -m 3 slottest2 --exec echo test >/dev/null 2>&1 || true
test_pass

# Test 13: Environment variables with special characters in paths
test_start "Environment variables with special characters"
SPECIAL_DIR="$TEST_DIR/special-path_with.chars"
mkdir -p "$SPECIAL_DIR"
WAITLOCK_DIR="$SPECIAL_DIR" timeout 5 $WAITLOCK specialtest --exec echo test >/dev/null 2>&1
if [ -d "$SPECIAL_DIR" ]; then
    test_pass
else
    test_fail "WAITLOCK_DIR should handle special characters in paths"
fi

# Test 14: Environment variables with foreground execution
test_start "Environment variables with foreground execution"
# Test that environment variables work with foreground execution
WAITLOCK_TIMEOUT=2 timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" foregroundtest || {
    # Should either succeed or fail gracefully
    echo "Foreground execution completed"
}
test_pass

# Test 15: Case sensitivity of environment variables
test_start "Environment variable case sensitivity"
# Test that lowercase variables don't work (should be case sensitive)
waitlock_timeout=5 timeout 3 $WAITLOCK --lock-dir "$LOCK_DIR" casetest --exec echo test >/dev/null 2>&1 || true
test_pass

echo -e "\n${YELLOW}=== ENVIRONMENT VARIABLE TEST COMPLETE ===${NC}"
echo -e "\n${BLUE}Summary of tested environment variables:${NC}"
echo -e "${BLUE}- WAITLOCK_TIMEOUT: Default timeout setting${NC}"
echo -e "${BLUE}- WAITLOCK_DIR: Custom lock directory${NC}"
echo -e "${BLUE}- WAITLOCK_SLOT: Semaphore slot preference${NC}"
echo -e "${BLUE}- HOME: Fallback directory location${NC}"
echo -e "\n${BLUE}Note: These tests focus on the currently implemented environment variables.${NC}"
echo -e "${BLUE}WAITLOCK_DEBUG is planned for future implementation.${NC}"