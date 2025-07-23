/*
 * Integration tests for waitlock
 * Tests combined functionality that happens at the main() level
 * and end-to-end scenarios
 */

#include "test.h"
#include "../core/core.h"
#include "../lock/lock.h"
#include "../process/process.h"
#include "../signal/signal.h"
#include "../checksum/checksum.h"
#include "../process_coordinator/process_coordinator.h"

/* Test framework */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n[INTEGRATION_TEST %d] %s\n", test_count, name); \
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

/* Test end-to-end mutex workflow */
int test_end_to_end_mutex(void) {
    TEST_START("End-to-end mutex workflow");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    /* Set up for mutex mode */
    opts.descriptor = "test_e2e_mutex";
    opts.max_holders = 1;
    opts.timeout = 5.0;
    opts.check_only = FALSE;
    opts.list_mode = FALSE;
    opts.done_mode = FALSE;
    opts.exec_argv = FALSE;
    
    /* Test 1: Acquire mutex lock */
    int acquire_result = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
    TEST_ASSERT(acquire_result == 0, "Should successfully acquire mutex lock");
    
    /* Test 2: Check that lock is held */
    int check_result = check_lock(opts.descriptor);
    TEST_ASSERT(check_result != 0, "Lock should be held after acquisition");
    
    /* Test 3: Try to acquire same lock from child process (should fail) */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        int child_result = acquire_lock(opts.descriptor, opts.max_holders, 1.0);
        exit(child_result == 0 ? 0 : 1);
    } else if (child_pid > 0) {
        /* Parent process */
        int status;
        waitpid(child_pid, &status, 0);
        if (WIFEXITED(status)) {
            TEST_ASSERT(WEXITSTATUS(status) == 1, "Child should fail to acquire held mutex");
        }
    }
    
    /* Test 4: Release lock */
    release_lock();
    
    /* Test 5: Check that lock is released */
    int check_result2 = check_lock(opts.descriptor);
    TEST_ASSERT(check_result2 == 0, "Lock should be available after release");
    
    /* Restore options */
    opts = saved_opts;
    
    return 0;
}

/* Test end-to-end semaphore workflow */
int test_end_to_end_semaphore(void) {
    TEST_START("End-to-end semaphore workflow");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    /* Set up for semaphore mode */
    opts.descriptor = "test_e2e_semaphore";
    opts.max_holders = 3;
    opts.timeout = 5.0;
    opts.check_only = FALSE;
    opts.list_mode = FALSE;
    opts.done_mode = FALSE;
    opts.exec_argv = FALSE;
    
    /* Test 1: Acquire first semaphore slot */
    int acquire_result1 = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
    TEST_ASSERT(acquire_result1 == 0, "Should successfully acquire first semaphore slot");
    
    /* Test 2: Acquire second and third slots from child processes */
    pid_t child_pids[2];
    int i;
    
    for (i = 0; i < 2; i++) {
        child_pids[i] = fork();
        if (child_pids[i] == 0) {
            /* Child process */
            int child_result = acquire_lock(opts.descriptor, opts.max_holders, 2.0);
            if (child_result == 0) {
                /* Hold the slot for a bit */
                sleep(1);
                release_lock();
                exit(0);
            }
            exit(1);
        }
    }
    
    /* Test 3: Try to acquire fourth slot (should fail) */
    sleep(1); /* Give children time to acquire slots */
    
    pid_t fourth_child = fork();
    if (fourth_child == 0) {
        /* Child process */
        int child_result = acquire_lock(opts.descriptor, opts.max_holders, 1.0);
        exit(child_result == 0 ? 0 : 1);
    } else if (fourth_child > 0) {
        /* Parent process */
        int status;
        waitpid(fourth_child, &status, 0);
        if (WIFEXITED(status)) {
            TEST_ASSERT(WEXITSTATUS(status) == 1, "Fourth slot should not be available");
        }
    }
    
    /* Test 4: Release parent's slot */
    release_lock();
    
    /* Test 5: Wait for children to complete */
    for (i = 0; i < 2; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
        if (WIFEXITED(status)) {
            TEST_ASSERT(WEXITSTATUS(status) == 0, "Child should successfully acquire and release slot");
        }
    }
    
    /* Test 6: Check that all slots are released */
    sleep(1);
    int check_result = check_lock(opts.descriptor);
    TEST_ASSERT(check_result == 0, "All semaphore slots should be available");
    
    /* Restore options */
    opts = saved_opts;
    
    return 0;
}

/* Test end-to-end done workflow */
int test_end_to_end_done(void) {
    TEST_START("End-to-end done workflow");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    const char *test_descriptor = "test_e2e_done";
    
    /* Test 1: Create child process that holds lock */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        install_signal_handlers(); /* Install signal handlers to respond to done_lock */
        
        opts.descriptor = test_descriptor;
        opts.max_holders = 1;
        opts.timeout = 5.0;
        
        int acquire_result = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
        if (acquire_result == 0) {
            /* Wait for done signal */
            while (1) {
                sleep(1);
            }
        }
        exit(1);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to acquire lock */
        
        /* Test 2: Verify lock is held */
        int check_result = check_lock(test_descriptor);
        TEST_ASSERT(check_result != 0, "Lock should be held by child");
        
        /* Test 3: Send done signal */
        opts.descriptor = test_descriptor;
        opts.done_mode = TRUE;
        
        int done_result = done_lock(opts.descriptor);
        TEST_ASSERT(done_result == 0, "Done signal should succeed");
        
        /* Test 4: Wait for child to exit */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Test 5: Verify lock is released */
        sleep(1);
        int check_result2 = check_lock(test_descriptor);
        TEST_ASSERT(check_result2 == 0, "Lock should be released after done signal");
    }
    
    /* Restore options */
    opts = saved_opts;
    
    return 0;
}

/* Test end-to-end exec workflow */
int test_end_to_end_exec(void) {
    TEST_START("End-to-end exec workflow");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    const char *test_descriptor = "test_e2e_exec";
    
    /* Test 1: Execute command with lock */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        opts.descriptor = test_descriptor;
        opts.max_holders = 1;
        opts.timeout = 5.0; /* FIX: Set proper timeout before calling exec_with_lock */
        char *argv[] = {"echo", "Hello from exec", NULL};
        opts.exec_argv = argv;
        
        int result = exec_with_lock(opts.descriptor, argv);
        exit(result);
    } else if (child_pid > 0) {
        /* Parent process */
        int status;
        waitpid(child_pid, &status, 0);
        
        if (WIFEXITED(status)) {
            TEST_ASSERT(WEXITSTATUS(status) == 0, "Exec mode should succeed");
        }
        
        /* Test 2: Verify lock is released after exec completes */
        sleep(1);
        int check_result = check_lock(test_descriptor);
        TEST_ASSERT(check_result == 0, "Lock should be released after exec completion");
    }
    
    /* Restore options */
    opts = saved_opts;
    
    return 0;
}

/* Test end-to-end timeout workflow */
int test_end_to_end_timeout(void) {
    TEST_START("End-to-end timeout workflow");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    const char *test_descriptor = "test_e2e_timeout";
    
    /* Test 1: Create child process that holds lock */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        opts.descriptor = test_descriptor;
        opts.max_holders = 1;
        opts.timeout = 5.0;
        
        int acquire_result = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
        if (acquire_result == 0) {
            /* Hold lock for 3 seconds */
            sleep(3);
            release_lock();
        }
        exit(0);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to acquire lock */
        
        /* Test 2: Try to acquire with timeout */
        opts.descriptor = test_descriptor;
        opts.max_holders = 1;
        opts.timeout = 1.0;
        
        time_t start_time = time(NULL);
        int timeout_result = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
        time_t end_time = time(NULL);
        
        TEST_ASSERT(timeout_result != 0, "Should timeout waiting for lock");
        TEST_ASSERT((end_time - start_time) >= 1, "Should respect timeout duration");
        TEST_ASSERT((end_time - start_time) <= 2, "Should not wait too long");
        
        /* Test 3: Wait for child to finish */
        int status;
        waitpid(child_pid, &status, 0);
        
        if (WIFEXITED(status)) {
            TEST_ASSERT(WEXITSTATUS(status) == 0, "Child should complete successfully");
        }
        
        /* Test 4: Verify lock is available after child releases it */
        sleep(1);
        int check_result = check_lock(test_descriptor);
        TEST_ASSERT(check_result == 0, "Lock should be available after child releases");
    }
    
    /* Restore options */
    opts = saved_opts;
    
    return 0;
}

/* Test end-to-end check workflow */
int test_end_to_end_check(void) {
    TEST_START("End-to-end check workflow");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    const char *test_descriptor = "test_e2e_check";
    
    /* Test 1: Check non-existent lock */
    opts.descriptor = test_descriptor;
    opts.check_only = TRUE;
    
    int check_result1 = check_lock(opts.descriptor);
    TEST_ASSERT(check_result1 == 0, "Non-existent lock should be available");
    
    /* Test 2: Create child process that holds lock */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        opts.descriptor = test_descriptor;
        opts.max_holders = 1;
        opts.timeout = 5.0;
        opts.check_only = FALSE;
        
        int acquire_result = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
        if (acquire_result == 0) {
            /* Hold lock for 2 seconds */
            sleep(2);
            release_lock();
        }
        exit(0);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to acquire lock */
        
        /* Test 3: Check held lock */
        int check_result2 = check_lock(opts.descriptor);
        TEST_ASSERT(check_result2 != 0, "Held lock should not be available");
        
        /* Test 4: Wait for child to finish */
        int status;
        waitpid(child_pid, &status, 0);
        
        if (WIFEXITED(status)) {
            TEST_ASSERT(WEXITSTATUS(status) == 0, "Child should complete successfully");
        }
        
        /* Test 5: Check released lock */
        sleep(1);
        int check_result3 = check_lock(opts.descriptor);
        TEST_ASSERT(check_result3 == 0, "Released lock should be available");
    }
    
    /* Restore options */
    opts = saved_opts;
    
    return 0;
}

/* Test end-to-end list workflow */
int test_end_to_end_list(void) {
    TEST_START("End-to-end list workflow");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    const char *test_descriptor = "test_e2e_list";
    
    /* Test 1: List when no locks exist */
    opts.list_mode = TRUE;
    opts.output_format = FMT_HUMAN;
    opts.show_all = FALSE;
    opts.stale_only = FALSE;
    
    printf("  → Testing list with no locks:\n");
    list_locks(opts.output_format, opts.show_all, opts.stale_only);
    TEST_ASSERT(1, "List with no locks completed");
    
    /* Test 2: Create child process that holds lock */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        opts.descriptor = test_descriptor;
        opts.max_holders = 1;
        opts.timeout = 5.0;
        opts.list_mode = FALSE;
        
        int acquire_result = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
        if (acquire_result == 0) {
            /* Hold lock for 3 seconds */
            sleep(3);
            release_lock();
        }
        exit(0);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to acquire lock */
        
        /* Test 3: List with active lock */
        printf("  → Testing list with active lock:\n");
        list_locks(FMT_HUMAN, FALSE, FALSE);
        TEST_ASSERT(1, "List with active lock completed");
        
        /* Test 4: List in CSV format */
        printf("  → Testing list in CSV format:\n");
        list_locks(FMT_CSV, FALSE, FALSE);
        TEST_ASSERT(1, "List in CSV format completed");
        
        /* Test 5: List in null format */
        printf("  → Testing list in null format:\n");
        list_locks(FMT_NULL, FALSE, FALSE);
        TEST_ASSERT(1, "List in null format completed");
        
        /* Test 6: Wait for child to finish */
        int status;
        waitpid(child_pid, &status, 0);
        
        if (WIFEXITED(status)) {
            TEST_ASSERT(WEXITSTATUS(status) == 0, "Child should complete successfully");
        }
        
        /* Test 7: List after lock is released */
        sleep(1);
        printf("  → Testing list after lock released:\n");
        list_locks(FMT_HUMAN, FALSE, FALSE);
        TEST_ASSERT(1, "List after lock released completed");
    }
    
    /* Restore options */
    opts = saved_opts;
    
    return 0;
}

/* Test signal handling integration */
int test_signal_handling_integration(void) {
    TEST_START("Signal handling integration");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    const char *test_descriptor = "test_signal_integration";
    
    /* Create ProcessCoordinator for parent-child coordination */
    ProcessCoordinator *pc = pc_create();
    if (pc == NULL) {
        return 1;
    }
    
    if (pc_prepare_fork(pc) != PC_SUCCESS) {
        pc_destroy(pc);
        return 1;
    }
    
    /* Test 1: Create child process that holds lock and installs signal handlers */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        pc_after_fork_child(pc);
        
        install_signal_handlers();
        
        opts.descriptor = test_descriptor;
        opts.max_holders = 1;
        opts.timeout = 1.0; // Use a small timeout to ensure acquisition or failure
        
        int acquire_result = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
        if (acquire_result == 0) {
            /* Signal parent that lock is acquired */
            char signal_char = 'S';
            pc_child_send(pc, &signal_char, 1);
            
            /* Wait for signal */
            while (1) {
                sleep(1);
            }
        } else {
            /* Signal parent that lock acquisition failed */
            char signal_char = 'F';
            pc_child_send(pc, &signal_char, 1);
        }
        pc_destroy(pc);
        exit(acquire_result == 0 ? 0 : 1);
    } else if (child_pid > 0) {
        /* Parent process */
        pc_after_fork_parent(pc, child_pid);
        
        /* Wait for child to acquire lock */
        char child_signal;
        pc_result_t result = pc_parent_receive(pc, &child_signal, 1, 5000); /* 5 second timeout */
        
        if (result == PC_SUCCESS && child_signal == 'S') {
            /* Test 2: Verify lock is held */
            int check_result = check_lock(test_descriptor);
            TEST_ASSERT(check_result != 0, "Lock should be held by child");
            
            /* Test 3: Send SIGTERM to child */
            kill(child_pid, SIGTERM);
            
            /* Test 4: Wait for child to exit */
            int status;
            pc_parent_wait_for_child_exit(pc, &status);
            
            if (WIFSIGNALED(status)) {
                TEST_ASSERT(WTERMSIG(status) == SIGTERM, "Child should be terminated by SIGTERM");
            }
            
            /* Test 5: Verify lock is cleaned up */
            sleep(1);
            int check_result2 = check_lock(test_descriptor);
            TEST_ASSERT(check_result2 == 0, "Lock should be cleaned up after signal");
        } else {
            TEST_ASSERT(0, "Child failed to acquire lock or communication failed");
            kill(child_pid, SIGTERM);
            int status;
            pc_parent_wait_for_child_exit(pc, &status);
        }
        pc_destroy(pc);
    }
    
    /* Restore options */
    opts = saved_opts;
    
    return 0;
}

/* Test stale lock cleanup integration */
int test_stale_lock_cleanup_integration(void) {
    TEST_START("Stale lock cleanup integration");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    const char *test_descriptor = "test_stale_cleanup";
    
    /* Create ProcessCoordinator for parent-child coordination */
    ProcessCoordinator *pc = pc_create();
    if (pc == NULL) {
        return 1;
    }
    
    if (pc_prepare_fork(pc) != PC_SUCCESS) {
        pc_destroy(pc);
        return 1;
    }
    
    /* Test 1: Create child process that dies without cleanup */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        pc_after_fork_child(pc);
        
        opts.descriptor = test_descriptor;
        opts.max_holders = 1;
        opts.timeout = 5.0;
        
        int acquire_result = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
        if (acquire_result == 0) {
            /* Signal parent that lock is acquired */
            char signal = 1;
            pc_child_send(pc, &signal, 1);
            
            /* Exit without calling release_lock() */
            pc_destroy(pc);
            _exit(0);
        }
        pc_destroy(pc);
        _exit(1);
    } else if (child_pid > 0) {
        /* Parent process */
        pc_after_fork_parent(pc, child_pid);
        
        /* Wait for child to acquire lock */
        char parent_signal;
        pc_result_t result = pc_parent_receive(pc, &parent_signal, 1, 5000); /* 5 second timeout */
        (void)result; /* Suppress unused variable warning */
        
        /* Test 2: Wait for child to die */
        int status;
        pc_parent_wait_for_child_exit(pc, &status);
        
        if (WIFEXITED(status)) {
            TEST_ASSERT(WEXITSTATUS(status) == 0, "Child should exit successfully");
        }
        
        /* Test 3: Try to acquire same lock (should work if cleanup works) */
        /* Give the system time to detect the stale process */
        sleep(2);
        opts.descriptor = test_descriptor;
        opts.max_holders = 1;
        opts.timeout = 5.0;
        
        int acquire_result = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
        TEST_ASSERT(acquire_result == 0, "Should be able to acquire lock after stale cleanup");
        
        if (acquire_result == 0) {
            release_lock();
        }
        
        pc_destroy(pc);
    }
    
    /* Restore options */
    opts = saved_opts;
    
    return 0;
}

/* Test multi-process coordination */
int test_multi_process_coordination(void) {
    TEST_START("Multi-process coordination");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    const char *test_descriptor = "test_multi_coord";
    int max_processes = 5;
    int max_holders = 2;
    
    /* Create multiple child processes */
    pid_t child_pids[max_processes];
    int i;
    
    for (i = 0; i < max_processes; i++) {
        child_pids[i] = fork();
        if (child_pids[i] == 0) {
            /* Child process */
            opts.descriptor = test_descriptor;
            opts.max_holders = max_holders;
            opts.timeout = 3.0;
            
            int acquire_result = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
            if (acquire_result == 0) {
                /* Hold lock for 1 second */
                sleep(1);
                release_lock();
                exit(0);
            }
            exit(1);
        } else if (child_pids[i] < 0) {
            TEST_ASSERT(0, "Failed to fork child process");
            break;
        }
    }
    
    /* Wait for all children to complete */
    int success_count = 0;
    for (i = 0; i < max_processes; i++) {
        if (child_pids[i] > 0) {
            int status;
            waitpid(child_pids[i], &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                success_count++;
            }
        }
    }
    
    TEST_ASSERT(success_count == max_processes, "All processes should acquire and release locks");
    
    /* Verify all locks are released */
    sleep(1);
    int check_result = check_lock(test_descriptor);
    TEST_ASSERT(check_result == 0, "All locks should be released after coordination");
    
    /* Restore options */
    opts = saved_opts;
    
    return 0;
}

/* Test framework summary */
void test_integration_summary(void) {
    printf("\n=== INTEGRATION TEST SUMMARY ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", pass_count);
    printf("Failed: %d\n", fail_count);
    if (fail_count == 0) {
        printf("All integration tests passed!\n");
    } else {
        printf("Some integration tests failed!\n");
    }
}

/* Main test runner for integration tests */
int run_integration_tests(void) {
    printf("=== INTEGRATION TEST SUITE ===\n");
    
    /* Reset counters */
    test_count = 0;
    pass_count = 0;
    fail_count = 0;
    
    /* Run all integration tests */
    test_end_to_end_mutex();
    test_end_to_end_semaphore();
    test_end_to_end_done();
    test_end_to_end_exec();
    test_end_to_end_timeout();
    test_end_to_end_check();
    test_end_to_end_list();
    test_signal_handling_integration();
    test_stale_lock_cleanup_integration();
    test_multi_process_coordination();
    
    test_integration_summary();
    
    return (fail_count > 0) ? 1 : 0;
}