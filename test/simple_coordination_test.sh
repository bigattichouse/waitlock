#!/bin/bash

# Simple coordination test to demonstrate & usage for script synchronization

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/simple_coordination_$$"
LOCK_DIR="$TEST_DIR/locks"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== SIMPLE COORDINATION TEST ===${NC}"

# Cleanup function
cleanup() {
    pkill -f "$WAITLOCK.*coordinationtest" 2>/dev/null || true
    rm -rf "$TEST_DIR" 2>/dev/null || true
}
trap cleanup EXIT

# Setup
mkdir -p "$LOCK_DIR"
make clean >/dev/null 2>&1 || true
make >/dev/null 2>&1

echo -e "${BLUE}Test: Two scripts trying to run simultaneously${NC}"

# Create worker function
run_worker() {
    local worker_id="$1"
    local log_file="$TEST_DIR/worker_$worker_id.log"
    
    echo "[$worker_id] Starting worker" >> "$log_file"
    
    # Start waitlock in background
    $WAITLOCK --lock-dir "$LOCK_DIR" coordinationtest &
    LOCK_PID=$!
    
    echo "[$worker_id] Waitlock started (PID: $LOCK_PID)" >> "$log_file"
    
    # Give it time to try to acquire
    sleep 2
    
    # Check if we got the lock
    if $WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -q "coordinationtest.*$LOCK_PID"; then
        echo "[$worker_id] SUCCESS: Got the lock!" >> "$log_file"
        echo "[$worker_id] Doing critical work..." >> "$log_file"
        
        # Simulate work
        sleep 3
        
        echo "[$worker_id] Work complete, releasing lock" >> "$log_file"
        kill $LOCK_PID
        wait $LOCK_PID 2>/dev/null || true
        return 0
    else
        echo "[$worker_id] FAILED: Could not get lock" >> "$log_file"
        kill $LOCK_PID 2>/dev/null || true
        return 1
    fi
}

# Export function so background processes can use it
export -f run_worker
export WAITLOCK LOCK_DIR TEST_DIR

# Run two workers simultaneously
echo "Starting worker 1..."
run_worker 1 &
PID1=$!

echo "Starting worker 2..."
run_worker 2 &
PID2=$!

# Wait for both to complete
wait $PID1
exit1=$?
wait $PID2
exit2=$?

echo -e "\n${YELLOW}Results:${NC}"
echo "Worker 1 exit code: $exit1"
echo "Worker 2 exit code: $exit2"

echo -e "\n${BLUE}Worker 1 log:${NC}"
cat "$TEST_DIR/worker_1.log" 2>/dev/null || echo "No log file"

echo -e "\n${BLUE}Worker 2 log:${NC}"
cat "$TEST_DIR/worker_2.log" 2>/dev/null || echo "No log file"

# Check results
if [ $exit1 -eq 0 ] && [ $exit2 -eq 1 ]; then
    echo -e "\n${GREEN}✓ Perfect coordination: Worker 1 got lock, Worker 2 was blocked${NC}"
elif [ $exit1 -eq 1 ] && [ $exit2 -eq 0 ]; then
    echo -e "\n${GREEN}✓ Perfect coordination: Worker 2 got lock, Worker 1 was blocked${NC}"
elif [ $exit1 -eq 0 ] && [ $exit2 -eq 0 ]; then
    echo -e "\n${RED}✗ Poor coordination: Both workers think they got the lock${NC}"
else
    echo -e "\n${RED}✗ Both workers failed${NC}"
fi

echo -e "\n${YELLOW}=== COORDINATION TEST COMPLETE ===${NC}"