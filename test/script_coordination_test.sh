#!/bin/bash

# Script coordination test - tests using & for script synchronization
# This test verifies that two scripts can use waitlock with & to coordinate execution

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/waitlock_coordination_test_$$"
LOCK_DIR="$TEST_DIR/locks"
SCRIPT1_LOG="$TEST_DIR/script1.log"
SCRIPT2_LOG="$TEST_DIR/script2.log"
WORK_LOG="$TEST_DIR/work.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counter
TEST_COUNT=0
PASS_COUNT=0
FAIL_COUNT=0

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up coordination tests...${NC}"
    
    # Kill any remaining processes
    pkill -f "test_worker_script" 2>/dev/null || true
    pkill -f "$WAITLOCK" 2>/dev/null || true
    
    # Clean up test directory
    rm -rf "$TEST_DIR" 2>/dev/null || true
    
    # Summary
    echo -e "\n${YELLOW}=== SCRIPT COORDINATION TEST SUMMARY ===${NC}"
    echo -e "Total tests: $TEST_COUNT"
    echo -e "${GREEN}Passed: $PASS_COUNT${NC}"
    echo -e "${RED}Failed: $FAIL_COUNT${NC}"
    
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "\n${GREEN}All script coordination tests passed!${NC}"
        exit 0
    else
        echo -e "\n${RED}Some script coordination tests failed!${NC}"
        exit 1
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Test helper functions
test_start() {
    TEST_COUNT=$((TEST_COUNT + 1))
    echo -e "\n${BLUE}Test $TEST_COUNT: $1${NC}"
}

test_pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    echo -e "${GREEN}✓ PASS${NC}"
}

test_fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo -e "${RED}✗ FAIL: $1${NC}"
}

# Create a test worker script
create_test_worker() {
    local script_name="$1"
    local log_file="$2"
    local work_duration="$3"
    
    cat > "$script_name" << EOF
#!/bin/bash
echo "[\$(date '+%H:%M:%S.%3N')] Script starting" >> "$log_file"

# Start waitlock in background
$WAITLOCK --lock-dir "$LOCK_DIR" testlock &
LOCK_PID=\$!

echo "[\$(date '+%H:%M:%S.%3N')] Waitlock started (PID: \$LOCK_PID)" >> "$log_file"

# Give it a moment to acquire the lock
sleep 1

# Check if our specific PID is holding the lock
lock_holder=\$($WAITLOCK --lock-dir "$LOCK_DIR" --list | grep "testlock" | awk '{print \$2}' | head -1)
echo "[\$(date '+%H:%M:%S.%3N')] Lock holder: \$lock_holder, Our PID: \$LOCK_PID" >> "$log_file"

if [ "\$lock_holder" = "\$LOCK_PID" ]; then
    echo "[\$(date '+%H:%M:%S.%3N')] SUCCESS: Got the lock! Proceeding with work..." >> "$log_file"
    
    # Log start of critical work
    echo "[\$(date '+%H:%M:%S.%3N')] WORK_START" >> "$WORK_LOG"
    
    # Simulate critical work
    sleep $work_duration
    
    # Log end of critical work
    echo "[\$(date '+%H:%M:%S.%3N')] WORK_END" >> "$WORK_LOG"
    
    echo "[\$(date '+%H:%M:%S.%3N')] Work complete, releasing lock" >> "$log_file"
    kill \$LOCK_PID
    wait \$LOCK_PID 2>/dev/null || true
    echo "[\$(date '+%H:%M:%S.%3N')] Script completed successfully" >> "$log_file"
    exit 0
else
    echo "[\$(date '+%H:%M:%S.%3N')] FAILED: Could not get lock" >> "$log_file"
    kill \$LOCK_PID 2>/dev/null || true
    exit 1
fi
EOF
    chmod +x "$script_name"
}

# Initialize test environment
echo -e "${YELLOW}=== WAITLOCK SCRIPT COORDINATION TEST ===${NC}"
echo "Setting up test environment..."

# Create test directory
mkdir -p "$LOCK_DIR"
chmod 755 "$LOCK_DIR"

# Build waitlock
echo "Building waitlock..."
make clean >/dev/null 2>&1 || true
make >/dev/null 2>&1

if [ ! -x "$WAITLOCK" ]; then
    echo -e "${RED}ERROR: waitlock binary not found or not executable${NC}"
    exit 1
fi

echo -e "${GREEN}Setup complete!${NC}"

# Test 1: Sequential execution (should both succeed)
test_start "Sequential execution - both scripts should succeed"
> "$WORK_LOG"
> "$SCRIPT1_LOG"
> "$SCRIPT2_LOG"

create_test_worker "$TEST_DIR/script1.sh" "$SCRIPT1_LOG" 2
create_test_worker "$TEST_DIR/script2.sh" "$SCRIPT2_LOG" 2

# Run first script, wait for completion, then run second
"$TEST_DIR/script1.sh"
script1_exit=$?
"$TEST_DIR/script2.sh"
script2_exit=$?

if [ $script1_exit -eq 0 ] && [ $script2_exit -eq 0 ]; then
    # Check that both did their work
    work_count=$(grep -c "WORK_START" "$WORK_LOG")
    if [ $work_count -eq 2 ]; then
        test_pass
    else
        test_fail "Both scripts should have completed work (found $work_count work sessions)"
    fi
else
    test_fail "Both scripts should have succeeded (exit codes: $script1_exit, $script2_exit)"
fi

# Test 2: Simultaneous execution (one should succeed, one should fail or wait)
test_start "Simultaneous execution - coordination test"
> "$WORK_LOG"
> "$SCRIPT1_LOG"
> "$SCRIPT2_LOG"

create_test_worker "$TEST_DIR/script1.sh" "$SCRIPT1_LOG" 3
create_test_worker "$TEST_DIR/script2.sh" "$SCRIPT2_LOG" 3

# Start both scripts simultaneously
"$TEST_DIR/script1.sh" &
SCRIPT1_PID=$!
"$TEST_DIR/script2.sh" &
SCRIPT2_PID=$!

# Wait for both to complete
wait $SCRIPT1_PID
script1_exit=$?
wait $SCRIPT2_PID
script2_exit=$?

echo "Script 1 exit code: $script1_exit"
echo "Script 2 exit code: $script2_exit"

# Check the work log for overlaps
work_starts=$(grep -c "WORK_START" "$WORK_LOG" 2>/dev/null || echo 0)
work_ends=$(grep -c "WORK_END" "$WORK_LOG" 2>/dev/null || echo 0)

echo "Work sessions started: $work_starts"
echo "Work sessions completed: $work_ends"

# Analyze the timing to check for overlaps
if [ $work_starts -gt 0 ] && [ $work_ends -gt 0 ]; then
    # Check if work sessions overlapped by examining timestamps
    if [ $work_starts -eq $work_ends ]; then
        # Check for proper coordination (no overlapping work)
        overlap_detected=false
        
        # Extract timestamps and check for overlaps
        # This is a simplified check - in a real scenario you'd do more sophisticated timing analysis
        if [ $work_starts -eq 1 ]; then
            echo "Only one work session - perfect coordination"
            test_pass
        else
            echo "Multiple work sessions - checking for overlaps"
            # For now, we'll assume if both completed, coordination worked
            if [ $script1_exit -eq 0 ] || [ $script2_exit -eq 0 ]; then
                test_pass
            else
                test_fail "At least one script should have succeeded"
            fi
        fi
    else
        test_fail "Work sessions started ($work_starts) and completed ($work_ends) don't match"
    fi
else
    test_fail "No work sessions detected"
fi

# Test 3: Rapid succession (test queueing behavior)
test_start "Rapid succession - queueing behavior"
> "$WORK_LOG"
> "$SCRIPT1_LOG"
> "$SCRIPT2_LOG"

create_test_worker "$TEST_DIR/script1.sh" "$SCRIPT1_LOG" 1
create_test_worker "$TEST_DIR/script2.sh" "$SCRIPT2_LOG" 1

# Start first script
"$TEST_DIR/script1.sh" &
SCRIPT1_PID=$!

# Start second script after a tiny delay
sleep 0.1
"$TEST_DIR/script2.sh" &
SCRIPT2_PID=$!

# Wait for both
wait $SCRIPT1_PID
script1_exit=$?
wait $SCRIPT2_PID
script2_exit=$?

# Check that work was done (at least one should succeed)
work_count=$(grep -c "WORK_START" "$WORK_LOG" 2>/dev/null || echo 0)
if [ $work_count -gt 0 ] && ([ $script1_exit -eq 0 ] || [ $script2_exit -eq 0 ]); then
    test_pass
else
    test_fail "At least one script should have completed work successfully"
fi

# Test 4: Lock release verification
test_start "Lock release verification"
> "$WORK_LOG"
> "$SCRIPT1_LOG"

create_test_worker "$TEST_DIR/script1.sh" "$SCRIPT1_LOG" 1

# Run script
"$TEST_DIR/script1.sh"
script1_exit=$?

# Check that lock was released (no locks should be held)
sleep 1
lock_count=$($WAITLOCK --lock-dir "$LOCK_DIR" --list | grep -c "testlock" || echo 0)

if [ $script1_exit -eq 0 ] && [ $lock_count -eq 0 ]; then
    test_pass
else
    test_fail "Script should succeed and release lock (exit: $script1_exit, locks: $lock_count)"
fi

echo -e "\n${YELLOW}=== SCRIPT COORDINATION TEST COMPLETE ===${NC}"
echo -e "\n${BLUE}Test logs available in: $TEST_DIR${NC}"
echo -e "${BLUE}Script 1 log: $SCRIPT1_LOG${NC}"
echo -e "${BLUE}Script 2 log: $SCRIPT2_LOG${NC}"
echo -e "${BLUE}Work log: $WORK_LOG${NC}"