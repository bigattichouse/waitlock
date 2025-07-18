#!/bin/bash

# Comprehensive test script for waitlock functionality
# Tests all major features including the new --done flag

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/waitlock_test_$$"
LOCK_DIR="$TEST_DIR/locks"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
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
    echo -e "\n${YELLOW}=== TEST SUMMARY ===${NC}"
    echo -e "Total tests: $TEST_COUNT"
    echo -e "${GREEN}Passed: $PASS_COUNT${NC}"
    echo -e "${RED}Failed: $FAIL_COUNT${NC}"
    
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "\n${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "\n${RED}Some tests failed!${NC}"
        exit 1
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Test helper functions
test_start() {
    TEST_COUNT=$((TEST_COUNT + 1))
    echo -e "\n${YELLOW}Test $TEST_COUNT: $1${NC}"
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
        if $WAITLOCK --list | grep -q "$desc"; then
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
        if ! $WAITLOCK --list | grep -q "$desc"; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    return 1
}

# Initialize test environment
echo -e "${YELLOW}=== WAITLOCK COMPREHENSIVE TEST SUITE ===${NC}"
echo "Setting up test environment..."

# Create test directory
mkdir -p "$LOCK_DIR"

# Also create the lock directory that waitlock will use
mkdir -p "$LOCK_DIR" 2>/dev/null || true

# Build waitlock
echo "Building waitlock..."
make clean >/dev/null 2>&1 || true
make >/dev/null 2>&1

if [ ! -x "$WAITLOCK" ]; then
    echo -e "${RED}ERROR: waitlock binary not found or not executable${NC}"
    exit 1
fi

# Set lock directory for tests
export WAITLOCK_DIR="$LOCK_DIR"

# Ensure the lock directory exists and is writable
mkdir -p "$LOCK_DIR"
chmod 755 "$LOCK_DIR"

echo -e "${GREEN}Setup complete!${NC}"

# Test 1: Basic help and version
test_start "Help and version commands"
if $WAITLOCK --help >/dev/null 2>&1 && $WAITLOCK --version >/dev/null 2>&1; then
    test_pass
else
    test_fail "Help or version command failed"
fi

# Test 2: Basic lock check (should fail for non-existent lock)
test_start "Check non-existent lock"
if $WAITLOCK --check nonexistent 2>/dev/null; then
    test_fail "Check should fail for non-existent lock"
else
    test_pass
fi

# Test 3: List empty locks
test_start "List empty locks"
output=$($WAITLOCK --list 2>/dev/null)
if echo "$output" | grep -q "DESCRIPTOR"; then
    test_pass
else
    test_fail "List command should show header"
fi

# Test 4: Basic mutex lock acquisition and release
test_start "Basic mutex lock (acquire and kill)"
mkdir -p "$LOCK_DIR"
$WAITLOCK --lock-dir "$LOCK_DIR" basicmutex > /dev/null 2>&1 &
LOCK_PID=$!

sleep 1

# Check if lock appears in list
if wait_for_process "basicmutex"; then
    # Verify lock exists
    if $WAITLOCK --lock-dir "$LOCK_DIR" --check basicmutex 2>/dev/null; then
        test_fail "Check should fail when lock is held"
    else
        # Kill the process and verify lock is released
        kill $LOCK_PID
        sleep 1
        
        if wait_for_process_gone "basicmutex"; then
            test_pass
        else
            test_fail "Lock should be released after process death"
        fi
    fi
else
    test_fail "Lock should appear in list"
fi

# Test 5: Semaphore with multiple holders
test_start "Semaphore with 3 holders"
mkdir -p "$LOCK_DIR"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 semtest > /dev/null 2>&1 &
PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 semtest > /dev/null 2>&1 &
PID2=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 semtest > /dev/null 2>&1 &
PID3=$!

sleep 2

# Check all three processes are listed
sem_count=$($WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -c "semtest" || echo 0)
if [ "$sem_count" -eq 3 ]; then
    # Kill all processes
    kill $PID1 $PID2 $PID3
    sleep 1
    
    if wait_for_process_gone "semtest"; then
        test_pass
    else
        test_fail "All semaphore slots should be released"
    fi
else
    test_fail "Should have 3 semaphore holders, got $sem_count"
    kill $PID1 $PID2 $PID3 2>/dev/null || true
fi

# Test 6: Exec mode
test_start "Exec mode"
result=$($WAITLOCK --lock-dir "$LOCK_DIR" exectest --exec echo "Hello World" 2>/dev/null)
if [ "$result" = "Hello World" ]; then
    test_pass
else
    test_fail "Exec mode should execute command and return output"
fi

# Test 7: Timeout functionality
test_start "Timeout functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" timeouttest > /dev/null 2>&1 &
TIMEOUT_PID=$!

sleep 1

# Try to acquire with timeout (should fail)
start_time=$(date +%s)
if timeout 10 $WAITLOCK --lock-dir "$LOCK_DIR" -t 2 timeouttest >/dev/null 2>&1; then
    test_fail "Second lock should timeout"
else
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    if [ $duration -ge 2 ] && [ $duration -le 5 ]; then
        test_pass
    else
        test_fail "Timeout should be around 2 seconds, got $duration"
    fi
fi

kill $TIMEOUT_PID 2>/dev/null || true

# Test 8: Output formats
test_start "Output formats (CSV and NULL)"
$WAITLOCK --lock-dir "$LOCK_DIR" formattest > /dev/null 2>&1 &
FORMAT_PID=$!

sleep 1

# Test CSV format
csv_output=$($WAITLOCK --lock-dir "$LOCK_DIR" --list --format csv 2>/dev/null)
if echo "$csv_output" | grep -q "formattest"; then
    test_pass
else
    test_fail "CSV format should show lock information"
fi

kill $FORMAT_PID 2>/dev/null || true

# Test 9: Stale lock detection
test_start "Stale lock detection"
# Create a fake stale lock by starting a process and killing it without cleanup
$WAITLOCK --lock-dir "$LOCK_DIR" staletest > /dev/null 2>&1 &
STALE_PID=$!

sleep 1

# Kill the process forcefully (simulating crash)
kill -9 $STALE_PID 2>/dev/null || true

sleep 1

# Check if stale lock is detected
if $WAITLOCK --lock-dir "$LOCK_DIR" --list --stale-only | grep -q "staletest"; then
    test_pass
else
    test_fail "Stale lock should be detected"
fi

# Test 10: --done functionality (NEW!)
test_start "DONE functionality - Basic mutex"
$WAITLOCK --lock-dir "$LOCK_DIR" donetest > /dev/null 2>&1 &
DONE_PID=$!

sleep 1

# Verify lock is held
if wait_for_process "donetest"; then
    # Use --done to signal release
    if $WAITLOCK --lock-dir "$LOCK_DIR" --done donetest 2>/dev/null; then
        # Wait for process to exit
        sleep 2
        
        if wait_for_process_gone "donetest"; then
            test_pass
        else
            test_fail "Process should exit after --done signal"
            kill $DONE_PID 2>/dev/null || true
        fi
    else
        test_fail "--done command should succeed"
        kill $DONE_PID 2>/dev/null || true
    fi
else
    test_fail "Lock should be acquired before testing --done"
fi

# Test 11: --done functionality with semaphore
test_start "DONE functionality - Semaphore"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 donesem > /dev/null 2>&1 &
DONE_SEM_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 donesem > /dev/null 2>&1 &
DONE_SEM_PID2=$!

sleep 1

# Verify both locks are held
sem_count=$($WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -c "donesem" || echo 0)
if [ "$sem_count" -eq 2 ]; then
    # Use --done to signal release of all semaphore slots
    if $WAITLOCK --lock-dir "$LOCK_DIR" --done donesem 2>/dev/null; then
        sleep 2
        
        if wait_for_process_gone "donesem"; then
            test_pass
        else
            test_fail "All semaphore processes should exit after --done signal"
            kill $DONE_SEM_PID1 $DONE_SEM_PID2 2>/dev/null || true
        fi
    else
        test_fail "--done command should succeed for semaphore"
        kill $DONE_SEM_PID1 $DONE_SEM_PID2 2>/dev/null || true
    fi
else
    test_fail "Should have 2 semaphore holders before --done test"
    kill $DONE_SEM_PID1 $DONE_SEM_PID2 2>/dev/null || true
}

# Test 12: --done on non-existent lock
test_start "DONE on non-existent lock"
if $WAITLOCK --lock-dir "$LOCK_DIR" --done nonexistentlock 2>/dev/null; then
    test_fail "--done should fail for non-existent lock"
else
    test_pass
fi

# Test 13: Verbose and quiet modes
test_start "Verbose and quiet modes"
export WAITLOCK_DEBUG=1
verbose_output=$($WAITLOCK --lock-dir "$LOCK_DIR" --verbose --list 2>&1)
unset WAITLOCK_DEBUG

$WAITLOCK --lock-dir "$LOCK_DIR" verbosetest > /dev/null 2>&1 &
VERBOSE_PID=$!

sleep 1

quiet_output=$($WAITLOCK --lock-dir "$LOCK_DIR" --quiet --list 2>&1)

kill $VERBOSE_PID 2>/dev/null || true

if [ ${#verbose_output} -gt ${#quiet_output} ]; then
    test_pass
else
    test_fail "Verbose mode should produce more output than quiet mode"
fi

# Test 14: Directory creation and permissions
test_start "Custom lock directory"
CUSTOM_DIR="/tmp/custom_waitlock_$$"
$WAITLOCK --lock-dir "$CUSTOM_DIR" customdirtest > /dev/null 2>&1 &
CUSTOM_PID=$!

sleep 1

if [ -d "$CUSTOM_DIR" ] && $WAITLOCK --lock-dir "$CUSTOM_DIR" --list | grep -q "customdirtest"; then
    test_pass
else
    test_fail "Custom lock directory should be created and used"
fi

kill $CUSTOM_PID 2>/dev/null || true
rm -rf "$CUSTOM_DIR" 2>/dev/null || true

echo -e "\n${YELLOW}=== COMPREHENSIVE TEST COMPLETE ===${NC}"
