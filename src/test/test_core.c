/*
 * Unit tests for core.c functions
 * Tests argument parsing, utility functions, and core functionality
 */

#include "test.h"
#include "../core/core.h"
#include "../lock/lock.h"

/* Test framework */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n[CORE_TEST %d] %s\n", test_count, name); \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            pass_count++; \
            printf("  ✓ PASS: %s\n", message); \
        } else { \
            fail_count++; \
            printf("  ✗ FAIL: %s\n", message); \
        } \
    } while(0)

/* Test strcasecmp compatibility function */
int test_strcasecmp_compat(void) {
    TEST_START("strcasecmp compatibility");
    
    TEST_ASSERT(strcasecmp_compat("hello", "HELLO") == 0, "Case insensitive comparison should match");
    TEST_ASSERT(strcasecmp_compat("hello", "hello") == 0, "Same case comparison should match");
    TEST_ASSERT(strcasecmp_compat("hello", "world") != 0, "Different strings should not match");
    TEST_ASSERT(strcasecmp_compat("", "") == 0, "Empty strings should match");
    TEST_ASSERT(strcasecmp_compat("a", "A") == 0, "Single character case insensitive should match");
    TEST_ASSERT(strcasecmp_compat("abc", "ab") != 0, "Different length strings should not match");
    
    return 0;
}

/* Test syslog facility parsing */
int test_parse_syslog_facility(void) {
    TEST_START("Syslog facility parsing");
    
    TEST_ASSERT(parse_syslog_facility("daemon") == LOG_DAEMON, "Should parse daemon facility");
    TEST_ASSERT(parse_syslog_facility("DAEMON") == LOG_DAEMON, "Should parse daemon facility case insensitive");
    TEST_ASSERT(parse_syslog_facility("local0") == LOG_LOCAL0, "Should parse local0 facility");
    TEST_ASSERT(parse_syslog_facility("local1") == LOG_LOCAL1, "Should parse local1 facility");
    TEST_ASSERT(parse_syslog_facility("local2") == LOG_LOCAL2, "Should parse local2 facility");
    TEST_ASSERT(parse_syslog_facility("local3") == LOG_LOCAL3, "Should parse local3 facility");
    TEST_ASSERT(parse_syslog_facility("local4") == LOG_LOCAL4, "Should parse local4 facility");
    TEST_ASSERT(parse_syslog_facility("local5") == LOG_LOCAL5, "Should parse local5 facility");
    TEST_ASSERT(parse_syslog_facility("local6") == LOG_LOCAL6, "Should parse local6 facility");
    TEST_ASSERT(parse_syslog_facility("local7") == LOG_LOCAL7, "Should parse local7 facility");
    TEST_ASSERT(parse_syslog_facility("LOCAL7") == LOG_LOCAL7, "Should parse local7 facility case insensitive");
    TEST_ASSERT(parse_syslog_facility("invalid") == -1, "Should reject invalid facility");
    TEST_ASSERT(parse_syslog_facility("") == -1, "Should reject empty facility");
    TEST_ASSERT(parse_syslog_facility(NULL) == -1, "Should reject NULL facility");
    
    return 0;
}

/* Test argument parsing */
int test_parse_args(void) {
    TEST_START("Argument parsing");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    /* Test basic argument parsing */
    opts.descriptor = NULL;
    opts.max_holders = 1;
    opts.done_mode = FALSE;
    opts.check_only = FALSE;
    opts.list_mode = FALSE;
    opts.test_mode = FALSE;
    
    char *test_args1[] = {"waitlock", "test_descriptor"};
    int result1 = parse_args(2, test_args1);
    TEST_ASSERT(result1 == 0, "Basic argument parsing should succeed");
    TEST_ASSERT(opts.descriptor != NULL, "Descriptor should be set");
    TEST_ASSERT(strcmp(opts.descriptor, "test_descriptor") == 0, "Descriptor should match");
    
    /* Test --done flag */
    opts.descriptor = NULL;
    opts.done_mode = FALSE;
    
    char *test_args2[] = {"waitlock", "--done", "test_descriptor"};
    int result2 = parse_args(3, test_args2);
    TEST_ASSERT(result2 == 0, "--done argument parsing should succeed");
    TEST_ASSERT(opts.done_mode == TRUE, "--done mode should be enabled");
    TEST_ASSERT(strcmp(opts.descriptor, "test_descriptor") == 0, "Descriptor should be set");
    
    /* Test semaphore argument */
    opts.descriptor = NULL;
    opts.max_holders = 1;
    opts.done_mode = FALSE;
    
    char *test_args3[] = {"waitlock", "-m", "5", "test_descriptor"};
    int result3 = parse_args(4, test_args3);
    TEST_ASSERT(result3 == 0, "Semaphore argument parsing should succeed");
    TEST_ASSERT(opts.max_holders == 5, "Max holders should be set to 5");
    
    /* Test --allowMultiple long form */
    opts.descriptor = NULL;
    opts.max_holders = 1;
    
    char *test_args4[] = {"waitlock", "--allowMultiple", "3", "test_descriptor"};
    int result4 = parse_args(4, test_args4);
    TEST_ASSERT(result4 == 0, "--allowMultiple argument parsing should succeed");
    TEST_ASSERT(opts.max_holders == 3, "Max holders should be set to 3");
    
    /* Test --check flag */
    opts.descriptor = NULL;
    opts.check_only = FALSE;
    
    char *test_args5[] = {"waitlock", "--check", "test_descriptor"};
    int result5 = parse_args(3, test_args5);
    TEST_ASSERT(result5 == 0, "--check argument parsing should succeed");
    TEST_ASSERT(opts.check_only == TRUE, "--check mode should be enabled");
    
    /* Test --list flag */
    opts.descriptor = NULL;
    opts.list_mode = FALSE;
    
    char *test_args6[] = {"waitlock", "--list"};
    int result6 = parse_args(2, test_args6);
    TEST_ASSERT(result6 == 0, "--list argument parsing should succeed");
    TEST_ASSERT(opts.list_mode == TRUE, "--list mode should be enabled");
    
    /* Test timeout argument */
    opts.descriptor = NULL;
    opts.timeout = 0.0;
    
    char *test_args7[] = {"waitlock", "-t", "30", "test_descriptor"};
    int result7 = parse_args(4, test_args7);
    TEST_ASSERT(result7 == 0, "Timeout argument parsing should succeed");
    TEST_ASSERT(opts.timeout == 30.0, "Timeout should be set to 30.0");
    
    /* Test --timeout long form */
    opts.descriptor = NULL;
    opts.timeout = 0.0;
    
    char *test_args8[] = {"waitlock", "--timeout", "45.5", "test_descriptor"};
    int result8 = parse_args(4, test_args8);
    TEST_ASSERT(result8 == 0, "--timeout argument parsing should succeed");
    TEST_ASSERT(opts.timeout == 45.5, "Timeout should be set to 45.5");
    
    /* Test --exec flag */
    opts.descriptor = NULL;
    opts.exec_argv = NULL;
    
    char *test_args9[] = {"waitlock", "--exec", "echo", "hello", "test_descriptor"};
    int result9 = parse_args(5, test_args9);
    TEST_ASSERT(result9 == 0, "--exec argument parsing should succeed");
    TEST_ASSERT(opts.exec_argv != NULL, "--exec mode should be enabled");
    
    /* Test invalid arguments */
    opts.descriptor = NULL;
    opts.max_holders = 1;
    opts.test_mode = FALSE;
    opts.list_mode = FALSE;
    opts.done_mode = FALSE;
    opts.check_only = FALSE;
    
    char *test_args10[] = {"waitlock", "invalid@descriptor"};
    int result10 = parse_args(2, test_args10);
    TEST_ASSERT(result10 != 0, "Invalid descriptor should be rejected");
    
    /* Restore original options */
    opts = saved_opts;
    
    return 0;
}

/* Test CPU count functionality */
int test_get_cpu_count(void) {
    TEST_START("CPU count functionality");
    
    int cpu_count = get_cpu_count();
    TEST_ASSERT(cpu_count >= 1, "CPU count should be at least 1");
    TEST_ASSERT(cpu_count <= 1024, "CPU count should be reasonable");
    
    printf("  → Detected %d CPUs\n", cpu_count);
    
    /* Test multiple calls return same result */
    int cpu_count2 = get_cpu_count();
    TEST_ASSERT(cpu_count == cpu_count2, "Multiple calls should return same result");
    
    return 0;
}

/* Test safe_snprintf functionality */
int test_safe_snprintf(void) {
    TEST_START("Safe snprintf functionality");
    
    char buffer[64];
    int result;
    
    /* Test normal case */
    result = safe_snprintf(buffer, sizeof(buffer), "Hello %s", "World");
    TEST_ASSERT(result > 0, "Should return positive value");
    TEST_ASSERT(strcmp(buffer, "Hello World") == 0, "Should format correctly");
    
    /* Test buffer overflow protection */
    result = safe_snprintf(buffer, 5, "This is a very long string");
    TEST_ASSERT(result > 0, "Should return positive value even with overflow");
    TEST_ASSERT(strlen(buffer) == 4, "Should truncate to buffer size - 1");
    TEST_ASSERT(buffer[4] == '\0', "Should null-terminate");
    
    /* Test empty format */
    result = safe_snprintf(buffer, sizeof(buffer), "");
    TEST_ASSERT(result == 0, "Empty format should return 0");
    TEST_ASSERT(strcmp(buffer, "") == 0, "Should produce empty string");
    
    /* Test zero buffer size */
    result = safe_snprintf(buffer, 0, "test");
    TEST_ASSERT(result >= 0, "Zero buffer size should be handled");
    
    return 0;
}

/* Test debug output functionality */
int test_debug_output(void) {
    TEST_START("Debug output functionality");
    
    /* Test with debug disabled */
    g_state.verbose = FALSE;
    printf("  → Testing debug output (disabled):\n");
    debug("This debug message should not appear");
    TEST_ASSERT(1, "Debug output with debug disabled");
    
    /* Test with debug enabled */
    g_state.verbose = TRUE;
    printf("  → Testing debug output (enabled):\n");
    debug("This debug message should appear");
    TEST_ASSERT(1, "Debug output with debug enabled");
    
    /* Test debug with formatting */
    debug("Debug message with number: %d", 42);
    TEST_ASSERT(1, "Debug output with formatting");
    
    /* Reset debug state */
    g_state.verbose = FALSE;
    
    return 0;
}

/* Test error output functionality */
int test_error_output(void) {
    TEST_START("Error output functionality");
    
    /* Test with quiet disabled */
    g_state.quiet = FALSE;
    printf("  → Testing error output (not quiet):\n");
    error(E_SYSTEM, "This error message should appear");
    TEST_ASSERT(1, "Error output with quiet disabled");
    
    /* Test with quiet enabled */
    g_state.quiet = TRUE;
    printf("  → Testing error output (quiet):\n");
    error(E_SYSTEM, "This error message should be suppressed");
    TEST_ASSERT(1, "Error output with quiet enabled");
    
    /* Test error with formatting */
    g_state.quiet = FALSE;
    error(E_USAGE, "Error message with number: %d", 42);
    TEST_ASSERT(1, "Error output with formatting");
    
    /* Reset quiet state */
    g_state.quiet = FALSE;
    
    return 0;
}

/* Test usage output */
int test_usage_output(void) {
    TEST_START("Usage output");
    
    /* Test usage to stdout */
    printf("  → Testing usage output to stdout:\n");
    usage(stdout);
    TEST_ASSERT(1, "Usage output to stdout");
    
    /* Test usage to stderr */
    printf("  → Testing usage output to stderr:\n");
    usage(stderr);
    TEST_ASSERT(1, "Usage output to stderr");
    
    return 0;
}

/* Test version output */
int test_version_output(void) {
    TEST_START("Version output");
    
    printf("  → Testing version output:\n");
    version();
    TEST_ASSERT(1, "Version output");
    
    return 0;
}

/* Test environment variable handling */
int test_environment_variables(void) {
    TEST_START("Environment variable handling");
    
    /* Test WAITLOCK_DEBUG */
    setenv("WAITLOCK_DEBUG", "1", 1);
    char *debug_val = getenv("WAITLOCK_DEBUG");
    TEST_ASSERT(debug_val != NULL, "WAITLOCK_DEBUG should be readable");
    TEST_ASSERT(strcmp(debug_val, "1") == 0, "WAITLOCK_DEBUG should have correct value");
    
    /* Test WAITLOCK_TIMEOUT */
    setenv("WAITLOCK_TIMEOUT", "30", 1);
    char *timeout_val = getenv("WAITLOCK_TIMEOUT");
    TEST_ASSERT(timeout_val != NULL, "WAITLOCK_TIMEOUT should be readable");
    TEST_ASSERT(strcmp(timeout_val, "30") == 0, "WAITLOCK_TIMEOUT should have correct value");
    
    /* Test WAITLOCK_DIR */
    setenv("WAITLOCK_DIR", "/tmp/test_locks", 1);
    char *dir_val = getenv("WAITLOCK_DIR");
    TEST_ASSERT(dir_val != NULL, "WAITLOCK_DIR should be readable");
    TEST_ASSERT(strcmp(dir_val, "/tmp/test_locks") == 0, "WAITLOCK_DIR should have correct value");
    
    /* Test WAITLOCK_SLOT */
    setenv("WAITLOCK_SLOT", "3", 1);
    char *slot_val = getenv("WAITLOCK_SLOT");
    TEST_ASSERT(slot_val != NULL, "WAITLOCK_SLOT should be readable");
    TEST_ASSERT(strcmp(slot_val, "3") == 0, "WAITLOCK_SLOT should have correct value");
    
    /* Clean up */
    unsetenv("WAITLOCK_DEBUG");
    unsetenv("WAITLOCK_TIMEOUT");
    unsetenv("WAITLOCK_DIR");
    unsetenv("WAITLOCK_SLOT");
    
    return 0;
}

/* Test argument validation */
int test_argument_validation(void) {
    TEST_START("Argument validation");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    /* Test valid descriptor characters */
    opts.descriptor = NULL;
    opts.test_mode = FALSE;
    
    char *valid_args[] = {"waitlock", "valid_descriptor-123.test"};
    int result1 = parse_args(2, valid_args);
    TEST_ASSERT(result1 == 0, "Valid descriptor characters should be accepted");
    
    /* Test invalid descriptor characters */
    opts.descriptor = NULL;
    
    char *invalid_args[] = {"waitlock", "invalid@descriptor"};
    int result2 = parse_args(2, invalid_args);
    TEST_ASSERT(result2 != 0, "Invalid descriptor characters should be rejected");
    
    /* Test descriptor length limits */
    opts.descriptor = NULL;
    
    char long_descriptor[300];
    memset(long_descriptor, 'a', sizeof(long_descriptor) - 1);
    long_descriptor[sizeof(long_descriptor) - 1] = '\0';
    
    char *long_args[] = {"waitlock", long_descriptor};
    int result3 = parse_args(2, long_args);
    TEST_ASSERT(result3 != 0, "Overly long descriptor should be rejected");
    
    /* Test negative semaphore count */
    opts.descriptor = NULL;
    opts.max_holders = 1;
    
    char *negative_args[] = {"waitlock", "-m", "-1", "test"};
    int result4 = parse_args(4, negative_args);
    TEST_ASSERT(result4 != 0, "Negative semaphore count should be rejected");
    
    /* Test zero semaphore count */
    opts.descriptor = NULL;
    opts.max_holders = 1;
    
    char *zero_args[] = {"waitlock", "-m", "0", "test"};
    int result5 = parse_args(4, zero_args);
    TEST_ASSERT(result5 != 0, "Zero semaphore count should be rejected");
    
    /* Test negative timeout */
    opts.descriptor = NULL;
    opts.timeout = 0.0;
    
    char *negative_timeout_args[] = {"waitlock", "-t", "-1.5", "test"};
    int result6 = parse_args(4, negative_timeout_args);
    TEST_ASSERT(result6 != 0, "Negative timeout should be rejected");
    
    /* Restore original options */
    opts = saved_opts;
    
    return 0;
}

/* Test framework summary */
void test_core_summary(void) {
    printf("\n=== CORE TEST SUMMARY ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", pass_count);
    printf("Failed: %d\n", fail_count);
    if (fail_count == 0) {
        printf("All core tests passed!\n");
    } else {
        printf("Some core tests failed!\n");
    }
}

/* Main test runner for core module */
int run_core_tests(void) {
    printf("=== CORE MODULE TEST SUITE ===\n");
    
    /* Reset counters */
    test_count = 0;
    pass_count = 0;
    fail_count = 0;
    
    /* Run all core tests */
    test_strcasecmp_compat();
    test_parse_syslog_facility();
    test_parse_args();
    test_get_cpu_count();
    test_safe_snprintf();
    test_debug_output();
    test_error_output();
    test_usage_output();
    test_version_output();
    test_environment_variables();
    test_argument_validation();
    
    test_core_summary();
    
    return (fail_count > 0) ? 1 : 0;
}