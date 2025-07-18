#!/bin/bash
# Debug exactly where the hang occurs in lock acquisition

echo "=== Debugging Lock Acquisition Hang ==="
echo ""

# Create clean test environment
TEST_DIR="/tmp/debug_hang_$$"
mkdir -p "$TEST_DIR"

# Test 1: Check if hang occurs with non-existent lock (should succeed immediately)
echo "Test 1: Non-existent lock (should succeed immediately)"
start_time=$(date +%s.%N)
timeout 2 ../../build/bin/waitlock --lock-dir "$TEST_DIR" --timeout 0.1 test_nonexistent &
TEST_PID=$!
sleep 0.5

if kill -0 $TEST_PID 2>/dev/null; then
    echo "❌ Process still running after 0.5s - HANGING"
    kill -9 $TEST_PID 2>/dev/null || true
    echo "   This suggests hang occurs BEFORE timeout logic"
else
    end_time=$(date +%s.%N)
    duration=$(echo "$end_time - $start_time" | bc -l)
    echo "✅ Process completed in ${duration}s"
fi

echo ""

# Test 2: Check if hang occurs with immediate return commands
echo "Test 2: Help command (should return immediately)"
start_time=$(date +%s.%N)
timeout 2 ../../build/bin/waitlock --timeout 0.1 --help >/dev/null 2>&1
exit_code=$?
end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc -l)

if [ $exit_code -eq 124 ]; then
    echo "❌ Help command timed out - HANGING"
else
    echo "✅ Help command completed in ${duration}s (exit code: $exit_code)"
fi

echo ""

# Test 3: Check if hang occurs with list command (no lock acquisition)
echo "Test 3: List command (should return immediately)"
start_time=$(date +%s.%N)
timeout 2 ../../build/bin/waitlock --lock-dir "$TEST_DIR" --list >/dev/null 2>&1
exit_code=$?
end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc -l)

if [ $exit_code -eq 124 ]; then
    echo "❌ List command timed out - HANGING"
else
    echo "✅ List command completed in ${duration}s (exit code: $exit_code)"
fi

echo ""

# Test 4: Check if hang occurs with check command (no lock acquisition)
echo "Test 4: Check command (should return immediately)"
start_time=$(date +%s.%N)
timeout 2 ../../build/bin/waitlock --lock-dir "$TEST_DIR" --check test_nonexistent >/dev/null 2>&1
exit_code=$?
end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc -l)

if [ $exit_code -eq 124 ]; then
    echo "❌ Check command timed out - HANGING"
else
    echo "✅ Check command completed in ${duration}s (exit code: $exit_code)"
fi

echo ""

# Test 5: Check if hang occurs with invalid arguments (should fail fast)
echo "Test 5: Invalid arguments (should fail immediately)"
start_time=$(date +%s.%N)
timeout 2 ../../build/bin/waitlock --invalid-flag >/dev/null 2>&1
exit_code=$?
end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc -l)

if [ $exit_code -eq 124 ]; then
    echo "❌ Invalid arguments timed out - HANGING"
else
    echo "✅ Invalid arguments failed in ${duration}s (exit code: $exit_code)"
fi

echo ""

# Test 6: Strace to see exact system call where hang occurs
echo "Test 6: System call trace of hanging process"
echo "Running: strace -f -e trace=all timeout 3 waitlock --timeout 0.1 test_hang"
strace -f -e trace=all timeout 3 ../../build/bin/waitlock --lock-dir "$TEST_DIR" --timeout 0.1 test_hang 2>&1 | head -50 | tail -20

echo ""
echo "=== Cleanup ==="
rm -rf "$TEST_DIR"
echo "Debug complete"