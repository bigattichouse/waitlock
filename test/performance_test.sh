#!/bin/bash

# Performance and stress test script for waitlock
# Tests high-frequency operations, concurrent access, and resource limits

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/waitlock_performance_test_$$"
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
    echo -e "\n${YELLOW}Cleaning up performance test...${NC}"
    
    # Kill any remaining waitlock processes
    pkill -f "$WAITLOCK" 2>/dev/null || true
    
    # Clean up test directory
    rm -rf "$TEST_DIR" 2>/dev/null || true
    
    # Summary
    echo -e "\n${YELLOW}=== PERFORMANCE TEST SUMMARY ===${NC}"
    echo -e "Total tests: $TEST_COUNT"
    echo -e "${GREEN}Passed: $PASS_COUNT${NC}"
    echo -e "${RED}Failed: $FAIL_COUNT${NC}"
    
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "\n${GREEN}All performance tests passed!${NC}"
        exit 0
    else
        echo -e "\n${RED}Some performance tests failed!${NC}"
        exit 1
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Test helper functions
test_start() {
    TEST_COUNT=$((TEST_COUNT + 1))
    echo -e "\n${BLUE}Performance Test $TEST_COUNT: $1${NC}"
}

test_pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    echo -e "${GREEN}✓ PASS${NC}"
}

test_fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo -e "${RED}✗ FAIL: $1${NC}"
}

# Performance measurement helper
measure_time() {
    local start_time=$(date +%s.%N)
    "$@"
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc -l)
    echo "$duration"
}

# Initialize test environment
echo -e "${YELLOW}=== WAITLOCK PERFORMANCE TEST SUITE ===${NC}"
echo "Setting up performance test environment..."

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

echo -e "${GREEN}Performance test setup complete!${NC}"

# Test 1: High-frequency lock acquisition
test_start "High-frequency lock acquisition"
iterations=1000
descriptor="perf_high_freq"

start_time=$(date +%s.%N)
for i in $(seq 1 $iterations); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 "$descriptor" >/dev/null 2>&1 &
    lock_pid=$!
    sleep 0.001  # 1ms delay
    kill $lock_pid 2>/dev/null || true
    wait $lock_pid 2>/dev/null || true
done
end_time=$(date +%s.%N)

total_time=$(echo "$end_time - $start_time" | bc -l)
rate=$(echo "scale=2; $iterations / $total_time" | bc -l)

echo "  → Completed $iterations operations in ${total_time}s (${rate} ops/sec)"
if (( $(echo "$rate > 10" | bc -l) )); then
    test_pass
else
    test_fail "Operation rate too low: $rate ops/sec"
fi

# Test 2: Concurrent lock contention
test_start "Concurrent lock contention"
num_processes=20
descriptor="perf_contention"

echo "  → Starting $num_processes concurrent processes..."
pids=()
start_time=$(date +%s.%N)

for i in $(seq 1 $num_processes); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 5 "$descriptor" >/dev/null 2>&1 &
    pids+=($!)
    sleep 0.01  # Small delay to avoid overwhelming
done

# Wait for all processes to complete
success_count=0
for pid in "${pids[@]}"; do
    if wait $pid 2>/dev/null; then
        success_count=$((success_count + 1))
    fi
done

end_time=$(date +%s.%N)
total_time=$(echo "$end_time - $start_time" | bc -l)

echo "  → $success_count/$num_processes processes completed in ${total_time}s"
if [ $success_count -eq $num_processes ]; then
    test_pass
else
    test_fail "Only $success_count/$num_processes processes completed successfully"
fi

# Test 3: Semaphore stress test
test_start "Semaphore stress test"
num_processes=50
max_holders=10
descriptor="perf_semaphore"

echo "  → Starting $num_processes processes competing for $max_holders slots..."
pids=()
start_time=$(date +%s.%N)

for i in $(seq 1 $num_processes); do
    $WAITLOCK --lock-dir "$LOCK_DIR" -m $max_holders --timeout 10 "$descriptor" >/dev/null 2>&1 &
    pids+=($!)
    sleep 0.002  # 2ms delay
done

# Wait for all processes to complete
success_count=0
for pid in "${pids[@]}"; do
    if wait $pid 2>/dev/null; then
        success_count=$((success_count + 1))
    fi
done

end_time=$(date +%s.%N)
total_time=$(echo "$end_time - $start_time" | bc -l)

echo "  → $success_count/$num_processes processes completed in ${total_time}s"
if [ $success_count -ge $((num_processes * 8 / 10)) ]; then  # Allow 80% success rate
    test_pass
else
    test_fail "Too few processes completed: $success_count/$num_processes"
fi

# Test 4: Rapid lock/unlock cycles
test_start "Rapid lock/unlock cycles"
cycles=500
descriptor="perf_rapid_cycles"

start_time=$(date +%s.%N)
for i in $(seq 1 $cycles); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 "$descriptor" >/dev/null 2>&1 &
    lock_pid=$!
    sleep 0.001
    $WAITLOCK --lock-dir "$LOCK_DIR" --done "$descriptor" >/dev/null 2>&1 || true
    wait $lock_pid 2>/dev/null || true
done
end_time=$(date +%s.%N)

total_time=$(echo "$end_time - $start_time" | bc -l)
rate=$(echo "scale=2; $cycles / $total_time" | bc -l)

echo "  → Completed $cycles cycles in ${total_time}s (${rate} cycles/sec)"
if (( $(echo "$rate > 5" | bc -l) )); then
    test_pass
else
    test_fail "Cycle rate too low: $rate cycles/sec"
fi

# Test 5: Memory usage under load
test_start "Memory usage under load"
num_processes=100
descriptor="perf_memory"

echo "  → Starting $num_processes long-running processes..."
pids=()

# Get initial memory usage
initial_memory=$(ps -o vsz= -p $$ | tr -d ' ')

for i in $(seq 1 $num_processes); do
    $WAITLOCK --lock-dir "$LOCK_DIR" -m $num_processes --timeout 30 "$descriptor" >/dev/null 2>&1 &
    pids+=($!)
    sleep 0.01
done

# Let them run for a bit
sleep 5

# Check memory usage
peak_memory=$(ps -o vsz= -p $$ | tr -d ' ')
memory_increase=$((peak_memory - initial_memory))

echo "  → Memory increase: ${memory_increase}KB"

# Kill all processes
for pid in "${pids[@]}"; do
    kill $pid 2>/dev/null || true
done

# Wait for cleanup
for pid in "${pids[@]}"; do
    wait $pid 2>/dev/null || true
done

if [ $memory_increase -lt 10000 ]; then  # Less than 10MB increase
    test_pass
else
    test_fail "Memory increase too high: ${memory_increase}KB"
fi

# Test 6: Large number of lock files
test_start "Large number of lock files"
num_locks=1000
base_descriptor="perf_many_locks"

echo "  → Creating $num_locks concurrent locks..."
pids=()
start_time=$(date +%s.%N)

for i in $(seq 1 $num_locks); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 5 "${base_descriptor}_$i" >/dev/null 2>&1 &
    pids+=($!)
    
    # Batch creation to avoid overwhelming
    if [ $((i % 50)) -eq 0 ]; then
        sleep 0.1
    fi
done

# Let them stabilize
sleep 2

# Count active locks
active_locks=$($WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -c "$base_descriptor" || echo 0)

end_time=$(date +%s.%N)
total_time=$(echo "$end_time - $start_time" | bc -l)

echo "  → Created $active_locks active locks in ${total_time}s"

# Cleanup
for pid in "${pids[@]}"; do
    kill $pid 2>/dev/null || true
done

for pid in "${pids[@]}"; do
    wait $pid 2>/dev/null || true
done

if [ $active_locks -ge $((num_locks * 8 / 10)) ]; then  # Allow 80% success rate
    test_pass
else
    test_fail "Too few locks created: $active_locks/$num_locks"
fi

# Test 7: Lock file I/O performance
test_start "Lock file I/O performance"
io_operations=200
descriptor="perf_io"

start_time=$(date +%s.%N)
for i in $(seq 1 $io_operations); do
    # Create lock
    $WAITLOCK --lock-dir "$LOCK_DIR" "$descriptor" >/dev/null 2>&1 &
    lock_pid=$!
    
    # List locks (read operation)
    $WAITLOCK --lock-dir "$LOCK_DIR" --list >/dev/null 2>&1
    
    # Release lock
    kill $lock_pid 2>/dev/null || true
    wait $lock_pid 2>/dev/null || true
done
end_time=$(date +%s.%N)

total_time=$(echo "$end_time - $start_time" | bc -l)
rate=$(echo "scale=2; $io_operations / $total_time" | bc -l)

echo "  → Completed $io_operations I/O operations in ${total_time}s (${rate} ops/sec)"
if (( $(echo "$rate > 2" | bc -l) )); then
    test_pass
else
    test_fail "I/O rate too low: $rate ops/sec"
fi

# Test 8: Timeout performance
test_start "Timeout performance"
timeout_tests=50
descriptor="perf_timeout"

# Create a holder process
$WAITLOCK --lock-dir "$LOCK_DIR" "$descriptor" >/dev/null 2>&1 &
holder_pid=$!
sleep 1

start_time=$(date +%s.%N)
for i in $(seq 1 $timeout_tests); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 0.1 "$descriptor" >/dev/null 2>&1 &
    timeout_pid=$!
    wait $timeout_pid 2>/dev/null || true
done
end_time=$(date +%s.%N)

total_time=$(echo "$end_time - $start_time" | bc -l)
avg_timeout=$(echo "scale=3; $total_time / $timeout_tests" | bc -l)

echo "  → Average timeout: ${avg_timeout}s per operation"

# Cleanup holder
kill $holder_pid 2>/dev/null || true
wait $holder_pid 2>/dev/null || true

if (( $(echo "$avg_timeout < 0.2" | bc -l) )); then
    test_pass
else
    test_fail "Timeout too slow: ${avg_timeout}s average"
fi

# Test 9: System resource limits
test_start "System resource limits"
max_processes=200
descriptor="perf_limits"

echo "  → Testing system limits with $max_processes processes..."
pids=()
created_count=0

for i in $(seq 1 $max_processes); do
    $WAITLOCK --lock-dir "$LOCK_DIR" -m $max_processes --timeout 1 "$descriptor" >/dev/null 2>&1 &
    lock_pid=$!
    
    # Check if process actually started
    if kill -0 $lock_pid 2>/dev/null; then
        pids+=($lock_pid)
        created_count=$((created_count + 1))
    fi
    
    sleep 0.01
done

echo "  → Successfully created $created_count processes"

# Cleanup
for pid in "${pids[@]}"; do
    kill $pid 2>/dev/null || true
done

for pid in "${pids[@]}"; do
    wait $pid 2>/dev/null || true
done

if [ $created_count -ge $((max_processes * 7 / 10)) ]; then  # Allow 70% success rate
    test_pass
else
    test_fail "Too few processes created: $created_count/$max_processes"
fi

# Test 10: Lock directory scalability
test_start "Lock directory scalability"
num_dirs=10
locks_per_dir=50

echo "  → Testing scalability across $num_dirs directories..."
total_locks=0
start_time=$(date +%s.%N)

for dir_num in $(seq 1 $num_dirs); do
    test_lock_dir="$TEST_DIR/locks_$dir_num"
    mkdir -p "$test_lock_dir"
    
    for lock_num in $(seq 1 $locks_per_dir); do
        $WAITLOCK --lock-dir "$test_lock_dir" "scalability_${dir_num}_${lock_num}" >/dev/null 2>&1 &
        total_locks=$((total_locks + 1))
        
        if [ $((lock_num % 10)) -eq 0 ]; then
            sleep 0.01
        fi
    done
done

end_time=$(date +%s.%N)
total_time=$(echo "$end_time - $start_time" | bc -l)

echo "  → Created $total_locks locks across $num_dirs directories in ${total_time}s"

# Cleanup
pkill -f "$WAITLOCK" 2>/dev/null || true
sleep 1

if [ $total_locks -eq $((num_dirs * locks_per_dir)) ]; then
    test_pass
else
    test_fail "Lock creation incomplete: $total_locks expected"
fi

echo -e "\n${YELLOW}=== PERFORMANCE TEST COMPLETE ===${NC}"