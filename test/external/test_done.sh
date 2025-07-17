#!/bin/bash
# Test --done functionality

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "Done Functionality"

# Test basic done functionality
test_start "Basic done functionality"
$WAITLOCK --lock-dir "$LOCK_DIR" done_basic >/dev/null 2>&1 &
DONE_PID=$!

if wait_for_lock "done_basic"; then
    if $WAITLOCK --lock-dir "$LOCK_DIR" --done done_basic >/dev/null 2>&1; then
        if wait_for_unlock "done_basic"; then
            test_pass "Basic done functionality works"
        else
            test_fail "Lock not released after done signal"
        fi
    else
        test_fail "Done signal failed"
    fi
else
    test_fail "Lock not acquired initially"
fi

kill $DONE_PID 2>/dev/null || true

# Test done on non-existent lock
test_start "Done on non-existent lock"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --done non_existent_done >/dev/null 2>&1; then
    test_pass "Done on non-existent lock correctly fails"
else
    test_fail "Done on non-existent lock should fail"
fi

# Test done with semaphore
test_start "Done with semaphore"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 done_semaphore >/dev/null 2>&1 &
DONE_SEM_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 done_semaphore >/dev/null 2>&1 &
DONE_SEM_PID2=$!

sleep 1

if $WAITLOCK --lock-dir "$LOCK_DIR" --done done_semaphore >/dev/null 2>&1; then
    if wait_for_unlock "done_semaphore"; then
        test_pass "Done with semaphore releases all holders"
    else
        test_fail "Semaphore holders not released after done signal"
    fi
else
    test_fail "Done signal failed for semaphore"
fi

kill $DONE_SEM_PID1 $DONE_SEM_PID2 2>/dev/null || true

# Test done exit code
test_start "Done exit code"
$WAITLOCK --lock-dir "$LOCK_DIR" done_exit_code >/dev/null 2>&1 &
DONE_EXIT_PID=$!

sleep 1

$WAITLOCK --lock-dir "$LOCK_DIR" --done done_exit_code >/dev/null 2>&1
DONE_EXIT_CODE=$?

if [ $DONE_EXIT_CODE -eq 0 ]; then
    test_pass "Done returns exit code 0 on success"
else
    test_fail "Done should return exit code 0 on success, got $DONE_EXIT_CODE"
fi

kill $DONE_EXIT_PID 2>/dev/null || true

# Test done with multiple holders
test_start "Done with multiple holders"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 done_multiple >/dev/null 2>&1 &
DONE_MULTI_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 done_multiple >/dev/null 2>&1 &
DONE_MULTI_PID2=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 done_multiple >/dev/null 2>&1 &
DONE_MULTI_PID3=$!

sleep 1

MULTI_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "done_multiple" || echo 0)
if [ "$MULTI_COUNT" -eq 3 ]; then
    if $WAITLOCK --lock-dir "$LOCK_DIR" --done done_multiple >/dev/null 2>&1; then
        if wait_for_unlock "done_multiple"; then
            test_pass "Done releases all multiple holders"
        else
            test_fail "Multiple holders not released after done signal"
        fi
    else
        test_fail "Done signal failed for multiple holders"
    fi
else
    test_fail "Expected 3 holders, got $MULTI_COUNT"
fi

kill $DONE_MULTI_PID1 $DONE_MULTI_PID2 $DONE_MULTI_PID3 2>/dev/null || true

# Test done timing
test_start "Done timing"
$WAITLOCK --lock-dir "$LOCK_DIR" done_timing >/dev/null 2>&1 &
DONE_TIMING_PID=$!

sleep 1

START_TIME=$(date +%s.%N)
if $WAITLOCK --lock-dir "$LOCK_DIR" --done done_timing >/dev/null 2>&1; then
    END_TIME=$(date +%s.%N)
    DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)
    
    if (( $(echo "$DURATION < 1.0" | bc -l) )); then
        test_pass "Done signal processed quickly"
    else
        test_fail "Done signal took too long: ${DURATION}s"
    fi
else
    test_fail "Done signal should succeed"
fi

kill $DONE_TIMING_PID 2>/dev/null || true

# Test done with process that ignores signals
test_start "Done with signal-ignoring process"
# This test simulates a process that might ignore SIGTERM
# We'll use sleep which should respond to signals
$WAITLOCK --lock-dir "$LOCK_DIR" done_ignore >/dev/null 2>&1 &
DONE_IGNORE_PID=$!

sleep 1

if $WAITLOCK --lock-dir "$LOCK_DIR" --done done_ignore >/dev/null 2>&1; then
    if wait_for_unlock "done_ignore"; then
        test_pass "Done works with signal-responsive process"
    else
        test_fail "Process should respond to done signal"
    fi
else
    test_fail "Done signal should succeed"
fi

kill $DONE_IGNORE_PID 2>/dev/null || true

# Test done with invalid descriptor
test_start "Done with invalid descriptor"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --done "invalid@descriptor" >/dev/null 2>&1; then
    test_pass "Done with invalid descriptor rejected"
else
    test_fail "Done with invalid descriptor should be rejected"
fi

# Test done with empty descriptor
test_start "Done with empty descriptor"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --done "" >/dev/null 2>&1; then
    test_pass "Done with empty descriptor rejected"
else
    test_fail "Done with empty descriptor should be rejected"
fi

# Test done after process already exited
test_start "Done after process exited"
$WAITLOCK --lock-dir "$LOCK_DIR" done_already_exited >/dev/null 2>&1 &
DONE_ALREADY_PID=$!

sleep 1

# Kill the process first
kill $DONE_ALREADY_PID 2>/dev/null || true
wait $DONE_ALREADY_PID 2>/dev/null || true

sleep 1

# Now try done (should fail since process is gone)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --done done_already_exited >/dev/null 2>&1; then
    test_pass "Done after process exited correctly fails"
else
    test_fail "Done after process exited should fail"
fi

# Test done with long descriptor
test_start "Done with long descriptor"
LONG_DESC=$(printf 'a%.0s' {1..200})
$WAITLOCK --lock-dir "$LOCK_DIR" "$LONG_DESC" >/dev/null 2>&1 &
DONE_LONG_PID=$!

sleep 1

if $WAITLOCK --lock-dir "$LOCK_DIR" --done "$LONG_DESC" >/dev/null 2>&1; then
    if wait_for_unlock "$LONG_DESC"; then
        test_pass "Done works with long descriptor"
    else
        test_fail "Long descriptor process should respond to done signal"
    fi
else
    test_fail "Done with long descriptor should succeed"
fi

kill $DONE_LONG_PID 2>/dev/null || true

# Test multiple done signals
test_start "Multiple done signals"
$WAITLOCK --lock-dir "$LOCK_DIR" done_multiple_signals >/dev/null 2>&1 &
DONE_MULTI_SIGNAL_PID=$!

sleep 1

# Send multiple done signals rapidly
for i in {1..5}; do
    $WAITLOCK --lock-dir "$LOCK_DIR" --done done_multiple_signals >/dev/null 2>&1 &
    sleep 0.1
done

# Should still work
if wait_for_unlock "done_multiple_signals"; then
    test_pass "Multiple done signals handled correctly"
else
    test_fail "Multiple done signals should not cause issues"
fi

kill $DONE_MULTI_SIGNAL_PID 2>/dev/null || true

# Test done with concurrent access
test_start "Done with concurrent access"
$WAITLOCK --lock-dir "$LOCK_DIR" done_concurrent >/dev/null 2>&1 &
DONE_CONCURRENT_PID=$!

sleep 1

# Start multiple done processes
$WAITLOCK --lock-dir "$LOCK_DIR" --done done_concurrent >/dev/null 2>&1 &
DONE_PROC1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" --done done_concurrent >/dev/null 2>&1 &
DONE_PROC2=$!

# Wait for done processes
wait $DONE_PROC1 $DONE_PROC2 2>/dev/null || true

if wait_for_unlock "done_concurrent"; then
    test_pass "Concurrent done signals handled correctly"
else
    test_fail "Concurrent done signals should work"
fi

kill $DONE_CONCURRENT_PID 2>/dev/null || true

test_suite_end