#!/bin/bash
# Debug timeout functionality

echo "=== Timeout Debug Test ==="

# Test 1: Check if timeout argument is parsed
echo "Test 1: Verify timeout parsing with --help"
echo "Running: waitlock --timeout 0.1 --help"
if timeout 3 ../../build/bin/waitlock --timeout 0.1 --help >/dev/null 2>&1; then
    echo "✓ Timeout argument accepted with --help"
else
    echo "✗ Timeout argument parsing failed"
fi

# Test 2: Test timeout with verbose output
echo ""
echo "Test 2: Check timeout behavior with debug"
echo "Running: WAITLOCK_DEBUG=1 waitlock --timeout 0.1 debug_test"
mkdir -p /tmp/debug_timeout
start_time=$(date +%s.%N)
# Use foreground execution with timeout wrapper
if [ -x "../../build/bin/waitlock" ]; then
    timeout 5 bash -c 'WAITLOCK_DEBUG=1 ../../build/bin/waitlock --lock-dir /tmp/debug_timeout --timeout 0.1 debug_test' 2>&1 | head -10
    exit_code=$?
    end_time=$(date +%s.%N)
    duration=$(echo "$end_time - $start_time" | bc -l)
    if [ $exit_code -eq 0 ]; then
        echo "✓ Process exited successfully, Duration: ${duration}s"
    else
        echo "✓ Process exited with timeout/error (expected), Duration: ${duration}s"
    fi
else
    echo "⚠ Waitlock binary not found - skipping test"
fi

# Test 3: Test with strace to see system calls
echo ""
echo "Test 3: Check system calls"
echo "Running: strace -e trace=nanosleep,gettimeofday waitlock --timeout 0.1 debug_test2"
timeout 3 strace -e trace=nanosleep,gettimeofday ../../build/bin/waitlock --lock-dir /tmp/debug_timeout --timeout 0.1 debug_test2 2>&1 | head -20 || echo "Strace completed or timed out"

echo ""
echo "=== Debug Complete ==="