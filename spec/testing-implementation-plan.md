# WaitLock Testing Implementation Plan
## Comprehensive Roadmap for Production-Ready Test Coverage

### Executive Summary

This document provides a detailed implementation plan for addressing the critical testing gaps identified in the waitlock project. The plan is structured in 3 phases over 6-8 weeks, prioritizing blocking issues first, then comprehensive coverage, and finally advanced infrastructure.

**Current State**: Excellent shell integration tests (95% coverage), failing C unit tests (17% pass rate)  
**Target State**: 100% reliable test suite with comprehensive error coverage suitable for enterprise deployment

---

## **PHASE 1: CRITICAL FIXES (Weeks 1-2)**
*Priority: BLOCKING - Must complete before any release*

### **1.1: Fix C Unit Test Process Synchronization** 
**File**: `src/test/test_lock.c`  
**Issue**: Race conditions causing 80%+ test failures  
**Timeline**: 3-4 days  

#### Problem Analysis
Current failing pattern:
```c
// BROKEN: Race condition in test_done_lock()
pid_t child_pid = fork();
if (child_pid == 0) {
    acquire_lock(test_descriptor, 1, 0.0);  // Child may fail
    sleep(5);  // Arbitrary wait
    exit(0);
}
sleep(1);  // Hope child acquired lock - FAILS OFTEN
```

#### Solution Implementation
```c
// FIXED: Proper inter-process communication
int sync_pipe[2];
pipe(sync_pipe);

pid_t child_pid = fork();
if (child_pid == 0) {
    // Child: Signal parent when lock acquired
    int result = acquire_lock(test_descriptor, 1, 0.0);
    char signal = (result == 0) ? 'S' : 'F';  // Success/Fail
    write(sync_pipe[1], &signal, 1);
    
    if (result == 0) {
        // Hold lock until parent signals completion
        char parent_signal;
        read(sync_pipe[0], &parent_signal, 1);
        release_lock();
    }
    exit(result);
}

// Parent: Wait for child confirmation
char child_signal;
if (read(sync_pipe[0], &child_signal, 1) == 1 && child_signal == 'S') {
    TEST_ASSERT(1, "Child successfully acquired lock");
    
    // Run tests...
    
    // Signal child to complete
    write(sync_pipe[1], "C", 1);
} else {
    TEST_ASSERT(0, "Child failed to acquire lock");
}
```

#### Files to Fix
- `src/test/test_lock.c` - Lock coordination tests
- `src/test/test_signal.c` - Signal handling tests  
- `src/test/test_integration.c` - Multi-process integration tests
- `src/test/test_core.c` - Core functionality with process interaction

#### Success Criteria
- All C unit tests pass consistently (3+ consecutive runs)
- No timeout failures due to process coordination
- Clear diagnostic output when tests fail

---

### **1.2: Fix C Unit Test Infrastructure**
**Files**: `src/test/*.c`  
**Issue**: Test isolation, cleanup, and framework reliability  
**Timeline**: 2-3 days  

#### Test Isolation Problems
```c
// CURRENT PROBLEM: Tests interfere with each other
test_acquire_lock();  // Leaves lock files
test_release_lock();  // Fails due to stale files
```

#### Solution: Proper Test Framework
```c
// NEW: test_framework.h
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

typedef struct {
    char test_dir[PATH_MAX];
    char lock_dir[PATH_MAX];
    pid_t test_pid;
    int cleanup_needed;
} test_context_t;

// Initialize test context for each test
int test_setup(test_context_t *ctx, const char *test_name);

// Cleanup test context after each test
int test_teardown(test_context_t *ctx);

// Enhanced test macros with context
#define TEST_START_CTX(ctx, name) \
    do { \
        test_setup(ctx, name); \
        test_count++; \
        printf("\n[TEST %d] %s (dir: %s)\n", test_count, name, ctx->test_dir); \
    } while(0)

#define TEST_END_CTX(ctx) \
    do { \
        test_teardown(ctx); \
    } while(0)

#endif
```

#### Implementation Details
```c
// test_framework.c
int test_setup(test_context_t *ctx, const char *test_name) {
    // Create unique test directory
    snprintf(ctx->test_dir, sizeof(ctx->test_dir), 
             "/tmp/waitlock_test_%s_%d_%ld", 
             test_name, getpid(), time(NULL));
    
    snprintf(ctx->lock_dir, sizeof(ctx->lock_dir), 
             "%s/locks", ctx->test_dir);
    
    // Create directories
    if (mkdir(ctx->test_dir, 0755) != 0) return -1;
    if (mkdir(ctx->lock_dir, 0755) != 0) return -1;
    
    // Set environment for this test
    setenv("WAITLOCK_DIR", ctx->lock_dir, 1);
    
    ctx->test_pid = getpid();
    ctx->cleanup_needed = 1;
    return 0;
}

int test_teardown(test_context_t *ctx) {
    if (!ctx->cleanup_needed) return 0;
    
    // Kill any child processes from this test
    // (Implementation details...)
    
    // Remove test directory
    char cmd[PATH_MAX + 20];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", ctx->test_dir);
    system(cmd);
    
    // Reset environment
    unsetenv("WAITLOCK_DIR");
    
    ctx->cleanup_needed = 0;
    return 0;
}
```

---

### **1.3: Enhanced Error Reporting and Diagnostics**
**Issue**: When tests fail, insufficient information to debug  
**Timeline**: 1-2 days  

#### Current Problem
```c
TEST_ASSERT(result == 0, "Should acquire lock");
// WHEN FAILS: No context about why it failed
```

#### Enhanced Diagnostics
```c
#define TEST_ASSERT_DETAILED(condition, format, ...) \
    do { \
        if (condition) { \
            pass_count++; \
            printf("  ✓ PASS: " format "\n", ##__VA_ARGS__); \
        } else { \
            fail_count++; \
            printf("  ✗ FAIL: " format "\n", ##__VA_ARGS__); \
            printf("    Context: errno=%d (%s)\n", errno, strerror(errno)); \
            printf("    PID: %d, Time: %ld\n", getpid(), time(NULL)); \
            printf("    Lock dir: %s\n", getenv("WAITLOCK_DIR")); \
            \
            /* Show current lock state */ \
            char cmd[512]; \
            snprintf(cmd, sizeof(cmd), "ls -la %s/ 2>/dev/null || echo 'No locks'", \
                     getenv("WAITLOCK_DIR")); \
            printf("    Current locks:\n"); \
            system(cmd); \
        } \
    } while(0)

// Usage:
int result = acquire_lock("test_lock", 1, 1.0);
TEST_ASSERT_DETAILED(result == 0, 
                    "acquire_lock('test_lock', 1, 1.0) should succeed, got %d", 
                    result);
```

---

### **1.4: Validation and Integration**
**Timeline**: 1 day  
**Goal**: Ensure all C unit test fixes work together  

#### Validation Process
1. **Individual Test Validation**: Each test module passes in isolation
2. **Sequential Test Validation**: All tests pass when run in sequence  
3. **Parallel Test Validation**: Tests can run concurrently without interference
4. **Stress Test Validation**: Tests pass under memory/CPU pressure

#### Success Metrics
- **Consistency**: 10 consecutive full test suite runs pass
- **Performance**: Full C unit test suite completes in <30 seconds
- **Reliability**: No flaky tests due to timing/race conditions
- **Diagnostics**: Clear failure reasons when tests do fail

---

## **PHASE 2: COMPREHENSIVE COVERAGE (Weeks 3-4)**
*Priority: HIGH - Production readiness*

### **2.1: Error Scenario Test Suite**
**New File**: `test/error_scenarios_comprehensive_test.sh`  
**Timeline**: 1 week  

#### Lock File Corruption Testing
```bash
#!/bin/bash
# test/corruption_handling_test.sh

test_checksum_validation() {
    test_start "Checksum validation on corrupted lock file"
    
    # Create valid lock
    $WAITLOCK valid_lock &
    LOCK_PID=$!
    
    # Wait for lock creation
    wait_for_lock "valid_lock"
    
    # Find and corrupt the lock file
    LOCK_FILE=$(find "$LOCK_DIR" -name "*valid_lock*" -type f)
    if [ -n "$LOCK_FILE" ]; then
        # Corrupt the checksum by modifying file content
        echo "CORRUPTED_DATA" >> "$LOCK_FILE"
        
        # Test that corruption is detected
        if $WAITLOCK --check valid_lock 2>&1 | grep -q "corruption\|checksum\|invalid"; then
            test_pass "Corruption properly detected"
        else
            test_fail "Corruption not detected"
        fi
        
        # Test that system recovers
        kill $LOCK_PID 2>/dev/null
        sleep 2
        
        if $WAITLOCK --check valid_lock; then
            test_pass "System recovered from corruption"
        else
            test_fail "System did not recover from corruption"
        fi
    else
        test_fail "Could not find lock file to corrupt"
    fi
}

test_partial_write_recovery() {
    test_start "Recovery from partial lock file writes"
    
    # Simulate partial write by creating incomplete lock file
    PARTIAL_FILE="$LOCK_DIR/partial_lock.slot0.lock"
    echo "INCOMPLETE" > "$PARTIAL_FILE"
    
    # Test that partial file is handled correctly
    if $WAITLOCK partial_lock 2>&1 | grep -q "invalid\|corrupt\|incomplete"; then
        test_pass "Partial write detected"
    else
        test_fail "Partial write not detected"
    fi
    
    # Verify file is cleaned up or made valid
    if $WAITLOCK partial_lock; then
        test_pass "System recovered from partial write"
        $WAITLOCK --done partial_lock
    else
        test_fail "System did not recover from partial write"
    fi
}
```

#### Filesystem Error Testing
```bash
test_permission_errors() {
    test_start "Permission error handling"
    
    # Test read-only lock directory
    chmod 555 "$LOCK_DIR"
    
    output=$($WAITLOCK readonly_test 2>&1)
    exit_code=$?
    
    if [ $exit_code -ne 0 ] && echo "$output" | grep -q -i "permission\|denied\|readonly"; then
        test_pass "Read-only directory error handled correctly"
    else
        test_fail "Read-only directory error not handled: $output"
    fi
    
    # Restore permissions
    chmod 755 "$LOCK_DIR"
}

test_disk_space_exhaustion() {
    test_start "Disk space exhaustion handling"
    
    # Create filesystem image with limited space
    LOOP_FILE="/tmp/small_fs_$$"
    MOUNT_POINT="/tmp/small_mount_$$"
    
    # Create 1MB filesystem
    dd if=/dev/zero of="$LOOP_FILE" bs=1024 count=1024 2>/dev/null
    
    if losetup --find "$LOOP_FILE" 2>/dev/null; then
        LOOP_DEV=$(losetup --associated "$LOOP_FILE" | cut -d: -f1)
        
        mkfs.ext4 -F "$LOOP_DEV" 2>/dev/null
        mkdir -p "$MOUNT_POINT"
        
        if mount "$LOOP_DEV" "$MOUNT_POINT" 2>/dev/null; then
            # Fill filesystem to near capacity
            dd if=/dev/zero of="$MOUNT_POINT/filler" bs=1024 count=1000 2>/dev/null || true
            
            # Test lock creation on full filesystem
            WAITLOCK_DIR="$MOUNT_POINT" $WAITLOCK full_fs_test 2>&1 | \
                grep -q -i "space\|full\|write.*fail" && \
                test_pass "Disk full error handled correctly" || \
                test_fail "Disk full error not handled correctly"
            
            # Cleanup
            umount "$MOUNT_POINT" 2>/dev/null || true
        fi
        
        losetup --detach "$LOOP_DEV" 2>/dev/null || true
    fi
    
    rm -f "$LOOP_FILE"
    rmdir "$MOUNT_POINT" 2>/dev/null || true
}
```

---

### **2.2: Edge Case Testing**
**New File**: `test/edge_cases_comprehensive_test.sh`  
**Timeline**: 3-4 days  

#### Boundary Condition Testing
```bash
test_descriptor_length_boundaries() {
    test_start "Descriptor length boundary testing"
    
    # Test maximum valid length (255 characters)
    MAX_DESC=$(printf 'a%.0s' {1..255})
    if $WAITLOCK "$MAX_DESC"; then
        test_pass "255-character descriptor accepted"
        $WAITLOCK --done "$MAX_DESC"
    else
        test_fail "255-character descriptor rejected"
    fi
    
    # Test over-length descriptor (256 characters)
    OVER_DESC=$(printf 'a%.0s' {1..256})
    if $WAITLOCK "$OVER_DESC" 2>&1 | grep -q -i "too.*long\|length\|limit"; then
        test_pass "256-character descriptor properly rejected"
    else
        test_fail "256-character descriptor not properly rejected"
    fi
    
    # Test empty descriptor
    if $WAITLOCK "" 2>&1 | grep -q -i "empty\|invalid\|descriptor"; then
        test_pass "Empty descriptor properly rejected"
    else
        test_fail "Empty descriptor not properly rejected"
    fi
}

test_special_character_handling() {
    test_start "Special character handling in descriptors"
    
    # Test descriptors with various special characters
    declare -a SPECIAL_CHARS=(
        "desc with spaces"
        "desc-with-dashes"
        "desc_with_underscores"
        "desc.with.dots"
        "desc/with/slashes"
        $'desc\nwith\nnewlines'
        $'desc\twith\ttabs'
        "desc'with'quotes"
        'desc"with"doublequotes'
        "desc\$with\$dollars"
        "desc&with&ampersands"
    )
    
    for desc in "${SPECIAL_CHARS[@]}"; do
        echo "  Testing: $(printf '%q' "$desc")"
        
        # Some characters should be accepted, others rejected
        output=$($WAITLOCK "$desc" 2>&1)
        exit_code=$?
        
        case "$desc" in
            *"/"*|*$'\n'*|*$'\t'*)
                # These should be rejected
                if [ $exit_code -ne 0 ]; then
                    test_pass "Properly rejected invalid character in: $(printf '%q' "$desc")"
                else
                    test_fail "Should have rejected invalid character in: $(printf '%q' "$desc")"
                    $WAITLOCK --done "$desc" 2>/dev/null || true
                fi
                ;;
            *)
                # These should be accepted
                if [ $exit_code -eq 0 ]; then
                    test_pass "Properly accepted valid descriptor: $(printf '%q' "$desc")"
                    $WAITLOCK --done "$desc" 2>/dev/null || true
                else
                    test_fail "Should have accepted valid descriptor: $(printf '%q' "$desc")"
                fi
                ;;
        esac
    done
}

test_timeout_edge_cases() {
    test_start "Timeout value edge cases"
    
    # Very small timeout
    start_time=$(date +%s.%N)
    $WAITLOCK --timeout 0.001 tiny_timeout_test &
    LOCK_PID=$!
    
    # Should timeout very quickly
    if $WAITLOCK --timeout 0.001 tiny_timeout_test 2>&1; then
        test_fail "Should have timed out with 0.001 second timeout"
    else
        end_time=$(date +%s.%N)
        duration=$(echo "$end_time - $start_time" | bc)
        
        if (( $(echo "$duration < 0.1" | bc -l) )); then
            test_pass "Very small timeout (0.001s) respected"
        else
            test_fail "Very small timeout took too long: ${duration}s"
        fi
    fi
    
    kill $LOCK_PID 2>/dev/null || true
    
    # Very large timeout (should not overflow)
    if $WAITLOCK --timeout 999999999 huge_timeout_test 2>&1 | grep -q -i "overflow\|invalid\|too.*large"; then
        test_pass "Very large timeout properly handled"
    else
        # Start the test but don't wait for it
        $WAITLOCK --timeout 999999999 huge_timeout_test &
        HUGE_PID=$!
        sleep 1
        
        if kill -0 $HUGE_PID 2>/dev/null; then
            test_pass "Very large timeout accepted and running"
            kill $HUGE_PID 2>/dev/null || true
        else
            test_fail "Very large timeout process died unexpectedly"
        fi
    fi
}
```

---

### **2.3: Command-Line Option Combination Testing**
**Timeline**: 2-3 days  

```bash
test_option_combinations() {
    test_start "Command-line option combinations"
    
    # Test complex valid combinations
    if $WAITLOCK --allowMultiple 3 --timeout 5.0 --syslog --exec "echo test" combo_test; then
        test_pass "Complex option combination accepted"
    else
        test_fail "Valid complex option combination rejected"
    fi
    
    # Test conflicting options
    output=$($WAITLOCK --quiet --verbose conflict_test 2>&1)
    if echo "$output" | grep -q -i "conflict\|incompatible\|cannot.*both"; then
        test_pass "Conflicting options (--quiet --verbose) properly detected"
    else
        test_fail "Conflicting options not detected: $output"
    fi
    
    # Test invalid combinations
    output=$($WAITLOCK --onePerCPU --allowMultiple 1 invalid_combo 2>&1)
    if echo "$output" | grep -q -i "conflict\|incompatible\|cannot.*both"; then
        test_pass "Invalid combination (--onePerCPU with --allowMultiple 1) detected"
    else
        test_fail "Invalid combination not detected: $output"
    fi
}
```

---

### **2.4: Resource Exhaustion and Stress Testing**
**New File**: `test/stress_and_limits_test.sh`  
**Timeline**: 2-3 days  

```bash
test_file_descriptor_limits() {
    test_start "File descriptor limit testing"
    
    # Get current file descriptor limit
    CURRENT_LIMIT=$(ulimit -n)
    
    # Set a low limit for testing
    ulimit -n 50
    
    # Try to create many locks (should hit FD limit)
    declare -a LOCK_PIDS=()
    LOCK_COUNT=0
    
    for i in {1..60}; do
        $WAITLOCK fd_limit_test_$i &
        PID=$!
        
        if kill -0 $PID 2>/dev/null; then
            LOCK_PIDS+=($PID)
            LOCK_COUNT=$((LOCK_COUNT + 1))
        else
            break
        fi
        
        sleep 0.1
    done
    
    if [ $LOCK_COUNT -lt 60 ]; then
        test_pass "File descriptor limit properly enforced (created $LOCK_COUNT locks)"
    else
        test_fail "File descriptor limit not enforced"
    fi
    
    # Cleanup
    for pid in "${LOCK_PIDS[@]}"; do
        kill $pid 2>/dev/null || true
    done
    
    # Restore original limit
    ulimit -n $CURRENT_LIMIT
}

test_concurrent_lock_stress() {
    test_start "High-concurrency stress testing"
    
    CONCURRENT_PROCESSES=100
    SEMAPHORE_SIZE=10
    
    declare -a STRESS_PIDS=()
    
    # Launch many processes trying to acquire semaphore slots
    for i in $(seq 1 $CONCURRENT_PROCESSES); do
        (
            if $WAITLOCK --allowMultiple $SEMAPHORE_SIZE --timeout 10 stress_semaphore; then
                # Hold for random time
                sleep $(echo "scale=2; $RANDOM/32767*2" | bc)
                $WAITLOCK --done stress_semaphore
                exit 0
            else
                exit 1
            fi
        ) &
        STRESS_PIDS+=($!)
    done
    
    # Wait for all processes to complete
    SUCCESS_COUNT=0
    TIMEOUT_COUNT=0
    
    for pid in "${STRESS_PIDS[@]}"; do
        if wait $pid; then
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            TIMEOUT_COUNT=$((TIMEOUT_COUNT + 1))
        fi
    done
    
    echo "  Results: $SUCCESS_COUNT successful, $TIMEOUT_COUNT timed out"
    
    # Validate results
    if [ $SUCCESS_COUNT -ge $SEMAPHORE_SIZE ] && [ $SUCCESS_COUNT -le $CONCURRENT_PROCESSES ]; then
        test_pass "Stress test completed successfully"
    else
        test_fail "Stress test results unexpected: $SUCCESS_COUNT successes"
    fi
    
    # Cleanup any remaining locks
    $WAITLOCK --list | grep stress_semaphore | while read line; do
        $WAITLOCK --done stress_semaphore 2>/dev/null || true
    done
}
```

---

## **PHASE 3: INFRASTRUCTURE & AUTOMATION (Weeks 5-6)**
*Priority: MEDIUM - Long-term maintainability*

### **3.1: Unified Test Discovery and Execution**
**New File**: `test/run_comprehensive_tests.sh`  
**Timeline**: 1 week  

```bash
#!/bin/bash
# Comprehensive test runner for all waitlock test suites

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
RESULTS_DIR="$BUILD_DIR/test-results"

# Test discovery patterns
C_TEST_PATTERN="src/test/test_*.c"
SHELL_TEST_PATTERN="test/*_test.sh"
EXTERNAL_TEST_PATTERN="test/external/test_*.sh"

# Colors and formatting
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

# Global counters
TOTAL_SUITES=0
PASSED_SUITES=0
FAILED_SUITES=0

# Results tracking
declare -a SUITE_RESULTS=()

# Initialize test environment
initialize_test_environment() {
    echo -e "${CYAN}=== WAITLOCK COMPREHENSIVE TEST SUITE ===${NC}"
    echo "Project: $PROJECT_ROOT"
    echo "Build: $BUILD_DIR"
    echo "Results: $RESULTS_DIR"
    
    # Create results directory
    mkdir -p "$RESULTS_DIR"
    
    # Ensure waitlock is built
    if [ ! -f "$BUILD_DIR/bin/waitlock" ]; then
        echo -e "${YELLOW}Building waitlock...${NC}"
        cd "$PROJECT_ROOT"
        make clean && make
    fi
    
    # Verify binary
    if [ ! -x "$BUILD_DIR/bin/waitlock" ]; then
        echo -e "${RED}ERROR: waitlock binary not found or not executable${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✓ Environment initialized${NC}"
}

# Run C unit tests
run_c_unit_tests() {
    echo -e "\n${CYAN}=== C UNIT TESTS ===${NC}"
    TOTAL_SUITES=$((TOTAL_SUITES + 1))
    
    local result_file="$RESULTS_DIR/c_unit_tests.log"
    
    cd "$PROJECT_ROOT/src"
    if make test > "$result_file" 2>&1; then
        PASSED_SUITES=$((PASSED_SUITES + 1))
        SUITE_RESULTS+=("✓ C Unit Tests")
        echo -e "${GREEN}✓ C unit tests passed${NC}"
    else
        FAILED_SUITES=$((FAILED_SUITES + 1))
        SUITE_RESULTS+=("✗ C Unit Tests")
        echo -e "${RED}✗ C unit tests failed${NC}"
        
        # Show failure summary
        echo -e "${YELLOW}Last 20 lines of output:${NC}"
        tail -20 "$result_file" | sed 's/^/  /'
    fi
    
    cd "$PROJECT_ROOT"
}

# Discover and run shell test suites
run_shell_test_suites() {
    echo -e "\n${CYAN}=== SHELL INTEGRATION TESTS ===${NC}"
    
    # Find all shell test files
    local test_files=($(find test/ -name "*_test.sh" -executable | sort))
    
    if [ ${#test_files[@]} -eq 0 ]; then
        echo -e "${YELLOW}No shell test files found${NC}"
        return
    fi
    
    for test_file in "${test_files[@]}"; do
        TOTAL_SUITES=$((TOTAL_SUITES + 1))
        
        local test_name=$(basename "$test_file" .sh)
        local result_file="$RESULTS_DIR/${test_name}.log"
        
        echo -e "${BLUE}Running: $test_file${NC}"
        
        if timeout 300 "$test_file" > "$result_file" 2>&1; then
            PASSED_SUITES=$((PASSED_SUITES + 1))
            SUITE_RESULTS+=("✓ $test_name")
            echo -e "${GREEN}  ✓ Passed${NC}"
        else
            FAILED_SUITES=$((FAILED_SUITES + 1))
            SUITE_RESULTS+=("✗ $test_name")
            echo -e "${RED}  ✗ Failed${NC}"
            
            # Show failure summary
            echo -e "${YELLOW}  Last 10 lines of output:${NC}"
            tail -10 "$result_file" | sed 's/^/    /'
        fi
    done
}

# Run external test suite
run_external_test_suite() {
    echo -e "\n${CYAN}=== EXTERNAL TEST SUITE ===${NC}"
    TOTAL_SUITES=$((TOTAL_SUITES + 1))
    
    local result_file="$RESULTS_DIR/external_tests.log"
    
    if [ -x "test/external/run_all_tests.sh" ]; then
        if timeout 300 test/external/run_all_tests.sh > "$result_file" 2>&1; then
            PASSED_SUITES=$((PASSED_SUITES + 1))
            SUITE_RESULTS+=("✓ External Tests")
            echo -e "${GREEN}✓ External tests passed${NC}"
        else
            FAILED_SUITES=$((FAILED_SUITES + 1))
            SUITE_RESULTS+=("✗ External Tests")
            echo -e "${RED}✗ External tests failed${NC}"
            
            echo -e "${YELLOW}Last 15 lines of output:${NC}"
            tail -15 "$result_file" | sed 's/^/  /'
        fi
    else
        echo -e "${YELLOW}External test runner not found or not executable${NC}"
        TOTAL_SUITES=$((TOTAL_SUITES - 1))
    fi
}

# Generate comprehensive test report
generate_test_report() {
    local report_file="$RESULTS_DIR/comprehensive_report.md"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    cat > "$report_file" << EOF
# WaitLock Comprehensive Test Report

**Generated**: $timestamp  
**Project**: $(basename "$PROJECT_ROOT")  
**Build**: $(cd "$PROJECT_ROOT" && git rev-parse --short HEAD 2>/dev/null || echo "unknown")  

## Summary

- **Total Test Suites**: $TOTAL_SUITES
- **Passed**: $PASSED_SUITES
- **Failed**: $FAILED_SUITES
- **Success Rate**: $(( (PASSED_SUITES * 100) / TOTAL_SUITES ))%

## Results by Suite

EOF

    for result in "${SUITE_RESULTS[@]}"; do
        echo "- $result" >> "$report_file"
    done
    
    cat >> "$report_file" << EOF

## Detailed Logs

Individual test logs are available in: \`$RESULTS_DIR/\`

EOF

    if [ $FAILED_SUITES -gt 0 ]; then
        cat >> "$report_file" << EOF
## Failed Test Analysis

The following test suites failed and require attention:

EOF
        for result in "${SUITE_RESULTS[@]}"; do
            if [[ "$result" == "✗"* ]]; then
                local suite_name=$(echo "$result" | sed 's/✗ //')
                echo "### $suite_name" >> "$report_file"
                echo "" >> "$report_file"
                echo "\`\`\`" >> "$report_file"
                if [ -f "$RESULTS_DIR/${suite_name// /_}.log" ]; then
                    tail -20 "$RESULTS_DIR/${suite_name// /_}.log" >> "$report_file"
                else
                    echo "Log file not found" >> "$report_file"
                fi
                echo "\`\`\`" >> "$report_file"
                echo "" >> "$report_file"
            fi
        done
    fi
    
    echo -e "${BLUE}Comprehensive report generated: $report_file${NC}"
}

# Display final summary
display_final_summary() {
    echo -e "\n${CYAN}=== FINAL TEST SUMMARY ===${NC}"
    echo "Total suites: $TOTAL_SUITES"
    echo -e "Passed: ${GREEN}$PASSED_SUITES${NC}"
    echo -e "Failed: ${RED}$FAILED_SUITES${NC}"
    
    if [ $FAILED_SUITES -eq 0 ]; then
        echo -e "\n${GREEN}🎉 ALL TESTS PASSED! 🎉${NC}"
        echo -e "${GREEN}waitlock is ready for production deployment${NC}"
        return 0
    else
        echo -e "\n${RED}❌ $FAILED_SUITES TEST SUITE(S) FAILED ❌${NC}"
        echo -e "${YELLOW}Review the detailed logs in $RESULTS_DIR${NC}"
        return 1
    fi
}

# Main execution
main() {
    local start_time=$(date +%s)
    
    initialize_test_environment
    run_c_unit_tests
    run_shell_test_suites
    run_external_test_suite
    generate_test_report
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    echo -e "\n${BLUE}Total execution time: ${duration}s${NC}"
    
    display_final_summary
}

# Script entry point
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
```

---

### **3.2: Cross-Platform Test Validation**
**New Files**: `test/platform_compatibility_test.sh`, CI configuration  
**Timeline**: 1 week  

#### Platform-Specific Test Cases
```bash
# test/platform_compatibility_test.sh
test_cpu_detection_cross_platform() {
    test_start "Cross-platform CPU detection"
    
    case "$(uname -s)" in
        Linux)
            # Linux: Should use /proc/cpuinfo
            if [ -f /proc/cpuinfo ]; then
                cpu_count=$($WAITLOCK --onePerCPU test_cpu &
                echo $!
                sleep 1
                $WAITLOCK --list | grep test_cpu | wc -l)
                
                expected_count=$(nproc)
                if [ "$cpu_count" -eq "$expected_count" ]; then
                    test_pass "Linux CPU detection correct: $cpu_count CPUs"
                else
                    test_fail "Linux CPU detection incorrect: got $cpu_count, expected $expected_count"
                fi
            fi
            ;;
        Darwin)
            # macOS: Should use sysctl
            expected_count=$(sysctl -n hw.ncpu)
            # Test implementation...
            ;;
        FreeBSD|OpenBSD|NetBSD)
            # BSD: Should use sysctl
            expected_count=$(sysctl -n hw.ncpu)
            # Test implementation...
            ;;
        *)
            test_skip "Unknown platform: $(uname -s)"
            ;;
    esac
}

test_signal_handling_cross_platform() {
    test_start "Cross-platform signal handling"
    
    # Test SIGTERM forwarding in --exec mode
    $WAITLOCK --exec "sleep 30" platform_signal_test &
    EXEC_PID=$!
    
    sleep 2
    
    # Send SIGTERM to waitlock process
    kill -TERM $EXEC_PID
    
    # Check if child process also terminated
    sleep 1
    if ! ps aux | grep -v grep | grep "sleep 30"; then
        test_pass "SIGTERM properly forwarded to child process"
    else
        test_fail "SIGTERM not properly forwarded"
        # Cleanup
        pkill -f "sleep 30" 2>/dev/null || true
    fi
}
```

#### GitHub Actions CI Configuration
```yaml
# .github/workflows/comprehensive-tests.yml
name: Comprehensive Test Suite

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  test-linux:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        os: [ubuntu-20.04, ubuntu-22.04]
        compiler: [gcc, clang]
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y build-essential autotools-dev autoconf
        
    - name: Build waitlock
      run: |
        autoreconf -i
        ./configure CC=${{ matrix.compiler }}
        make clean
        make
        
    - name: Run comprehensive test suite
      run: |
        ./test/run_comprehensive_tests.sh
        
    - name: Upload test results
      if: always()
      uses: actions/upload-artifact@v3
      with:
        name: test-results-${{ matrix.os }}-${{ matrix.compiler }}
        path: build/test-results/

  test-macos:
    runs-on: macos-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        brew install autoconf automake
        
    - name: Build and test
      run: |
        autoreconf -i
        ./configure
        make clean && make
        ./test/run_comprehensive_tests.sh

  test-freebsd:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Test on FreeBSD
      uses: vmactions/freebsd-vm@v0
      with:
        usesh: true
        run: |
          pkg install -y autotools gmake
          autoreconf -i
          ./configure
          gmake clean && gmake
          ./test/run_comprehensive_tests.sh
```

---

### **3.3: Performance Regression Testing**
**New File**: `test/performance_regression_test.sh`  
**Timeline**: 3-4 days  

```bash
#!/bin/bash
# Performance regression testing and monitoring

# Performance test configurations
PERF_TEST_ITERATIONS=10
BASELINE_FILE="test/performance_baseline.json"
RESULTS_FILE="build/test-results/performance_results.json"

# Benchmark lock acquisition performance
benchmark_lock_acquisition() {
    local test_name="lock_acquisition"
    local iterations=1000
    
    echo "Benchmarking lock acquisition ($iterations iterations)..."
    
    local start_time=$(date +%s.%N)
    
    for i in $(seq 1 $iterations); do
        $WAITLOCK perf_test_$i &
        local pid=$!
        $WAITLOCK --done perf_test_$i
        wait $pid 2>/dev/null || true
    done
    
    local end_time=$(date +%s.%N)
    local total_time=$(echo "$end_time - $start_time" | bc)
    local avg_time=$(echo "scale=6; $total_time / $iterations" | bc)
    
    echo "Average lock acquisition time: ${avg_time}s"
    
    # Store result
    local result="{\"test\":\"$test_name\",\"avg_time\":$avg_time,\"total_time\":$total_time,\"iterations\":$iterations}"
    echo "$result"
}

benchmark_semaphore_contention() {
    local test_name="semaphore_contention"
    local processes=50
    local semaphore_size=10
    local duration=30
    
    echo "Benchmarking semaphore contention ($processes processes, $semaphore_size slots, ${duration}s)..."
    
    local start_time=$(date +%s.%N)
    local pids=()
    
    # Launch competing processes
    for i in $(seq 1 $processes); do
        (
            local acquired=0
            local start=$(date +%s)
            
            while [ $(($(date +%s) - start)) -lt $duration ]; do
                if $WAITLOCK --allowMultiple $semaphore_size --timeout 1 semaphore_perf_test; then
                    acquired=$((acquired + 1))
                    sleep 0.1  # Simulate work
                    $WAITLOCK --done semaphore_perf_test
                fi
            done
            
            echo $acquired > "/tmp/perf_result_$$_$i"
        ) &
        pids+=($!)
    done
    
    # Wait for all processes
    for pid in "${pids[@]}"; do
        wait $pid
    done
    
    local end_time=$(date +%s.%N)
    local total_time=$(echo "$end_time - $start_time" | bc)
    
    # Collect results
    local total_acquisitions=0
    for i in $(seq 1 $processes); do
        if [ -f "/tmp/perf_result_$$_$i" ]; then
            local count=$(cat "/tmp/perf_result_$$_$i")
            total_acquisitions=$((total_acquisitions + count))
            rm -f "/tmp/perf_result_$$_$i"
        fi
    done
    
    local throughput=$(echo "scale=2; $total_acquisitions / $total_time" | bc)
    
    echo "Semaphore throughput: ${throughput} acquisitions/second"
    
    local result="{\"test\":\"$test_name\",\"throughput\":$throughput,\"total_acquisitions\":$total_acquisitions,\"duration\":$total_time,\"processes\":$processes}"
    echo "$result"
}

# Memory usage profiling
profile_memory_usage() {
    local test_name="memory_usage"
    
    echo "Profiling memory usage..."
    
    # Create many concurrent locks
    local pids=()
    for i in {1..100}; do
        $WAITLOCK memory_test_$i &
        pids+=($!)
    done
    
    sleep 2
    
    # Measure memory usage
    local memory_kb=$(ps -o pid,vsz,rss --ppid $$ | grep -v PID | awk '{sum_vsz+=$2; sum_rss+=$3} END {print sum_vsz, sum_rss}')
    local vsz=$(echo $memory_kb | cut -d' ' -f1)
    local rss=$(echo $memory_kb | cut -d' ' -f2)
    
    echo "Memory usage: VSZ=${vsz}KB, RSS=${rss}KB"
    
    # Cleanup
    for pid in "${pids[@]}"; do
        kill $pid 2>/dev/null || true
    done
    
    local result="{\"test\":\"$test_name\",\"vsz_kb\":$vsz,\"rss_kb\":$rss,\"concurrent_locks\":100}"
    echo "$result"
}

# Compare with baseline and detect regressions
check_performance_regression() {
    local current_results="$1"
    
    if [ ! -f "$BASELINE_FILE" ]; then
        echo "No baseline file found. Current results will become baseline."
        echo "$current_results" > "$BASELINE_FILE"
        return 0
    fi
    
    echo "Checking for performance regressions..."
    
    # Compare key metrics (simplified comparison)
    local baseline_lock_time=$(jq -r '.[] | select(.test=="lock_acquisition") | .avg_time' "$BASELINE_FILE")
    local current_lock_time=$(echo "$current_results" | jq -r '.[] | select(.test=="lock_acquisition") | .avg_time')
    
    if [ -n "$baseline_lock_time" ] && [ -n "$current_lock_time" ]; then
        local regression=$(echo "scale=4; ($current_lock_time - $baseline_lock_time) / $baseline_lock_time * 100" | bc)
        
        echo "Lock acquisition performance change: ${regression}%"
        
        # Fail if more than 20% regression
        if (( $(echo "$regression > 20" | bc -l) )); then
            echo "❌ PERFORMANCE REGRESSION DETECTED: ${regression}% slower"
            return 1
        elif (( $(echo "$regression < -10" | bc -l) )); then
            echo "🎉 PERFORMANCE IMPROVEMENT: ${regression}% faster"
        else
            echo "✅ Performance within acceptable range"
        fi
    fi
    
    return 0
}

# Main performance test runner
main() {
    echo "=== WAITLOCK PERFORMANCE REGRESSION TESTING ==="
    
    # Ensure clean environment
    pkill -f waitlock 2>/dev/null || true
    sleep 1
    
    mkdir -p "$(dirname "$RESULTS_FILE")"
    
    # Run benchmarks
    local results="["
    results+=$(benchmark_lock_acquisition)","
    results+=$(benchmark_semaphore_contention)","
    results+=$(profile_memory_usage)
    results+="]"
    
    # Save results
    echo "$results" | jq . > "$RESULTS_FILE"
    
    echo "Performance results saved to: $RESULTS_FILE"
    
    # Check for regressions
    if check_performance_regression "$results"; then
        echo "✅ No performance regressions detected"
        exit 0
    else
        echo "❌ Performance regression detected"
        exit 1
    fi
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
```

---

## **SUCCESS CRITERIA & VALIDATION**

### **Phase 1 Success Criteria (Critical)**
- [ ] All 6 C unit test suites pass consistently (10 consecutive runs)
- [ ] Zero timeout failures due to race conditions
- [ ] Clear diagnostic output for any remaining failures
- [ ] Test execution time under 60 seconds for full C unit test suite

### **Phase 2 Success Criteria (High Priority)**
- [ ] 90%+ coverage of error scenarios (corruption, filesystem, resource limits)
- [ ] Comprehensive edge case testing (boundaries, special characters)
- [ ] All command-line option combinations validated
- [ ] Stress testing with 100+ concurrent processes passes

### **Phase 3 Success Criteria (Medium Priority)**
- [ ] Unified test runner executing all test suites
- [ ] Cross-platform testing on Linux, macOS, and FreeBSD
- [ ] Performance baseline established and regression detection working
- [ ] CI/CD pipeline running comprehensive tests automatically

### **Overall Project Success Criteria**
- [ ] 100% test suite pass rate across all platforms
- [ ] Zero known critical bugs or race conditions
- [ ] Performance within 20% of baseline across releases
- [ ] Comprehensive error handling for all failure modes
- [ ] Production deployment confidence achieved

---

## **TIMELINE SUMMARY**

| Week | Phase | Focus | Deliverables |
|------|--------|--------|-------------|
| 1 | 1.1-1.2 | Fix C unit test race conditions and infrastructure | Reliable C unit tests |
| 2 | 1.3-1.4 | Enhanced diagnostics and validation | 100% C unit test pass rate |
| 3 | 2.1-2.2 | Error scenarios and edge cases | Comprehensive error coverage |
| 4 | 2.3-2.4 | Option combinations and stress testing | Production-ready reliability |
| 5 | 3.1-3.2 | Unified testing and cross-platform | Automated test infrastructure |
| 6 | 3.3-3.4 | Performance monitoring and optimization | Complete test ecosystem |

**Total Estimated Effort**: 6-8 weeks  
**Critical Path**: Phase 1 completion blocks all other work  
**Risk Mitigation**: Each phase has clear success criteria and validation steps

This comprehensive plan addresses all identified testing gaps while maintaining focus on the most critical issues first. The phased approach ensures that blocking problems are resolved before building advanced infrastructure, maximizing the return on testing investment.