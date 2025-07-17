#!/bin/bash
# Test --exec functionality

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "Exec Functionality"

# Test basic exec
test_start "Basic exec"
if $WAITLOCK --lock-dir "$LOCK_DIR" --exec echo "test output" test_exec_basic >/dev/null 2>&1; then
    test_pass "Basic exec works"
else
    test_fail "Basic exec should work"
fi

# Test exec with command that fails
test_start "Exec with failing command"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --exec false test_exec_fail >/dev/null 2>&1; then
    test_pass "Exec propagates command failure"
else
    test_fail "Exec should propagate command failure"
fi

# Test exec with multiple arguments
test_start "Exec with multiple arguments"
if $WAITLOCK --lock-dir "$LOCK_DIR" --exec echo "arg1" "arg2" "arg3" test_exec_multi >/dev/null 2>&1; then
    test_pass "Exec works with multiple arguments"
else
    test_fail "Exec should work with multiple arguments"
fi

# Test exec with shell command
test_start "Exec with shell command"
if $WAITLOCK --lock-dir "$LOCK_DIR" --exec sh -c "echo hello | grep hello" test_exec_shell >/dev/null 2>&1; then
    test_pass "Exec works with shell commands"
else
    test_fail "Exec should work with shell commands"
fi

# Test exec lock release on command completion
test_start "Exec releases lock on completion"
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sleep 1 test_exec_release >/dev/null 2>&1 &
EXEC_PID=$!

# Wait for lock acquisition
sleep 0.5

# Verify lock exists
if wait_for_lock "test_exec_release"; then
    # Wait for command to complete
    wait $EXEC_PID 2>/dev/null || true
    
    # Verify lock is released
    if wait_for_unlock "test_exec_release"; then
        test_pass "Exec releases lock on command completion"
    else
        test_fail "Exec should release lock on command completion"
    fi
else
    test_fail "Exec should acquire lock initially"
fi

# Test exec with timeout
test_start "Exec with timeout"
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sleep 10 test_exec_timeout_holder >/dev/null 2>&1 &
EXEC_TIMEOUT_PID=$!

sleep 1

# Try to acquire with timeout
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 --exec echo "should timeout" test_exec_timeout_holder >/dev/null 2>&1; then
    test_pass "Exec respects timeout"
else
    test_fail "Exec should respect timeout"
fi

kill $EXEC_TIMEOUT_PID 2>/dev/null || true

# Test exec with signal handling
test_start "Exec with signal handling"
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sleep 10 test_exec_signal >/dev/null 2>&1 &
EXEC_SIGNAL_PID=$!

sleep 1

# Send signal
kill -TERM $EXEC_SIGNAL_PID 2>/dev/null || true

# Wait for cleanup
sleep 1

# Verify lock is released
if wait_for_unlock "test_exec_signal"; then
    test_pass "Exec handles signals properly"
else
    test_fail "Exec should handle signals and release lock"
fi

# Test exec with semaphore
test_start "Exec with semaphore"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 --exec sleep 2 test_exec_semaphore >/dev/null 2>&1 &
EXEC_SEM_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 --exec sleep 2 test_exec_semaphore >/dev/null 2>&1 &
EXEC_SEM_PID2=$!

sleep 1

# Both should be running
SEM_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "test_exec_semaphore" || echo 0)
if [ "$SEM_COUNT" -eq 2 ]; then
    test_pass "Exec works with semaphore"
else
    test_fail "Exec should work with semaphore"
fi

wait $EXEC_SEM_PID1 $EXEC_SEM_PID2 2>/dev/null || true

# Test exec exit code propagation
test_start "Exec exit code propagation"
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sh -c "exit 42" test_exec_exit_code >/dev/null 2>&1
EXEC_EXIT_CODE=$?

if [ $EXEC_EXIT_CODE -eq 42 ]; then
    test_pass "Exec propagates command exit code"
else
    test_fail "Exec should propagate command exit code, got $EXEC_EXIT_CODE"
fi

# Test exec with command not found
test_start "Exec with command not found"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --exec nonexistent_command test_exec_not_found >/dev/null 2>&1; then
    test_pass "Exec handles command not found"
else
    test_fail "Exec should handle command not found"
fi

# Test exec with empty command
test_start "Exec with empty command"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --exec "" test_exec_empty >/dev/null 2>&1; then
    test_pass "Exec rejects empty command"
else
    test_fail "Exec should reject empty command"
fi

# Test exec with long running command
test_start "Exec with long running command"
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sleep 3 test_exec_long >/dev/null 2>&1 &
EXEC_LONG_PID=$!

sleep 1

# Should be holding lock
if wait_for_lock "test_exec_long"; then
    # Wait for completion
    wait $EXEC_LONG_PID 2>/dev/null || true
    
    # Should release lock
    if wait_for_unlock "test_exec_long"; then
        test_pass "Exec handles long running commands"
    else
        test_fail "Exec should release lock after long command"
    fi
else
    test_fail "Exec should hold lock during long command"
fi

# Test exec with environment variables
test_start "Exec with environment variables"
if $WAITLOCK --lock-dir "$LOCK_DIR" --exec env test_exec_env >/dev/null 2>&1; then
    test_pass "Exec preserves environment"
else
    test_fail "Exec should preserve environment"
fi

# Test exec with path-based command
test_start "Exec with path-based command"
if $WAITLOCK --lock-dir "$LOCK_DIR" --exec /bin/echo "path command" test_exec_path >/dev/null 2>&1; then
    test_pass "Exec works with path-based commands"
else
    test_fail "Exec should work with path-based commands"
fi

# Test exec with special characters in arguments
test_start "Exec with special characters"
if $WAITLOCK --lock-dir "$LOCK_DIR" --exec echo "arg with spaces" test_exec_special >/dev/null 2>&1; then
    test_pass "Exec handles special characters in arguments"
else
    test_fail "Exec should handle special characters in arguments"
fi

test_suite_end