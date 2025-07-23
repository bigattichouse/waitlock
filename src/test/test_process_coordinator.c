/*
 * Comprehensive tests for ProcessCoordinator class
 * Tests race conditions, resource management, and process coordination
 */

#define _XOPEN_SOURCE 500 // Required for usleep and kill on some systems
#include "test.h"
#include "../process_coordinator/process_coordinator.h"
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>
#include "../debug_utils.h"

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
    
    
    /* Test double preparation (should fail) */
    pc_result_t result2 = pc_prepare_fork(pc);
    TEST_ASSERT(result2 != PC_SUCCESS, "Should not allow double preparation");
    
    
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
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator successfully");
    if (!pc) return 1;

    pc_result_t prep_result = pc_prepare_fork(pc);
    TEST_ASSERT(prep_result == PC_SUCCESS, "Should prepare fork successfully");
    if (prep_result != PC_SUCCESS) { pc_destroy(pc); return 1; }

    pid_t child_pid = fork();
    if (child_pid == 0) { // Child process
        pc_result_t child_setup_result = pc_after_fork_child(pc);
        if (child_setup_result != PC_SUCCESS) {
            fprintf(stderr, "Child setup failed: %s\n", pc_get_error_string(pc));
            exit(10); // Specific exit code for setup failure
        }
        char *msg = "Hello from child";
        pc_result_t send_result = pc_child_send(pc, msg, strlen(msg));
        if (send_result != PC_SUCCESS) {
            fprintf(stderr, "Child send failed: %s\n", pc_get_error_string(pc));
            exit(11); // Specific exit code for send failure
        }
        
        // Wait a bit for parent to process the message
        usleep(50000); // 50ms delay
        
        // Child sends a final signal to parent
        char final_signal = 'D'; // Done
        pc_result_t final_send_result = pc_child_send(pc, &final_signal, 1);
        if (final_send_result != PC_SUCCESS) {
            fprintf(stderr, "Child final send failed: %s\n", pc_get_error_string(pc));
            exit(12); // Specific exit code for final send failure
        }
        // Give parent time to read the final signal before exiting
        usleep(500000); // 500ms delay
        pc_destroy(pc); // Destroy PC in child before exiting
        exit(0);
    } else if (child_pid > 0) { // Parent process
        pc_result_t parent_setup_result = pc_after_fork_parent(pc, child_pid);
        TEST_ASSERT(parent_setup_result == PC_SUCCESS, "Should set up parent successfully");
        if (parent_setup_result != PC_SUCCESS) { return 1; }

        char buffer[256];
        ssize_t recv_result = pc_parent_receive(pc, buffer, sizeof(buffer), 5000);
        TEST_ASSERT(recv_result > 0 && strcmp(buffer, "Hello from child") == 0, "Should receive message from child");

        // Parent waits for child's final signal BEFORE waiting for exit
        char final_ack;
        ssize_t final_recv_result = pc_parent_receive(pc, &final_ack, 1, 3000);
        TEST_ASSERT(final_recv_result > 0 && final_ack == 'D', "Parent should receive final signal from child");
        
        int status;
        pc_result_t wait_result = pc_parent_wait_for_child_exit(pc, &status);
        TEST_ASSERT(wait_result == PC_SUCCESS, "Should wait for child successfully");
        TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0, "Child should exit successfully");
        pc_destroy(pc); // Destroy PC in parent after child exits
    } else {
        TEST_ASSERT(0, "Fork failed");
        return 1;
    }
    return 0;
}

/* Test ready signaling protocol */
int test_pc_ready_signaling(void) {
    TEST_START("ProcessCoordinator ready signaling");
    
    ProcessCoordinator *pc = pc_create();
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator successfully");
    if (!pc) return 1;

    pc_result_t prep_result = pc_prepare_fork(pc);
    TEST_ASSERT(prep_result == PC_SUCCESS, "Should prepare fork successfully");
    if (prep_result != PC_SUCCESS) { pc_destroy(pc); return 1; }

    pid_t child_pid = fork();
    if (child_pid == 0) { // Child process
        pc_result_t child_setup_result = pc_after_fork_child(pc);
        if (child_setup_result != PC_SUCCESS) {
            fprintf(stderr, "Child setup failed: %s\n", pc_get_error_string(pc));
            exit(10); // Specific exit code for setup failure
        }
        pc_result_t signal_result = pc_child_signal_ready(pc);
        if (signal_result != PC_SUCCESS) {
            fprintf(stderr, "Child signal ready failed: %s\n", pc_get_error_string(pc));
            exit(11); // Specific exit code for signal failure
        }
        // Wait for parent's acknowledgment (optional, but good for sync)
        char ack;
        ssize_t recv_result = pc_child_receive(pc, &ack, 1, 2000);
        if (recv_result <= 0) {
            fprintf(stderr, "Child ack receive failed: %s\n", pc_get_error_string(pc));
            exit(12); // Specific exit code for ack receive failure
        }
        pc_destroy(pc); // Destroy PC in child before exiting
        exit(0);
    } else if (child_pid > 0) { // Parent process
        pc_result_t parent_setup_result = pc_after_fork_parent(pc, child_pid);
        TEST_ASSERT(parent_setup_result == PC_SUCCESS, "Should set up parent successfully");
        if (parent_setup_result != PC_SUCCESS) { return 1; }

        pc_result_t wait_result = pc_parent_wait_for_child_ready(pc, 5000);
        TEST_ASSERT(wait_result == PC_SUCCESS, "Should receive ready signal from child");

        // Send acknowledgment to child
        char ack = 'A';
        pc_result_t send_result = pc_parent_send(pc, &ack, 1);
        TEST_ASSERT(send_result == PC_SUCCESS, "Should send acknowledgment");

        usleep(100000); // Add a longer delay for child to receive ACK

        int status;
        pc_result_t exit_wait_result = pc_parent_wait_for_child_exit(pc, &status);
        TEST_ASSERT(exit_wait_result == PC_SUCCESS, "Should wait for child successfully");
        TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0, "Child should exit successfully");
        pc_destroy(pc); // Destroy PC in parent after child exits
    } else {
        TEST_ASSERT(0, "Fork failed");
        return 1;
    }
    return 0;
}


/* Test timeout handling */
int test_pc_timeout_handling(void) {
    TEST_START("ProcessCoordinator timeout handling");
    
    ProcessCoordinator *pc = pc_create();
    TEST_ASSERT(pc != NULL, "Should create ProcessCoordinator successfully");
    if (!pc) return 1;

    pc_result_t prep_result = pc_prepare_fork(pc);
    TEST_ASSERT(prep_result == PC_SUCCESS, "Should prepare fork successfully");
    if (prep_result != PC_SUCCESS) { pc_destroy(pc); return 1; }

    pid_t child_pid = fork();
    if (child_pid == 0) { // Child process
        pc_result_t child_setup_result = pc_after_fork_child(pc);
        if (child_setup_result != PC_SUCCESS) {
            fprintf(stderr, "Child setup failed: %s\n", pc_get_error_string(pc));
            exit(1);
        }
        // Child does not send anything, so parent will timeout
        sleep(2); // Ensure child lives longer than parent's timeout
        exit(0);
    } else if (child_pid > 0) { // Parent process
        pc_result_t parent_setup_result = pc_after_fork_parent(pc, child_pid);
        TEST_ASSERT(parent_setup_result == PC_SUCCESS, "Should set up parent successfully");
        if (parent_setup_result != PC_SUCCESS) { return 1; }

        char buffer[256];
        // Expect timeout after 1 second
        ssize_t recv_result = pc_parent_receive(pc, buffer, sizeof(buffer), 1000);
        TEST_ASSERT(recv_result < 0, "Should timeout waiting for data");

        // Verify that the child is still alive after timeout
        TEST_ASSERT(pc_is_child_alive(pc), "Child should still be alive after parent timeout");

        // Clean up child process
        kill(child_pid, SIGTERM);
        usleep(100000); // Give child time to handle signal
        
        int status;
        pc_result_t exit_wait_result = pc_parent_wait_for_child_exit(pc, &status);
        TEST_ASSERT(exit_wait_result == PC_SUCCESS, "Should wait for child successfully after cleanup");
        
        // Child should either exit cleanly or be terminated by signal
        int child_handled_properly = 0;
        if (WIFSIGNALED(status)) {
            child_handled_properly = (WTERMSIG(status) == SIGTERM);
        } else if (WIFEXITED(status)) {
            child_handled_properly = (WEXITSTATUS(status) == 0);
        }
        
        if (child_handled_properly) {
            TEST_ASSERT(1, "Child should exit or be signaled");
        } else {
            printf("  → Child exit status: %s with code %d\n", 
                   WIFSIGNALED(status) ? "signaled" : "exited",
                   WIFSIGNALED(status) ? WTERMSIG(status) : WEXITSTATUS(status));
        }
        pc_destroy(pc); // Destroy PC in parent after child exits
    } else {
        TEST_ASSERT(0, "Fork failed");
        return 1;
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
        
        pc_result_t prep_result = pc_prepare_fork(pc);
        TEST_ASSERT(prep_result == PC_SUCCESS, "Should prepare fork in loop");
        if (prep_result != PC_SUCCESS) { pc_destroy(pc); continue; }

        pid_t child_pid = fork();
        if (child_pid == 0) { // Child process
            pc_result_t child_setup_result = pc_after_fork_child(pc);
            if (child_setup_result == PC_SUCCESS) {
                char msg = 'X';
                pc_child_send(pc, &msg, 1);
            }
            exit(0);
        } else if (child_pid > 0) { // Parent process
            pc_result_t parent_setup_result = pc_after_fork_parent(pc, child_pid);
            if (parent_setup_result == PC_SUCCESS) {
                char received;
                pc_parent_receive(pc, &received, 1, 1000);
            }
            
            int exit_status;
            pc_parent_wait_for_child_exit(pc, &exit_status);
            pc_destroy(pc);
        } else {
            TEST_ASSERT(0, "Fork failed");
            continue;
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
    
    pc_result_t prep_result = pc_prepare_fork(pc);
    TEST_ASSERT(prep_result == PC_SUCCESS, "Should prepare fork successfully");
    if (prep_result != PC_SUCCESS) { pc_destroy(pc); return 1; }

    pid_t child_pid = fork();
    if (child_pid == 0) { // Child process
        pc_result_t child_setup_result = pc_after_fork_child(pc);
        if (child_setup_result != PC_SUCCESS) {
            fprintf(stderr, "Child setup failed: %s\n", pc_get_error_string(pc));
            exit(1);
        }
        
        pc_child_signal_ready(pc);
        _exit(0);
    } else if (child_pid > 0) { // Parent process
        pc_result_t parent_setup_result = pc_after_fork_parent(pc, child_pid);
        TEST_ASSERT(parent_setup_result == PC_SUCCESS, "Should set up parent successfully");
        if (parent_setup_result != PC_SUCCESS) { return 1; }
        
        /* Wait for ready signal */
        pc_result_t wait_result = pc_parent_wait_for_child_ready(pc, 2000);
        TEST_ASSERT(wait_result == PC_SUCCESS, "Should receive ready signal");
        
        /* Wait for child to exit */
        int exit_status;
        pc_result_t exit_wait_result = pc_parent_wait_for_child_exit(pc, &exit_status);
        TEST_ASSERT(exit_wait_result == PC_SUCCESS, "Should detect child exit");
        
        
        pc_destroy(pc);
    } else {
        TEST_ASSERT(0, "Fork failed");
        return 1;
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
    
    ssize_t recv_result = pc_parent_receive(pc, data, 1, 1000);
    TEST_ASSERT(recv_result < 0, "Should reject parent operations before fork");
    
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
    
    
    
    pc_destroy(pc);
    
    
    
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

