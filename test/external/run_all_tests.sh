#!/bin/bash
# Main test runner for all external waitlock tests

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="$SCRIPT_DIR"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to run a test file and track results
run_test_file() {
    local test_file="$1"
    local test_name="$(basename "$test_file" .sh)"
    
    echo -e "${BLUE}Running $test_name...${NC}"
    
    if [ -x "$test_file" ]; then
        if "$test_file"; then
            echo -e "${GREEN}✓ $test_name passed${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            echo -e "${RED}✗ $test_name failed${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    else
        echo -e "${RED}✗ $test_name is not executable${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo ""
}

# Function to make test files executable
make_executable() {
    local test_file="$1"
    if [ -f "$test_file" ] && [ ! -x "$test_file" ]; then
        chmod +x "$test_file"
        echo "Made $test_file executable"
    fi
}

# Main test runner
main() {
    echo -e "${YELLOW}Waitlock External Test Suite${NC}"
    echo -e "${YELLOW}============================${NC}"
    echo ""
    
    # Check if waitlock binary exists
    if ! command -v waitlock >/dev/null 2>&1; then
        echo -e "${RED}Error: waitlock binary not found in PATH${NC}"
        echo "Please build waitlock first: make"
        exit 1
    fi
    
    # Get the directory containing this script
    cd "$TEST_DIR"
    
    # List of test files in order
    TEST_FILES=(
        "test_help_version.sh"
        "test_list.sh"
        "test_check.sh"
        "test_mutex.sh"
        "test_semaphore.sh"
        "test_timeout.sh"
        "test_done.sh"
        "test_exec.sh"
        "test_onepercpu.sh"
        "test_excludecpus.sh"
        "test_environment.sh"
        "test_scenarios.sh"
    )
    
    # Make all test files executable
    for test_file in "${TEST_FILES[@]}"; do
        make_executable "$test_file"
    done
    
    # Also make the framework executable
    make_executable "test_framework.sh"
    
    # Run all tests
    for test_file in "${TEST_FILES[@]}"; do
        if [ -f "$test_file" ]; then
            run_test_file "./$test_file"
        else
            echo -e "${RED}Warning: Test file $test_file not found${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            TOTAL_TESTS=$((TOTAL_TESTS + 1))
        fi
    done
    
    # Print summary
    echo -e "${YELLOW}Test Summary:${NC}"
    echo -e "Total tests: $TOTAL_TESTS"
    echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"
    
    if [ $FAILED_TESTS -gt 0 ]; then
        echo -e "${RED}Failed: $FAILED_TESTS${NC}"
        echo ""
        echo -e "${RED}Some tests failed. Please check the output above.${NC}"
        exit 1
    else
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    fi
}

# Allow running specific test files
if [ $# -gt 0 ]; then
    case "$1" in
        help|--help|-h)
            echo "Usage: $0 [test_name]"
            echo ""
            echo "Available tests:"
            for test_file in test_*.sh; do
                if [ -f "$test_file" ]; then
                    echo "  $(basename "$test_file" .sh)"
                fi
            done
            echo ""
            echo "Run without arguments to run all tests"
            exit 0
            ;;
        *)
            # Try to run specific test
            test_name="$1"
            if [[ ! "$test_name" =~ \.sh$ ]]; then
                test_name="${test_name}.sh"
            fi
            
            if [ -f "$test_name" ]; then
                make_executable "$test_name"
                run_test_file "./$test_name"
                
                if [ $FAILED_TESTS -gt 0 ]; then
                    exit 1
                else
                    exit 0
                fi
            else
                echo -e "${RED}Error: Test file $test_name not found${NC}"
                exit 1
            fi
            ;;
    esac
else
    main
fi