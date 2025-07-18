#!/bin/bash
# Test script to verify the no-contention hanging fix
# This test ensures that the fix for the no-contention hanging issue continues to work

set -e

# Get script directory and set up paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
WAITLOCK="$PROJECT_DIR/build/bin/waitlock"
TEST_DIR="/tmp/waitlock_no_contention_test_$$"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Helper functions
setup_test_env() {
    mkdir -p "$TEST_DIR"
    echo "Test environment created: $TEST_DIR"
}

cleanup_test_env() {
    rm -rf "$TEST_DIR"
    echo "Test environment cleaned up"
}

run_test() {
    local test_name="$1"
    local test_cmd="$2"
    local expected_exit_code="$3"
    local max_time="$4"
    
    TESTS_RUN=$((TESTS_RUN + 1))
    
    echo -e "\n${YELLOW}[TEST $TESTS_RUN] $test_name${NC}"
    echo "Command: $test_cmd"
    echo "Expected exit code: $expected_exit_code"
    echo "Max time: ${max_time}s"
    
    # Run the command with timeout
    start_time=$(date +%s.%N)
    
    if timeout "$max_time" bash -c "$test_cmd" >/dev/null 2>&1; then
        actual_exit_code=0
    else
        actual_exit_code=$?
        # timeout command returns 124 when it kills the process
        if [ $actual_exit_code -eq 124 ]; then
            echo -e "${RED}TIMEOUT: Command took longer than ${max_time}s${NC}"
            TESTS_FAILED=$((TESTS_FAILED + 1))
            return 1
        fi
    fi
    
    end_time=$(date +%s.%N)
    duration=$(echo "$end_time - $start_time" | bc -l)
    
    echo "Actual exit code: $actual_exit_code"
    echo "Duration: ${duration}s"
    
    if [ "$actual_exit_code" -eq "$expected_exit_code" ]; then
        echo -e "${GREEN}PASS${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}FAIL: Expected exit code $expected_exit_code, got $actual_exit_code${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Test 1: Basic no-contention scenario should acquire lock quickly
test_basic_no_contention() {
    local test_name="Basic no-contention lock acquisition"
    # Start in background and check if it starts quickly
    $WAITLOCK --lock-dir "$TEST_DIR" --timeout 0.1 test_basic &
    local bg_pid=$!
    sleep 0.2  # Give it time to acquire the lock
    
    # Check if the process is still running (it should be, holding the lock)
    if kill -0 $bg_pid 2>/dev/null; then
        echo -e "${GREEN}PASS: Lock acquired and process is running${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        kill $bg_pid 2>/dev/null || true
        wait $bg_pid 2>/dev/null || true
    else
        echo -e "${RED}FAIL: Process exited unexpectedly${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    TESTS_RUN=$((TESTS_RUN + 1))
}

# Test 2: Multiple concurrent no-contention scenarios
test_multiple_no_contention() {
    local test_name="Multiple concurrent no-contention locks"
    local cmd="$WAITLOCK --lock-dir '$TEST_DIR' --timeout 0.1 test_multi_1 & $WAITLOCK --lock-dir '$TEST_DIR' --timeout 0.1 test_multi_2 & $WAITLOCK --lock-dir '$TEST_DIR' --timeout 0.1 test_multi_3 & wait"
    run_test "$test_name" "$cmd" 0 1.0
}

# Test 3: Semaphore with available slots should work immediately
test_semaphore_available_slots() {
    local test_name="Semaphore with available slots"
    local cmd="$WAITLOCK --lock-dir '$TEST_DIR' --timeout 0.1 -m 3 test_semaphore"
    run_test "$test_name" "$cmd" 0 0.5
}

# Test 4: Timeout still works with contention
test_timeout_with_contention() {
    local test_name="Timeout still works with contention"
    # Start a background process that holds the lock
    $WAITLOCK --lock-dir "$TEST_DIR" --timeout 10 test_contention &
    local bg_pid=$!
    sleep 0.2  # Give it time to acquire the lock
    
    # Now try to acquire the same lock with a short timeout
    local cmd="$WAITLOCK --lock-dir '$TEST_DIR' --timeout 0.1 test_contention"
    run_test "$test_name" "$cmd" 2 0.5  # Should timeout (exit code 2)
    
    # Clean up background process
    kill $bg_pid 2>/dev/null || true
    wait $bg_pid 2>/dev/null || true
}

# Test 5: Very short timeout (edge case)
test_very_short_timeout() {
    local test_name="Very short timeout edge case"
    local cmd="$WAITLOCK --lock-dir '$TEST_DIR' --timeout 0.01 test_short_timeout"
    run_test "$test_name" "$cmd" 0 0.3
}

# Test 6: Zero timeout (should succeed immediately if no contention)
test_zero_timeout() {
    local test_name="Zero timeout no-contention"
    local cmd="$WAITLOCK --lock-dir '$TEST_DIR' --timeout 0 test_zero_timeout"
    run_test "$test_name" "$cmd" 0 0.2
}

# Test 7: Check command should work quickly
test_check_command_speed() {
    local test_name="Check command speed"
    local cmd="$WAITLOCK --lock-dir '$TEST_DIR' --check nonexistent_lock"
    run_test "$test_name" "$cmd" 0 0.2
}

# Test 8: List command should work quickly
test_list_command_speed() {
    local test_name="List command speed"
    local cmd="$WAITLOCK --lock-dir '$TEST_DIR' --list"
    run_test "$test_name" "$cmd" 0 0.2
}

# Test 9: Exec command should work without hanging
test_exec_command() {
    local test_name="Exec command no-contention"
    local cmd="$WAITLOCK --lock-dir '$TEST_DIR' --timeout 0.1 test_exec --exec echo 'test'"
    run_test "$test_name" "$cmd" 0 0.5
}

# Test 10: Complex scenario with cleanup
test_complex_scenario() {
    local test_name="Complex scenario with stale lock cleanup"
    
    # Create a fake stale lock file
    mkdir -p "$TEST_DIR"
    echo "fake lock file" > "$TEST_DIR/test_complex.slot0.fakehost.99999.lock"
    
    # This should clean up the stale lock and acquire successfully
    local cmd="$WAITLOCK --lock-dir '$TEST_DIR' --timeout 0.1 test_complex"
    run_test "$test_name" "$cmd" 0 0.5
}

# Main execution
main() {
    echo "=========================================="
    echo "  NO-CONTENTION HANGING FIX TEST SUITE"
    echo "=========================================="
    echo ""
    echo "This test suite verifies that the fix for the no-contention"
    echo "hanging issue continues to work correctly."
    echo ""
    
    # Check if waitlock binary exists
    if [ ! -f "$WAITLOCK" ]; then
        echo -e "${RED}ERROR: waitlock binary not found at $WAITLOCK${NC}"
        echo "Please run 'make' first to build the project."
        exit 1
    fi
    
    # Check if bc is available for time calculations
    if ! command -v bc >/dev/null 2>&1; then
        echo -e "${YELLOW}WARNING: bc not available, time calculations may be imprecise${NC}"
    fi
    
    setup_test_env
    
    # Run all tests
    echo -e "\n${YELLOW}Starting test execution...${NC}"
    
    test_basic_no_contention
    test_multiple_no_contention
    test_semaphore_available_slots
    test_timeout_with_contention
    test_very_short_timeout
    test_zero_timeout
    test_check_command_speed
    test_list_command_speed
    test_exec_command
    test_complex_scenario
    
    cleanup_test_env
    
    # Print summary
    echo ""
    echo "=========================================="
    echo "               TEST SUMMARY"
    echo "=========================================="
    echo "Tests run: $TESTS_RUN"
    echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "\n${GREEN}ALL TESTS PASSED!${NC}"
        echo "The no-contention hanging fix is working correctly."
        exit 0
    else
        echo -e "\n${RED}SOME TESTS FAILED!${NC}"
        echo "The no-contention hanging fix may have regressed."
        exit 1
    fi
}

# Run main function
main "$@"