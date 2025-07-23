/*
 * Unit tests for waitlock - Main test runner
 * This file coordinates running all modular test suites
 */

#include "../waitlock.h"
#include "test.h"
#include "test_framework.h"

/* External test suite functions */
extern int run_checksum_tests(void);
extern int run_core_tests(void);
extern int run_process_coordinator_tests(void);
extern int run_lock_tests(void);
extern int run_process_tests(void);
extern int run_signal_tests(void);
extern int run_integration_tests(void);

/* Test results tracking */
static int total_tests = 0;
static int passed_tests = 0;
static int failed_tests = 0;

/* Test suite runner */
static int run_test_suite(const char *suite_name, int (*test_func)(void)) {
    printf("\n============================================================\n");
    printf("Running %s test suite...\n", suite_name);
    printf("============================================================\n");
    
    int result = test_func();
    
    if (result == 0) {
        printf("✓ %s test suite: PASSED\n", suite_name);
        passed_tests++;
    } else {
        printf("✗ %s test suite: FAILED\n", suite_name);
        failed_tests++;
    }
    
    total_tests++;
    return result;
}

/* Main test runner */
int run_unit_tests(void) {
    printf("============================================================\n");
    printf("                 WAITLOCK UNIT TEST SUITE\n");
    printf("============================================================\n");
    
    /* Clean up any leftover test artifacts from previous runs */
    test_cleanup_global();
    
    /* Run all test suites with cleanup between each */
    run_test_suite("Checksum", run_checksum_tests);
    test_cleanup_between_suites();
    
    run_test_suite("Core", run_core_tests);
    test_cleanup_between_suites();
    
    run_test_suite("ProcessCoordinator", run_process_coordinator_tests);
    test_cleanup_between_suites();
    
    run_test_suite("Process", run_process_tests);
    test_cleanup_between_suites();
    
    run_test_suite("Signal", run_signal_tests);
    test_cleanup_between_suites();
    
    run_test_suite("Lock", run_lock_tests);
    test_cleanup_between_suites();
    
    run_test_suite("Integration", run_integration_tests);
    
    /* Print final summary */
    printf("\n============================================================\n");
    printf("                    TEST SUMMARY\n");
    printf("============================================================\n");
    printf("Total test suites: %d\n", total_tests);
    printf("Passed: %d\n", passed_tests);
    printf("Failed: %d\n", failed_tests);
    
    if (failed_tests > 0) {
        printf("\n✗ OVERALL RESULT: FAILED\n");
        printf("============================================================\n");
        return 1;
    } else {
        printf("\n✓ OVERALL RESULT: PASSED\n");
        printf("============================================================\n");
        return 0;
    }
}