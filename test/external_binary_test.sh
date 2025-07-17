#!/bin/bash

# Comprehensive external test for waitlock binary
# Tests the binary from the shell exactly as end users would use it
# This is a black-box test that only uses the external interface

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/waitlock_external_test_$$"
LOCK_DIR="$TEST_DIR/locks"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test counter
TEST_COUNT=0
PASS_COUNT=0
FAIL_COUNT=0

# Test timing
START_TIME=$(date +%s)

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up external test environment...${NC}"
    
    # Kill any remaining waitlock processes
    pkill -f "$WAITLOCK" 2>/dev/null || true
    
    # Clean up test directory
    rm -rf "$TEST_DIR" 2>/dev/null || true
    
    # Clean up environment variables
    unset WAITLOCK_DEBUG WAITLOCK_TIMEOUT WAITLOCK_DIR WAITLOCK_SLOT HOME
    
    # Calculate total time
    END_TIME=$(date +%s)
    TOTAL_TIME=$((END_TIME - START_TIME))
    
    # Summary
    echo -e "\n${CYAN}============================================================${NC}"
    echo -e "${CYAN}=== EXTERNAL BINARY TEST SUMMARY ===${NC}"
    echo -e "${CYAN}============================================================${NC}"
    echo -e "Total tests: $TEST_COUNT"
    echo -e "Execution time: ${TOTAL_TIME}s"
    echo -e "${GREEN}Passed: $PASS_COUNT${NC}"
    echo -e "${RED}Failed: $FAIL_COUNT${NC}"
    
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "\n${GREEN}🎉 ALL EXTERNAL TESTS PASSED! 🎉${NC}"
        echo -e "${GREEN}waitlock binary is working correctly from the shell!${NC}"
        exit 0
    else
        echo -e "\n${RED}❌ SOME EXTERNAL TESTS FAILED ❌${NC}"
        echo -e "${RED}The waitlock binary has issues that need to be fixed.${NC}"
        exit 1
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Test helper functions
test_start() {
    TEST_COUNT=$((TEST_COUNT + 1))
    echo -e "\n${BLUE}External Test $TEST_COUNT: $1${NC}"
}

test_pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    echo -e "${GREEN}  ✓ PASS${NC}"
}

test_fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo -e "${RED}  ✗ FAIL: $1${NC}"
}

# Wait for process to appear in lock list
wait_for_lock() {
    local descriptor="$1"
    local timeout=5
    local count=0
    
    while [ $count -lt $timeout ]; do
        if $WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -q "$descriptor"; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    return 1
}

# Wait for process to disappear from lock list
wait_for_unlock() {
    local descriptor="$1"
    local timeout=5
    local count=0
    
    while [ $count -lt $timeout ]; do
        if ! $WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -q "$descriptor"; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    return 1
}

# Initialize test environment
echo -e "${CYAN}============================================================${NC}"
echo -e "${CYAN}=== WAITLOCK EXTERNAL BINARY TEST SUITE ===${NC}"
echo -e "${CYAN}============================================================${NC}"
echo -e "${YELLOW}Testing waitlock binary from shell (black-box testing)${NC}"
echo

# Create test directory
mkdir -p "$LOCK_DIR"
chmod 755 "$LOCK_DIR"

# Build waitlock
echo "Building waitlock..."
make clean >/dev/null 2>&1 || true
make >/dev/null 2>&1

if [ ! -x "$WAITLOCK" ]; then
    echo -e "${RED}ERROR: waitlock binary not found or not executable${NC}"
    echo "Please run 'make' to build the binary first."
    exit 1
fi

echo -e "${GREEN}External test setup complete!${NC}"
echo -e "${CYAN}Lock directory: $LOCK_DIR${NC}"
echo -e "${CYAN}Waitlock binary: $WAITLOCK${NC}"

# Test 1: Basic help and version
test_start "Help and version output"
echo "  → Testing --help flag..."
if $WAITLOCK --help >/dev/null 2>&1; then
    echo "    Help output available"
else
    test_fail "Help command failed"
    return
fi

echo "  → Testing --version flag..."
if $WAITLOCK --version >/dev/null 2>&1; then
    echo "    Version output available"
    test_pass
else
    test_fail "Version command failed"
fi

# Test 2: Basic mutex lock
test_start "Basic mutex lock functionality"
echo "  → Starting mutex lock in background..."
$WAITLOCK --lock-dir "$LOCK_DIR" basic_mutex >/dev/null 2>&1 &
MUTEX_PID=$!

echo "  → Waiting for lock to appear..."
if wait_for_lock "basic_mutex"; then
    echo "    Lock appeared in list"
    
    echo "  → Verifying lock is held..."
    if ! $WAITLOCK --lock-dir "$LOCK_DIR" --check basic_mutex >/dev/null 2>&1; then
        echo "    Check correctly shows lock is held"
        
        echo "  → Terminating lock holder..."
        kill $MUTEX_PID 2>/dev/null || true
        
        echo "  → Waiting for lock to be released..."
        if wait_for_unlock "basic_mutex"; then
            echo "    Lock properly released"
            test_pass
        else
            test_fail "Lock was not released after process termination"
        fi
    else
        test_fail "Check incorrectly shows lock is available"
        kill $MUTEX_PID 2>/dev/null || true
    fi
else
    test_fail "Lock did not appear in list"
    kill $MUTEX_PID 2>/dev/null || true
fi

# Test 3: Semaphore functionality
test_start "Semaphore functionality"
echo "  → Starting 3 semaphore holders (max 3)..."
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 semaphore_test >/dev/null 2>&1 &
SEM_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 semaphore_test >/dev/null 2>&1 &
SEM_PID2=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 semaphore_test >/dev/null 2>&1 &
SEM_PID3=$!

sleep 2

echo "  → Counting active semaphore holders..."
SEM_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "semaphore_test" || echo 0)

if [ "$SEM_COUNT" -eq 3 ]; then
    echo "    All 3 semaphore slots occupied"
    
    echo "  → Trying to acquire 4th slot (should fail)..."
    if ! $WAITLOCK --lock-dir "$LOCK_DIR" -m 3 --timeout 1 semaphore_test >/dev/null 2>&1; then
        echo "    4th slot correctly rejected"
        test_pass
    else
        test_fail "4th slot should have been rejected"
    fi
else
    test_fail "Expected 3 semaphore holders, got $SEM_COUNT"
fi

# Cleanup semaphore holders
kill $SEM_PID1 $SEM_PID2 $SEM_PID3 2>/dev/null || true

# Test 4: Timeout functionality
test_start "Timeout functionality"
echo "  → Starting lock holder..."
$WAITLOCK --lock-dir "$LOCK_DIR" timeout_test >/dev/null 2>&1 &
TIMEOUT_PID=$!

sleep 1

echo "  → Testing timeout behavior..."
START_TIME=$(date +%s)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 2 timeout_test >/dev/null 2>&1; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    echo "    Timeout occurred after ${DURATION}s"
    
    if [ $DURATION -ge 2 ] && [ $DURATION -le 4 ]; then
        test_pass
    else
        test_fail "Timeout duration incorrect: ${DURATION}s (expected ~2s)"
    fi
else
    test_fail "Timeout should have occurred"
fi

kill $TIMEOUT_PID 2>/dev/null || true

# Test 5: Check functionality
test_start "Check functionality"
echo "  → Testing check on non-existent lock..."
if $WAITLOCK --lock-dir "$LOCK_DIR" --check non_existent_lock >/dev/null 2>&1; then
    echo "    Non-existent lock correctly shows as available"
    
    echo "  → Starting lock holder..."
    $WAITLOCK --lock-dir "$LOCK_DIR" check_test >/dev/null 2>&1 &
    CHECK_PID=$!
    
    sleep 1
    
    echo "  → Testing check on held lock..."
    if ! $WAITLOCK --lock-dir "$LOCK_DIR" --check check_test >/dev/null 2>&1; then
        echo "    Held lock correctly shows as unavailable"
        
        kill $CHECK_PID 2>/dev/null || true
        sleep 1
        
        echo "  → Testing check on released lock..."
        if $WAITLOCK --lock-dir "$LOCK_DIR" --check check_test >/dev/null 2>&1; then
            echo "    Released lock correctly shows as available"
            test_pass
        else
            test_fail "Released lock should show as available"
        fi
    else
        test_fail "Held lock should show as unavailable"
        kill $CHECK_PID 2>/dev/null || true
    fi
else
    test_fail "Non-existent lock should show as available"
fi

# Test 6: List functionality
test_start "List functionality"
echo "  → Testing empty list..."
LIST_OUTPUT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null)
if echo "$LIST_OUTPUT" | grep -q "DESCRIPTOR"; then
    echo "    Empty list shows header"
    
    echo "  → Starting lock for list test..."
    $WAITLOCK --lock-dir "$LOCK_DIR" list_test >/dev/null 2>&1 &
    LIST_PID=$!
    
    sleep 1
    
    echo "  → Testing list with active lock..."
    if $WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -q "list_test"; then
        echo "    Active lock appears in list"
        
        echo "  → Testing CSV format..."
        if $WAITLOCK --lock-dir "$LOCK_DIR" --list --format csv 2>/dev/null | grep -q "list_test"; then
            echo "    CSV format works"
            
            echo "  → Testing null format..."
            if $WAITLOCK --lock-dir "$LOCK_DIR" --list --format null 2>/dev/null | grep -q "list_test"; then
                echo "    Null format works"
                test_pass
            else
                test_fail "Null format failed"
            fi
        else
            test_fail "CSV format failed"
        fi
    else
        test_fail "Active lock should appear in list"
    fi
    
    kill $LIST_PID 2>/dev/null || true
else
    test_fail "List should show header"
fi

# Test 7: Done functionality
test_start "Done functionality"
echo "  → Starting lock holder..."
$WAITLOCK --lock-dir "$LOCK_DIR" done_test >/dev/null 2>&1 &
DONE_PID=$!

sleep 1

echo "  → Verifying lock is held..."
if wait_for_lock "done_test"; then
    echo "    Lock confirmed active"
    
    echo "  → Sending done signal..."
    if $WAITLOCK --lock-dir "$LOCK_DIR" --done done_test >/dev/null 2>&1; then
        echo "    Done signal sent successfully"
        
        echo "  → Waiting for lock to be released..."
        if wait_for_unlock "done_test"; then
            echo "    Lock properly released after done signal"
            test_pass
        else
            test_fail "Lock was not released after done signal"
        fi
    else
        test_fail "Done signal failed"
    fi
else
    test_fail "Lock was not acquired initially"
fi

kill $DONE_PID 2>/dev/null || true

# Test 8: Exec functionality
test_start "Exec functionality"
echo "  → Testing command execution with lock..."
EXEC_OUTPUT=$($WAITLOCK --lock-dir "$LOCK_DIR" --exec "echo Hello World" exec_test 2>/dev/null)

if [ "$EXEC_OUTPUT" = "Hello World" ]; then
    echo "    Command executed successfully"
    
    echo "  → Verifying lock was released..."
    if $WAITLOCK --lock-dir "$LOCK_DIR" --check exec_test >/dev/null 2>&1; then
        echo "    Lock properly released after command"
        test_pass
    else
        test_fail "Lock was not released after command execution"
    fi
else
    test_fail "Command execution failed or returned wrong output"
fi

# Test 9: Environment variables
test_start "Environment variable support"
echo "  → Testing WAITLOCK_DIR environment variable..."
CUSTOM_DIR="$TEST_DIR/custom_locks"
mkdir -p "$CUSTOM_DIR"

WAITLOCK_DIR="$CUSTOM_DIR" $WAITLOCK env_test >/dev/null 2>&1 &
ENV_PID=$!

sleep 1

echo "  → Checking if lock file was created in custom directory..."
if ls "$CUSTOM_DIR"/*.lock >/dev/null 2>&1; then
    echo "    Lock file created in custom directory"
    
    kill $ENV_PID 2>/dev/null || true
    
    echo "  → Testing WAITLOCK_TIMEOUT environment variable..."
    START_TIME=$(date +%s)
    WAITLOCK_TIMEOUT=1 $WAITLOCK --lock-dir "$LOCK_DIR" timeout_env_test >/dev/null 2>&1 &
    TIMEOUT_ENV_PID=$!
    sleep 0.5
    
    # This should timeout in ~1 second due to environment variable
    if ! WAITLOCK_TIMEOUT=1 $WAITLOCK --lock-dir "$LOCK_DIR" timeout_env_test >/dev/null 2>&1; then
        END_TIME=$(date +%s)
        DURATION=$((END_TIME - START_TIME))
        
        if [ $DURATION -ge 1 ] && [ $DURATION -le 3 ]; then
            echo "    Environment timeout respected"
            test_pass
        else
            test_fail "Environment timeout not respected: ${DURATION}s"
        fi
    else
        test_fail "Environment timeout should have occurred"
    fi
    
    kill $TIMEOUT_ENV_PID 2>/dev/null || true
else
    test_fail "Lock file not created in custom directory"
    kill $ENV_PID 2>/dev/null || true
fi

# Test 10: Signal handling
test_start "Signal handling"
echo "  → Starting lock holder..."
$WAITLOCK --lock-dir "$LOCK_DIR" signal_test >/dev/null 2>&1 &
SIGNAL_PID=$!

sleep 1

echo "  → Verifying lock is held..."
if wait_for_lock "signal_test"; then
    echo "    Lock confirmed active"
    
    echo "  → Sending SIGTERM..."
    kill -TERM $SIGNAL_PID 2>/dev/null || true
    
    echo "  → Waiting for cleanup..."
    if wait_for_unlock "signal_test"; then
        echo "    Lock properly cleaned up after signal"
        test_pass
    else
        test_fail "Lock was not cleaned up after signal"
    fi
else
    test_fail "Lock was not acquired initially"
fi

# Test 11: Multiple concurrent locks
test_start "Multiple concurrent locks"
echo "  → Starting multiple different locks..."
$WAITLOCK --lock-dir "$LOCK_DIR" concurrent_1 >/dev/null 2>&1 &
CONC_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" concurrent_2 >/dev/null 2>&1 &
CONC_PID2=$!
$WAITLOCK --lock-dir "$LOCK_DIR" concurrent_3 >/dev/null 2>&1 &
CONC_PID3=$!

sleep 2

echo "  → Counting active locks..."
CONC_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "concurrent_" || echo 0)

if [ "$CONC_COUNT" -eq 3 ]; then
    echo "    All 3 concurrent locks active"
    test_pass
else
    test_fail "Expected 3 concurrent locks, got $CONC_COUNT"
fi

kill $CONC_PID1 $CONC_PID2 $CONC_PID3 2>/dev/null || true

# Test 12: Lock contention
test_start "Lock contention"
echo "  → Starting first lock holder..."
$WAITLOCK --lock-dir "$LOCK_DIR" contention_test >/dev/null 2>&1 &
CONT_PID1=$!

sleep 1

echo "  → Starting second waiter..."
$WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 contention_test >/dev/null 2>&1 &
CONT_PID2=$!

echo "  → Waiting for second process to timeout..."
if ! wait $CONT_PID2 2>/dev/null; then
    echo "    Second process correctly timed out"
    
    echo "  → Releasing first lock..."
    kill $CONT_PID1 2>/dev/null || true
    
    echo "  → Trying to acquire lock again..."
    if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 contention_test >/dev/null 2>&1; then
        echo "    Lock successfully acquired after release"
        test_pass
    else
        test_fail "Could not acquire lock after release"
    fi
else
    test_fail "Second process should have timed out"
    kill $CONT_PID1 2>/dev/null || true
fi

# Test 13: Stdin input
test_start "Stdin input"
echo "  → Testing descriptor from stdin..."
if echo "stdin_test" | $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 >/dev/null 2>&1; then
    echo "    Stdin input works"
    test_pass
else
    test_fail "Stdin input failed"
fi

# Test 14: Error conditions
test_start "Error conditions"
echo "  → Testing invalid descriptor..."
if ! $WAITLOCK --lock-dir "$LOCK_DIR" "invalid@descriptor" >/dev/null 2>&1; then
    echo "    Invalid descriptor correctly rejected"
    
    echo "  → Testing missing descriptor..."
    if ! $WAITLOCK --lock-dir "$LOCK_DIR" >/dev/null 2>&1; then
        echo "    Missing descriptor correctly rejected"
        
        echo "  → Testing invalid timeout..."
        if ! $WAITLOCK --lock-dir "$LOCK_DIR" --timeout -1 error_test >/dev/null 2>&1; then
            echo "    Invalid timeout correctly rejected"
            test_pass
        else
            test_fail "Invalid timeout should be rejected"
        fi
    else
        test_fail "Missing descriptor should be rejected"
    fi
else
    test_fail "Invalid descriptor should be rejected"
fi

# Test 15: Stale lock cleanup
test_start "Stale lock cleanup"
echo "  → Creating process that will die abruptly..."
$WAITLOCK --lock-dir "$LOCK_DIR" stale_test >/dev/null 2>&1 &
STALE_PID=$!

sleep 1

echo "  → Killing process with SIGKILL..."
kill -9 $STALE_PID 2>/dev/null || true

echo "  → Waiting for cleanup..."
sleep 2

echo "  → Trying to acquire same lock..."
if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 2 stale_test >/dev/null 2>&1; then
    echo "    Stale lock properly cleaned up"
    test_pass
else
    test_fail "Stale lock was not cleaned up"
fi

# Test 16: Lock persistence
test_start "Lock persistence"
echo "  → Starting lock holder..."
$WAITLOCK --lock-dir "$LOCK_DIR" persistence_test >/dev/null 2>&1 &
PERSIST_PID=$!

sleep 1

echo "  → Verifying lock file exists..."
if ls "$LOCK_DIR"/persistence_test.*.lock >/dev/null 2>&1; then
    echo "    Lock file created"
    
    echo "  → Killing process and checking cleanup..."
    kill $PERSIST_PID 2>/dev/null || true
    
    sleep 1
    
    echo "  → Verifying lock file is cleaned up..."
    if ! ls "$LOCK_DIR"/persistence_test.*.lock >/dev/null 2>&1; then
        echo "    Lock file properly cleaned up"
        test_pass
    else
        test_fail "Lock file was not cleaned up"
    fi
else
    test_fail "Lock file was not created"
    kill $PERSIST_PID 2>/dev/null || true
fi

# Test 17: Output formats verification
test_start "Output formats verification"
echo "  → Starting lock for output test..."
$WAITLOCK --lock-dir "$LOCK_DIR" output_test >/dev/null 2>&1 &
OUTPUT_PID=$!

sleep 1

echo "  → Testing human format..."
HUMAN_OUTPUT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list --format human 2>/dev/null)
if echo "$HUMAN_OUTPUT" | grep -q "DESCRIPTOR" && echo "$HUMAN_OUTPUT" | grep -q "output_test"; then
    echo "    Human format correct"
    
    echo "  → Testing CSV format..."
    CSV_OUTPUT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list --format csv 2>/dev/null)
    if echo "$CSV_OUTPUT" | grep -q "descriptor,pid" && echo "$CSV_OUTPUT" | grep -q "output_test"; then
        echo "    CSV format correct"
        
        echo "  → Testing null format..."
        NULL_OUTPUT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list --format null 2>/dev/null)
        if echo "$NULL_OUTPUT" | grep -q "output_test"; then
            echo "    Null format correct"
            test_pass
        else
            test_fail "Null format incorrect"
        fi
    else
        test_fail "CSV format incorrect"
    fi
else
    test_fail "Human format incorrect"
fi

kill $OUTPUT_PID 2>/dev/null || true

# Test 18: Complex real-world scenario
test_start "Complex real-world scenario"
echo "  → Simulating database backup coordination..."

# Start multiple backup processes
$WAITLOCK --lock-dir "$LOCK_DIR" --exec "sleep 2" db_backup >/dev/null 2>&1 &
BACKUP_PID1=$!

$WAITLOCK --lock-dir "$LOCK_DIR" --exec "sleep 1" db_backup >/dev/null 2>&1 &
BACKUP_PID2=$!

$WAITLOCK --lock-dir "$LOCK_DIR" --exec "sleep 1" db_backup >/dev/null 2>&1 &
BACKUP_PID3=$!

echo "  → Waiting for all backup processes to complete..."
wait $BACKUP_PID1 $BACKUP_PID2 $BACKUP_PID3 2>/dev/null || true

echo "  → Verifying all locks are released..."
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -q "db_backup"; then
    echo "    All backup processes completed and locks released"
    test_pass
else
    test_fail "Some backup locks still active"
fi

# Final cleanup check
echo -e "\n${YELLOW}Performing final cleanup verification...${NC}"
REMAINING_LOCKS=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -v "DESCRIPTOR" | grep -v "^$" | wc -l)
echo "Remaining locks: $REMAINING_LOCKS"

if [ "$REMAINING_LOCKS" -eq 0 ]; then
    echo -e "${GREEN}✓ All locks properly cleaned up${NC}"
else
    echo -e "${YELLOW}⚠ Some locks still active (may be expected)${NC}"
fi

echo -e "\n${CYAN}External binary testing complete!${NC}"