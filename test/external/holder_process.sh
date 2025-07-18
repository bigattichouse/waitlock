#!/bin/bash
# Utility script to create a holder process for testing lock contention

if [ $# -lt 1 ]; then
    echo "Usage: $0 <descriptor> [max_holders] [duration]"
    echo "  descriptor: lock descriptor name"
    echo "  max_holders: maximum number of holders (default: 1 for mutex)"
    echo "  duration: how long to hold the lock in seconds (default: 10)"
    exit 1
fi

DESCRIPTOR="$1"
MAX_HOLDERS="${2:-1}"
DURATION="${3:-10}"

# Set up lock directory
LOCK_DIR="/tmp/waitlock_test_$$"
mkdir -p "$LOCK_DIR"

echo "Creating holder process for descriptor '$DESCRIPTOR'"
echo "Max holders: $MAX_HOLDERS"
echo "Duration: ${DURATION}s"
echo "Lock directory: $LOCK_DIR"

# Create holder process
if [ "$MAX_HOLDERS" -eq 1 ]; then
    # Mutex
    echo "Acquiring mutex lock..."
    timeout $((DURATION + 5)) ../../build/bin/waitlock --lock-dir "$LOCK_DIR" "$DESCRIPTOR" sleep "$DURATION" &
else
    # Semaphore
    echo "Acquiring semaphore lock..."
    timeout $((DURATION + 5)) ../../build/bin/waitlock --lock-dir "$LOCK_DIR" --semaphore "$MAX_HOLDERS" "$DESCRIPTOR" sleep "$DURATION" &
fi

HOLDER_PID=$!
echo "Holder PID: $HOLDER_PID"

# Wait a moment for the lock to be acquired
sleep 0.1

# List locks to confirm
echo "Current locks:"
../../build/bin/waitlock --lock-dir "$LOCK_DIR" --list

# Output lock directory for other tests to use
echo "LOCK_DIR=$LOCK_DIR"
echo "HOLDER_PID=$HOLDER_PID"

# Wait for the holder to finish
wait $HOLDER_PID 2>/dev/null

# Clean up
rm -rf "$LOCK_DIR"
echo "Holder process finished and cleaned up"