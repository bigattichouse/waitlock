#!/bin/bash
# Test framework for external waitlock tests

# Configuration
WAITLOCK="../../build/bin/waitlock"
TEST_DIR="${TEST_DIR:-/tmp/waitlock_external_test_$$}"
LOCK_DIR="$TEST_DIR/locks"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test counters (global for all test files)
if [ -z "$GLOBAL_TEST_COUNT" ]; then
    export GLOBAL_TEST_COUNT=0
    export GLOBAL_PASS_COUNT=0
    export GLOBAL_FAIL_COUNT=0
fi

# Per-suite counters
TEST_COUNT=0
PASS_COUNT=0
FAIL_COUNT=0

# Setup function - call at start of each test suite
test_suite_start() {
    local suite_name="$1"
    echo -e "${CYAN}=== $suite_name ===${NC}"
    
    # Create test directory if it doesn't exist
    mkdir -p "$LOCK_DIR"
    chmod 755 "$LOCK_DIR"
    
    # Reset per-suite counters
    TEST_COUNT=0
    PASS_COUNT=0
    FAIL_COUNT=0
}

# Cleanup function - call at end of each test suite
test_suite_end() {
    # Update global counters
    export GLOBAL_TEST_COUNT=$((GLOBAL_TEST_COUNT + TEST_COUNT))
    export GLOBAL_PASS_COUNT=$((GLOBAL_PASS_COUNT + PASS_COUNT))
    export GLOBAL_FAIL_COUNT=$((GLOBAL_FAIL_COUNT + FAIL_COUNT))
    
    # Suite summary
    echo -e "${YELLOW}Suite: $TEST_COUNT tests, $PASS_COUNT passed, $FAIL_COUNT failed${NC}"
    
    # Kill any remaining processes from this suite
    pkill -f "$WAITLOCK" 2>/dev/null || true
}

# Test helper functions
test_start() {
    TEST_COUNT=$((TEST_COUNT + 1))
    echo -e "${BLUE}  Test $TEST_COUNT: $1${NC}"
}

test_pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    echo -e "${GREEN}    ✓ $1${NC}"
}

test_fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo -e "${RED}    ✗ $1${NC}"
}

# Wait for process to appear in lock list
wait_for_lock() {
    local descriptor="$1"
    local timeout="${2:-5}"
    local count=0
    
    while [ $count -lt $timeout ]; do
        if $WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -q "$descriptor"; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    return 1
}

# Wait for process to disappear from lock list
wait_for_unlock() {
    local descriptor="$1"
    local timeout="${2:-5}"
    local count=0
    
    while [ $count -lt $timeout ]; do
        if ! $WAITLOCK --lock-dir "$LOCK_DIR" --list 2>/dev/null | grep -q "$descriptor"; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    return 1
}

# Check if binary exists and is executable
check_binary() {
    if [ ! -x "$WAITLOCK" ]; then
        echo -e "${RED}ERROR: waitlock binary not found: $WAITLOCK${NC}"
        echo "Please run 'make' to build the binary first."
        exit 1
    fi
}

# Final cleanup - call once at the very end
final_cleanup() {
    # Kill any remaining waitlock processes
    pkill -f "$WAITLOCK" 2>/dev/null || true
    
    # Clean up test directory
    rm -rf "$TEST_DIR" 2>/dev/null || true
    
    # Clean up environment variables
    unset WAITLOCK_DEBUG WAITLOCK_TIMEOUT WAITLOCK_DIR WAITLOCK_SLOT
}

# Ensure we can run the tests
check_binary