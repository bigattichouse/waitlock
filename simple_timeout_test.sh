#!/bin/bash
# Simple timeout test

WAITLOCK="./src/waitlock"

echo "=== Simple Timeout Test ==="

# Clean up
rm -f /var/lock/waitlock/simple_test*.lock 2>/dev/null || true

echo "Step 1: Testing timeout with no conflict"
echo "Running: $WAITLOCK --timeout 1.0 simple_test_unique"

# This should succeed immediately since no lock exists
start_time=$(date +%s)
timeout 5 $WAITLOCK --timeout 1.0 simple_test_unique &
TEST_PID=$!

# Wait a moment and check if lock was acquired
sleep 2
if $WAITLOCK --check simple_test_unique; then
    echo "❌ FAIL: Lock was not acquired"
else
    echo "✅ PASS: Lock was acquired successfully"
fi

# Clean up
kill $TEST_PID 2>/dev/null || true

echo "Step 2: Testing timeout with conflict"
echo "First acquiring a blocking lock..."

# Start a lock holder
$WAITLOCK simple_test_conflict &
HOLDER_PID=$!
sleep 1

echo "Now testing timeout against existing lock..."
start_time=$(date +%s)
$WAITLOCK --timeout 1.0 simple_test_conflict
exit_code=$?
end_time=$(date +%s)
elapsed=$((end_time - start_time))

echo "Timeout test completed:"
echo "  Exit code: $exit_code"
echo "  Elapsed time: $elapsed seconds"

if [ $exit_code -eq 2 ] && [ $elapsed -ge 1 ] && [ $elapsed -le 2 ]; then
    echo "✅ PASS: Timeout worked correctly"
else
    echo "❌ FAIL: Timeout behavior incorrect"
fi

# Cleanup
kill $HOLDER_PID 2>/dev/null || true
rm -f /var/lock/waitlock/simple_test*.lock 2>/dev/null || true