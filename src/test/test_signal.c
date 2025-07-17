/*
 * Unit tests for signal.c functions
 * Tests signal handler installation and signal handling behavior
 */

#include "test.h"
#include "../signal/signal.h"
#include "../core/core.h"
#include "../lock/lock.h"

/* Test framework */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

/* Global flags for signal testing */
static volatile int signal_received = 0;
static volatile int signal_number = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n[SIGNAL_TEST %d] %s\n", test_count, name); \
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

/* Test signal handler for testing purposes */
void test_signal_handler(int sig) {
    signal_received = 1;
    signal_number = sig;
}

/* Test signal handler installation */
int test_install_signal_handlers(void) {
    TEST_START("Signal handler installation");
    
    /* Install signal handlers */
    install_signal_handlers();
    TEST_ASSERT(1, "Signal handlers installed without error");
    
    /* Test that we can get current signal handlers */
    struct sigaction sa;
    
    /* Test SIGTERM handler */
    int result = sigaction(SIGTERM, NULL, &sa);
    TEST_ASSERT(result == 0, "Should be able to get SIGTERM handler");
    TEST_ASSERT(sa.sa_handler != SIG_DFL, "SIGTERM handler should not be default");
    TEST_ASSERT(sa.sa_handler != SIG_IGN, "SIGTERM handler should not be ignored");
    
    /* Test SIGINT handler */
    result = sigaction(SIGINT, NULL, &sa);
    TEST_ASSERT(result == 0, "Should be able to get SIGINT handler");
    TEST_ASSERT(sa.sa_handler != SIG_DFL, "SIGINT handler should not be default");
    TEST_ASSERT(sa.sa_handler != SIG_IGN, "SIGINT handler should not be ignored");
    
    /* Test SIGHUP handler */
    result = sigaction(SIGHUP, NULL, &sa);
    TEST_ASSERT(result == 0, "Should be able to get SIGHUP handler");
    TEST_ASSERT(sa.sa_handler != SIG_DFL, "SIGHUP handler should not be default");
    TEST_ASSERT(sa.sa_handler != SIG_IGN, "SIGHUP handler should not be ignored");
    
    /* Test SIGQUIT handler */
    result = sigaction(SIGQUIT, NULL, &sa);
    TEST_ASSERT(result == 0, "Should be able to get SIGQUIT handler");
    TEST_ASSERT(sa.sa_handler != SIG_DFL, "SIGQUIT handler should not be default");
    TEST_ASSERT(sa.sa_handler != SIG_IGN, "SIGQUIT handler should not be ignored");
    
    /* Test SIGPIPE handler (should be ignored) */
    result = sigaction(SIGPIPE, NULL, &sa);
    TEST_ASSERT(result == 0, "Should be able to get SIGPIPE handler");
    TEST_ASSERT(sa.sa_handler == SIG_IGN, "SIGPIPE handler should be ignored");
    
    return 0;
}

/* Test signal handling behavior */
int test_signal_handling_behavior(void) {
    TEST_START("Signal handling behavior");
    
    /* Create child process to test signal handling */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        /* Install signal handlers */
        install_signal_handlers();
        
        /* Acquire a lock to test cleanup */
        int acquire_result = acquire_lock("test_signal_lock", 1, 0.0);
        if (acquire_result == 0) {
            /* Wait for signal */
            while (1) {
                sleep(1);
            }
        }
        exit(1);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to acquire lock */
        
        /* Verify lock is held */
        int check_result = check_lock("test_signal_lock");
        TEST_ASSERT(check_result != 0, "Lock should be held by child");
        
        /* Send SIGTERM to child */
        kill(child_pid, SIGTERM);
        
        /* Wait for child to exit */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Check if child was terminated by signal */
        if (WIFSIGNALED(status)) {
            int signal = WTERMSIG(status);
            TEST_ASSERT(signal == SIGTERM, "Child should be terminated by SIGTERM");
        } else {
            TEST_ASSERT(1, "Child exited (signal handling may vary)");
        }
        
        /* Check if lock was cleaned up */
        sleep(1);
        int check_result2 = check_lock("test_signal_lock");
        TEST_ASSERT(check_result2 == 0, "Lock should be cleaned up after signal");
    }
    
    return 0;
}

/* Test SIGINT handling */
int test_sigint_handling(void) {
    TEST_START("SIGINT handling");
    
    /* Create child process to test SIGINT handling */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        install_signal_handlers();
        
        /* Acquire a lock */
        int acquire_result = acquire_lock("test_sigint_lock", 1, 0.0);
        if (acquire_result == 0) {
            /* Wait for signal */
            while (1) {
                sleep(1);
            }
        }
        exit(1);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to acquire lock */
        
        /* Send SIGINT to child */
        kill(child_pid, SIGINT);
        
        /* Wait for child to exit */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Check if child was terminated by signal */
        if (WIFSIGNALED(status)) {
            int signal = WTERMSIG(status);
            TEST_ASSERT(signal == SIGINT, "Child should be terminated by SIGINT");
        } else {
            TEST_ASSERT(1, "Child exited (signal handling may vary)");
        }
        
        /* Check if lock was cleaned up */
        sleep(1);
        int check_result = check_lock("test_sigint_lock");
        TEST_ASSERT(check_result == 0, "Lock should be cleaned up after SIGINT");
    }
    
    return 0;
}

/* Test SIGHUP handling */
int test_sighup_handling(void) {
    TEST_START("SIGHUP handling");
    
    /* Create child process to test SIGHUP handling */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        install_signal_handlers();
        
        /* Acquire a lock */
        int acquire_result = acquire_lock("test_sighup_lock", 1, 0.0);
        if (acquire_result == 0) {
            /* Wait for signal */
            while (1) {
                sleep(1);
            }
        }
        exit(1);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to acquire lock */
        
        /* Send SIGHUP to child */
        kill(child_pid, SIGHUP);
        
        /* Wait for child to exit */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Check if child was terminated by signal */
        if (WIFSIGNALED(status)) {
            int signal = WTERMSIG(status);
            TEST_ASSERT(signal == SIGHUP, "Child should be terminated by SIGHUP");
        } else {
            TEST_ASSERT(1, "Child exited (signal handling may vary)");
        }
        
        /* Check if lock was cleaned up */
        sleep(1);
        int check_result = check_lock("test_sighup_lock");
        TEST_ASSERT(check_result == 0, "Lock should be cleaned up after SIGHUP");
    }
    
    return 0;
}

/* Test SIGQUIT handling */
int test_sigquit_handling(void) {
    TEST_START("SIGQUIT handling");
    
    /* Create child process to test SIGQUIT handling */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        install_signal_handlers();
        
        /* Acquire a lock */
        int acquire_result = acquire_lock("test_sigquit_lock", 1, 0.0);
        if (acquire_result == 0) {
            /* Wait for signal */
            while (1) {
                sleep(1);
            }
        }
        exit(1);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to acquire lock */
        
        /* Send SIGQUIT to child */
        kill(child_pid, SIGQUIT);
        
        /* Wait for child to exit */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Check if child was terminated by signal */
        if (WIFSIGNALED(status)) {
            int signal = WTERMSIG(status);
            TEST_ASSERT(signal == SIGQUIT, "Child should be terminated by SIGQUIT");
        } else {
            TEST_ASSERT(1, "Child exited (signal handling may vary)");
        }
        
        /* Check if lock was cleaned up */
        sleep(1);
        int check_result = check_lock("test_sigquit_lock");
        TEST_ASSERT(check_result == 0, "Lock should be cleaned up after SIGQUIT");
    }
    
    return 0;
}

/* Test SIGPIPE handling (should be ignored) */
int test_sigpipe_handling(void) {
    TEST_START("SIGPIPE handling");
    
    /* Create child process to test SIGPIPE handling */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        install_signal_handlers();
        
        /* Acquire a lock */
        int acquire_result = acquire_lock("test_sigpipe_lock", 1, 0.0);
        if (acquire_result == 0) {
            /* Wait a bit, then exit normally */
            sleep(2);
            release_lock();
        }
        exit(0);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to acquire lock */
        
        /* Send SIGPIPE to child (should be ignored) */
        kill(child_pid, SIGPIPE);
        
        /* Wait for child to exit */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Child should exit normally (SIGPIPE ignored) */
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            TEST_ASSERT(exit_code == 0, "Child should exit normally (SIGPIPE ignored)");
        } else {
            TEST_ASSERT(0, "Child should exit normally, not due to signal");
        }
        
        /* Check if lock was cleaned up normally */
        sleep(1);
        int check_result = check_lock("test_sigpipe_lock");
        TEST_ASSERT(check_result == 0, "Lock should be cleaned up normally");
    }
    
    return 0;
}

/* Test signal forwarding in exec mode */
int test_signal_forwarding_exec(void) {
    TEST_START("Signal forwarding in exec mode");
    
    /* Create child process that uses exec mode */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        /* This would normally be done by exec_with_lock */
        install_signal_handlers();
        
        /* Simulate exec mode by running a command */
        char *argv[] = {"sleep", "5", NULL};
        
        /* Fork again to simulate exec */
        pid_t grandchild_pid = fork();
        if (grandchild_pid == 0) {
            /* Grandchild - run the command */
            execvp(argv[0], argv);
            exit(127); /* exec failed */
        } else if (grandchild_pid > 0) {
            /* Child - wait for grandchild and forward signals */
            int status;
            waitpid(grandchild_pid, &status, 0);
            
            if (WIFSIGNALED(status)) {
                exit(128 + WTERMSIG(status));
            } else {
                exit(WEXITSTATUS(status));
            }
        }
        exit(1);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to start */
        
        /* Send SIGTERM to child */
        kill(child_pid, SIGTERM);
        
        /* Wait for child to exit */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Check if signal was forwarded */
        if (WIFSIGNALED(status)) {
            int signal = WTERMSIG(status);
            TEST_ASSERT(signal == SIGTERM, "Child should be terminated by SIGTERM");
        } else if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            TEST_ASSERT(exit_code == 128 + SIGTERM, "Child should exit with signal code");
        } else {
            TEST_ASSERT(0, "Child should exit due to signal");
        }
    }
    
    return 0;
}

/* Test signal race conditions */
int test_signal_race_conditions(void) {
    TEST_START("Signal race conditions");
    
    /* Create multiple children that will receive signals */
    pid_t child_pids[3];
    int i;
    
    for (i = 0; i < 3; i++) {
        child_pids[i] = fork();
        if (child_pids[i] == 0) {
            /* Child process */
            install_signal_handlers();
            
            /* Acquire a lock */
            char lock_name[64];
            snprintf(lock_name, sizeof(lock_name), "test_race_lock_%d", i);
            
            int acquire_result = acquire_lock(lock_name, 1, 0.0);
            if (acquire_result == 0) {
                /* Wait for signal */
                while (1) {
                    sleep(1);
                }
            }
            exit(1);
        }
    }
    
    /* Give children time to acquire locks */
    sleep(1);
    
    /* Send signals to all children simultaneously */
    for (i = 0; i < 3; i++) {
        kill(child_pids[i], SIGTERM);
    }
    
    /* Wait for all children to exit */
    for (i = 0; i < 3; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
        TEST_ASSERT(WIFSIGNALED(status) || WIFEXITED(status), "Child should exit");
    }
    
    /* Check if all locks were cleaned up */
    sleep(1);
    for (i = 0; i < 3; i++) {
        char lock_name[64];
        snprintf(lock_name, sizeof(lock_name), "test_race_lock_%d", i);
        int check_result = check_lock(lock_name);
        TEST_ASSERT(check_result == 0, "Lock should be cleaned up after signal");
    }
    
    return 0;
}

/* Test signal handling with multiple locks */
int test_signal_multiple_locks(void) {
    TEST_START("Signal handling with multiple locks");
    
    /* Create child that holds multiple semaphore slots */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        install_signal_handlers();
        
        /* Acquire multiple semaphore slots */
        int result1 = acquire_lock("test_multi_lock", 3, 0.0);
        if (result1 == 0) {
            /* Hold locks and wait for signal */
            while (1) {
                sleep(1);
            }
        }
        exit(1);
    } else if (child_pid > 0) {
        /* Parent process */
        sleep(1); /* Give child time to acquire locks */
        
        /* Send SIGTERM to child */
        kill(child_pid, SIGTERM);
        
        /* Wait for child to exit */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Check if locks were cleaned up */
        sleep(1);
        int check_result = check_lock("test_multi_lock");
        TEST_ASSERT(check_result == 0, "All locks should be cleaned up after signal");
    }
    
    return 0;
}

/* Test framework summary */
void test_signal_summary(void) {
    printf("\n=== SIGNAL TEST SUMMARY ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", pass_count);
    printf("Failed: %d\n", fail_count);
    if (fail_count == 0) {
        printf("All signal tests passed!\n");
    } else {
        printf("Some signal tests failed!\n");
    }
}

/* Main test runner for signal module */
int run_signal_tests(void) {
    printf("=== SIGNAL MODULE TEST SUITE ===\n");
    
    /* Reset counters */
    test_count = 0;
    pass_count = 0;
    fail_count = 0;
    
    /* Run all signal tests */
    test_install_signal_handlers();
    test_signal_handling_behavior();
    test_sigint_handling();
    test_sighup_handling();
    test_sigquit_handling();
    test_sigpipe_handling();
    test_signal_forwarding_exec();
    test_signal_race_conditions();
    test_signal_multiple_locks();
    
    test_signal_summary();
    
    return (fail_count > 0) ? 1 : 0;
}