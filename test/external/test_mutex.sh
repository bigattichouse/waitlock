#!/bin/bash
# Test basic mutex functionality

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "Mutex Functionality"

# Test basic mutex lock
test_start "Basic mutex lock"
$WAITLOCK --lock-dir "$LOCK_DIR" basic_mutex >/dev/null 2>&1 &
MUTEX_PID=$!

if wait_for_lock "basic_mutex"; then
    test_pass "Mutex lock acquired and appears in list"
else
    test_fail "Mutex lock should appear in list"
fi

kill $MUTEX_PID 2>/dev/null || true

# Test mutex exclusivity
test_start "Mutex exclusivity"
$WAITLOCK --lock-dir "$LOCK_DIR" exclusive_mutex >/dev/null 2>&1 &
EXCLUSIVE_PID=$!

sleep 1

# Try to acquire same lock (should fail)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 exclusive_mutex >/dev/null 2>&1; then
    test_pass "Second process correctly blocked by mutex"
else
    test_fail "Second process should be blocked by mutex"
fi

kill $EXCLUSIVE_PID 2>/dev/null || true

# Test mutex release on process exit
test_start "Mutex release on process exit"
$WAITLOCK --lock-dir "$LOCK_DIR" release_mutex >/dev/null 2>&1 &
RELEASE_PID=$!

sleep 1

# Kill the process
kill $RELEASE_PID 2>/dev/null || true

# Verify lock is released
if wait_for_unlock "release_mutex"; then
    test_pass "Mutex released when process exits"
else
    test_fail "Mutex should be released when process exits"
fi

# Test mutex with explicit lock directory
test_start "Mutex with explicit lock directory"
$WAITLOCK --lock-dir "$LOCK_DIR" explicit_mutex >/dev/null 2>&1 &
EXPLICIT_PID=$!

sleep 1

if wait_for_lock "explicit_mutex"; then
    test_pass "Mutex works with explicit lock directory"
else
    test_fail "Mutex should work with explicit lock directory"
fi

kill $EXPLICIT_PID 2>/dev/null || true

# Test mutex with -d short form
test_start "Mutex with -d short form"
$WAITLOCK -d "$LOCK_DIR" short_mutex >/dev/null 2>&1 &
SHORT_PID=$!

sleep 1

if wait_for_lock "short_mutex"; then
    test_pass "Mutex works with -d short form"
else
    test_fail "Mutex should work with -d short form"
fi

kill $SHORT_PID 2>/dev/null || true

# Test mutex acquisition order
test_start "Mutex acquisition order"
$WAITLOCK --lock-dir "$LOCK_DIR" order_mutex >/dev/null 2>&1 &
ORDER_PID1=$!

sleep 1

# Start second process (should wait)
$WAITLOCK --lock-dir "$LOCK_DIR" --timeout 5 order_mutex >/dev/null 2>&1 &
ORDER_PID2=$!

sleep 1

# Kill first process
kill $ORDER_PID1 2>/dev/null || true

# Second process should acquire the lock
if wait $ORDER_PID2 2>/dev/null; then
    test_pass "Second process acquired lock after first released"
else
    test_fail "Second process should acquire lock after first released"
fi

# Test mutex with different descriptors
test_start "Different mutex descriptors"
$WAITLOCK --lock-dir "$LOCK_DIR" mutex_a >/dev/null 2>&1 &
MUTEX_A_PID=$!
$WAITLOCK --lock-dir "$LOCK_DIR" mutex_b >/dev/null 2>&1 &
MUTEX_B_PID=$!

sleep 1

# Both should be acquired (different descriptors)
MUTEX_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "mutex_" || echo 0)
if [ "$MUTEX_COUNT" -eq 2 ]; then
    test_pass "Different mutex descriptors can be held simultaneously"
else
    test_fail "Different mutex descriptors should be independent"
fi

kill $MUTEX_A_PID $MUTEX_B_PID 2>/dev/null || true

# Test mutex with signal interruption
test_start "Mutex with signal interruption"
$WAITLOCK --lock-dir "$LOCK_DIR" signal_mutex >/dev/null 2>&1 &
SIGNAL_PID=$!

sleep 1

# Send SIGTERM
kill -TERM $SIGNAL_PID 2>/dev/null || true

# Verify lock is cleaned up
if wait_for_unlock "signal_mutex"; then
    test_pass "Mutex cleaned up after signal"
else
    test_fail "Mutex should be cleaned up after signal"
fi

# Test mutex with SIGKILL
test_start "Mutex with SIGKILL"
$WAITLOCK --lock-dir "$LOCK_DIR" sigkill_mutex >/dev/null 2>&1 &
SIGKILL_PID=$!

sleep 1

# Send SIGKILL
kill -9 $SIGKILL_PID 2>/dev/null || true

# Give time for cleanup
sleep 2

# Try to acquire same lock (should work if cleanup worked)
if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 sigkill_mutex >/dev/null 2>&1; then
    test_pass "Mutex cleaned up after SIGKILL"
else
    test_fail "Mutex should be cleaned up after SIGKILL"
fi

# Test mutex with long descriptor
test_start "Mutex with long descriptor"
LONG_DESC=$(printf 'a%.0s' {1..200})
$WAITLOCK --lock-dir "$LOCK_DIR" "$LONG_DESC" >/dev/null 2>&1 &
LONG_PID=$!

sleep 1

if wait_for_lock "$LONG_DESC"; then
    test_pass "Mutex works with long descriptor"
else
    test_fail "Mutex should work with long descriptor"
fi

kill $LONG_PID 2>/dev/null || true

# Test mutex with special characters in descriptor
test_start "Mutex with valid special characters"
$WAITLOCK --lock-dir "$LOCK_DIR" "mutex_test-123.valid" >/dev/null 2>&1 &
SPECIAL_PID=$!

sleep 1

if wait_for_lock "mutex_test-123.valid"; then
    test_pass "Mutex works with valid special characters"
else
    test_fail "Mutex should work with valid special characters"
fi

kill $SPECIAL_PID 2>/dev/null || true

test_suite_end