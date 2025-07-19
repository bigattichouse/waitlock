#!/bin/bash

echo "=== Testing Script Coordination with & ==="

# Function to simulate critical work
do_critical_work() {
    echo "Doing critical work that must not overlap..."
    sleep 3
    echo "Critical work complete"
}

# Start waitlock in background
echo "Attempting to acquire lock..."
./build/bin/waitlock --lock-dir /tmp/coordination_test$$ testlock &
LOCK_PID=$!

echo "Waitlock started in background (PID: $LOCK_PID)"

# Give it a moment to acquire the lock
sleep 1

# Check if our specific PID is holding the lock
echo "Checking if we acquired the lock..."
lock_holder=$(./build/bin/waitlock --lock-dir /tmp/coordination_test$$ --list | grep "testlock" | awk '{print $2}' | head -1)
echo "Lock holder PID: $lock_holder, Our PID: $LOCK_PID"
if [ "$lock_holder" = "$LOCK_PID" ]; then
    echo "✓ SUCCESS: Got the lock! Proceeding with critical work..."
    do_critical_work
    echo "Releasing lock..."
    kill $LOCK_PID
    wait $LOCK_PID 2>/dev/null || true
    echo "✓ Script completed successfully"
else
    echo "✗ FAILED: Couldn't get lock, another script is running"
    kill $LOCK_PID 2>/dev/null || true
    exit 1
fi