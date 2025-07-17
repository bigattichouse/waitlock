#!/bin/bash
# Test timeout functionality (--timeout, -t)

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "Timeout Functionality"

# Test basic timeout
test_start "Basic timeout"
$WAITLOCK --lock-dir "$LOCK_DIR" timeout_basic >/dev/null 2>&1 &
TIMEOUT_PID=$!

sleep 1

START_TIME=$(date +%s)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 2 timeout_basic >/dev/null 2>&1; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    if [ $DURATION -ge 2 ] && [ $DURATION -le 4 ]; then
        test_pass "Timeout occurred after ~2 seconds"
    else
        test_fail "Timeout duration incorrect: ${DURATION}s (expected ~2s)"
    fi
else
    test_fail "Timeout should have occurred"
fi

kill $TIMEOUT_PID 2>/dev/null || true

# Test -t short form
test_start "Timeout short form (-t)"
$WAITLOCK --lock-dir "$LOCK_DIR" timeout_short >/dev/null 2>&1 &
TIMEOUT_SHORT_PID=$!

sleep 1

START_TIME=$(date +%s)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" -t 1 timeout_short >/dev/null 2>&1; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    if [ $DURATION -ge 1 ] && [ $DURATION -le 3 ]; then
        test_pass "Short form timeout works"
    else
        test_fail "Short form timeout duration incorrect: ${DURATION}s"
    fi
else
    test_fail "Short form timeout should have occurred"
fi

kill $TIMEOUT_SHORT_PID 2>/dev/null || true

# Test fractional timeout
test_start "Fractional timeout"
$WAITLOCK --lock-dir "$LOCK_DIR" timeout_fractional >/dev/null 2>&1 &
TIMEOUT_FRAC_PID=$!

sleep 1

START_TIME=$(date +%s.%N)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 0.5 timeout_fractional >/dev/null 2>&1; then
    END_TIME=$(date +%s.%N)
    DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)
    
    if (( $(echo "$DURATION >= 0.4 && $DURATION <= 0.8" | bc -l) )); then
        test_pass "Fractional timeout works"
    else
        test_fail "Fractional timeout duration incorrect: ${DURATION}s"
    fi
else
    test_fail "Fractional timeout should have occurred"
fi

kill $TIMEOUT_FRAC_PID 2>/dev/null || true

# Test zero timeout (immediate)
test_start "Zero timeout (immediate)"
$WAITLOCK --lock-dir "$LOCK_DIR" timeout_zero >/dev/null 2>&1 &
TIMEOUT_ZERO_PID=$!

sleep 1

START_TIME=$(date +%s)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 0 timeout_zero >/dev/null 2>&1; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    if [ $DURATION -le 1 ]; then
        test_pass "Zero timeout returns immediately"
    else
        test_fail "Zero timeout should return immediately, took ${DURATION}s"
    fi
else
    test_fail "Zero timeout should fail immediately"
fi

kill $TIMEOUT_ZERO_PID 2>/dev/null || true

# Test timeout with semaphore
test_start "Timeout with semaphore"
$WAITLOCK --lock-dir "$LOCK_DIR" -m 1 timeout_semaphore >/dev/null 2>&1 &
TIMEOUT_SEM_PID=$!

sleep 1

START_TIME=$(date +%s)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" -m 1 --timeout 1 timeout_semaphore >/dev/null 2>&1; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    if [ $DURATION -ge 1 ] && [ $DURATION -le 3 ]; then
        test_pass "Timeout works with semaphore"
    else
        test_fail "Semaphore timeout duration incorrect: ${DURATION}s"
    fi
else
    test_fail "Semaphore timeout should have occurred"
fi

kill $TIMEOUT_SEM_PID 2>/dev/null || true

# Test timeout success case
test_start "Timeout success case"
$WAITLOCK --lock-dir "$LOCK_DIR" timeout_success >/dev/null 2>&1 &
TIMEOUT_SUCCESS_PID=$!

sleep 1

# Kill the holder after 1 second
(sleep 1; kill $TIMEOUT_SUCCESS_PID 2>/dev/null) &

# Should succeed within timeout
if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 3 timeout_success >/dev/null 2>&1; then
    test_pass "Timeout allows acquisition when lock becomes available"
else
    test_fail "Should acquire lock when it becomes available within timeout"
fi

# Test timeout with no existing lock
test_start "Timeout with no existing lock"
if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 timeout_no_lock >/dev/null 2>&1; then
    test_pass "Timeout allows immediate acquisition of available lock"
else
    test_fail "Should immediately acquire available lock regardless of timeout"
fi

# Test timeout exit code
test_start "Timeout exit code"
$WAITLOCK --lock-dir "$LOCK_DIR" timeout_exit_code >/dev/null 2>&1 &
TIMEOUT_EXIT_PID=$!

sleep 1

$WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 timeout_exit_code >/dev/null 2>&1
TIMEOUT_EXIT_CODE=$?

if [ $TIMEOUT_EXIT_CODE -eq 2 ]; then
    test_pass "Timeout returns exit code 2"
else
    test_fail "Timeout should return exit code 2, got $TIMEOUT_EXIT_CODE"
fi

kill $TIMEOUT_EXIT_PID 2>/dev/null || true

# Test large timeout value
test_start "Large timeout value"
$WAITLOCK --lock-dir "$LOCK_DIR" timeout_large >/dev/null 2>&1 &
TIMEOUT_LARGE_PID=$!

sleep 1

# Start with large timeout but kill holder quickly
(sleep 1; kill $TIMEOUT_LARGE_PID 2>/dev/null) &

if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 3600 timeout_large >/dev/null 2>&1; then
    test_pass "Large timeout value accepted"
else
    test_fail "Large timeout value should be accepted"
fi

# Test negative timeout
test_start "Negative timeout"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --timeout -1 timeout_negative >/dev/null 2>&1; then
    test_pass "Negative timeout rejected"
else
    test_fail "Negative timeout should be rejected"
fi

# Test timeout precision
test_start "Timeout precision"
$WAITLOCK --lock-dir "$LOCK_DIR" timeout_precision >/dev/null 2>&1 &
TIMEOUT_PRECISION_PID=$!

sleep 1

# Test multiple timeouts for consistency
CONSISTENT=true
for i in {1..3}; do
    START_TIME=$(date +%s.%N)
    $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 0.5 timeout_precision >/dev/null 2>&1 || true
    END_TIME=$(date +%s.%N)
    DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)
    
    if ! (( $(echo "$DURATION >= 0.4 && $DURATION <= 0.8" | bc -l) )); then
        CONSISTENT=false
        break
    fi
done

if $CONSISTENT; then
    test_pass "Timeout precision is consistent"
else
    test_fail "Timeout precision is inconsistent"
fi

kill $TIMEOUT_PRECISION_PID 2>/dev/null || true

# Test timeout with signal interruption
test_start "Timeout with signal interruption"
$WAITLOCK --lock-dir "$LOCK_DIR" timeout_signal >/dev/null 2>&1 &
TIMEOUT_SIGNAL_PID=$!

sleep 1

# Start waiter with timeout
$WAITLOCK --lock-dir "$LOCK_DIR" --timeout 5 timeout_signal >/dev/null 2>&1 &
TIMEOUT_WAITER_PID=$!

sleep 1

# Send signal to waiter
kill -TERM $TIMEOUT_WAITER_PID 2>/dev/null || true

# Waiter should exit due to signal, not timeout
if ! wait $TIMEOUT_WAITER_PID 2>/dev/null; then
    test_pass "Timeout interrupted by signal"
else
    test_fail "Timeout should be interrupted by signal"
fi

kill $TIMEOUT_SIGNAL_PID 2>/dev/null || true

test_suite_end