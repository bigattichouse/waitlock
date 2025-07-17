#!/bin/bash
# Test --excludeCPUs functionality

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "ExcludeCPUs Functionality"

# Get CPU count for testing
CPU_COUNT=$(nproc)

# Test basic excludeCPUs
test_start "Basic excludeCPUs"
if [ $CPU_COUNT -gt 2 ]; then
    EXCLUDE_COUNT=2
    EXPECTED_COUNT=$((CPU_COUNT - EXCLUDE_COUNT))
    
    # Start processes up to expected count
    EXCLUDE_PIDS=()
    for i in $(seq 1 $EXPECTED_COUNT); do
        $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs $EXCLUDE_COUNT exclude_basic >/dev/null 2>&1 &
        EXCLUDE_PIDS+=($!)
    done

    sleep 2

    # Should allow expected count
    EXCLUDE_RUNNING=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "exclude_basic" || echo 0)
    if [ "$EXCLUDE_RUNNING" -eq "$EXPECTED_COUNT" ]; then
        test_pass "ExcludeCPUs allows $EXPECTED_COUNT processes (excluded $EXCLUDE_COUNT)"
    else
        test_fail "ExcludeCPUs should allow $EXPECTED_COUNT processes, got $EXCLUDE_RUNNING"
    fi

    # Clean up
    for pid in "${EXCLUDE_PIDS[@]}"; do
        kill $pid 2>/dev/null || true
    done
else
    test_pass "ExcludeCPUs test skipped (insufficient CPUs)"
fi

# Test excludeCPUs limit enforcement
test_start "ExcludeCPUs limit enforcement"
if [ $CPU_COUNT -gt 1 ]; then
    EXCLUDE_LIMIT=1
    EXPECTED_LIMIT=$((CPU_COUNT - EXCLUDE_LIMIT))
    
    # Fill all available slots
    EXCLUDE_LIMIT_PIDS=()
    for i in $(seq 1 $EXPECTED_LIMIT); do
        $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs $EXCLUDE_LIMIT exclude_limit >/dev/null 2>&1 &
        EXCLUDE_LIMIT_PIDS+=($!)
    done

    sleep 1

    # One more should fail
    if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs $EXCLUDE_LIMIT --timeout 1 exclude_limit >/dev/null 2>&1; then
        test_pass "ExcludeCPUs enforces reduced limit"
    else
        test_fail "ExcludeCPUs should enforce reduced limit"
    fi

    # Clean up
    for pid in "${EXCLUDE_LIMIT_PIDS[@]}"; do
        kill $pid 2>/dev/null || true
    done
else
    test_pass "ExcludeCPUs limit test skipped (insufficient CPUs)"
fi

# Test excludeCPUs with explicit CPU count
test_start "ExcludeCPUs with explicit CPU count"
EXPLICIT_CPUS=4
EXCLUDE_EXPLICIT=1
EXPECTED_EXPLICIT=$((EXPLICIT_CPUS - EXCLUDE_EXPLICIT))

EXCLUDE_EXPLICIT_PIDS=()
for i in $(seq 1 $EXPECTED_EXPLICIT); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --cpus $EXPLICIT_CPUS --excludeCPUs $EXCLUDE_EXPLICIT exclude_explicit >/dev/null 2>&1 &
    EXCLUDE_EXPLICIT_PIDS+=($!)
done

sleep 1

# Should allow expected count
EXPLICIT_RUNNING=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "exclude_explicit" || echo 0)
if [ "$EXPLICIT_RUNNING" -eq "$EXPECTED_EXPLICIT" ]; then
    test_pass "ExcludeCPUs works with explicit CPU count"
else
    test_fail "ExcludeCPUs should work with explicit CPU count"
fi

# Clean up
for pid in "${EXCLUDE_EXPLICIT_PIDS[@]}"; do
    kill $pid 2>/dev/null || true
done

# Test excludeCPUs error conditions
test_start "ExcludeCPUs error conditions"
# Test excluding all CPUs
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs $CPU_COUNT exclude_all >/dev/null 2>&1; then
    test_pass "ExcludeCPUs rejects excluding all CPUs"
else
    test_fail "ExcludeCPUs should reject excluding all CPUs"
fi

# Test excluding more than available
OVER_EXCLUDE=$((CPU_COUNT + 1))
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs $OVER_EXCLUDE exclude_over >/dev/null 2>&1; then
    test_pass "ExcludeCPUs rejects excluding more than available"
else
    test_fail "ExcludeCPUs should reject excluding more than available"
fi

# Test negative exclude count
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs -1 exclude_negative >/dev/null 2>&1; then
    test_pass "ExcludeCPUs rejects negative exclude count"
else
    test_fail "ExcludeCPUs should reject negative exclude count"
fi

# Test excludeCPUs without onePerCPU
test_start "ExcludeCPUs without onePerCPU"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --excludeCPUs 1 exclude_no_onecpu >/dev/null 2>&1; then
    test_pass "ExcludeCPUs requires onePerCPU"
else
    test_fail "ExcludeCPUs should require onePerCPU"
fi

# Test excludeCPUs with semaphore
test_start "ExcludeCPUs with semaphore"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --allowMultiple 2 --excludeCPUs 1 exclude_semaphore >/dev/null 2>&1; then
    test_pass "ExcludeCPUs rejects semaphore mode"
else
    test_fail "ExcludeCPUs should reject semaphore mode"
fi

# Test excludeCPUs slot release
test_start "ExcludeCPUs slot release"
if [ $CPU_COUNT -gt 1 ]; then
    $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs 1 exclude_release >/dev/null 2>&1 &
    EXCLUDE_RELEASE_PID=$!

    sleep 1

    # Kill the process
    kill $EXCLUDE_RELEASE_PID 2>/dev/null || true

    # Should be able to acquire again
    if $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs 1 --timeout 2 exclude_release >/dev/null 2>&1; then
        test_pass "ExcludeCPUs slot released and reacquired"
    else
        test_fail "ExcludeCPUs slot should be released and reacquired"
    fi
else
    test_pass "ExcludeCPUs release test skipped (insufficient CPUs)"
fi

# Test excludeCPUs with timeout
test_start "ExcludeCPUs with timeout"
if [ $CPU_COUNT -gt 1 ]; then
    EXCLUDE_TIMEOUT=1
    EXPECTED_TIMEOUT=$((CPU_COUNT - EXCLUDE_TIMEOUT))
    
    # Fill all slots
    EXCLUDE_TIMEOUT_PIDS=()
    for i in $(seq 1 $EXPECTED_TIMEOUT); do
        $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs $EXCLUDE_TIMEOUT exclude_timeout >/dev/null 2>&1 &
        EXCLUDE_TIMEOUT_PIDS+=($!)
    done

    sleep 1

    # Should timeout
    START_TIME=$(date +%s)
    if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs $EXCLUDE_TIMEOUT --timeout 2 exclude_timeout >/dev/null 2>&1; then
        END_TIME=$(date +%s)
        DURATION=$((END_TIME - START_TIME))
        
        if [ $DURATION -ge 2 ] && [ $DURATION -le 4 ]; then
            test_pass "ExcludeCPUs respects timeout"
        else
            test_fail "ExcludeCPUs timeout duration incorrect: ${DURATION}s"
        fi
    else
        test_fail "ExcludeCPUs should timeout when all slots full"
    fi

    # Clean up
    for pid in "${EXCLUDE_TIMEOUT_PIDS[@]}"; do
        kill $pid 2>/dev/null || true
    done
else
    test_pass "ExcludeCPUs timeout test skipped (insufficient CPUs)"
fi

# Test excludeCPUs with exec
test_start "ExcludeCPUs with exec"
if [ $CPU_COUNT -gt 1 ]; then
    $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs 1 --exec sleep 2 exclude_exec >/dev/null 2>&1 &
    EXCLUDE_EXEC_PID=$!

    sleep 1

    # Should be holding lock
    if wait_for_lock "exclude_exec"; then
        # Wait for completion
        wait $EXCLUDE_EXEC_PID 2>/dev/null || true
        
        # Should release lock
        if wait_for_unlock "exclude_exec"; then
            test_pass "ExcludeCPUs works with exec"
        else
            test_fail "ExcludeCPUs should work with exec"
        fi
    else
        test_fail "ExcludeCPUs exec should hold lock"
    fi
else
    test_pass "ExcludeCPUs exec test skipped (insufficient CPUs)"
fi

# Test excludeCPUs consistency
test_start "ExcludeCPUs consistency"
if [ $CPU_COUNT -gt 1 ]; then
    $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs 1 exclude_consistent >/dev/null 2>&1 &
    EXCLUDE_CONSISTENT_PID=$!

    sleep 1

    # Try to acquire with different exclude count
    if ! $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs 2 --timeout 1 exclude_consistent >/dev/null 2>&1; then
        test_pass "ExcludeCPUs detects inconsistent exclude counts"
    else
        test_fail "ExcludeCPUs should detect inconsistent exclude counts"
    fi

    kill $EXCLUDE_CONSISTENT_PID 2>/dev/null || true
else
    test_pass "ExcludeCPUs consistency test skipped (insufficient CPUs)"
fi

# Test excludeCPUs with signal handling
test_start "ExcludeCPUs with signal handling"
if [ $CPU_COUNT -gt 1 ]; then
    $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs 1 exclude_signal >/dev/null 2>&1 &
    EXCLUDE_SIGNAL_PID=$!

    sleep 1

    # Send signal
    kill -TERM $EXCLUDE_SIGNAL_PID 2>/dev/null || true

    # Should release slot
    if wait_for_unlock "exclude_signal"; then
        test_pass "ExcludeCPUs handles signals properly"
    else
        test_fail "ExcludeCPUs should handle signals and release slot"
    fi
else
    test_pass "ExcludeCPUs signal test skipped (insufficient CPUs)"
fi

test_suite_end