#!/bin/bash
# Comprehensive test status summary for waitlock

set -e
source "$(dirname "$0")/test_framework.sh"

echo "=================================================="
echo "           WAITLOCK TEST STATUS SUMMARY"
echo "=================================================="
echo ""

# Test categories and their status
declare -A TEST_RESULTS

run_test_suite() {
    local test_name="$1"
    local test_script="$2"
    
    echo "Testing: $test_name"
    echo "----------------------------------------"
    
    if [ -f "$test_script" ]; then
        # Run test with timeout and capture results
        local output=$(timeout 15 "./$test_script" 2>&1 || true)
        local passed=$(echo "$output" | grep -c "✓" || echo "0")
        local failed=$(echo "$output" | grep -c "✗" || echo "0")
        local total=$((passed + failed))
        
        if [ $total -gt 0 ]; then
            local percent=$((passed * 100 / total))
            echo "  Results: $passed/$total passed ($percent%)"
            TEST_RESULTS["$test_name"]="$passed/$total"
            
            # Show failures if any
            if [ $failed -gt 0 ]; then
                echo "  Failures:"
                echo "$output" | grep "✗" | sed 's/^/    /'
            fi
        else
            echo "  Status: No test results found"
            TEST_RESULTS["$test_name"]="0/0"
        fi
    else
        echo "  Status: Test script not found"
        TEST_RESULTS["$test_name"]="N/A"
    fi
    echo ""
}

# Run test suites
run_test_suite "Help & Version" "test_help_version.sh"
run_test_suite "List Functionality" "test_list.sh"  
run_test_suite "Check Functionality" "test_check.sh"
run_test_suite "Mutex Operations" "test_mutex.sh"
run_test_suite "Semaphore Operations" "test_semaphore.sh"
run_test_suite "Timeout Functionality" "test_timeout.sh"
run_test_suite "Done Operations" "test_done.sh"
run_test_suite "Exec Operations" "test_exec.sh"
run_test_suite "CPU-based Locking" "test_onepercpu.sh"
run_test_suite "CPU Exclusion" "test_excludecpus.sh"
run_test_suite "Environment Variables" "test_environment.sh"
run_test_suite "Complex Scenarios" "test_scenarios.sh"

echo "=================================================="
echo "                  FINAL SUMMARY"
echo "=================================================="

total_passed=0
total_tests=0

for test_name in "${!TEST_RESULTS[@]}"; do
    result="${TEST_RESULTS[$test_name]}"
    if [[ "$result" =~ ^([0-9]+)/([0-9]+)$ ]]; then
        passed="${BASH_REMATCH[1]}"
        tests="${BASH_REMATCH[2]}"
        total_passed=$((total_passed + passed))
        total_tests=$((total_tests + tests))
        
        if [ $tests -gt 0 ]; then
            percent=$((passed * 100 / tests))
            printf "%-20s: %s (%d%%)\n" "$test_name" "$result" "$percent"
        else
            printf "%-20s: %s\n" "$test_name" "$result"
        fi
    else
        printf "%-20s: %s\n" "$test_name" "$result"
    fi
done

echo "----------------------------------------"
if [ $total_tests -gt 0 ]; then
    overall_percent=$((total_passed * 100 / total_tests))
    echo "OVERALL RESULTS: $total_passed/$total_tests passed ($overall_percent%)"
    
    if [ $overall_percent -ge 80 ]; then
        echo "STATUS: ✅ EXCELLENT - Ready for production"
    elif [ $overall_percent -ge 60 ]; then
        echo "STATUS: ✅ GOOD - Core functionality working"
    elif [ $overall_percent -ge 40 ]; then
        echo "STATUS: ⚠️  FAIR - Major features need work"
    else
        echo "STATUS: ❌ POOR - Significant issues remain"
    fi
else
    echo "STATUS: ❓ UNKNOWN - Unable to run tests"
fi

echo "=================================================="