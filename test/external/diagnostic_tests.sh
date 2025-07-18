#!/bin/bash
# Diagnostic tests to isolate hanging issues in waitlock

set -e
source "$(dirname "$0")/test_framework.sh"

echo "=================================================="
echo "         WAITLOCK DIAGNOSTIC TEST SUITE"
echo "=================================================="

test_suite_start "Diagnostic Tests"

# Test 1: Basic lock acquisition (should work)
test_start "Basic lock acquisition (background)"
timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" diag_test1 >/dev/null 2>&1 &
LOCK_PID=$!
sleep 1

if kill -0 $LOCK_PID 2>/dev/null; then
    test_pass "Lock process started and running"
    kill $LOCK_PID 2>/dev/null || true
else
    test_fail "Lock process exited unexpectedly"
fi

# Test 2: Lock acquisition with timeout (should timeout)
test_start "Lock acquisition with immediate timeout"
$WAITLOCK --lock-dir "$LOCK_DIR" diag_test2 >/dev/null 2>&1 &
LOCK_PID2=$!
sleep 1

start_time=$(date +%s)
if timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 0.1 diag_test2 >/dev/null 2>&1; then
    test_fail "Should have timed out but succeeded"
else
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    if [ $duration -le 2 ]; then
        test_pass "Timeout worked correctly ($duration seconds)"
    else
        test_fail "Timeout took too long ($duration seconds)"
    fi
fi

kill $LOCK_PID2 2>/dev/null || true

# Test 3: Check functionality
test_start "Check command functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" diag_test3 >/dev/null 2>&1 &
LOCK_PID3=$!
sleep 1

if $WAITLOCK --lock-dir "$LOCK_DIR" --check diag_test3 >/dev/null 2>&1; then
    test_fail "Check should return busy (exit 1) but returned available (exit 0)"
else
    test_pass "Check correctly shows lock as busy"
fi

kill $LOCK_PID3 2>/dev/null || true

# Test 4: List functionality  
test_start "List command functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" diag_test4 >/dev/null 2>&1 &
LOCK_PID4=$!
sleep 1

list_output=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null)
if echo "$list_output" | grep -q "diag_test4"; then
    test_pass "List shows active lock"
else
    test_fail "List does not show active lock"
fi

kill $LOCK_PID4 2>/dev/null || true

# Test 5: Semaphore basic functionality
test_start "Semaphore lock acquisition"
timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" -m 3 diag_sem >/dev/null 2>&1 &
SEM_PID1=$!
sleep 1

timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" -m 3 diag_sem >/dev/null 2>&1 &
SEM_PID2=$!
sleep 1

if kill -0 $SEM_PID1 2>/dev/null && kill -0 $SEM_PID2 2>/dev/null; then
    test_pass "Multiple semaphore holders work"
else
    test_fail "Semaphore holders failed"
fi

kill $SEM_PID1 $SEM_PID2 2>/dev/null || true

# Test 6: Done command  
test_start "Done command functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" diag_done_test >/dev/null 2>&1 &
DONE_PID=$!
sleep 1

# Send done signal
if $WAITLOCK --lock-dir "$LOCK_DIR" --done diag_done_test >/dev/null 2>&1; then
    sleep 1
    if ! kill -0 $DONE_PID 2>/dev/null; then
        test_pass "Done command successfully signaled process"
    else
        test_fail "Process still running after done command"
        kill $DONE_PID 2>/dev/null || true
    fi
else
    test_fail "Done command failed"
    kill $DONE_PID 2>/dev/null || true
fi

# Test 7: Exec functionality
test_start "Exec command basic functionality"
start_time=$(date +%s)
if timeout 5 $WAITLOCK --lock-dir "$LOCK_DIR" --exec echo "test" diag_exec >/dev/null 2>&1; then
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    if [ $duration -le 3 ]; then
        test_pass "Exec command completed quickly ($duration seconds)"
    else
        test_fail "Exec command took too long ($duration seconds)"
    fi
else
    test_fail "Exec command failed or timed out"
fi

# Test 8: Signal handling
test_start "Signal handling"
$WAITLOCK --lock-dir "$LOCK_DIR" diag_signal >/dev/null 2>&1 &
SIGNAL_PID=$!
sleep 1

if kill -TERM $SIGNAL_PID 2>/dev/null; then
    sleep 1
    if ! kill -0 $SIGNAL_PID 2>/dev/null; then
        test_pass "Process responds to SIGTERM"
    else
        test_fail "Process does not respond to SIGTERM"
        kill -9 $SIGNAL_PID 2>/dev/null || true
    fi
else
    test_fail "Could not send signal to process"
fi

test_suite_end
echo ""
echo "=================================================="
echo "If these basic tests fail, we have fundamental issues."
echo "If they pass but other tests hang, the issue is in"
echo "specific test logic or more complex scenarios."
echo "=================================================="