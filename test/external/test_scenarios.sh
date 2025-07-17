#!/bin/bash
# Test real-world scenarios based on README examples

set -e
source "$(dirname "$0")/test_framework.sh"

test_suite_start "Real-world Scenarios"

# Test basic exclusive access scenario
test_start "Basic exclusive access scenario"
# Start a long-running process
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sleep 3 build_process >/dev/null 2>&1 &
BUILD_PID=$!

sleep 1

# Try to run another build (should wait)
$WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 build_process >/dev/null 2>&1 &
WAIT_BUILD_PID=$!

# First should succeed, second should timeout
if wait $BUILD_PID 2>/dev/null; then
    if ! wait $WAIT_BUILD_PID 2>/dev/null; then
        test_pass "Basic exclusive access works"
    else
        test_fail "Second process should have timed out"
    fi
else
    test_fail "First process should have succeeded"
fi

# Test parallel build with CPU limits
test_start "Parallel build with CPU limits"
CPU_COUNT=$(nproc)

# Start multiple builds limited by CPU count
PARALLEL_PIDS=()
for i in $(seq 1 $CPU_COUNT); do
    $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --exec sleep 2 parallel_build >/dev/null 2>&1 &
    PARALLEL_PIDS+=($!)
done

sleep 1

# All should be running
PARALLEL_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "parallel_build" || echo 0)
if [ "$PARALLEL_COUNT" -eq "$CPU_COUNT" ]; then
    test_pass "Parallel build respects CPU limits"
else
    test_fail "Parallel build should respect CPU limits"
fi

# Wait for completion
for pid in "${PARALLEL_PIDS[@]}"; do
    wait $pid 2>/dev/null || true
done

# Test database migration scenario
test_start "Database migration scenario"
# Start migration
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sleep 2 db_migration >/dev/null 2>&1 &
MIGRATION_PID=$!

sleep 1

# Check if migration is running
if $WAITLOCK --lock-dir "$LOCK_DIR" --check db_migration >/dev/null 2>&1; then
    test_fail "Migration should be running (check should fail)"
else
    test_pass "Migration is running (check correctly failed)"
fi

# Wait for completion
wait $MIGRATION_PID 2>/dev/null || true

# Now check should succeed
if $WAITLOCK --lock-dir "$LOCK_DIR" --check db_migration >/dev/null 2>&1; then
    test_pass "Migration completed (check now succeeds)"
else
    test_fail "Migration should be completed"
fi

# Test backup scenario with timeout
test_start "Backup scenario with timeout"
# Start backup process
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sleep 5 backup_process >/dev/null 2>&1 &
BACKUP_PID=$!

sleep 1

# Try to start another backup with timeout
START_TIME=$(date +%s)
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 2 backup_process >/dev/null 2>&1; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    if [ $DURATION -ge 2 ] && [ $DURATION -le 4 ]; then
        test_pass "Backup timeout scenario works"
    else
        test_fail "Backup timeout duration incorrect: ${DURATION}s"
    fi
else
    test_fail "Second backup should have timed out"
fi

kill $BACKUP_PID 2>/dev/null || true

# Test resource pool scenario
test_start "Resource pool scenario"
POOL_SIZE=3
RESOURCE_PIDS=()

# Start processes using resource pool
for i in $(seq 1 $POOL_SIZE); do
    $WAITLOCK --lock-dir "$LOCK_DIR" -m $POOL_SIZE --exec sleep 2 resource_pool >/dev/null 2>&1 &
    RESOURCE_PIDS+=($!)
done

sleep 1

# All should be running
POOL_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "resource_pool" || echo 0)
if [ "$POOL_COUNT" -eq "$POOL_SIZE" ]; then
    test_pass "Resource pool allows $POOL_SIZE concurrent processes"
else
    test_fail "Resource pool should allow $POOL_SIZE processes, got $POOL_COUNT"
fi

# Try to exceed pool
if ! $WAITLOCK --lock-dir "$LOCK_DIR" -m $POOL_SIZE --timeout 1 resource_pool >/dev/null 2>&1; then
    test_pass "Resource pool properly limits access"
else
    test_fail "Resource pool should limit access"
fi

# Wait for completion
for pid in "${RESOURCE_PIDS[@]}"; do
    wait $pid 2>/dev/null || true
done

# Test service restart scenario
test_start "Service restart scenario"
# Start service
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sleep 3 service_restart >/dev/null 2>&1 &
SERVICE_PID=$!

sleep 1

# Try to restart (should wait)
$WAITLOCK --lock-dir "$LOCK_DIR" --timeout 5 service_restart >/dev/null 2>&1 &
RESTART_PID=$!

sleep 1

# Stop original service
kill $SERVICE_PID 2>/dev/null || true

# Restart should now proceed
if wait $RESTART_PID 2>/dev/null; then
    test_pass "Service restart scenario works"
else
    test_fail "Service restart should have succeeded"
fi

# Test deployment pipeline scenario
test_start "Deployment pipeline scenario"
# Start deployment
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sleep 2 deployment >/dev/null 2>&1 &
DEPLOY_PID=$!

sleep 1

# Check deployment status
if ! $WAITLOCK --lock-dir "$LOCK_DIR" --check deployment >/dev/null 2>&1; then
    # Try to list active deployments
    DEPLOY_LIST=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep deployment || echo "")
    if [ -n "$DEPLOY_LIST" ]; then
        test_pass "Deployment pipeline scenario works"
    else
        test_fail "Deployment should be listed"
    fi
else
    test_fail "Deployment should be running"
fi

wait $DEPLOY_PID 2>/dev/null || true

# Test maintenance window scenario
test_start "Maintenance window scenario"
# Start maintenance
$WAITLOCK --lock-dir "$LOCK_DIR" --exec sleep 2 maintenance >/dev/null 2>&1 &
MAINT_PID=$!

sleep 1

# Multiple services should wait
SERVICE_PIDS=()
for i in {1..3}; do
    $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 4 maintenance >/dev/null 2>&1 &
    SERVICE_PIDS+=($!)
done

# Wait for maintenance to complete
wait $MAINT_PID 2>/dev/null || true

# Services should now be able to proceed
SUCCESS_COUNT=0
for pid in "${SERVICE_PIDS[@]}"; do
    if wait $pid 2>/dev/null; then
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    fi
done

if [ $SUCCESS_COUNT -eq 3 ]; then
    test_pass "Maintenance window scenario works"
else
    test_fail "All services should proceed after maintenance"
fi

# Test batch processing scenario
test_start "Batch processing scenario"
BATCH_SIZE=2
BATCH_PIDS=()

# Start batch jobs
for i in $(seq 1 $BATCH_SIZE); do
    $WAITLOCK --lock-dir "$LOCK_DIR" -m $BATCH_SIZE --exec sleep 1 batch_job >/dev/null 2>&1 &
    BATCH_PIDS+=($!)
done

sleep 0.5

# Should allow batch size
BATCH_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "batch_job" || echo 0)
if [ "$BATCH_COUNT" -eq "$BATCH_SIZE" ]; then
    test_pass "Batch processing allows $BATCH_SIZE concurrent jobs"
else
    test_fail "Batch processing should allow $BATCH_SIZE jobs, got $BATCH_COUNT"
fi

# Wait for completion
for pid in "${BATCH_PIDS[@]}"; do
    wait $pid 2>/dev/null || true
done

# Test critical section scenario
test_start "Critical section scenario"
# Start critical operation
$WAITLOCK --lock-dir "$LOCK_DIR" critical_section >/dev/null 2>&1 &
CRITICAL_PID=$!

sleep 1

# Other operations should wait
$WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 critical_section >/dev/null 2>&1 &
WAIT_CRITICAL_PID=$!

# First should hold lock, second should timeout
if ! wait $WAIT_CRITICAL_PID 2>/dev/null; then
    test_pass "Critical section properly serializes access"
else
    test_fail "Critical section should serialize access"
fi

kill $CRITICAL_PID 2>/dev/null || true

# Test emergency shutdown scenario
test_start "Emergency shutdown scenario"
# Start multiple processes
EMERGENCY_PIDS=()
for i in {1..3}; do
    $WAITLOCK --lock-dir "$LOCK_DIR" -m 3 running_service >/dev/null 2>&1 &
    EMERGENCY_PIDS+=($!)
done

sleep 1

# Send shutdown signal to all
$WAITLOCK --lock-dir "$LOCK_DIR" --done running_service >/dev/null 2>&1

# All should terminate
if wait_for_unlock "running_service"; then
    test_pass "Emergency shutdown scenario works"
else
    test_fail "Emergency shutdown should terminate all processes"
fi

# Clean up any remaining processes
for pid in "${EMERGENCY_PIDS[@]}"; do
    kill $pid 2>/dev/null || true
done

# Test load balancing scenario
test_start "Load balancing scenario"
if [ $CPU_COUNT -gt 1 ]; then
    WORKERS=$((CPU_COUNT - 1))
    WORKER_PIDS=()
    
    # Start workers with CPU exclusion
    for i in $(seq 1 $WORKERS); do
        $WAITLOCK --lock-dir "$LOCK_DIR" --onePerCPU --excludeCPUs 1 --exec sleep 2 worker >/dev/null 2>&1 &
        WORKER_PIDS+=($!)
    done

    sleep 1

    # Should have correct number of workers
    WORKER_COUNT=$($WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -c "worker" || echo 0)
    if [ "$WORKER_COUNT" -eq "$WORKERS" ]; then
        test_pass "Load balancing scenario works"
    else
        test_fail "Load balancing should start $WORKERS workers, got $WORKER_COUNT"
    fi

    # Wait for completion
    for pid in "${WORKER_PIDS[@]}"; do
        wait $pid 2>/dev/null || true
    done
else
    test_pass "Load balancing test skipped (insufficient CPUs)"
fi

# Test cleanup scenario
test_start "Cleanup scenario"
# Start process that will be killed
$WAITLOCK --lock-dir "$LOCK_DIR" cleanup_test >/dev/null 2>&1 &
CLEANUP_PID=$!

sleep 1

# Kill process abruptly
kill -9 $CLEANUP_PID 2>/dev/null || true

# Give time for cleanup
sleep 2

# Should be able to acquire lock again
if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout 1 cleanup_test >/dev/null 2>&1; then
    test_pass "Cleanup scenario works"
else
    test_fail "Cleanup should allow lock reacquisition"
fi

test_suite_end