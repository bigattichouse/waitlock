#!/bin/bash
# Test --check command-line option

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "Check Command"

# Test check on non-existent lock
test_start "Check non-existent lock"
if $WAITLOCK --lock-dir "$LOCK_DIR" --check non_existent_lock >/dev/null 2>&1; then
    test_pass "Non-existent lock shows as available (exit 0)"
else
    test_fail "Non-existent lock should show as available"
fi

# Test check on held lock
test_start "Check held lock"
$WAITLOCK --lock-dir "$LOCK_DIR" check_held >/dev/null 2>&1 &
CHECK_HELD_PID=$!

sleep 1

if ! $WAITLOCK --lock-dir "$LOCK_DIR" --check check_held >/dev/null 2>&1; then
    test_pass "Held lock shows as unavailable (exit 1)"
else
    test_fail "Held lock should show as unavailable"
fi

kill $CHECK_HELD_PID 2>/dev/null || true

# Test check on released lock
test_start "Check released lock"
$WAITLOCK --lock-dir "$LOCK_DIR" check_released >/dev/null 2>&1 &
CHECK_RELEASED_PID=$!

sleep 1
kill $CHECK_RELEASED_PID 2>/dev/null || true
sleep 1

if $WAITLOCK --lock-dir "$LOCK_DIR" --check check_released >/dev/null 2>&1; then
    test_pass "Released lock shows as available (exit 0)"
else
    test_fail "Released lock should show as available"
fi

# Test check with semaphore (partially full)
test_start "Check semaphore (partially full)"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 check_sem >/dev/null 2>&1 &
CHECK_SEM_PID=$!

sleep 1

if $WAITLOCK --lock-dir "$LOCK_DIR" --check check_sem >/dev/null 2>&1; then
    test_pass "Partially full semaphore shows as available"
else
    test_fail "Partially full semaphore should show as available"
fi

kill $CHECK_SEM_PID 2>/dev/null || true

# Test check with semaphore (full)
test_start "Check semaphore (full)"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 check_sem_full >/dev/null 2>&1 &
CHECK_SEM_FULL_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 check_sem_full >/dev/null 2>&1 &
CHECK_SEM_FULL_PID2=$!

sleep 1

if ! $WAITLOCK --lock-dir "$LOCK_DIR" --check check_sem_full >/dev/null 2>&1; then
    test_pass "Full semaphore shows as unavailable"
else
    test_fail "Full semaphore should show as unavailable"
fi

kill $CHECK_SEM_FULL_PID1 $CHECK_SEM_FULL_PID2 2>/dev/null || true

# Test check exit codes
test_start "Check exit codes"
$WAITLOCK --lock-dir "$LOCK_DIR" check_exit_codes >/dev/null 2>&1 &
CHECK_EXIT_PID=$!

sleep 1

# Should exit with code 1 (lock held)
$WAITLOCK --lock-dir "$LOCK_DIR" --check check_exit_codes >/dev/null 2>&1
CHECK_EXIT_CODE=$?

if [ $CHECK_EXIT_CODE -eq 1 ]; then
    test_pass "Check returns exit code 1 for held lock"
else
    test_fail "Check should return exit code 1 for held lock, got $CHECK_EXIT_CODE"
fi

kill $CHECK_EXIT_PID 2>/dev/null || true
sleep 1

# Should exit with code 0 (lock available)
$WAITLOCK --lock-dir "$LOCK_DIR" --check check_exit_codes >/dev/null 2>&1
CHECK_EXIT_CODE=$?

if [ $CHECK_EXIT_CODE -eq 0 ]; then
    test_pass "Check returns exit code 0 for available lock"
else
    test_fail "Check should return exit code 0 for available lock, got $CHECK_EXIT_CODE"
fi

# Test check with invalid descriptor
test_start "Check invalid descriptor"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --check "invalid@descriptor" >/dev/null 2>&1; then
    CHECK_INVALID_CODE=$?
    if [ $CHECK_INVALID_CODE -eq 3 ]; then
        test_pass "Check returns exit code 3 for invalid descriptor"
    else
        test_pass "Check rejects invalid descriptor (exit code $CHECK_INVALID_CODE)"
    fi
else
    test_fail "Check should reject invalid descriptor"
fi

# Test check with timeout (should be ignored)
test_start "Check with timeout (should be ignored)"
$WAITLOCK --lock-dir "$LOCK_DIR" check_timeout >/dev/null 2>&1 &
CHECK_TIMEOUT_PID=$!

sleep 1

# Check should return immediately regardless of timeout
START_TIME=$(date +%s)
$WAITLOCK --lock-dir "$LOCK_DIR" --check --timeout 10 check_timeout >/dev/null 2>&1
END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

if [ $DURATION -lt 2 ]; then
    test_pass "Check ignores timeout and returns immediately"
else
    test_fail "Check should ignore timeout, took ${DURATION}s"
fi

kill $CHECK_TIMEOUT_PID 2>/dev/null || true

# Test check with long descriptor name
test_start "Check with long descriptor name"
LONG_DESC=$(printf 'a%.0s' {1..200})
if $WAITLOCK --lock-dir "$LOCK_DIR" --check "$LONG_DESC" >/dev/null 2>&1; then
    test_pass "Check handles long descriptor names"
else
    test_fail "Check should handle long descriptor names"
fi

# Test check with empty descriptor
test_start "Check with empty descriptor"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --check "" >/dev/null 2>&1; then
    test_pass "Check rejects empty descriptor"
else
    test_fail "Check should reject empty descriptor"
fi

# Test check behavior with stale locks
test_start "Check behavior with stale locks"
$WAITLOCK --lock-dir "$LOCK_DIR" check_stale >/dev/null 2>&1 &
CHECK_STALE_PID=$!

sleep 1

# Kill process abruptly
kill -9 $CHECK_STALE_PID 2>/dev/null || true

# Give time for stale detection
sleep 2

if $WAITLOCK --lock-dir "$LOCK_DIR" --check check_stale >/dev/null 2>&1; then
    test_pass "Check properly handles stale locks"
else
    test_fail "Check should detect and handle stale locks"
fi

test_suite_end