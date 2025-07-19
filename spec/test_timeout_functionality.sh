#!/bin/bash
# Test script to validate waitlock timeout functionality

set -e

WAITLOCK="./src/waitlock"
LOCK_DIR="/var/lock/waitlock"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}=== WaitLock Timeout Functionality Test ===${NC}"

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    pkill -f "waitlock.*test_timeout" 2>/dev/null || true
    rm -f "$LOCK_DIR"/test_timeout*.lock 2>/dev/null || true
    sleep 1
}

# Set cleanup trap
trap cleanup EXIT

# Test helper functions
test_start() {
    echo -e "\n${BLUE}Test: $1${NC}"
}

test_pass() {
    echo -e "${GREEN}✓ PASS: $1${NC}"
}

test_fail() {
    echo -e "${RED}✗ FAIL: $1${NC}"
}

# Test 1: Timeout with no lock conflict (should succeed immediately)
test_start "Timeout with no lock conflict"
cleanup

# Start waitlock with timeout when no conflicting lock exists
timeout 10 $WAITLOCK --timeout 5.0 test_timeout_no_conflict &
LOCK_PID=$!
sleep 2

# Check if lock was acquired
if $WAITLOCK --check test_timeout_no_conflict; then
    test_fail "Lock should have been acquired but --check says it's available"
    RESULT1="FAIL"
else
    test_pass "Lock was acquired successfully with timeout flag"
    RESULT1="PASS"
fi

# Cleanup this test
kill $LOCK_PID 2>/dev/null || true
sleep 1

# Test 2: Timeout with lock conflict (should timeout)
test_start "Timeout with lock conflict"
cleanup

# First, acquire the lock without timeout
$WAITLOCK test_timeout_conflict &
HOLDER_PID=$!
sleep 2

# Verify the lock is held
if $WAITLOCK --check test_timeout_conflict; then
    test_fail "Initial lock was not acquired properly"
    kill $HOLDER_PID 2>/dev/null || true
    exit 1
else
    test_pass "Initial lock acquired successfully"
fi

# Now try to acquire the same lock with a short timeout
start_time=$(date +%s)
timeout 10 $WAITLOCK --timeout 1.0 test_timeout_conflict 2>/dev/null
timeout_exit_code=$?
end_time=$(date +%s)
elapsed=$((end_time - start_time))

if [ $timeout_exit_code -eq 2 ] && [ $elapsed -ge 1 ] && [ $elapsed -le 3 ]; then
    test_pass "Timeout occurred correctly after ~$elapsed seconds"
    RESULT2="PASS"
else
    test_fail "Expected timeout after ~1 second, got exit code $timeout_exit_code after $elapsed seconds"
    RESULT2="FAIL"
fi

# Cleanup
kill $HOLDER_PID 2>/dev/null || true
sleep 1

# Test 3: Zero timeout (should fail immediately if conflicted)
test_start "Zero timeout with conflict"
cleanup

# Acquire lock
$WAITLOCK test_timeout_zero &
HOLDER_PID=$!
sleep 2

# Try zero timeout
start_time=$(date +%s)
timeout 5 $WAITLOCK --timeout 0 test_timeout_zero 2>/dev/null
zero_exit_code=$?
end_time=$(date +%s)
zero_elapsed=$((end_time - start_time))

if [ $zero_exit_code -eq 2 ] && [ $zero_elapsed -eq 0 ]; then
    test_pass "Zero timeout failed immediately as expected"
    RESULT3="PASS"
else
    test_fail "Zero timeout should fail immediately, got exit code $zero_exit_code after $zero_elapsed seconds"
    RESULT3="FAIL"
fi

# Cleanup
kill $HOLDER_PID 2>/dev/null || true
sleep 1

# Test 4: Timeout without conflict on fresh descriptor
test_start "Timeout on fresh descriptor"
cleanup

# Use a completely new descriptor that definitely doesn't exist
FRESH_DESC="test_timeout_fresh_$(date +%s)"

start_time=$(date +%s)
timeout 10 $WAITLOCK --timeout 3.0 "$FRESH_DESC" &
FRESH_PID=$!
sleep 2

# Should have acquired immediately
if $WAITLOCK --check "$FRESH_DESC"; then
    test_fail "Fresh descriptor should have been acquired"
    RESULT4="FAIL"
else
    test_pass "Fresh descriptor acquired successfully with timeout"
    RESULT4="PASS"
fi

kill $FRESH_PID 2>/dev/null || true

# Final summary
echo -e "\n${BLUE}=== TEST SUMMARY ===${NC}"
echo -e "Test 1 (No conflict):     $RESULT1"
echo -e "Test 2 (With conflict):   $RESULT2"  
echo -e "Test 3 (Zero timeout):    $RESULT3"
echo -e "Test 4 (Fresh descriptor): $RESULT4"

if [ "$RESULT1" = "PASS" ] && [ "$RESULT2" = "PASS" ] && [ "$RESULT3" = "PASS" ] && [ "$RESULT4" = "PASS" ]; then
    echo -e "\n${GREEN}🎉 ALL TIMEOUT TESTS PASSED! 🎉${NC}"
    exit 0
else
    echo -e "\n${RED}❌ Some timeout tests failed${NC}"
    exit 1
fi