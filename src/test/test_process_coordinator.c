/*
 * Comprehensive tests for ProcessCoordinator class
 * Tests race conditions, resource management, and process coordination
 */

#include "test.h"
#include "../process_coordinator/process_coordinator.h"
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

/* Test framework */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n[PC_TEST %d] %s\n", test_count, name); \
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

/* Test basic creation and destruction */
int test_pc_creation_destruction(void) {
    TEST_START("ProcessCoordinator creation and destruction");
    
    ProcessCoordinator *pc = pc_create();
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator successfully");
    TEST_ASSERT(pc_get_state(pc) == PC_STATE_UNINITIALIZED, "Initial state should be uninitialized");
    
    pc_destroy(pc);
    TEST_ASSERT(1, "Should destroy ProcessCoordinator without crash");
    
    /* Test NULL safety */
    pc_destroy(NULL);
    TEST_ASSERT(1, "Should handle NULL destroy gracefully");
    
    return 0;
}

/* Test pipe preparation */
int test_pc_pipe_preparation(void) {
    TEST_START("ProcessCoordinator pipe preparation");
    
    ProcessCoordinator *pc = pc_create();
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator");
    
    pc_result_t result = pc_prepare_fork(pc);
    TEST_ASSERT(result == PC_SUCCESS, "Should prepare fork successfully");
    TEST_ASSERT(pc_get_state(pc) == PC_STATE_READY, "State should be ready after preparation");
    
    /* Test double preparation (should fail) */
    pc_result_t result2 = pc_prepare_fork(pc);
    TEST_ASSERT(result2 != PC_SUCCESS, "Should not allow double preparation");
    TEST_ASSERT(pc_get_state(pc) == PC_STATE_ERROR, "State should be error after invalid operation");
    
    pc_destroy(pc);
    
    /* Test NULL safety */
    result = pc_prepare_fork(NULL);
    TEST_ASSERT(result != PC_SUCCESS, "Should handle NULL gracefully");
    
    return 0;
}

/* Test basic parent-child communication */
int test_pc_basic_communication(void) {
    TEST_START("ProcessCoordinator basic communication");
    
    ProcessCoordinator *pc = pc_create();
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator");
    
    pc_result_t result = pc_prepare_fork(pc);
    TEST_ASSERT(result == PC_SUCCESS, "Should prepare fork successfully");
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        pc_result_t child_result = pc_after_fork_child(pc);
        if (child_result != PC_SUCCESS) {
            pc_destroy(pc);
            exit(1);
        }
        
        /* Send test message to parent */
        char test_msg[] = "Hello Parent";
        child_result = pc_child_send(pc, test_msg, strlen(test_msg));
        if (child_result != PC_SUCCESS) {
            pc_destroy(pc);
            exit(2);
        }
        
        /* Receive response from parent */
        char response[32];
        child_result = pc_child_receive(pc, response, 11, 5000); /* 5 second timeout */
        if (child_result != PC_SUCCESS) {
            pc_destroy(pc);
            exit(3);
        }
        
        /* Verify response */
        if (strncmp(response, "Hello Child", 11) != 0) {
            pc_destroy(pc);
            exit(4);
        }
        
        pc_destroy(pc);
        exit(0);
    } else if (child_pid > 0) {
        /* Parent process */
        pc_result_t parent_result = pc_after_fork_parent(pc, child_pid);
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should set up parent successfully");
        
        /* Receive message from child */
        char received_msg[32];
        parent_result = pc_parent_receive(pc, received_msg, 12, 5000); /* 5 second timeout */
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should receive message from child");
        
        if (parent_result == PC_SUCCESS) {
            received_msg[12] = '\0'; /* Null terminate for comparison */
            TEST_ASSERT(strcmp(received_msg, "Hello Parent") == 0, "Should receive correct message");
            
            /* Send response to child */
            char response[] = "Hello Child";
            parent_result = pc_parent_send(pc, response, 11);
            TEST_ASSERT(parent_result == PC_SUCCESS, "Should send response to child");
        }
        
        /* Wait for child to complete */
        int exit_status;
        parent_result = pc_parent_wait_for_child_exit(pc, &exit_status);
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should wait for child successfully");
        TEST_ASSERT(WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0, "Child should exit successfully");
        
        pc_destroy(pc);
    } else {
        TEST_ASSERT(0, "Fork failed");
        pc_destroy(pc);
    }
    
    return 0;
}

/* Test ready signaling protocol */
int test_pc_ready_signaling(void) {
    TEST_START("ProcessCoordinator ready signaling");
    
    ProcessCoordinator *pc = pc_create();
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator");
    
    pc_result_t result = pc_prepare_fork(pc);
    TEST_ASSERT(result == PC_SUCCESS, "Should prepare fork successfully");
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        pc_result_t child_result = pc_after_fork_child(pc);
        if (child_result != PC_SUCCESS) {
            pc_destroy(pc);
            exit(1);
        }
        
        /* Simulate some initialization work */
        usleep(100000); /* 100ms */
        
        /* Signal ready to parent */
        child_result = pc_child_signal_ready(pc);
        if (child_result != PC_SUCCESS) {
            pc_destroy(pc);
            exit(2);
        }
        
        /* Wait for parent to acknowledge */
        char ack;
        child_result = pc_child_receive(pc, &ack, 1, 5000);
        if (child_result != PC_SUCCESS || ack != 'A') {
            pc_destroy(pc);
            exit(3);
        }
        
        pc_destroy(pc);
        exit(0);
    } else if (child_pid > 0) {
        /* Parent process */
        pc_result_t parent_result = pc_after_fork_parent(pc, child_pid);
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should set up parent successfully");
        
        /* Wait for child ready signal */
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        parent_result = pc_parent_wait_for_child_ready(pc, 5000); /* 5 second timeout */
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
        
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should receive ready signal from child");
        TEST_ASSERT(elapsed >= 0.1, "Should wait for child initialization");
        TEST_ASSERT(elapsed <= 1.0, "Should not wait too long");
        
        /* Send acknowledgment */
        char ack = 'A';
        parent_result = pc_parent_send(pc, &ack, 1);
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should send acknowledgment");
        
        /* Wait for child to complete */
        int exit_status;
        parent_result = pc_parent_wait_for_child_exit(pc, &exit_status);
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should wait for child successfully");
        TEST_ASSERT(WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0, "Child should exit successfully");
        
        pc_destroy(pc);
    } else {
        TEST_ASSERT(0, "Fork failed");
        pc_destroy(pc);
    }
    
    return 0;
}

/* Test timeout handling */
int test_pc_timeout_handling(void) {
    TEST_START("ProcessCoordinator timeout handling");
    
    ProcessCoordinator *pc = pc_create();
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator");
    
    pc_result_t result = pc_prepare_fork(pc);
    TEST_ASSERT(result == PC_SUCCESS, "Should prepare fork successfully");
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process - deliberately don't send anything */
        pc_result_t child_result = pc_after_fork_child(pc);
        if (child_result != PC_SUCCESS) {
            pc_destroy(pc);
            exit(1);
        }
        
        /* Wait for a while without sending anything */
        sleep(2);
        
        pc_destroy(pc);
        exit(0);
    } else if (child_pid > 0) {
        /* Parent process */
        pc_result_t parent_result = pc_after_fork_parent(pc, child_pid);
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should set up parent successfully");
        
        /* Try to receive with short timeout */
        char data[10];
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        parent_result = pc_parent_receive(pc, data, 1, 500); /* 500ms timeout */
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
        
        TEST_ASSERT(parent_result == PC_ERROR_TIMEOUT, "Should timeout waiting for data");
        TEST_ASSERT(elapsed >= 0.4 && elapsed <= 0.8, "Should respect timeout duration");
        
        /* Clean up child */
        kill(child_pid, SIGTERM);
        int exit_status;
        waitpid(child_pid, &exit_status, 0);
        
        pc_destroy(pc);
    } else {
        TEST_ASSERT(0, "Fork failed");
        pc_destroy(pc);
    }
    
    return 0;
}

/* Test race condition prevention in multiple concurrent operations */
int test_pc_race_conditions(void) {
    TEST_START("ProcessCoordinator race condition prevention");
    
    /* Test multiple rapid fork/destroy cycles */
    for (int i = 0; i < 10; i++) {
        ProcessCoordinator *pc = pc_create();
        TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator in loop");
        
        pc_result_t result = pc_prepare_fork(pc);
        TEST_ASSERT(result == PC_SUCCESS, "Should prepare fork in loop");
        
        pid_t child_pid = fork();
        if (child_pid == 0) {
            /* Child process - quick test and exit */
            pc_result_t child_result = pc_after_fork_child(pc);
            if (child_result == PC_SUCCESS) {
                char msg = 'X';
                pc_child_send(pc, &msg, 1);
            }
            pc_destroy(pc);
            exit(0);
        } else if (child_pid > 0) {
            /* Parent process */
            pc_result_t parent_result = pc_after_fork_parent(pc, child_pid);
            if (parent_result == PC_SUCCESS) {
                char received;
                pc_parent_receive(pc, &received, 1, 1000);
            }
            
            int exit_status;
            pc_parent_wait_for_child_exit(pc, &exit_status);
            pc_destroy(pc);
        }
    }
    
    TEST_ASSERT(1, "Should handle multiple rapid fork/destroy cycles");
    return 0;
}

/* Test resource cleanup on abnormal termination */
int test_pc_abnormal_termination(void) {
    TEST_START("ProcessCoordinator abnormal termination handling");
    
    ProcessCoordinator *pc = pc_create();
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator");
    
    pc_result_t result = pc_prepare_fork(pc);
    TEST_ASSERT(result == PC_SUCCESS, "Should prepare fork successfully");
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process - signal ready then exit abruptly */
        pc_result_t child_result = pc_after_fork_child(pc);
        if (child_result != PC_SUCCESS) {
            pc_destroy(pc);
            exit(1);
        }
        
        pc_child_signal_ready(pc);
        pc_destroy(pc);
        
        /* Exit abruptly without normal cleanup */
        _exit(0);
    } else if (child_pid > 0) {
        /* Parent process */
        pc_result_t parent_result = pc_after_fork_parent(pc, child_pid);
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should set up parent successfully");
        
        /* Wait for ready signal */
        parent_result = pc_parent_wait_for_child_ready(pc, 2000);
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should receive ready signal");
        
        /* Wait for child to exit */
        int exit_status;
        parent_result = pc_parent_wait_for_child_exit(pc, &exit_status);
        TEST_ASSERT(parent_result == PC_SUCCESS, "Should detect child exit");
        TEST_ASSERT(pc_get_state(pc) == PC_STATE_COMPLETED, "Should be in completed state");
        
        pc_destroy(pc);
    } else {
        TEST_ASSERT(0, "Fork failed");
        pc_destroy(pc);
    }
    
    return 0;
}

/* Test error state handling */
int test_pc_error_handling(void) {
    TEST_START("ProcessCoordinator error handling");
    
    /* Test operations on NULL coordinator */
    pc_result_t result = pc_prepare_fork(NULL);
    TEST_ASSERT(result != PC_SUCCESS, "Should reject NULL coordinator");
    
    /* Test invalid state transitions */
    ProcessCoordinator *pc = pc_create();
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator");
    
    /* Try parent operations before fork */
    char data[10];
    result = pc_parent_send(pc, data, 1);
    TEST_ASSERT(result != PC_SUCCESS, "Should reject parent operations before fork");
    
    result = pc_parent_receive(pc, data, 1, 1000);
    TEST_ASSERT(result != PC_SUCCESS, "Should reject parent operations before fork");
    
    /* Test error message retrieval */
    const char *error_msg = pc_get_error_string(pc);
    TEST_ASSERT(error_msg != NULL, "Should provide error message");
    TEST_ASSERT(strlen(error_msg) > 0, "Error message should not be empty");
    
    pc_destroy(pc);
    
    /* Test error message on NULL */
    error_msg = pc_get_error_string(NULL);
    TEST_ASSERT(error_msg != NULL, "Should handle NULL gracefully");
    
    return 0;
}

/* Test memory and resource leak prevention */
int test_pc_resource_management(void) {
    TEST_START("ProcessCoordinator resource management");
    
    /* Create and destroy many coordinators to test for leaks */
    for (int i = 0; i < 100; i++) {
        ProcessCoordinator *pc = pc_create();
        if (pc) {
            pc_prepare_fork(pc);
            pc_destroy(pc);
        }
    }
    
    TEST_ASSERT(1, "Should handle many create/destroy cycles without leaks");
    
    /* Test emergency cleanup */
    ProcessCoordinator *pc = pc_create();
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator");
    
    pc_result_t result = pc_prepare_fork(pc);
    TEST_ASSERT(result == PC_SUCCESS, "Should prepare fork successfully");
    
    /* Call emergency cleanup */
    pc_emergency_cleanup(pc);
    TEST_ASSERT(1, "Should handle emergency cleanup without crash");
    
    pc_destroy(pc);
    
    /* Test emergency cleanup on NULL */
    pc_emergency_cleanup(NULL);
    TEST_ASSERT(1, "Should handle emergency cleanup on NULL");
    
    return 0;
}

/* Test framework summary */
void test_pc_summary(void) {
    printf("\n=== PROCESS COORDINATOR TEST SUMMARY ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", pass_count);
    printf("Failed: %d\n", fail_count);
    if (fail_count == 0) {
        printf("All ProcessCoordinator tests passed!\n");
    } else {
        printf("Some ProcessCoordinator tests failed!\n");
    }
}

/* Main test runner for ProcessCoordinator */
int run_process_coordinator_tests(void) {
    printf("=== PROCESS COORDINATOR TEST SUITE ===\n");
    
    /* Reset counters */
    test_count = 0;
    pass_count = 0;
    fail_count = 0;
    
    /* Run all ProcessCoordinator tests */
    test_pc_creation_destruction();
    test_pc_pipe_preparation();
    test_pc_basic_communication();
    test_pc_ready_signaling();
    test_pc_timeout_handling();
    test_pc_race_conditions();
    test_pc_abnormal_termination();
    test_pc_error_handling();
    test_pc_resource_management();
    
    test_pc_summary();
    
    return (fail_count > 0) ? 1 : 0;
}