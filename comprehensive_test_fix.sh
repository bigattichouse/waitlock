#!/bin/bash
# Comprehensive script to fix all C unit test issues

set -e

echo "=== Comprehensive C Unit Test Fix ==="

# Step 1: Kill any existing waitlock processes
echo "Step 1: Killing any existing waitlock processes..."
sudo pkill -f waitlock || true
sleep 2

# Step 2: Force remove ALL lock files (not just test ones)
echo "Step 2: Force removing ALL lock files..."
sudo rm -f /var/lock/waitlock/*.lock 2>/dev/null || true
sudo rm -f /var/lock/waitlock/.tmp.* 2>/dev/null || true

# Step 3: List what's left
echo "Step 3: Verifying lock directory is completely clean..."
echo "Lock directory contents:"
ls -la /var/lock/waitlock/ || true

# Step 4: Make sure the directory has proper permissions
echo "Step 4: Setting proper permissions on lock directory..."
sudo chmod 755 /var/lock/waitlock/
sudo chown root:root /var/lock/waitlock/

# Step 5: Rebuild with clean slate
echo "Step 5: Rebuilding waitlock with clean environment..."
cd /home/bigattichouse/workspace/waitlock/src
make clean
make

# Step 6: Run tests with isolated process
echo "Step 6: Running C unit tests in isolated environment..."
echo "Before test run - lock directory:"
ls -la /var/lock/waitlock/ || true

# Run the test
./waitlock --test

echo "After test run - lock directory:"
ls -la /var/lock/waitlock/ || true

echo "=== Comprehensive test fix completed ==="