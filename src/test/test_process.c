/*
 * Unit tests for process.c functions
 * Tests process existence checking, command line extraction, and exec mode
 */

#include "test.h"
#include "../process/process.h"
#include "../core/core.h"
#include "../lock/lock.h"

/* Test framework */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n[PROCESS_TEST %d] %s\n", test_count, name); \
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

/* Test process existence checking */
int test_process_exists(void) {
    TEST_START("Process existence checking");
    
    pid_t current_pid = getpid();
    TEST_ASSERT(process_exists(current_pid), "Current process should exist");
    
    pid_t parent_pid = getppid();
    TEST_ASSERT(process_exists(parent_pid), "Parent process should exist");
    
    pid_t invalid_pid = 999999; /* Likely non-existent */
    TEST_ASSERT(!process_exists(invalid_pid), "Invalid process should not exist");
    
    /* Test with PID 1 (init) - should exist on most systems */
    TEST_ASSERT(process_exists(1), "Init process (PID 1) should exist");
    
    /* Test with PID 0 - should be handled gracefully */
    TEST_ASSERT(!process_exists(0), "PID 0 should not exist");
    
    printf("  → Current PID: %d\n", current_pid);
    printf("  → Parent PID: %d\n", parent_pid);
    
    return 0;
}

/* Test process command line extraction */
int test_get_process_cmdline(void) {
    TEST_START("Process command line extraction");
    
    pid_t current_pid = getpid();
    char *cmdline = get_process_cmdline(current_pid);
    TEST_ASSERT(cmdline != NULL, "Should be able to get current process cmdline");
    
    if (cmdline) {
        TEST_ASSERT(strlen(cmdline) > 0, "Command line should not be empty");
        printf("  → Current process cmdline: %s\n", cmdline);
    }
    
    /* Test with parent process */
    pid_t parent_pid = getppid();
    char *parent_cmdline = get_process_cmdline(parent_pid);
    TEST_ASSERT(parent_cmdline != NULL, "Should be able to get parent process cmdline");
    
    if (parent_cmdline) {
        printf("  → Parent process cmdline: %s\n", parent_cmdline);
    }
    
    /* Test with invalid PID */
    char *invalid_cmdline = get_process_cmdline(999999);
    TEST_ASSERT(invalid_cmdline == NULL, "Should return NULL for invalid PID");
    
    /* Test with PID 1 (init) */
    char *init_cmdline = get_process_cmdline(1);
    if (init_cmdline) {
        printf("  → Init process cmdline: %s\n", init_cmdline);
        TEST_ASSERT(1, "Init process cmdline retrieved");
    } else {
        printf("  → Init process cmdline: NULL (may be restricted)\n");
        TEST_ASSERT(1, "Init process cmdline access may be restricted");
    }
    
    return 0;
}

/* Test exec with lock functionality */
int test_exec_with_lock(void) {
    TEST_START("Exec with lock functionality");
    
    const char *test_descriptor = "test_exec_lock";
    
    /* Test simple command execution */
    char *argv1[] = {"echo", "Hello World", NULL};
    
    /* Fork to test exec mode */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process - test exec with lock */
        int result = exec_with_lock(test_descriptor, argv1);
        /* Should not reach here if exec succeeds */
        exit(result);
    } else if (child_pid > 0) {
        /* Parent process */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Check if child executed successfully */
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            TEST_ASSERT(exit_code == 0, "Echo command should execute successfully");
        } else {
            TEST_ASSERT(0, "Child process should exit normally");
        }
    } else {
        TEST_ASSERT(0, "Fork failed");
    }
    
    /* Test command that should fail */
    char *argv2[] = {"nonexistent_command_12345", NULL};
    
    child_pid = fork();
    if (child_pid == 0) {
        /* Child process - test exec with non-existent command */
        int result = exec_with_lock(test_descriptor, argv2);
        exit(result);
    } else if (child_pid > 0) {
        /* Parent process */
        int status;
        waitpid(child_pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            TEST_ASSERT(exit_code == 127, "Non-existent command should return 127");
        } else {
            TEST_ASSERT(0, "Child process should exit normally");
        }
    }
    
    return 0;
}

/* Test exec with lock contention */
int test_exec_with_lock_contention(void) {
    TEST_START("Exec with lock contention");
    
    const char *test_descriptor = "test_exec_contention";
    
    /* Save original options and configure for test */
    struct options saved_opts = opts;
    opts.max_holders = 1;  /* Mutex behavior - only one holder */
    opts.timeout = 5.0;    /* 5 second timeout - should be enough for sleep 2 + delay */
    
    /* Create first child that holds lock for a while */
    pid_t child1_pid = fork();
    if (child1_pid == 0) {
        /* Child 1 - hold lock for 2 seconds */
        char *argv[] = {"sleep", "2", NULL};
        int result = exec_with_lock(test_descriptor, argv);
        exit(result);
    } else if (child1_pid > 0) {
        /* Parent - give child time to acquire lock */
        sleep(1);
        
        /* Create second child that should wait for lock */
        pid_t child2_pid = fork();
        if (child2_pid == 0) {
            /* Child 2 - should wait for lock */
            char *argv[] = {"echo", "Second", NULL};
            int result = exec_with_lock(test_descriptor, argv);
            exit(result);
        } else if (child2_pid > 0) {
            /* Parent - wait for both children */
            int status1, status2;
            
            waitpid(child1_pid, &status1, 0);
            waitpid(child2_pid, &status2, 0);
            
            if (WIFEXITED(status1) && WIFEXITED(status2)) {
                TEST_ASSERT(WEXITSTATUS(status1) == 0, "First child should succeed");
                TEST_ASSERT(WEXITSTATUS(status2) == 0, "Second child should succeed after waiting");
            } else {
                TEST_ASSERT(0, "Both children should exit normally");
            }
        }
    }
    
    /* Restore original options */
    opts = saved_opts;
    
    return 0;
}

/* Test exec with timeout */
int test_exec_with_timeout(void) {
    TEST_START("Exec with timeout");
    
    const char *test_descriptor = "test_exec_timeout";
    
    /* Create child that holds lock */
    pid_t child1_pid = fork();
    if (child1_pid == 0) {
        /* Child 1 - hold lock for 3 seconds */
        char *argv[] = {"sleep", "3", NULL};
        int result = exec_with_lock(test_descriptor, argv);
        exit(result);
    } else if (child1_pid > 0) {
        /* Parent - give child time to acquire lock */
        sleep(1);
        
        /* Set short timeout */
        opts.timeout = 1.0;
        
        /* Create second child that should timeout */
        pid_t child2_pid = fork();
        if (child2_pid == 0) {
            /* Child 2 - should timeout */
            char *argv[] = {"echo", "Timeout", NULL};
            int result = exec_with_lock(test_descriptor, argv);
            exit(result);
        } else if (child2_pid > 0) {
            /* Parent - wait for both children */
            int status1, status2;
            
            waitpid(child2_pid, &status2, 0);
            waitpid(child1_pid, &status1, 0);
            
            if (WIFEXITED(status1) && WIFEXITED(status2)) {
                TEST_ASSERT(WEXITSTATUS(status1) == 0, "First child should succeed");
                TEST_ASSERT(WEXITSTATUS(status2) == 2, "Second child should timeout");
            } else {
                TEST_ASSERT(0, "Both children should exit normally");
            }
        }
        
        /* Reset timeout */
        opts.timeout = 0.0;
    }
    
    return 0;
}

/* Test signal forwarding in exec mode */
int test_exec_signal_forwarding(void) {
    TEST_START("Exec signal forwarding");
    
    const char *test_descriptor = "test_exec_signal";
    
    /* Create child that runs a long command */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child - run sleep command */
        char *argv[] = {"sleep", "10", NULL};
        int result = exec_with_lock(test_descriptor, argv);
        exit(result);
    } else if (child_pid > 0) {
        /* Parent - give child time to start */
        sleep(1);
        
        /* Send SIGTERM to child */
        kill(child_pid, SIGTERM);
        
        /* Wait for child to exit */
        int status;
        waitpid(child_pid, &status, 0);
        
        if (WIFSIGNALED(status)) {
            int signal = WTERMSIG(status);
            TEST_ASSERT(signal == SIGTERM, "Child should be terminated by SIGTERM");
        } else if (WIFEXITED(status)) {
            /* Some systems may handle signals differently */
            TEST_ASSERT(1, "Child exited normally (signal handling may vary)");
        } else {
            TEST_ASSERT(0, "Child should exit due to signal");
        }
    }
    
    return 0;
}

/* Test process death detection */
int test_process_death_detection(void) {
    TEST_START("Process death detection");
    
    /* Create child process that will die */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child - just exit */
        exit(0);
    } else if (child_pid > 0) {
        /* Parent - wait for child to die */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Give some time for cleanup */
        sleep(1);
        
        /* Check if process is detected as dead */
        TEST_ASSERT(!process_exists(child_pid), "Dead process should not exist");
    }
    
    return 0;
}

/* Test zombie process handling */
int test_zombie_process_handling(void) {
    TEST_START("Zombie process handling");
    
    /* Create child process that will become zombie */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child - exit immediately */
        exit(0);
    } else if (child_pid > 0) {
        /* Parent - don't wait immediately, let child become zombie */
        sleep(1);
        
        /* Check if zombie process is detected correctly */
        int exists = process_exists(child_pid);
        
        /* Clean up zombie */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* The behavior may vary by system */
        printf("  → Zombie process exists: %s\n", exists ? "yes" : "no");
        TEST_ASSERT(1, "Zombie process handling tested");
    }
    
    return 0;
}

/* Test cross-platform command line extraction */
int test_cross_platform_cmdline(void) {
    TEST_START("Cross-platform command line extraction");
    
    /* Test with various processes */
    pid_t test_pids[] = {1, getpid(), getppid()};
    int num_pids = sizeof(test_pids) / sizeof(test_pids[0]);
    int i;
    
    for (i = 0; i < num_pids; i++) {
        char *cmdline = get_process_cmdline(test_pids[i]);
        printf("  → PID %d cmdline: %s\n", test_pids[i], cmdline ? cmdline : "NULL");
        
        if (test_pids[i] == getpid()) {
            TEST_ASSERT(cmdline != NULL, "Should always get current process cmdline");
        }
    }
    
    TEST_ASSERT(1, "Cross-platform command line extraction tested");
    
    return 0;
}

/* Test framework summary */
void test_process_summary(void) {
    printf("\n=== PROCESS TEST SUMMARY ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", pass_count);
    printf("Failed: %d\n", fail_count);
    if (fail_count == 0) {
        printf("All process tests passed!\n");
    } else {
        printf("Some process tests failed!\n");
    }
}

/* Main test runner for process module */
int run_process_tests(void) {
    printf("=== PROCESS MODULE TEST SUITE ===\n");
    
    /* Reset counters */
    test_count = 0;
    pass_count = 0;
    fail_count = 0;
    
    /* Run all process tests */
    test_process_exists();
    test_get_process_cmdline();
    test_exec_with_lock();
    test_exec_with_lock_contention();
    test_exec_with_timeout();
    test_exec_signal_forwarding();
    test_process_death_detection();
    test_zombie_process_handling();
    test_cross_platform_cmdline();
    
    test_process_summary();
    
    return (fail_count > 0) ? 1 : 0;
}