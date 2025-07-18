#!/bin/bash
# Progressive test to isolate exactly where the hang occurs in waitlock

echo "=== Progressive Hang Test ==="
echo ""

# Test 1: Basic timeout logic works (confirmed from minimal test)
echo "✅ Test 1: Basic timeout logic - CONFIRMED WORKING"
echo ""

# Test 2: Check if issue is in argument parsing
echo "Test 2: Argument parsing"
echo "Running: waitlock --timeout 0.1 --version (should exit immediately)"
start_time=$(date +%s.%N)
timeout 2 ../../build/bin/waitlock --timeout 0.1 --version >/dev/null 2>&1
exit_code=$?
end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc -l)

if [ $exit_code -eq 124 ]; then
    echo "❌ Argument parsing with timeout hangs"
else
    echo "✅ Argument parsing works (${duration}s)"
fi
echo ""

# Test 3: Check if issue is in signal handler installation  
echo "Test 3: Signal handlers + timeout"
echo "Running: strace -e trace=rt_sigaction timeout 2 waitlock --timeout 0.1 no_desc"
strace -e trace=rt_sigaction timeout 2 ../../build/bin/waitlock --timeout 0.1 no_desc 2>&1 | head -10
echo ""

# Test 4: Check if issue is in lock directory creation
echo "Test 4: Lock directory operations"
TEST_DIR="/tmp/progressive_test_$$"
mkdir -p "$TEST_DIR"
echo "Running: waitlock --lock-dir $TEST_DIR --timeout 0.1 --list (should work)"
start_time=$(date +%s.%N)
timeout 2 ../../build/bin/waitlock --lock-dir "$TEST_DIR" --timeout 0.1 --list >/dev/null 2>&1
exit_code=$?
end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc -l)

if [ $exit_code -eq 124 ]; then
    echo "❌ List command with timeout hangs"
else
    echo "✅ List command with timeout works (${duration}s)"
fi
echo ""

# Test 5: Check if issue is specifically in lock acquisition vs other operations
echo "Test 5: Lock acquisition vs other operations"
echo "Running: waitlock --lock-dir $TEST_DIR --timeout 0.1 --check nonexistent"
start_time=$(date +%s.%N)
timeout 2 ../../build/bin/waitlock --lock-dir "$TEST_DIR" --timeout 0.1 --check nonexistent >/dev/null 2>&1
exit_code=$?
end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc -l)

if [ $exit_code -eq 124 ]; then
    echo "❌ Check command with timeout hangs"
else
    echo "✅ Check command with timeout works (${duration}s)"
fi
echo ""

# Test 6: Try to isolate the exact line where hang occurs with gdb
echo "Test 6: GDB backtrace of hanging process"
echo "Running waitlock in background and getting backtrace..."
../../build/bin/waitlock --lock-dir "$TEST_DIR" --timeout 0.1 test_hang &
HANG_PID=$!
sleep 1

if kill -0 $HANG_PID 2>/dev/null; then
    echo "Process $HANG_PID is hanging, getting backtrace..."
    gdb -batch -ex "attach $HANG_PID" -ex "bt" -ex "detach" -ex "quit" 2>/dev/null || echo "GDB failed"
    kill -9 $HANG_PID 2>/dev/null
else
    echo "Process exited normally"
fi
echo ""

# Cleanup
rm -rf "$TEST_DIR"
echo "=== Test Complete ==="