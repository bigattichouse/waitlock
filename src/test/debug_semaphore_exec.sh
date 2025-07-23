#!/bin/bash

# Standalone test script to debug semaphore and exec issues
# This avoids linking problems by using the waitlock binary directly

WAITLOCK="./build/bin/waitlock"
TMPDIR="/tmp/waitlock_debug_$$"
mkdir -p "$TMPDIR"

echo "=== STANDALONE SEMAPHORE AND EXEC DEBUG TEST ==="
echo "Using temporary directory: $TMPDIR"

# Test 1: Semaphore Race Condition Test
echo ""
echo "=== SEMAPHORE RACE CONDITION TEST ==="
echo "Testing semaphore with max_holders=3"

DESCRIPTOR="test_semaphore_race"
LOCK_DIR="$TMPDIR/locks"
mkdir -p "$LOCK_DIR"

echo "[Test] Starting semaphore race condition test..."

# Function to acquire lock and hold it
acquire_and_hold() {
    local child_id=$1
    local hold_time=$2
    echo "[Child $child_id] Attempting to acquire lock..."
    
    # Try to acquire the lock
    timeout 10 $WAITLOCK --lock-dir "$LOCK_DIR" -m 3 -t 2.0 "$DESCRIPTOR" &
    local waitlock_pid=$!
    
    # Check if waitlock is still running (means it got the lock)
    sleep 0.5
    if kill -0 $waitlock_pid 2>/dev/null; then
        echo "[Child $child_id] SUCCESS: Acquired lock (PID $waitlock_pid)"
        
        # Let it hold for specified time
        sleep $hold_time
        
        # Kill waitlock to release the lock
        kill $waitlock_pid 2>/dev/null
        wait $waitlock_pid 2>/dev/null
        echo "[Child $child_id] Released lock"
        return 0
    else
        echo "[Child $child_id] FAILED: Could not acquire lock"
        return 1
    fi
}

# Start 4 processes trying to acquire 3 semaphore slots
echo "[Parent] Starting 4 children to compete for 3 semaphore slots..."

pids=()
results=()

# Start children in background
for i in {1..4}; do
    (
        if acquire_and_hold $i 4; then
            exit 0  # Success
        else
            exit 1  # Failed
        fi
    ) &
    pids[$i]=$!
done

# Give them time to compete
sleep 2

# Check how many are running (successfully acquired locks)
running_count=0
for i in {1..4}; do
    if kill -0 ${pids[$i]} 2>/dev/null; then
        running_count=$((running_count + 1))
        echo "[Parent] Child $i is running (has lock)"
    else
        wait ${pids[$i]}
        if [ $? -eq 0 ]; then
            echo "[Parent] Child $i completed successfully but already exited"
        else
            echo "[Parent] Child $i failed to acquire lock"
        fi
    fi
done

echo "[Parent] Currently running children (holding locks): $running_count"

if [ $running_count -eq 3 ]; then
    echo "PASS: Exactly 3 processes acquired locks (semaphore working correctly)"
elif [ $running_count -gt 3 ]; then
    echo "FAIL: Too many processes acquired locks ($running_count > 3) - SEMAPHORE BUG!"
else
    echo "UNEXPECTED: Fewer processes acquired locks than expected ($running_count < 3)"
fi

# Wait for all children to complete
echo "[Parent] Waiting for all children to complete..."
for i in {1..4}; do
    wait ${pids[$i]} 2>/dev/null || true
done

# Check lock cleanup
sleep 1
lock_files=$(ls "$LOCK_DIR" 2>/dev/null | wc -l)
if [ $lock_files -eq 0 ]; then
    echo "PASS: All locks cleaned up properly"
else
    echo "FAIL: $lock_files lock files remain"
    ls -la "$LOCK_DIR"
fi

# Test 2: Exec Timeout Issue Test
echo ""
echo "=== EXEC TIMEOUT ISSUE TEST ==="
echo "Testing exec_with_lock timeout handling..."

DESCRIPTOR2="test_exec_timeout"

# Test 2a: Simple exec test
echo "[Test 2a] Simple exec test with proper timeout..."
timeout 10 $WAITLOCK --lock-dir "$LOCK_DIR" -t 5.0 -e "echo 'Hello from exec test'" "$DESCRIPTOR2"
if [ $? -eq 0 ]; then
    echo "PASS: Simple exec succeeded"
else
    echo "FAIL: Simple exec failed"
fi

# Test 2b: Exec with lock contention
echo "[Test 2b] Testing exec with lock contention..."

# Start a holder process
echo "[Holder] Acquiring lock for 4 seconds..."
timeout 10 $WAITLOCK --lock-dir "$LOCK_DIR" -t 5.0 "$DESCRIPTOR2" &
holder_pid=$!
sleep 1  # Give holder time to acquire

# Check if holder got the lock
if kill -0 $holder_pid 2>/dev/null; then
    echo "[Holder] Successfully acquired lock"
    
    # Now try exec with contention - should wait and succeed
    echo "[ExecChild] Trying exec with 6 second timeout (holder will release in ~3 sec)..."
    timeout 10 $WAITLOCK --lock-dir "$LOCK_DIR" -t 6.0 -e "echo 'Should succeed after wait'" "$DESCRIPTOR2" &
    exec_pid=$!
    
    # Let holder run for 3 more seconds then kill it
    sleep 3
    kill $holder_pid 2>/dev/null
    wait $holder_pid 2>/dev/null
    echo "[Holder] Released lock"
    
    # Wait for exec to complete
    wait $exec_pid
    exec_result=$?
    
    if [ $exec_result -eq 0 ]; then
        echo "PASS: Exec with contention succeeded"
    else
        echo "FAIL: Exec with contention failed (exit code: $exec_result)"
    fi
else
    echo "FAIL: Holder could not acquire lock"
    kill $exec_pid 2>/dev/null || true
fi

# Test 2c: Check the specific timeout=0.0 issue
echo "[Test 2c] Testing timeout=0.0 issue (should fail immediately)..."
start_time=$(date +%s)
timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" -t 0.0 -e "echo 'Should fail immediately'" "$DESCRIPTOR2"
end_time=$(date +%s)
duration=$((end_time - start_time))

if [ $duration -le 1 ]; then
    echo "PASS: timeout=0.0 failed quickly (${duration}s) as expected"
else
    echo "FAIL: timeout=0.0 took too long (${duration}s)"
fi

# Final cleanup check
sleep 1
lock_files=$(ls "$LOCK_DIR" 2>/dev/null | wc -l)
if [ $lock_files -eq 0 ]; then
    echo "PASS: All exec test locks cleaned up properly"
else
    echo "FAIL: $lock_files lock files remain after exec tests"
    ls -la "$LOCK_DIR"
fi

echo ""
echo "=== DEBUG INFORMATION ==="
echo "Lock directory contents during test:"
ls -la "$LOCK_DIR" 2>/dev/null || echo "No lock files found"

echo ""
echo "=== TEST COMPLETE ==="
rm -rf "$TMPDIR"
echo "Cleaned up temporary directory: $TMPDIR"