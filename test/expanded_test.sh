#!/bin/bash

# Expanded comprehensive test script for waitlock functionality
# Tests all major features including environment variables, edge cases, and advanced functionality

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/waitlock_expanded_test_$$"
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
    
    # Clean up any custom lock directories
    rm -rf "/tmp/custom_waitlock_$$" 2>/dev/null || true
    
    # Restore environment
    unset WAITLOCK_DEBUG WAITLOCK_TIMEOUT WAITLOCK_DIR WAITLOCK_SLOT HOME
    
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
echo -e "${YELLOW}=== WAITLOCK EXPANDED TEST SUITE ===${NC}"
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

# Test 1: Environment Variables - WAITLOCK_DEBUG
test_start "Environment variable WAITLOCK_DEBUG"
WAITLOCK_DEBUG=1 output=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>&1)
if echo "$output" | grep -q "DEBUG"; then
    test_pass
else
    test_fail "WAITLOCK_DEBUG should enable debug output"
fi

# Test 2: Environment Variables - WAITLOCK_TIMEOUT
test_start "Environment variable WAITLOCK_TIMEOUT"
$WAITLOCK --lock-dir "$LOCK_DIR" envtimeout > /dev/null 2>&1 &
ENV_PID=$!
sleep 1

start_time=$(date +%s)
WAITLOCK_TIMEOUT=2 timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" envtimeout >/dev/null 2>&1 || true
end_time=$(date +%s)
duration=$((end_time - start_time))

kill $ENV_PID 2>/dev/null || true
wait $ENV_PID 2>/dev/null || true

if [ $duration -ge 2 ] && [ $duration -le 4 ]; then
    test_pass
else
    test_fail "WAITLOCK_TIMEOUT should set timeout, got $duration seconds"
fi

# Test 3: Environment Variables - WAITLOCK_DIR
test_start "Environment variable WAITLOCK_DIR"
CUSTOM_DIR="/tmp/custom_waitlock_$$"
mkdir -p "$CUSTOM_DIR"

WAITLOCK_DIR="$CUSTOM_DIR" $WAITLOCK envdir > /dev/null 2>&1 &
ENV_DIR_PID=$!
sleep 1

if [ -d "$CUSTOM_DIR" ] && ls "$CUSTOM_DIR"/*.lock >/dev/null 2>&1; then
    test_pass
else
    test_fail "WAITLOCK_DIR should create locks in custom directory"
fi

kill $ENV_DIR_PID 2>/dev/null || true
wait $ENV_DIR_PID 2>/dev/null || true
rm -rf "$CUSTOM_DIR"

# Test 4: Environment Variables - WAITLOCK_SLOT
test_start "Environment variable WAITLOCK_SLOT"
WAITLOCK_SLOT=2 $WAITLOCK --lock-dir "$LOCK_DIR" -m 5 envslot > /dev/null 2>&1 &
ENV_SLOT_PID=$!
sleep 1

slot_output=$($WAITLOCK --lock-dir "$LOCK_DIR" --list | grep "envslot" | awk '{print $3}')
kill $ENV_SLOT_PID 2>/dev/null || true
wait $ENV_SLOT_PID 2>/dev/null || true

if [ "$slot_output" = "2" ]; then
    test_pass
else
    test_fail "WAITLOCK_SLOT should set preferred slot, got slot $slot_output"
fi

# Test 5: Environment Variables - HOME fallback
test_start "HOME environment variable fallback"
TEMP_HOME="/tmp/testhome_$$"
mkdir -p "$TEMP_HOME"

HOME="$TEMP_HOME" $WAITLOCK hometest > /dev/null 2>&1 &
HOME_PID=$!
sleep 1

if [ -d "$TEMP_HOME/.waitlock" ] && ls "$TEMP_HOME/.waitlock"/*.lock >/dev/null 2>&1; then
    test_pass
else
    test_fail "HOME should be used as fallback lock directory"
fi

kill $HOME_PID 2>/dev/null || true
wait $HOME_PID 2>/dev/null || true
rm -rf "$TEMP_HOME"

# Test 6: Internal test suite
test_start "Internal test suite (--test)"
if $WAITLOCK --test >/dev/null 2>&1; then
    test_pass
else
    test_fail "Internal test suite should pass"
fi

# Test 7: CPU-based locking (--onePerCPU)
test_start "CPU-based locking (--onePerCPU)"
$WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU cpulock > /dev/null 2>&1 &
CPU_PID=$!
sleep 1

# Check if lock was acquired
if wait_for_process "cpulock"; then
    test_pass
else
    test_fail "CPU-based locking should work"
fi

kill $CPU_PID 2>/dev/null || true
wait $CPU_PID 2>/dev/null || true

# Test 8: CPU exclusion (--excludeCPUs)
test_start "CPU exclusion (--excludeCPUs)"
$WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs 1 cpuexclude > /dev/null 2>&1 &
CPU_EXCLUDE_PID=$!
sleep 1

if wait_for_process "cpuexclude"; then
    test_pass
else
    test_fail "CPU exclusion should work"
fi

kill $CPU_EXCLUDE_PID 2>/dev/null || true
wait $CPU_EXCLUDE_PID 2>/dev/null || true

# Test 9: Stdin input for descriptor
test_start "Stdin input for descriptor"
echo "stdintest" | $WAITLOCK --lock-dir "$LOCK_DIR" > /dev/null 2>&1 &
STDIN_PID=$!
sleep 1

if wait_for_process "stdintest"; then
    test_pass
else
    test_fail "Stdin input should work"
fi

kill $STDIN_PID 2>/dev/null || true
wait $STDIN_PID 2>/dev/null || true

# Test 10: Invalid descriptor characters
test_start "Invalid descriptor characters"
if $WAITLOCK --lock-dir "$LOCK_DIR" "invalid@descriptor" 2>/dev/null; then
    test_fail "Invalid descriptor should be rejected"
else
    test_pass
fi

# Test 11: Descriptor too long
test_start "Descriptor too long"
LONG_DESC=$(printf 'a%.0s' {1..300})
if $WAITLOCK --lock-dir "$LOCK_DIR" "$LONG_DESC" 2>/dev/null; then
    test_fail "Long descriptor should be rejected"
else
    test_pass
fi

# Test 12: Syslog functionality
test_start "Syslog functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" --syslog syslogtest > /dev/null 2>&1 &
SYSLOG_PID=$!
sleep 1

# Check if process started (we can't easily verify syslog output in tests)
if wait_for_process "syslogtest"; then
    test_pass
else
    test_fail "Syslog functionality should work"
fi

kill $SYSLOG_PID 2>/dev/null || true
wait $SYSLOG_PID 2>/dev/null || true

# Test 13: Syslog facility
test_start "Syslog facility"
$WAITLOCK --lock-dir "$LOCK_DIR" --syslog --syslog-facility local0 facilitytest > /dev/null 2>&1 &
FACILITY_PID=$!
sleep 1

if wait_for_process "facilitytest"; then
    test_pass
else
    test_fail "Syslog facility should work"
fi

kill $FACILITY_PID 2>/dev/null || true
wait $FACILITY_PID 2>/dev/null || true

# Test 14: Lock directory creation
test_start "Lock directory creation"
NONEXISTENT_DIR="/tmp/nonexistent_$$"
$WAITLOCK --lock-dir "$NONEXISTENT_DIR" dircreate > /dev/null 2>&1 &
DIR_PID=$!
sleep 1

if [ -d "$NONEXISTENT_DIR" ]; then
    test_pass
else
    test_fail "Lock directory should be created"
fi

kill $DIR_PID 2>/dev/null || true
wait $DIR_PID 2>/dev/null || true
rm -rf "$NONEXISTENT_DIR"

# Test 15: Permission denied handling
test_start "Permission denied handling"
READONLY_DIR="/tmp/readonly_$$"
mkdir -p "$READONLY_DIR"
chmod 444 "$READONLY_DIR"

if $WAITLOCK --lock-dir "$READONLY_DIR" permtest 2>/dev/null; then
    test_fail "Should fail on readonly directory"
else
    test_pass
fi

rm -rf "$READONLY_DIR"

# Test 16: Concurrent semaphore with preferred slots
test_start "Concurrent semaphore with preferred slots"
WAITLOCK_SLOT=0 $WAITLOCK --lock-dir "$LOCK_DIR" -m 3 slottest > /dev/null 2>&1 &
SLOT0_PID=$!
WAITLOCK_SLOT=1 $WAITLOCK --lock-dir "$LOCK_DIR" -m 3 slottest > /dev/null 2>&1 &
SLOT1_PID=$!
WAITLOCK_SLOT=2 $WAITLOCK --lock-dir "$LOCK_DIR" -m 3 slottest > /dev/null 2>&1 &
SLOT2_PID=$!

sleep 2

# Check all slots are occupied
slot_count=$($WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -c "slottest" || echo 0)
if [ "$slot_count" -eq 3 ]; then
    test_pass
else
    test_fail "Should have 3 semaphore slots occupied, got $slot_count"
fi

kill $SLOT0_PID $SLOT1_PID $SLOT2_PID 2>/dev/null || true
wait $SLOT0_PID $SLOT1_PID $SLOT2_PID 2>/dev/null || true

# Test 17: Stale lock detection
test_start "Stale lock detection"
$WAITLOCK --lock-dir "$LOCK_DIR" staletest > /dev/null 2>&1 &
STALE_PID=$!
sleep 1

# Kill process without cleanup (simulate crash)
kill -9 $STALE_PID 2>/dev/null || true

# Check if stale lock is detected
if $WAITLOCK --lock-dir "$LOCK_DIR" --list --stale-only | grep -q "staletest"; then
    test_pass
else
    test_fail "Stale lock should be detected"
fi

# Test 18: --done with stale locks
test_start "--done with stale locks"
if $WAITLOCK --lock-dir "$LOCK_DIR" --done staletest >/dev/null 2>&1; then
    test_pass
else
    test_fail "--done should clean up stale locks"
fi

# Test 19: Binary vs text lock file format
test_start "Binary vs text lock file format"
$WAITLOCK --lock-dir "$LOCK_DIR" formattest > /dev/null 2>&1 &
FORMAT_PID=$!
sleep 1

# Check if lock file exists and is readable
LOCK_FILE=$(ls "$LOCK_DIR"/formattest.*.lock 2>/dev/null | head -1)
if [ -f "$LOCK_FILE" ] && [ -r "$LOCK_FILE" ]; then
    test_pass
else
    test_fail "Lock file should be created and readable"
fi

kill $FORMAT_PID 2>/dev/null || true
wait $FORMAT_PID 2>/dev/null || true

# Test 20: Signal handling (SIGTERM)
test_start "Signal handling (SIGTERM)"
$WAITLOCK --lock-dir "$LOCK_DIR" sigtest > /dev/null 2>&1 &
SIG_PID=$!
sleep 1

# Send SIGTERM and check if lock is released
kill -TERM $SIG_PID 2>/dev/null || true
sleep 1

if wait_for_process_gone "sigtest"; then
    test_pass
else
    test_fail "Process should exit and release lock on SIGTERM"
fi

# Test 21: Output formats (CSV)
test_start "Output formats (CSV)"
$WAITLOCK --lock-dir "$LOCK_DIR" csvtest > /dev/null 2>&1 &
CSV_PID=$!
sleep 1

csv_output=$($WAITLOCK --lock-dir "$LOCK_DIR" --list --format csv)
if echo "$csv_output" | grep -q "csvtest" && echo "$csv_output" | grep -q ","; then
    test_pass
else
    test_fail "CSV format should work"
fi

kill $CSV_PID 2>/dev/null || true
wait $CSV_PID 2>/dev/null || true

# Test 22: Output formats (NULL)
test_start "Output formats (NULL)"
$WAITLOCK --lock-dir "$LOCK_DIR" nulltest > /dev/null 2>&1 &
NULL_PID=$!
sleep 1

null_output=$($WAITLOCK --lock-dir "$LOCK_DIR" --list --format null)
if echo "$null_output" | grep -q "nulltest"; then
    test_pass
else
    test_fail "NULL format should work"
fi

kill $NULL_PID 2>/dev/null || true
wait $NULL_PID 2>/dev/null || true

# Test 23: Quiet mode
test_start "Quiet mode"
quiet_output=$($WAITLOCK --lock-dir "$LOCK_DIR" --quiet --list 2>&1)
verbose_output=$($WAITLOCK --lock-dir "$LOCK_DIR" --verbose --list 2>&1)

if [ ${#quiet_output} -le ${#verbose_output} ]; then
    test_pass
else
    test_fail "Quiet mode should produce less output"
fi

# Test 24: Multiple --done commands
test_start "Multiple --done commands"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 multitest > /dev/null 2>&1 &
MULTI1_PID=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 multitest > /dev/null 2>&1 &
MULTI2_PID=$!
sleep 1

# Use --done to release all
if $WAITLOCK --lock-dir "$LOCK_DIR" --done multitest >/dev/null 2>&1; then
    sleep 2
    if wait_for_process_gone "multitest"; then
        test_pass
    else
        test_fail "All processes should exit after --done"
    fi
else
    test_fail "--done should work with multiple holders"
fi

kill $MULTI1_PID $MULTI2_PID 2>/dev/null || true
wait $MULTI1_PID $MULTI2_PID 2>/dev/null || true

# Test 25: Command execution with lock
test_start "Command execution with lock"
result=$($WAITLOCK --lock-dir "$LOCK_DIR" --exec echo "test output" exectest 2>/dev/null)
if [ "$result" = "test output" ]; then
    test_pass
else
    test_fail "Command execution should return output"
fi

# Test 26: --done with non-existent lock
test_start "--done with non-existent lock"
if $WAITLOCK --lock-dir "$LOCK_DIR" --done nonexistent 2>/dev/null; then
    test_fail "--done should fail for non-existent lock"
else
    test_pass
fi

echo -e "\n${YELLOW}=== EXPANDED TEST COMPLETE ===${NC}"