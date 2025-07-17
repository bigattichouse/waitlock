#!/bin/bash
# Test --onePerCPU functionality

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "OnePerCPU Functionality"

# Get CPU count for testing
CPU_COUNT=$(nproc)

# Test basic onePerCPU
test_start "Basic onePerCPU"
# Start processes up to CPU count
ONECPU_PIDS=()
for i in $(seq 1 $CPU_COUNT); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU onecpu_basic >/dev/null 2>&1 &
    ONECPU_PIDS+=($!)
done

sleep 2

# All should be running
ONECPU_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "onecpu_basic" || echo 0)
if [ "$ONECPU_COUNT" -eq "$CPU_COUNT" ]; then
    test_pass "OnePerCPU allows $CPU_COUNT processes"
else
    test_fail "OnePerCPU should allow $CPU_COUNT processes, got $ONECPU_COUNT"
fi

# Clean up
for pid in "${ONECPU_PIDS[@]}"; do
    kill $pid 2>/dev/null || true
done

# Test onePerCPU limit enforcement
test_start "OnePerCPU limit enforcement"
# Start processes up to CPU count
ONECPU_LIMIT_PIDS=()
for i in $(seq 1 $CPU_COUNT); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU onecpu_limit >/dev/null 2>&1 &
    ONECPU_LIMIT_PIDS+=($!)
done

sleep 1

# One more should fail with timeout
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --timeout 1 onecpu_limit >/dev/null 2>&1; then
    test_pass "OnePerCPU enforces CPU limit"
else
    test_fail "OnePerCPU should enforce CPU limit"
fi

# Clean up
for pid in "${ONECPU_LIMIT_PIDS[@]}"; do
    kill $pid 2>/dev/null || true
done

# Test onePerCPU with explicit CPU count
test_start "OnePerCPU with explicit CPU count"
EXPLICIT_COUNT=2
ONECPU_EXPLICIT_PIDS=()
for i in $(seq 1 $EXPLICIT_COUNT); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --cpus $EXPLICIT_COUNT onecpu_explicit >/dev/null 2>&1 &
    ONECPU_EXPLICIT_PIDS+=($!)
done

sleep 1

# Should allow exactly the specified count
EXPLICIT_RUNNING=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "onecpu_explicit" || echo 0)
if [ "$EXPLICIT_RUNNING" -eq "$EXPLICIT_COUNT" ]; then
    test_pass "OnePerCPU respects explicit CPU count"
else
    test_fail "OnePerCPU should respect explicit CPU count"
fi

# Clean up
for pid in "${ONECPU_EXPLICIT_PIDS[@]}"; do
    kill $pid 2>/dev/null || true
done

# Test onePerCPU slot release
test_start "OnePerCPU slot release"
$WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU onecpu_release >/dev/null 2>&1 &
ONECPU_RELEASE_PID=$!

sleep 1

# Kill the process
kill $ONECPU_RELEASE_PID 2>/dev/null || true

# Should be able to acquire again
if $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --timeout 2 onecpu_release >/dev/null 2>&1; then
    test_pass "OnePerCPU slot released and reacquired"
else
    test_fail "OnePerCPU slot should be released and reacquired"
fi

# Test onePerCPU with timeout
test_start "OnePerCPU with timeout"
# Fill all slots
ONECPU_TIMEOUT_PIDS=()
for i in $(seq 1 $CPU_COUNT); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU onecpu_timeout >/dev/null 2>&1 &
    ONECPU_TIMEOUT_PIDS+=($!)
done

sleep 1

# Should timeout
START_TIME=$(date +%s)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --timeout 2 onecpu_timeout >/dev/null 2>&1; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    if [ $DURATION -ge 2 ] && [ $DURATION -le 4 ]; then
        test_pass "OnePerCPU respects timeout"
    else
        test_fail "OnePerCPU timeout duration incorrect: ${DURATION}s"
    fi
else
    test_fail "OnePerCPU should timeout when all slots full"
fi

# Clean up
for pid in "${ONECPU_TIMEOUT_PIDS[@]}"; do
    kill $pid 2>/dev/null || true
done

# Test onePerCPU with different descriptors
test_start "OnePerCPU with different descriptors"
$WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU onecpu_desc_a >/dev/null 2>&1 &
ONECPU_DESC_A_PID=$!
$WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU onecpu_desc_b >/dev/null 2>&1 &
ONECPU_DESC_B_PID=$!

sleep 1

# Both should be running (different descriptors)
DESC_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "onecpu_desc_" || echo 0)
if [ "$DESC_COUNT" -eq 2 ]; then
    test_pass "OnePerCPU works with different descriptors"
else
    test_fail "OnePerCPU should work with different descriptors"
fi

kill $ONECPU_DESC_A_PID $ONECPU_DESC_B_PID 2>/dev/null || true

# Test onePerCPU with exec
test_start "OnePerCPU with exec"
$WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --exec sleep 2 onecpu_exec >/dev/null 2>&1 &
ONECPU_EXEC_PID=$!

sleep 1

# Should be holding lock
if wait_for_lock "onecpu_exec"; then
    # Wait for completion
    wait $ONECPU_EXEC_PID 2>/dev/null || true
    
    # Should release lock
    if wait_for_unlock "onecpu_exec"; then
        test_pass "OnePerCPU works with exec"
    else
        test_fail "OnePerCPU should work with exec"
    fi
else
    test_fail "OnePerCPU exec should hold lock"
fi

# Test onePerCPU error conditions
test_start "OnePerCPU error conditions"
# Test with zero CPUs
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --cpus 0 onecpu_zero >/dev/null 2>&1; then
    test_pass "OnePerCPU rejects zero CPUs"
else
    test_fail "OnePerCPU should reject zero CPUs"
fi

# Test with negative CPUs
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --cpus -1 onecpu_negative >/dev/null 2>&1; then
    test_pass "OnePerCPU rejects negative CPUs"
else
    test_fail "OnePerCPU should reject negative CPUs"
fi

# Test onePerCPU with signal handling
test_start "OnePerCPU with signal handling"
$WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU onecpu_signal >/dev/null 2>&1 &
ONECPU_SIGNAL_PID=$!

sleep 1

# Send signal
kill -TERM $ONECPU_SIGNAL_PID 2>/dev/null || true

# Should release slot
if wait_for_unlock "onecpu_signal"; then
    test_pass "OnePerCPU handles signals properly"
else
    test_fail "OnePerCPU should handle signals and release slot"
fi

# Test onePerCPU with large CPU count
test_start "OnePerCPU with large CPU count"
LARGE_CPU_COUNT=100
if $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --cpus $LARGE_CPU_COUNT onecpu_large >/dev/null 2>&1; then
    test_pass "OnePerCPU accepts large CPU count"
else
    test_fail "OnePerCPU should accept large CPU count"
fi

# Test onePerCPU consistency
test_start "OnePerCPU consistency"
$WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU onecpu_consistent >/dev/null 2>&1 &
ONECPU_CONSISTENT_PID=$!

sleep 1

# Try to acquire with different CPU count
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --cpus 2 --timeout 1 onecpu_consistent >/dev/null 2>&1; then
    test_pass "OnePerCPU detects inconsistent CPU counts"
else
    test_fail "OnePerCPU should detect inconsistent CPU counts"
fi

kill $ONECPU_CONSISTENT_PID 2>/dev/null || true

# Test onePerCPU with mixed mode
test_start "OnePerCPU with mixed mode"
$WAITLOCK --lock-dir "$LOCK_DIR" onecpu_mixed >/dev/null 2>&1 &
ONECPU_MIXED_PID=$!

sleep 1

# Try to acquire with onePerCPU
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --timeout 1 onecpu_mixed >/dev/null 2>&1; then
    test_pass "OnePerCPU rejects mixed mode access"
else
    test_fail "OnePerCPU should reject mixed mode access"
fi

kill $ONECPU_MIXED_PID 2>/dev/null || true

test_suite_end