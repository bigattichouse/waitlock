#!/bin/bash
# Test environment variable functionality

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "Environment Variables"

# Test WAITLOCK_LOCK_DIR environment variable
test_start "WAITLOCK_LOCK_DIR environment variable"
export WAITLOCK_LOCK_DIR="$LOCK_DIR"
$WAITLOCK env_lock_dir >/dev/null 2>&1 &
ENV_LOCK_DIR_PID=$!

sleep 1

if wait_for_lock "env_lock_dir"; then
    test_pass "WAITLOCK_LOCK_DIR environment variable works"
else
    test_fail "WAITLOCK_LOCK_DIR environment variable should work"
fi

kill $ENV_LOCK_DIR_PID 2>/dev/null || true
unset WAITLOCK_LOCK_DIR

# Test WAITLOCK_TIMEOUT environment variable
test_start "WAITLOCK_TIMEOUT environment variable"
$WAITLOCK --lock-dir "$LOCK_DIR" env_timeout >/dev/null 2>&1 &
ENV_TIMEOUT_PID=$!

sleep 1

export WAITLOCK_TIMEOUT=2
START_TIME=$(date +%s)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" env_timeout >/dev/null 2>&1; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    if [ $DURATION -ge 2 ] && [ $DURATION -le 4 ]; then
        test_pass "WAITLOCK_TIMEOUT environment variable works"
    else
        test_fail "WAITLOCK_TIMEOUT timeout duration incorrect: ${DURATION}s"
    fi
else
    test_fail "WAITLOCK_TIMEOUT should cause timeout"
fi

kill $ENV_TIMEOUT_PID 2>/dev/null || true
unset WAITLOCK_TIMEOUT

# Test WAITLOCK_ALLOW_MULTIPLE environment variable
test_start "WAITLOCK_ALLOW_MULTIPLE environment variable"
export WAITLOCK_ALLOW_MULTIPLE=3
$WAITLOCK --lock-dir "$LOCK_DIR" env_multiple >/dev/null 2>&1 &
ENV_MULTI_PID1=$!
$WAITLOCK --lock-dir "$LOCK_DIR" env_multiple >/dev/null 2>&1 &
ENV_MULTI_PID2=$!
$WAITLOCK --lock-dir "$LOCK_DIR" env_multiple >/dev/null 2>&1 &
ENV_MULTI_PID3=$!

sleep 1

# All should be running
MULTI_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "env_multiple" || echo 0)
if [ "$MULTI_COUNT" -eq 3 ]; then
    test_pass "WAITLOCK_ALLOW_MULTIPLE environment variable works"
else
    test_fail "WAITLOCK_ALLOW_MULTIPLE should allow 3 processes, got $MULTI_COUNT"
fi

kill $ENV_MULTI_PID1 $ENV_MULTI_PID2 $ENV_MULTI_PID3 2>/dev/null || true
unset WAITLOCK_ALLOW_MULTIPLE

# Test WAITLOCK_ONE_PER_CPU environment variable
test_start "WAITLOCK_ONE_PER_CPU environment variable"
CPU_COUNT=$(nproc)
export WAITLOCK_ONE_PER_CPU=1

# Start processes up to CPU count
ENV_ONECPU_PIDS=()
for i in $(seq 1 $CPU_COUNT); do
    $WAITLOCK --lock-dir "$LOCK_DIR" env_onecpu >/dev/null 2>&1 &
    ENV_ONECPU_PIDS+=($!)
done

sleep 2

# All should be running
ONECPU_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "env_onecpu" || echo 0)
if [ "$ONECPU_COUNT" -eq "$CPU_COUNT" ]; then
    test_pass "WAITLOCK_ONE_PER_CPU environment variable works"
else
    test_fail "WAITLOCK_ONE_PER_CPU should allow $CPU_COUNT processes, got $ONECPU_COUNT"
fi

# Clean up
for pid in "${ENV_ONECPU_PIDS[@]}"; do
    kill $pid 2>/dev/null || true
done
unset WAITLOCK_ONE_PER_CPU

# Test WAITLOCK_CPUS environment variable
test_start "WAITLOCK_CPUS environment variable"
export WAITLOCK_CPUS=2
export WAITLOCK_ONE_PER_CPU=1

ENV_CPUS_PIDS=()
for i in $(seq 1 2); do
    $WAITLOCK --lock-dir "$LOCK_DIR" env_cpus >/dev/null 2>&1 &
    ENV_CPUS_PIDS+=($!)
done

sleep 1

# Should allow exactly 2
CPUS_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "env_cpus" || echo 0)
if [ "$CPUS_COUNT" -eq 2 ]; then
    test_pass "WAITLOCK_CPUS environment variable works"
else
    test_fail "WAITLOCK_CPUS should allow 2 processes, got $CPUS_COUNT"
fi

# Clean up
for pid in "${ENV_CPUS_PIDS[@]}"; do
    kill $pid 2>/dev/null || true
done
unset WAITLOCK_CPUS
unset WAITLOCK_ONE_PER_CPU

# Test WAITLOCK_EXCLUDE_CPUS environment variable
test_start "WAITLOCK_EXCLUDE_CPUS environment variable"
if [ $CPU_COUNT -gt 1 ]; then
    EXCLUDE_COUNT=1
    EXPECTED_COUNT=$((CPU_COUNT - EXCLUDE_COUNT))
    
    export WAITLOCK_ONE_PER_CPU=1
    export WAITLOCK_EXCLUDE_CPUS=$EXCLUDE_COUNT
    
    ENV_EXCLUDE_PIDS=()
    for i in $(seq 1 $EXPECTED_COUNT); do
        $WAITLOCK --lock-dir "$LOCK_DIR" env_exclude >/dev/null 2>&1 &
        ENV_EXCLUDE_PIDS+=($!)
    done

    sleep 1

    # Should allow expected count
    EXCLUDE_RUNNING=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "env_exclude" || echo 0)
    if [ "$EXCLUDE_RUNNING" -eq "$EXPECTED_COUNT" ]; then
        test_pass "WAITLOCK_EXCLUDE_CPUS environment variable works"
    else
        test_fail "WAITLOCK_EXCLUDE_CPUS should allow $EXPECTED_COUNT processes, got $EXCLUDE_RUNNING"
    fi

    # Clean up
    for pid in "${ENV_EXCLUDE_PIDS[@]}"; do
        kill $pid 2>/dev/null || true
    done
    unset WAITLOCK_ONE_PER_CPU
    unset WAITLOCK_EXCLUDE_CPUS
else
    test_pass "WAITLOCK_EXCLUDE_CPUS test skipped (insufficient CPUs)"
fi

# Test command line overrides environment
test_start "Command line overrides environment"
export WAITLOCK_TIMEOUT=5
$WAITLOCK --lock-dir "$LOCK_DIR" override_test >/dev/null 2>&1 &
OVERRIDE_PID=$!

sleep 1

# Command line timeout should override environment
START_TIME=$(date +%s)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 override_test >/dev/null 2>&1; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    if [ $DURATION -ge 1 ] && [ $DURATION -le 3 ]; then
        test_pass "Command line overrides environment timeout"
    else
        test_fail "Command line timeout should override environment"
    fi
else
    test_fail "Command line timeout should override environment"
fi

kill $OVERRIDE_PID 2>/dev/null || true
unset WAITLOCK_TIMEOUT

# Test environment variable validation
test_start "Environment variable validation"
# Test invalid timeout
export WAITLOCK_TIMEOUT="invalid"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" env_invalid_timeout >/dev/null 2>&1; then
    test_pass "Invalid WAITLOCK_TIMEOUT rejected"
else
    test_fail "Invalid WAITLOCK_TIMEOUT should be rejected"
fi
unset WAITLOCK_TIMEOUT

# Test invalid allow multiple
export WAITLOCK_ALLOW_MULTIPLE="invalid"
if ! $WAITLOCK --lock-dir "$LOCK_DIR" env_invalid_multiple >/dev/null 2>&1; then
    test_pass "Invalid WAITLOCK_ALLOW_MULTIPLE rejected"
else
    test_fail "Invalid WAITLOCK_ALLOW_MULTIPLE should be rejected"
fi
unset WAITLOCK_ALLOW_MULTIPLE

# Test environment variable precedence
test_start "Environment variable precedence"
export WAITLOCK_LOCK_DIR="$LOCK_DIR"
export WAITLOCK_ALLOW_MULTIPLE=2

# Should use both environment variables
$WAITLOCK env_precedence >/dev/null 2>&1 &
ENV_PREC_PID1=$!
$WAITLOCK env_precedence >/dev/null 2>&1 &
ENV_PREC_PID2=$!

sleep 1

PREC_COUNT=$($WAITLOCK --list 2>/dev/null | grep -c "env_precedence" || echo 0)
if [ "$PREC_COUNT" -eq 2 ]; then
    test_pass "Environment variable precedence works"
else
    test_fail "Environment variable precedence should work"
fi

kill $ENV_PREC_PID1 $ENV_PREC_PID2 2>/dev/null || true
unset WAITLOCK_LOCK_DIR
unset WAITLOCK_ALLOW_MULTIPLE

# Test environment with exec
test_start "Environment with exec"
export WAITLOCK_LOCK_DIR="$LOCK_DIR"
if $WAITLOCK --exec echo "test" env_exec >/dev/null 2>&1; then
    test_pass "Environment variables work with exec"
else
    test_fail "Environment variables should work with exec"
fi
unset WAITLOCK_LOCK_DIR

# Test environment variable case sensitivity
test_start "Environment variable case sensitivity"
export waitlock_lock_dir="$LOCK_DIR"  # lowercase
if ! $WAITLOCK env_case_test >/dev/null 2>&1; then
    test_pass "Environment variables are case sensitive"
else
    test_fail "Environment variables should be case sensitive"
fi
unset waitlock_lock_dir

# Test empty environment variables
test_start "Empty environment variables"
export WAITLOCK_LOCK_DIR=""
if ! $WAITLOCK env_empty >/dev/null 2>&1; then
    test_pass "Empty WAITLOCK_LOCK_DIR rejected"
else
    test_fail "Empty WAITLOCK_LOCK_DIR should be rejected"
fi
unset WAITLOCK_LOCK_DIR

# Test environment variable with special characters
test_start "Environment with special characters"
SPECIAL_LOCK_DIR="$LOCK_DIR/test with spaces"
mkdir -p "$SPECIAL_LOCK_DIR"
export WAITLOCK_LOCK_DIR="$SPECIAL_LOCK_DIR"

$WAITLOCK env_special >/dev/null 2>&1 &
ENV_SPECIAL_PID=$!

sleep 1

if wait_for_lock "env_special"; then
    test_pass "Environment handles special characters in paths"
else
    test_fail "Environment should handle special characters in paths"
fi

kill $ENV_SPECIAL_PID 2>/dev/null || true
unset WAITLOCK_LOCK_DIR
rm -rf "$SPECIAL_LOCK_DIR"

test_suite_end