#!/bin/bash
# Test semaphore functionality (--allowMultiple, -m)

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "Semaphore Functionality"

# Test basic semaphore
test_start "Basic semaphore (3 holders)"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 basic_sem >/dev/null 2>&1 &
SEM_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 basic_sem >/dev/null 2>&1 &
SEM_PID2=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 basic_sem >/dev/null 2>&1 &
SEM_PID3=$!

sleep 2

SEM_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "basic_sem" || echo 0)
if [ "$SEM_COUNT" -eq 3 ]; then
    test_pass "All 3 semaphore slots acquired"
else
    test_fail "Expected 3 semaphore holders, got $SEM_COUNT"
fi

kill $SEM_PID1 $SEM_PID2 $SEM_PID3 2>/dev/null || true

# Test semaphore limit enforcement
test_start "Semaphore limit enforcement"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 limited_sem >/dev/null 2>&1 &
LIMITED_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 limited_sem >/dev/null 2>&1 &
LIMITED_PID2=$!

sleep 1

# Third should fail with timeout
if ! $WAITLOCK --lock-dir "$LOCK_DIR" -m 2 --timeout 1 limited_sem >/dev/null 2>&1; then
    test_pass "Third process correctly blocked by semaphore limit"
else
    test_fail "Third process should be blocked by semaphore limit"
fi

kill $LIMITED_PID1 $LIMITED_PID2 2>/dev/null || true

# Test --allowMultiple long form
test_start "Semaphore long form (--allowMultiple)"
$WAITLOCK --lock-dir "$LOCK_DIR" --allowMultiple 2 long_sem >/dev/null 2>&1 &
LONG_SEM_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" --allowMultiple 2 long_sem >/dev/null 2>&1 &
LONG_SEM_PID2=$!

sleep 1

LONG_SEM_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "long_sem" || echo 0)
if [ "$LONG_SEM_COUNT" -eq 2 ]; then
    test_pass "Long form semaphore works"
else
    test_fail "Long form semaphore should work"
fi

kill $LONG_SEM_PID1 $LONG_SEM_PID2 2>/dev/null || true

# Test semaphore slot release
test_start "Semaphore slot release"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 release_sem >/dev/null 2>&1 &
RELEASE_SEM_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 release_sem >/dev/null 2>&1 &
RELEASE_SEM_PID2=$!

sleep 1

# Kill first process
kill $RELEASE_SEM_PID1 2>/dev/null || true

# Third process should now be able to acquire
if $WAITLOCK --lock-dir "$LOCK_DIR" -m 2 --timeout 2 release_sem >/dev/null 2>&1; then
    test_pass "Semaphore slot released and reacquired"
else
    test_fail "Semaphore slot should be released and reacquired"
fi

kill $RELEASE_SEM_PID2 2>/dev/null || true

# Test semaphore with value 1 (should act like mutex)
test_start "Semaphore with value 1"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 1 mutex_sem >/dev/null 2>&1 &
MUTEX_SEM_PID=$!

sleep 1

if ! $WAITLOCK --lock-dir "$LOCK_DIR" -m 1 --timeout 1 mutex_sem >/dev/null 2>&1; then
    test_pass "Semaphore with value 1 acts like mutex"
else
    test_fail "Semaphore with value 1 should act like mutex"
fi

kill $MUTEX_SEM_PID 2>/dev/null || true

# Test large semaphore value
test_start "Large semaphore value"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 10 large_sem >/dev/null 2>&1 &
LARGE_SEM_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 10 large_sem >/dev/null 2>&1 &
LARGE_SEM_PID2=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 10 large_sem >/dev/null 2>&1 &
LARGE_SEM_PID3=$!

sleep 1

LARGE_SEM_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "large_sem" || echo 0)
if [ "$LARGE_SEM_COUNT" -eq 3 ]; then
    test_pass "Large semaphore value works"
else
    test_fail "Large semaphore value should work"
fi

kill $LARGE_SEM_PID1 $LARGE_SEM_PID2 $LARGE_SEM_PID3 2>/dev/null || true

# Test semaphore with signal
test_start "Semaphore with signal"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 signal_sem >/dev/null 2>&1 &
SIGNAL_SEM_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" -m 2 signal_sem >/dev/null 2>&1 &
SIGNAL_SEM_PID2=$!

sleep 1

# Send signal to first process
kill -TERM $SIGNAL_SEM_PID1 2>/dev/null || true

# Third process should now be able to acquire
if $WAITLOCK --lock-dir "$LOCK_DIR" -m 2 --timeout 2 signal_sem >/dev/null 2>&1; then
    test_pass "Semaphore slot released after signal"
else
    test_fail "Semaphore slot should be released after signal"
fi

kill $SIGNAL_SEM_PID2 2>/dev/null || true

# Test semaphore error conditions
test_start "Semaphore error conditions"
# Test with zero value
if ! $WAITLOCK --lock-dir "$LOCK_DIR" -m 0 zero_sem >/dev/null 2>&1; then
    test_pass "Zero semaphore value rejected"
else
    test_fail "Zero semaphore value should be rejected"
fi

# Test with negative value  
if ! $WAITLOCK --lock-dir "$LOCK_DIR" -m -1 negative_sem >/dev/null 2>&1; then
    test_pass "Negative semaphore value rejected"
else
    test_fail "Negative semaphore value should be rejected"
fi

# Test semaphore inconsistency detection
test_start "Semaphore inconsistency detection"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 3 inconsistent_sem >/dev/null 2>&1 &
INCONSISTENT_PID1=$!

sleep 1

# Try to acquire with different max_holders value
if ! $WAITLOCK --lock-dir "$LOCK_DIR" -m 5 --timeout 1 inconsistent_sem >/dev/null 2>&1; then
    test_pass "Inconsistent semaphore values detected"
else
    test_fail "Inconsistent semaphore values should be detected"
fi

kill $INCONSISTENT_PID1 2>/dev/null || true

# Test semaphore with mixed mutex/semaphore
test_start "Mixed mutex/semaphore access"
$WAITLOCK --lock-dir "$LOCK_DIR" mixed_lock >/dev/null 2>&1 &
MIXED_PID1=$!

sleep 1

# Try to acquire as semaphore
if ! $WAITLOCK --lock-dir "$LOCK_DIR" -m 2 --timeout 1 mixed_lock >/dev/null 2>&1; then
    test_pass "Mixed mutex/semaphore access rejected"
else
    test_fail "Mixed mutex/semaphore access should be rejected"
fi

kill $MIXED_PID1 2>/dev/null || true

test_suite_end