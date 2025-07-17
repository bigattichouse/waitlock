#!/bin/bash

# Simple test focused on --done functionality
set -e

WAITLOCK="./build/bin/waitlock"
LOCK_DIR="/tmp/simple_waitlock_test"

# Cleanup
cleanup() {
    echo "Cleaning up..."
    pkill -f "$WAITLOCK" 2>/dev/null || true
    rm -rf "$LOCK_DIR" 2>/dev/null || true
}

trap cleanup EXIT

# Setup
echo "=== SIMPLE WAITLOCK --DONE TEST ==="
make clean && make

# Create lock directory
mkdir -p "$LOCK_DIR"
chmod 755 "$LOCK_DIR"

echo "Test 1: Basic --done functionality"

# Start a waitlock process
echo "Starting waitlock process..."
$WAITLOCK --lock-dir "$LOCK_DIR" --verbose mylock > /tmp/waitlock.log 2>&1 &
LOCK_PID=$!

# Give it time to acquire the lock
sleep 2

# Check if lock file exists
echo "Checking lock files..."
ls -la "$LOCK_DIR"

# Check if lock appears in list
echo "Checking lock list..."
$WAITLOCK --lock-dir "$LOCK_DIR" --list

# Use --done to release it
echo "Using --done to release lock..."
$WAITLOCK --lock-dir "$LOCK_DIR" --verbose --done mylock

# Wait and check if process exited
sleep 2
echo "Checking if process exited..."
if kill -0 $LOCK_PID 2>/dev/null; then
    echo "Process still running"
    kill $LOCK_PID
else
    echo "Process exited successfully"
fi

echo "Lock files after --done:"
ls -la "$LOCK_DIR" || echo "Directory empty"

echo "Process output:"
cat /tmp/waitlock.log || echo "No log file"

echo "Test complete!"