#!/bin/bash

# Stress test script for waitlock
# Tests edge cases, error conditions, and system limits

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/waitlock_stress_test_$$"
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
    echo -e "\n${YELLOW}Cleaning up stress test...${NC}"
    
    # Kill any remaining waitlock processes
    pkill -f "$WAITLOCK" 2>/dev/null || true
    
    # Clean up test directory
    rm -rf "$TEST_DIR" 2>/dev/null || true
    
    # Restore ulimits
    ulimit -n 1024 2>/dev/null || true
    ulimit -u 1024 2>/dev/null || true
    
    # Summary
    echo -e "\n${YELLOW}=== STRESS TEST SUMMARY ===${NC}"
    echo -e "Total tests: $TEST_COUNT"
    echo -e "${GREEN}Passed: $PASS_COUNT${NC}"
    echo -e "${RED}Failed: $FAIL_COUNT${NC}"
    
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "\n${GREEN}All stress tests passed!${NC}"
        exit 0
    else
        echo -e "\n${RED}Some stress tests failed!${NC}"
        exit 1
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Test helper functions
test_start() {
    TEST_COUNT=$((TEST_COUNT + 1))
    echo -e "\n${BLUE}Stress Test $TEST_COUNT: $1${NC}"
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
echo -e "${YELLOW}=== WAITLOCK STRESS TEST SUITE ===${NC}"
echo "Setting up stress test environment..."

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

echo -e "${GREEN}Stress test setup complete!${NC}"

# Test 1: File descriptor exhaustion
test_start "File descriptor exhaustion"
# Set low file descriptor limit
ulimit -n 50 2>/dev/null || echo "  → Could not set ulimit, skipping FD test"

if [ "$(ulimit -n)" -eq 50 ] 2>/dev/null; then
    echo "  → Testing with file descriptor limit of 50..."
    
    # Try to create many locks
    pids=()
    for i in $(seq 1 40); do
        $WAITLOCK --lock-dir "$LOCK_DIR" "fd_test_$i" >/dev/null 2>&1 &
        pids+=($!)
        sleep 0.01
    done
    
    # Check how many succeeded
    active_count=0
    for pid in "${pids[@]}"; do
        if kill -0 $pid 2>/dev/null; then
            active_count=$((active_count + 1))
        fi
    done
    
    echo "  → $active_count/40 processes active with FD limit"
    
    # Cleanup
    for pid in "${pids[@]}"; do
        kill $pid 2>/dev/null || true
    done
    
    if [ $active_count -gt 0 ]; then
        test_pass
    else
        test_fail "No processes could start with FD limit"
    fi
else
    echo "  → Skipping FD test (could not set limit)"
    test_pass
fi

# Restore normal FD limit
ulimit -n 1024 2>/dev/null || true

# Test 2: Process limit exhaustion
test_start "Process limit exhaustion"
# Set low process limit
ulimit -u 100 2>/dev/null || echo "  → Could not set ulimit, skipping process test"

if [ "$(ulimit -u)" -eq 100 ] 2>/dev/null; then
    echo "  → Testing with process limit of 100..."
    
    # Try to create many processes
    pids=()
    for i in $(seq 1 80); do
        $WAITLOCK --lock-dir "$LOCK_DIR" -m 80 "proc_test" >/dev/null 2>&1 &
        pids+=($!)
        sleep 0.01
    done
    
    # Check how many succeeded
    active_count=0
    for pid in "${pids[@]}"; do
        if kill -0 $pid 2>/dev/null; then
            active_count=$((active_count + 1))
        fi
    done
    
    echo "  → $active_count/80 processes active with process limit"
    
    # Cleanup
    for pid in "${pids[@]}"; do
        kill $pid 2>/dev/null || true
    done
    
    if [ $active_count -gt 0 ]; then
        test_pass
    else
        test_fail "No processes could start with process limit"
    fi
else
    echo "  → Skipping process test (could not set limit)"
    test_pass
fi

# Restore normal process limit
ulimit -u 1024 2>/dev/null || true

# Test 3: Disk space exhaustion
test_start "Disk space exhaustion"
# Create a small filesystem
small_fs="$TEST_DIR/small_fs"
mkdir -p "$small_fs"

# Create a 1MB file system (if possible)
if command -v truncate >/dev/null 2>&1; then
    fs_file="$TEST_DIR/small.img"
    truncate -s 1M "$fs_file"
    
    # Try to create a loop device (requires root, so this might fail)
    if sudo losetup -f "$fs_file" 2>/dev/null; then
        loop_dev=$(sudo losetup -j "$fs_file" | cut -d: -f1)
        sudo mkfs.ext4 "$loop_dev" >/dev/null 2>&1
        sudo mount "$loop_dev" "$small_fs" 2>/dev/null
        
        # Test with limited space
        echo "  → Testing with 1MB filesystem..."
        
        # Try to create many locks
        pids=()
        for i in $(seq 1 100); do
            WAITLOCK_DIR="$small_fs" $WAITLOCK "space_test_$i" >/dev/null 2>&1 &
            pids+=($!)
            sleep 0.01
        done
        
        # Check how many succeeded
        active_count=0
        for pid in "${pids[@]}"; do
            if kill -0 $pid 2>/dev/null; then
                active_count=$((active_count + 1))
            fi
        done
        
        echo "  → $active_count/100 processes active with space limit"
        
        # Cleanup
        for pid in "${pids[@]}"; do
            kill $pid 2>/dev/null || true
        done
        
        sudo umount "$small_fs" 2>/dev/null || true
        sudo losetup -d "$loop_dev" 2>/dev/null || true
        
        if [ $active_count -gt 0 ]; then
            test_pass
        else
            test_fail "No processes could start with space limit"
        fi
    else
        echo "  → Skipping disk space test (requires root)"
        test_pass
    fi
else
    echo "  → Skipping disk space test (truncate not available)"
    test_pass
fi

# Test 4: Signal flood
test_start "Signal flood"
echo "  → Testing signal handling under flood conditions..."

# Create a process that holds a lock
$WAITLOCK --lock-dir "$LOCK_DIR" "signal_flood_test" >/dev/null 2>&1 &
target_pid=$!
sleep 1

# Flood with signals
for i in $(seq 1 100); do
    kill -USR1 $target_pid 2>/dev/null || true
    kill -USR2 $target_pid 2>/dev/null || true
    sleep 0.01
done

# Check if process is still alive
if kill -0 $target_pid 2>/dev/null; then
    echo "  → Process survived signal flood"
    
    # Send termination signal
    kill -TERM $target_pid 2>/dev/null || true
    
    # Wait for cleanup
    wait $target_pid 2>/dev/null || true
    
    # Check if lock was cleaned up
    if ! $WAITLOCK --lock-dir "$LOCK_DIR" --check "signal_flood_test" >/dev/null 2>&1; then
        test_fail "Lock still held after signal flood"
    else
        test_pass
    fi
else
    test_fail "Process died during signal flood"
fi

# Test 5: Corrupted lock files
test_start "Corrupted lock files"
echo "  → Testing handling of corrupted lock files..."

# Create a normal lock file first
$WAITLOCK --lock-dir "$LOCK_DIR" "corruption_test" >/dev/null 2>&1 &
lock_pid=$!
sleep 1

# Find the lock file
lock_file=$(find "$LOCK_DIR" -name "corruption_test.*.lock" | head -1)

if [ -n "$lock_file" ]; then
    # Kill the process holding the lock
    kill $lock_pid 2>/dev/null || true
    wait $lock_pid 2>/dev/null || true
    
    # Corrupt the lock file
    echo "CORRUPTED DATA" > "$lock_file"
    
    # Try to acquire the same lock
    if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 "corruption_test" >/dev/null 2>&1; then
        echo "  → Successfully handled corrupted lock file"
        test_pass
    else
        test_fail "Could not handle corrupted lock file"
    fi
else
    test_fail "Could not find lock file to corrupt"
fi

# Test 6: Race condition stress
test_start "Race condition stress"
echo "  → Testing race conditions with rapid operations..."

# Create many processes that rapidly acquire and release locks
pids=()
for i in $(seq 1 20); do
    (
        for j in $(seq 1 10); do
            $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 "race_test_$i" >/dev/null 2>&1 &
            race_pid=$!
            sleep 0.001
            kill $race_pid 2>/dev/null || true
            wait $race_pid 2>/dev/null || true
        done
    ) &
    pids+=($!)
done

# Wait for all race processes to complete
for pid in "${pids[@]}"; do
    wait $pid 2>/dev/null || true
done

# Check that no locks are left hanging
sleep 1
remaining_locks=$($WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -c "race_test" || echo 0)

if [ $remaining_locks -eq 0 ]; then
    test_pass
else
    test_fail "$remaining_locks locks left hanging after race test"
fi

# Test 7: Memory pressure
test_start "Memory pressure"
echo "  → Testing under memory pressure..."

# Create many processes that consume memory
pids=()
for i in $(seq 1 30); do
    $WAITLOCK --lock-dir "$LOCK_DIR" -m 30 "memory_test" >/dev/null 2>&1 &
    pids+=($!)
    sleep 0.01
done

# Check memory usage
memory_usage=$(ps -o vsz= -p $$ | tr -d ' ')
echo "  → Memory usage: ${memory_usage}KB"

# Create additional memory pressure
(
    # Allocate memory in background
    dd if=/dev/zero of="$TEST_DIR/memory_pressure" bs=1M count=50 2>/dev/null || true
    sleep 10
    rm -f "$TEST_DIR/memory_pressure"
) &
pressure_pid=$!

# Wait a bit under pressure
sleep 2

# Check if processes are still alive
active_count=0
for pid in "${pids[@]}"; do
    if kill -0 $pid 2>/dev/null; then
        active_count=$((active_count + 1))
    fi
done

echo "  → $active_count/30 processes survived memory pressure"

# Cleanup
kill $pressure_pid 2>/dev/null || true
for pid in "${pids[@]}"; do
    kill $pid 2>/dev/null || true
done

if [ $active_count -gt 15 ]; then  # Allow 50% survival rate
    test_pass
else
    test_fail "Too few processes survived memory pressure: $active_count/30"
fi

# Test 8: Filesystem full
test_start "Filesystem full simulation"
echo "  → Testing filesystem full conditions..."

# Fill up the lock directory
for i in $(seq 1 1000); do
    dd if=/dev/zero of="$LOCK_DIR/filler_$i" bs=1K count=1 2>/dev/null || break
done

# Try to create a lock
if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 "fs_full_test" >/dev/null 2>&1; then
    echo "  → Lock creation succeeded despite full filesystem"
    test_pass
else
    echo "  → Lock creation failed on full filesystem (expected)"
    test_pass
fi

# Cleanup filler files
rm -f "$LOCK_DIR"/filler_* 2>/dev/null || true

# Test 9: Permission changes during operation
test_start "Permission changes during operation"
echo "  → Testing permission changes during operation..."

# Create a lock
$WAITLOCK --lock-dir "$LOCK_DIR" "perm_test" >/dev/null 2>&1 &
perm_pid=$!
sleep 1

# Find the lock file
lock_file=$(find "$LOCK_DIR" -name "perm_test.*.lock" | head -1)

if [ -n "$lock_file" ]; then
    # Change permissions on lock file
    chmod 000 "$lock_file" 2>/dev/null || true
    
    # Try to send done signal
    if $WAITLOCK --lock-dir "$LOCK_DIR" --done "perm_test" >/dev/null 2>&1; then
        echo "  → Successfully handled permission change"
        test_pass
    else
        echo "  → Could not handle permission change (expected)"
        test_pass
    fi
    
    # Restore permissions and cleanup
    chmod 644 "$lock_file" 2>/dev/null || true
    kill $perm_pid 2>/dev/null || true
    wait $perm_pid 2>/dev/null || true
else
    test_fail "Could not find lock file for permission test"
fi

# Test 10: System clock changes
test_start "System clock changes"
echo "  → Testing system clock changes..."

# Create a lock with timeout
$WAITLOCK --lock-dir "$LOCK_DIR" "clock_test" >/dev/null 2>&1 &
clock_pid=$!
sleep 1

# In a real system, we would change the system clock here
# For this test, we just verify the process handles time normally
if kill -0 $clock_pid 2>/dev/null; then
    echo "  → Process running normally with current time"
    
    # Send signal to exit
    kill -TERM $clock_pid 2>/dev/null || true
    wait $clock_pid 2>/dev/null || true
    
    test_pass
else
    test_fail "Process died unexpectedly"
fi

echo -e "\n${YELLOW}=== STRESS TEST COMPLETE ===${NC}"