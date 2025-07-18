#!/bin/bash

# Foreground coordination test - demonstrates the proper way to use waitlock for script coordination

set -e

WAITLOCK="./build/bin/waitlock"
TEST_DIR="/tmp/foreground_coordination_$$"
LOCK_DIR="$TEST_DIR/locks"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== FOREGROUND COORDINATION TEST ===${NC}"

# Cleanup function
cleanup() {
    pkill -f "$WAITLOCK.*foregroundtest" 2>/dev/null || true
    rm -rf "$TEST_DIR" 2>/dev/null || true
}
trap cleanup EXIT

# Setup
mkdir -p "$LOCK_DIR"
make clean >/dev/null 2>&1 || true
make >/dev/null 2>&1

# Create worker script using foreground approach
create_worker_script() {
    local script_path="$1"
    local worker_id="$2"
    local timeout="$3"
    
    cat > "$script_path" << EOF
#!/bin/bash
echo "[$worker_id] Starting at \$(date '+%H:%M:%S')"

# Use --exec mode for proper foreground coordination
if $WAITLOCK --lock-dir "$LOCK_DIR" --timeout $timeout foregroundtest --exec bash -c "
    echo '[$worker_id] SUCCESS: Got the lock at \$(date +%H:%M:%S)'
    echo '[$worker_id] Doing critical work...'
    
    # Simulate work
    sleep 2
    
    echo '[$worker_id] Work complete at \$(date +%H:%M:%S)'
    echo '[$worker_id] Lock will be released automatically'
"; then
    exit 0
else
    echo "[$worker_id] FAILED: Could not get lock at \$(date '+%H:%M:%S')"
    exit 1
fi
EOF
    chmod +x "$script_path"
}

echo -e "${BLUE}Test 1: Sequential execution (both should succeed)${NC}"
create_worker_script "$TEST_DIR/worker1.sh" "Worker1" 10
create_worker_script "$TEST_DIR/worker2.sh" "Worker2" 10

# Run sequentially
echo "Running worker 1..."
"$TEST_DIR/worker1.sh"
exit1=$?

echo "Running worker 2..."
"$TEST_DIR/worker2.sh"
exit2=$?

if [ $exit1 -eq 0 ] && [ $exit2 -eq 0 ]; then
    echo -e "${GREEN}✓ Sequential test passed${NC}"
else
    echo -e "${RED}✗ Sequential test failed (exit codes: $exit1, $exit2)${NC}"
fi

echo -e "\n${BLUE}Test 2: Simultaneous execution with short timeout (one should succeed, one should fail)${NC}"
create_worker_script "$TEST_DIR/worker1.sh" "Worker1" 1
create_worker_script "$TEST_DIR/worker2.sh" "Worker2" 1

# Run simultaneously
echo "Starting both workers simultaneously..."
"$TEST_DIR/worker1.sh" &
PID1=$!
"$TEST_DIR/worker2.sh" &
PID2=$!

# Wait for both
wait $PID1
exit1=$?
wait $PID2
exit2=$?

echo "Results:"
echo "Worker 1 exit code: $exit1"
echo "Worker 2 exit code: $exit2"

# One should succeed, one should fail
if ([ $exit1 -eq 0 ] && [ $exit2 -eq 1 ]) || ([ $exit1 -eq 1 ] && [ $exit2 -eq 0 ]); then
    echo -e "${GREEN}✓ Perfect coordination: One succeeded, one failed${NC}"
elif [ $exit1 -eq 0 ] && [ $exit2 -eq 0 ]; then
    echo -e "${RED}✗ Poor coordination: Both succeeded (shouldn't happen)${NC}"
else
    echo -e "${RED}✗ Both failed${NC}"
fi

echo -e "\n${BLUE}Test 3: Simultaneous execution with long timeout (both should eventually succeed)${NC}"
create_worker_script "$TEST_DIR/worker1.sh" "Worker1" 15
create_worker_script "$TEST_DIR/worker2.sh" "Worker2" 15

# Run simultaneously
echo "Starting both workers simultaneously with long timeout..."
"$TEST_DIR/worker1.sh" &
PID1=$!
"$TEST_DIR/worker2.sh" &
PID2=$!

# Wait for both
wait $PID1
exit1=$?
wait $PID2
exit2=$?

echo "Results:"
echo "Worker 1 exit code: $exit1"
echo "Worker 2 exit code: $exit2"

# Both should eventually succeed
if [ $exit1 -eq 0 ] && [ $exit2 -eq 0 ]; then
    echo -e "${GREEN}✓ Queuing worked: Both eventually succeeded${NC}"
else
    echo -e "${RED}✗ Queuing failed (exit codes: $exit1, $exit2)${NC}"
fi

echo -e "\n${BLUE}Test 4: Immediate check (no waiting)${NC}"
create_worker_script "$TEST_DIR/worker1.sh" "Worker1" 0
create_worker_script "$TEST_DIR/worker2.sh" "Worker2" 0

# Start first worker in background (will hold lock)
$WAITLOCK --lock-dir "$LOCK_DIR" foregroundtest &
HOLDER_PID=$!
sleep 1

# Try immediate check
echo "Trying immediate check while lock is held..."
"$TEST_DIR/worker1.sh"
exit1=$?

# Clean up holder
kill $HOLDER_PID 2>/dev/null || true
wait $HOLDER_PID 2>/dev/null || true

# Now try again (should succeed immediately)
echo "Trying immediate check after lock is released..."
"$TEST_DIR/worker2.sh"
exit2=$?

if [ $exit1 -eq 1 ] && [ $exit2 -eq 0 ]; then
    echo -e "${GREEN}✓ Immediate check worked: Failed when busy, succeeded when available${NC}"
else
    echo -e "${RED}✗ Immediate check failed (exit codes: $exit1, $exit2)${NC}"
fi

echo -e "\n${YELLOW}=== FOREGROUND COORDINATION TEST COMPLETE ===${NC}"
echo -e "\n${GREEN}Summary: Foreground approach is much more reliable than background (&) approach${NC}"
echo -e "${GREEN}- Use --timeout for coordination${NC}"
echo -e "${GREEN}- Lock is automatically released when process exits${NC}"
echo -e "${GREEN}- No need to manage background processes${NC}"
echo -e "${GREEN}- Clear success/failure indication${NC}"