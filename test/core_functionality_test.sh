#!/bin/bash

# Core functionality test - focuses on features that should work
# This test verifies the essential waitlock functionality

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/waitlock_core_test_$$"
LOCK_DIR="$TEST_DIR/locks"

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
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    
    # Kill any remaining waitlock processes
    pkill -f "$WAITLOCK" 2>/dev/null || true
    
    # Clean up test directory
    rm -rf "$TEST_DIR" 2>/dev/null || true
    
    # Summary
    echo -e "\n${YELLOW}=== CORE FUNCTIONALITY TEST SUMMARY ===${NC}"
    echo -e "Total tests: $TEST_COUNT"
    echo -e "${GREEN}Passed: $PASS_COUNT${NC}"
    echo -e "${RED}Failed: $FAIL_COUNT${NC}"
    
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "\n${GREEN}All core functionality tests passed!${NC}"
        exit 0
    else
        echo -e "\n${RED}Some core functionality tests failed!${NC}"
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

# Wait for process to appear
wait_for_process() {
    local desc="$1"
    local timeout=5
    local count=0
    
    while [ $count -lt $timeout ]; do
        if $WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -q "$desc"; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    return 1
}

# Wait for process to disappear
wait_for_process_gone() {
    local desc="$1"
    local timeout=5
    local count=0
    
    while [ $count -lt $timeout ]; do
        if ! $WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -q "$desc"; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    return 1
}

# Initialize test environment
echo -e "${YELLOW}=== WAITLOCK CORE FUNCTIONALITY TEST ===${NC}"
echo "Setting up test environment..."

# Create test directory
mkdir -p "$LOCK_DIR"
chmod 755 "$LOCK_DIR"

# Build waitlock
echo "Building waitlock..."
make clean >/dev/null 2>&1 || true
make >/dev/null 2>&1

if [ ! -x "$WAITLOCK" ]; then
    echo -e "${RED}ERROR: waitlock binary not found or not executable${NC}"
    exit 1
fi

echo -e "${GREEN}Setup complete!${NC}"

# Test 1: Basic help and version
test_start "Help and version commands"
if $WAITLOCK --help >/dev/null 2>&1 && $WAITLOCK --version >/dev/null 2>&1; then
    test_pass
else
    test_fail "Help or version command failed"
fi

# Test 2: Basic mutex lock
test_start "Basic mutex lock acquisition"
$WAITLOCK --lock-dir "$LOCK_DIR" basicmutex > /dev/null 2>&1 &
MUTEX_PID=$!
sleep 1

if wait_for_process "basicmutex"; then
    test_pass
else
    test_fail "Basic mutex lock should be acquired"
fi

kill $MUTEX_PID 2>/dev/null || true
wait $MUTEX_PID 2>/dev/null || true

# Test 3: --done functionality
test_start "--done functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" donetest > /dev/null 2>&1 &
DONE_PID=$!
sleep 1

if wait_for_process "donetest"; then
    # Use --done to release
    if $WAITLOCK --lock-dir "$LOCK_DIR" --done donetest >/dev/null 2>&1; then
        sleep 2
        if wait_for_process_gone "donetest"; then
            test_pass
        else
            test_fail "Process should exit after --done"
        fi
    else
        test_fail "--done command should succeed"
    fi
else
    test_fail "Lock should be acquired first"
fi

kill $DONE_PID 2>/dev/null || true
wait $DONE_PID 2>/dev/null || true

# Test 4: Basic semaphore
test_start "Basic semaphore functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 semtest > /dev/null 2>&1 &
SEM1_PID=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 semtest > /dev/null 2>&1 &
SEM2_PID=$!
sleep 2

sem_count=$($WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -c "semtest" || echo 0)
if [ "$sem_count" -eq 2 ]; then
    test_pass
else
    test_fail "Should have 2 semaphore holders, got $sem_count"
fi

kill $SEM1_PID $SEM2_PID 2>/dev/null || true
wait $SEM1_PID $SEM2_PID 2>/dev/null || true

# Test 5: --done with semaphore
test_start "--done with semaphore"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 donesem > /dev/null 2>&1 &
DONE_SEM1_PID=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 donesem > /dev/null 2>&1 &
DONE_SEM2_PID=$!
sleep 1

if $WAITLOCK --lock-dir "$LOCK_DIR" --done donesem >/dev/null 2>&1; then
    sleep 2
    if wait_for_process_gone "donesem"; then
        test_pass
    else
        test_fail "All semaphore processes should exit after --done"
    fi
else
    test_fail "--done should work with semaphore"
fi

kill $DONE_SEM1_PID $DONE_SEM2_PID 2>/dev/null || true
wait $DONE_SEM1_PID $DONE_SEM2_PID 2>/dev/null || true

# Test 6: Lock listing
test_start "Lock listing functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" listtest > /dev/null 2>&1 &
LIST_PID=$!
sleep 1

list_output=$($WAITLOCK --lock-dir "$LOCK_DIR" --list)
if echo "$list_output" | grep -q "listtest"; then
    test_pass
else
    test_fail "Lock should appear in list"
fi

kill $LIST_PID 2>/dev/null || true
wait $LIST_PID 2>/dev/null || true

# Test 7: Check functionality
test_start "Check functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" checktest > /dev/null 2>&1 &
CHECK_PID=$!
sleep 1

# Check should fail when lock is held
if $WAITLOCK --lock-dir "$LOCK_DIR" --check checktest >/dev/null 2>&1; then
    test_fail "Check should fail when lock is held"
else
    test_pass
fi

kill $CHECK_PID 2>/dev/null || true
wait $CHECK_PID 2>/dev/null || true

# Test 8: Check when lock is available
test_start "Check when lock is available"
if $WAITLOCK --lock-dir "$LOCK_DIR" --check availabletest >/dev/null 2>&1; then
    test_pass
else
    test_fail "Check should succeed when lock is available"
fi

# Test 9: Timeout functionality
test_start "Timeout functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" timeouttest > /dev/null 2>&1 &
TIMEOUT_PID=$!
sleep 1

start_time=$(date +%s)
timeout 10 $WAITLOCK --lock-dir "$LOCK_DIR" -t 2 timeouttest >/dev/null 2>&1 || true
end_time=$(date +%s)
duration=$((end_time - start_time))

kill $TIMEOUT_PID 2>/dev/null || true
wait $TIMEOUT_PID 2>/dev/null || true

if [ $duration -ge 2 ] && [ $duration -le 4 ]; then
    test_pass
else
    test_fail "Timeout should be around 2 seconds, got $duration"
fi

# Test 10: Command execution
test_start "Command execution (--exec)"
result=$($WAITLOCK --lock-dir "$LOCK_DIR" --exec echo "test output" exectest 2>/dev/null)
if [ "$result" = "test output" ]; then
    test_pass
else
    test_fail "Command execution should work"
fi

# Test 11: Signal handling
test_start "Signal handling (SIGTERM)"
$WAITLOCK --lock-dir "$LOCK_DIR" sigtest > /dev/null 2>&1 &
SIG_PID=$!
sleep 1

kill -TERM $SIG_PID 2>/dev/null || true
sleep 1

if wait_for_process_gone "sigtest"; then
    test_pass
else
    test_fail "Process should exit on SIGTERM"
fi

# Test 12: Lock cleanup on process death
test_start "Lock cleanup on process death"
$WAITLOCK --lock-dir "$LOCK_DIR" cleanuptest > /dev/null 2>&1 &
CLEANUP_PID=$!
sleep 1

# Kill process forcefully
kill -9 $CLEANUP_PID 2>/dev/null || true

# Try to acquire the same lock (should work if cleanup happened)
if timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" -t 2 cleanuptest >/dev/null 2>&1; then
    test_pass
else
    test_fail "Lock should be cleaned up after process death"
fi

echo -e "\n${YELLOW}=== CORE FUNCTIONALITY TEST COMPLETE ===${NC}"