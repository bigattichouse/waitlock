#!/bin/bash
# Script to fix C unit test cleanup and isolation issues

set -e

echo "=== Fixing C Unit Test Cleanup Issues ==="

# Clean up all existing test lock files
echo "Step 1: Cleaning up all existing test lock files..."
sudo rm -f /var/lock/waitlock/test_*.lock 2>/dev/null || true
sudo rm -f /var/lock/waitlock/*_test*.lock 2>/dev/null || true

# Also clean any other stale test files we've seen
sudo rm -f /var/lock/waitlock/manual_test.slot0.lock 2>/dev/null || true
sudo rm -f /var/lock/waitlock/strace_test.slot0.lock 2>/dev/null || true
sudo rm -f /var/lock/waitlock/small_timeout_test.slot0.lock 2>/dev/null || true
sudo rm -f /var/lock/waitlock/strace_debug_test.slot0.lock 2>/dev/null || true
sudo rm -f /var/lock/waitlock/debug_with_verbose_test.slot0.lock 2>/dev/null || true
sudo rm -f /var/lock/waitlock/clean_test_lock2.slot0.lock 2>/dev/null || true
sudo rm -f /var/lock/waitlock/clean_test_lock.slot0.lock 2>/dev/null || true
sudo rm -f /var/lock/waitlock/debug_trace_test.slot0.lock 2>/dev/null || true

echo "Step 2: Verifying lock directory is clean..."
ls -la /var/lock/waitlock/ || true

echo "Step 3: Running C unit tests with fresh environment..."
cd /home/bigattichouse/workspace/waitlock/src
make clean && make

echo "Step 4: Testing with isolated environment..."
./waitlock --test

echo "=== Test cleanup fix completed ==="