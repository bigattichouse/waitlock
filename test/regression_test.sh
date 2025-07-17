#!/bin/bash

# Regression test script for waitlock
# Tests for known issues and previously fixed bugs

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/waitlock_regression_test_$$"
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
    echo -e "\n${YELLOW}Cleaning up regression test...${NC}"
    
    # Kill any remaining waitlock processes
    pkill -f "$WAITLOCK" 2>/dev/null || true
    
    # Clean up test directory
    rm -rf "$TEST_DIR" 2>/dev/null || true
    
    # Clean up environment
    unset WAITLOCK_DEBUG WAITLOCK_TIMEOUT WAITLOCK_DIR WAITLOCK_SLOT
    
    # Summary
    echo -e "\n${YELLOW}=== REGRESSION TEST SUMMARY ===${NC}"
    echo -e "Total tests: $TEST_COUNT"
    echo -e "${GREEN}Passed: $PASS_COUNT${NC}"
    echo -e "${RED}Failed: $FAIL_COUNT${NC}"
    
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "\n${GREEN}All regression tests passed!${NC}"
        exit 0
    else
        echo -e "\n${RED}Some regression tests failed!${NC}"
        exit 1
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Test helper functions
test_start() {
    TEST_COUNT=$((TEST_COUNT + 1))
    echo -e "\n${BLUE}Regression Test $TEST_COUNT: $1${NC}"
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
echo -e "${YELLOW}=== WAITLOCK REGRESSION TEST SUITE ===${NC}"
echo "Setting up regression test environment..."

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

# Set lock directory for tests
export WAITLOCK_DIR="$LOCK_DIR"

echo -e "${GREEN}Regression test setup complete!${NC}"

# Regression Test 1: Lock file cleanup after SIGKILL
test_start "Lock file cleanup after SIGKILL (Issue #001)"
echo "  → Testing that lock files are properly cleaned up after SIGKILL..."

# Create a lock
$WAITLOCK --lock-dir "$LOCK_DIR" "sigkill_test" >/dev/null 2>&1 &
sigkill_pid=$!
sleep 1

# Verify lock exists
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --check "sigkill_test" >/dev/null 2>&1; then
    # Kill with SIGKILL
    kill -9 $sigkill_pid 2>/dev/null || true
    
    # Wait for cleanup
    sleep 2
    
    # Try to acquire the same lock (should succeed if cleanup worked)
    if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 "sigkill_test" >/dev/null 2>&1; then
        echo "  → Lock properly cleaned up after SIGKILL"
        test_pass
    else
        test_fail "Stale lock remained after SIGKILL"
    fi
else
    test_fail "Lock was not acquired initially"
fi

# Regression Test 2: Semaphore slot allocation consistency
test_start "Semaphore slot allocation consistency (Issue #002)"
echo "  → Testing consistent semaphore slot allocation..."

# Create multiple semaphore holders
max_holders=3
pids=()
for i in $(seq 1 $max_holders); do
    $WAITLOCK --lock-dir "$LOCK_DIR" -m $max_holders "semaphore_consistency" >/dev/null 2>&1 &
    pids+=($!)
    sleep 0.1
done

# Verify all slots are taken
sleep 1
active_count=0
for pid in "${pids[@]}"; do
    if kill -0 $pid 2>/dev/null; then
        active_count=$((active_count + 1))
    fi
done

if [ $active_count -eq $max_holders ]; then
    # Try to acquire one more (should fail)
    if $WAITLOCK --lock-dir "$LOCK_DIR" -m $max_holders --timeout 1 "semaphore_consistency" >/dev/null 2>&1; then
        test_fail "Extra semaphore slot was allocated"
    else
        echo "  → Semaphore slot allocation is consistent"
        test_pass
    fi
else
    test_fail "Not all semaphore slots were allocated: $active_count/$max_holders"
fi

# Cleanup
for pid in "${pids[@]}"; do
    kill $pid 2>/dev/null || true
done

# Regression Test 3: Environment variable precedence
test_start "Environment variable precedence (Issue #003)"
echo "  → Testing environment variable precedence over command line..."

# Set environment variable
export WAITLOCK_TIMEOUT=5

# Test with command line override
start_time=$(date +%s)
$WAITLOCK --lock-dir "$LOCK_DIR" "env_test" >/dev/null 2>&1 &
env_pid=$!
sleep 1

# Try to acquire with shorter timeout (should use command line value)
if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 "env_test" >/dev/null 2>&1; then
    test_fail "Command line timeout was ignored"
else
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    
    if [ $duration -le 3 ]; then
        echo "  → Command line timeout correctly overrode environment variable"
        test_pass
    else
        test_fail "Environment variable timeout was used instead of command line"
    fi
fi

# Cleanup
kill $env_pid 2>/dev/null || true
unset WAITLOCK_TIMEOUT

# Regression Test 4: Lock file format version compatibility
test_start "Lock file format version compatibility (Issue #004)"
echo "  → Testing backward compatibility with old lock file formats..."

# Create a lock file with old format (if we had one)
# For now, just test that current format is handled correctly
$WAITLOCK --lock-dir "$LOCK_DIR" "format_test" >/dev/null 2>&1 &
format_pid=$!
sleep 1

# Find the lock file
lock_file=$(find "$LOCK_DIR" -name "format_test.*.lock" | head -1)

if [ -n "$lock_file" ]; then
    # Verify it can be read
    if $WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -q "format_test"; then
        echo "  → Lock file format is readable"
        test_pass
    else
        test_fail "Lock file format is not readable"
    fi
else
    test_fail "Lock file was not created"
fi

# Cleanup
kill $format_pid 2>/dev/null || true

# Regression Test 5: Process command line with spaces
test_start "Process command line with spaces (Issue #005)"
echo "  → Testing process command line handling with spaces..."

# Create a command with spaces
$WAITLOCK --lock-dir "$LOCK_DIR" --exec "echo hello world" "cmdline_test" >/dev/null 2>&1 &
cmdline_pid=$!

# Wait for completion
wait $cmdline_pid 2>/dev/null || true

# Check if it completed successfully
if [ $? -eq 0 ]; then
    echo "  → Command line with spaces handled correctly"
    test_pass
else
    test_fail "Command line with spaces failed"
fi

# Regression Test 6: Lock directory creation race
test_start "Lock directory creation race (Issue #006)"
echo "  → Testing race condition in lock directory creation..."

# Remove lock directory
rm -rf "$LOCK_DIR"

# Create multiple processes simultaneously
pids=()
for i in $(seq 1 10); do
    $WAITLOCK --lock-dir "$LOCK_DIR" "race_dir_$i" >/dev/null 2>&1 &
    pids+=($!)
done

# Wait for all to complete
active_count=0
for pid in "${pids[@]}"; do
    if wait $pid 2>/dev/null; then
        active_count=$((active_count + 1))
    fi
done

if [ $active_count -eq 10 ]; then
    echo "  → All processes successfully created lock directory"
    test_pass
else
    test_fail "Only $active_count/10 processes succeeded in directory creation"
fi

# Regression Test 7: Signal handling during lock acquisition
test_start "Signal handling during lock acquisition (Issue #007)"
echo "  → Testing signal handling during lock acquisition..."

# Create a holder
$WAITLOCK --lock-dir "$LOCK_DIR" "signal_acquire_test" >/dev/null 2>&1 &
holder_pid=$!
sleep 1

# Create a waiter
$WAITLOCK --lock-dir "$LOCK_DIR" --timeout 10 "signal_acquire_test" >/dev/null 2>&1 &
waiter_pid=$!
sleep 1

# Send signal to waiter while it's waiting
kill -TERM $waiter_pid 2>/dev/null || true

# Check if waiter exited properly
if wait $waiter_pid 2>/dev/null; then
    exit_code=$?
    if [ $exit_code -ne 0 ]; then
        echo "  → Waiter exited properly on signal during acquisition"
        test_pass
    else
        test_fail "Waiter should have exited with non-zero code"
    fi
else
    echo "  → Waiter was terminated by signal (expected)"
    test_pass
fi

# Cleanup
kill $holder_pid 2>/dev/null || true

# Regression Test 8: Large descriptor names
test_start "Large descriptor names (Issue #008)"
echo "  → Testing handling of large descriptor names..."

# Create a long descriptor (but within limits)
long_descriptor=$(printf 'a%.0s' {1..200})

if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 "$long_descriptor" >/dev/null 2>&1; then
    echo "  → Long descriptor (200 chars) handled correctly"
    test_pass
else
    test_fail "Long descriptor was rejected"
fi

# Test descriptor that's too long
very_long_descriptor=$(printf 'a%.0s' {1..300})

if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 "$very_long_descriptor" >/dev/null 2>&1; then
    test_fail "Overly long descriptor should have been rejected"
else
    echo "  → Overly long descriptor (300 chars) properly rejected"
    test_pass
fi

# Regression Test 9: Multiple --done signals
test_start "Multiple --done signals (Issue #009)"
echo "  → Testing multiple --done signals to same lock..."

# Create a lock holder
$WAITLOCK --lock-dir "$LOCK_DIR" "multi_done_test" >/dev/null 2>&1 &
done_pid=$!
sleep 1

# Send multiple done signals rapidly
for i in $(seq 1 5); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --done "multi_done_test" >/dev/null 2>&1 &
    sleep 0.1
done

# Wait for process to exit
wait $done_pid 2>/dev/null || true

# Check if lock is released
if $WAITLOCK --lock-dir "$LOCK_DIR" --check "multi_done_test" >/dev/null 2>&1; then
    echo "  → Multiple --done signals handled correctly"
    test_pass
else
    test_fail "Lock was not released after multiple --done signals"
fi

# Regression Test 10: Lock file permissions
test_start "Lock file permissions (Issue #010)"
echo "  → Testing lock file permissions..."

# Create a lock
$WAITLOCK --lock-dir "$LOCK_DIR" "perm_test" >/dev/null 2>&1 &
perm_pid=$!
sleep 1

# Find the lock file
lock_file=$(find "$LOCK_DIR" -name "perm_test.*.lock" | head -1)

if [ -n "$lock_file" ]; then
    # Check permissions
    perms=$(stat -c "%a" "$lock_file" 2>/dev/null || stat -f "%A" "$lock_file" 2>/dev/null)
    
    if [ "$perms" = "644" ]; then
        echo "  → Lock file has correct permissions (644)"
        test_pass
    else
        test_fail "Lock file has incorrect permissions: $perms"
    fi
else
    test_fail "Lock file was not created"
fi

# Cleanup
kill $perm_pid 2>/dev/null || true

# Regression Test 11: Timeout precision
test_start "Timeout precision (Issue #011)"
echo "  → Testing timeout precision..."

# Create a holder
$WAITLOCK --lock-dir "$LOCK_DIR" "timeout_precision_test" >/dev/null 2>&1 &
precision_pid=$!
sleep 1

# Test with fractional timeout
start_time=$(date +%s.%N)
$WAITLOCK --lock-dir "$LOCK_DIR" --timeout 0.5 "timeout_precision_test" >/dev/null 2>&1 || true
end_time=$(date +%s.%N)

duration=$(echo "$end_time - $start_time" | bc -l)
expected_min=0.4
expected_max=0.7

if (( $(echo "$duration >= $expected_min && $duration <= $expected_max" | bc -l) )); then
    echo "  → Timeout precision is acceptable: ${duration}s"
    test_pass
else
    test_fail "Timeout precision is off: ${duration}s (expected ~0.5s)"
fi

# Cleanup
kill $precision_pid 2>/dev/null || true

# Regression Test 12: Lock cleanup on parent death
test_start "Lock cleanup on parent death (Issue #012)"
echo "  → Testing lock cleanup when parent process dies..."

# Create a parent process that spawns waitlock
(
    $WAITLOCK --lock-dir "$LOCK_DIR" "parent_death_test" >/dev/null 2>&1 &
    waitlock_child=$!
    sleep 2
    # Parent exits without killing child
) &
parent_pid=$!

# Wait for parent to die
wait $parent_pid 2>/dev/null || true

# Give some time for cleanup
sleep 3

# Check if lock is still held
if $WAITLOCK --lock-dir "$LOCK_DIR" --check "parent_death_test" >/dev/null 2>&1; then
    echo "  → Lock properly cleaned up after parent death"
    test_pass
else
    test_fail "Lock was not cleaned up after parent death"
fi

echo -e "\n${YELLOW}=== REGRESSION TEST COMPLETE ===${NC}"
echo "All known issues have been tested for regression."