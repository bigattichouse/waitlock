#include "test.h"
#include "../lock/lock.h"
#include "../core/core.h"
#include "../process/process.h"
#include "../process_coordinator/process_coordinator.h"
#include <time.h>

// Define the global variables required by the modules under test
struct global_state g_state;
struct options opts;

/* Test framework */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n[FAILING_TEST %d] %s\n", test_count, name); \
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

int test_semaphore_slots(void) {
    TEST_START("Semaphore slot allocation");
    
    const char *test_descriptor = "test_semaphore_slots";
    int max_holders = 3;
    
    /* Create separate ProcessCoordinator for each child for race-free coordination */
    ProcessCoordinator *pcs[max_holders];
    pid_t child_pids[max_holders];
    int i;
    
    /* Initialize ProcessCoordinators */
    for (i = 0; i < max_holders; i++) {
        pcs[i] = pc_create();
        if (!pcs[i]) {
            printf("  ✗ FAIL: Cannot create ProcessCoordinator %d\n", i);
            fail_count++;
            /* Cleanup previous coordinators */
            for (int j = 0; j < i; j++) {
                pc_destroy(pcs[j]);
            }
            return 1;
        }
    }
    
    /* Create multiple child processes to test slot allocation */
    for (i = 0; i < max_holders; i++) {
        pc_result_t prep_result = pc_prepare_fork(pcs[i]);
        if (prep_result != PC_SUCCESS) {
            printf("  ✗ FAIL: ProcessCoordinator prepare_fork failed: %s\n", pc_get_error_string(pcs[i]));
            fail_count++;
            /* Cleanup all coordinators */
            for (int j = 0; j < max_holders; j++) {
                pc_destroy(pcs[j]);
            }
            return 1;
        }
        
        child_pids[i] = fork();
        if (child_pids[i] == 0) {
            /* Child process - acquire semaphore slot */
            pc_result_t child_result = pc_after_fork_child(pcs[i]);
            if (child_result != PC_SUCCESS) {
                printf("Child: ProcessCoordinator setup failed: %s\n", pc_get_error_string(pcs[i]));
                pc_destroy(pcs[i]);
                exit(1);
            }
            
            /* Reset global state for child (critical for avoiding state corruption) */
            extern struct global_state g_state;
            g_state.lock_fd = -1;
            g_state.lock_path[0] = '\0';
            g_state.child_pid = 0;
            
            int acquire_result = acquire_lock(test_descriptor, max_holders, 2.0);
            
            /* Signal parent about result using ProcessCoordinator */
            char status_msg[64];
            snprintf(status_msg, sizeof(status_msg), "%s:%d", 
                     (acquire_result == 0) ? "SUCCESS" : "FAILED", acquire_result);
            
            pc_result_t send_result = pc_child_send(pcs[i], status_msg, strlen(status_msg));
            if (send_result != PC_SUCCESS) {
                printf("Child: Failed to send status: %s\n", pc_get_error_string(pcs[i]));
            }
            
            if (acquire_result == 0) {
                /* Hold the lock for parent to test 4th slot */
                sleep(3); /* Hold lock for 3 seconds */
                release_lock();
            }
            
            pc_destroy(pcs[i]);
            exit(acquire_result == 0 ? 0 : 1);
        } else if (child_pids[i] < 0) {
            TEST_ASSERT(0, "Failed to fork child process");
            /* Cleanup all coordinators */
            for (int j = 0; j < max_holders; j++) {
                pc_destroy(pcs[j]);
            }
            return 1;
        }
        
        /* Parent: complete fork coordination */
        pc_result_t parent_result = pc_after_fork_parent(pcs[i], child_pids[i]);
        if (parent_result != PC_SUCCESS) {
            printf("  ✗ FAIL: ProcessCoordinator after_fork_parent failed: %s\n", pc_get_error_string(pcs[i]));
            fail_count++;
            /* Cleanup all coordinators */
            for (int j = 0; j < max_holders; j++) {
                pc_destroy(pcs[j]);
            }
            return 1;
        }
    }
    
    /* Wait for all children to acquire their slots using ProcessCoordinator */
    int successful_acquisitions = 0;
    for (i = 0; i < max_holders; i++) {
        char child_status[64];
        
        /* Receive status from each child */
        pc_result_t recv_result = pc_parent_receive(pcs[i], child_status, sizeof(child_status) - 1, 10000);
        if (recv_result == PC_SUCCESS) {
            child_status[sizeof(child_status) - 1] = '\0';
            if (strstr(child_status, "SUCCESS:") != NULL) {
                successful_acquisitions++;
            }
        }
    }
    
    printf("  → Successful acquisitions: %d/%d\n", successful_acquisitions, max_holders);
    TEST_ASSERT(successful_acquisitions == max_holders, 
                "All children should successfully acquire semaphore slots");
    
    /* Give children extra time to fully establish their locks */
    sleep(1);
    
    /* Test that all slots are occupied - try to acquire one more with longer timeout */
    int fourth_result = acquire_lock(test_descriptor, max_holders, 2.0);
    TEST_ASSERT(fourth_result != 0, "Fourth slot should not be available");
    
    /* Wait for all children to complete naturally */
    for (i = 0; i < max_holders; i++) {
        if (child_pids[i] > 0) {
            int status;
            waitpid(child_pids[i], &status, 0);
            TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0, 
                       "Child should successfully acquire and release slot");
        }
    }
    
    /* Cleanup all ProcessCoordinators */
    for (i = 0; i < max_holders; i++) {
        pc_destroy(pcs[i]);
    }
    return 0;
}

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
    opts.exec_argv = NULL;
    
    /* Test 1: Acquire first semaphore slot */
    int acquire_result1 = acquire_lock(opts.descriptor, opts.max_holders, opts.timeout);
    TEST_ASSERT(acquire_result1 == 0, "Should successfully acquire first semaphore slot");
    
    /* Test 2: Use separate ProcessCoordinators for race-free coordination of remaining slots */
    ProcessCoordinator *pcs[2];
    pid_t child_pids[2];
    int i;
    
    /* Initialize ProcessCoordinators */
    for (i = 0; i < 2; i++) {
        pcs[i] = pc_create();
        if (!pcs[i]) {
            release_lock();
            /* Cleanup previous coordinators */
            for (int j = 0; j < i; j++) {
                pc_destroy(pcs[j]);
            }
            opts = saved_opts;
            TEST_ASSERT(0, "Cannot create ProcessCoordinator");
            return 1;
        }
    }
    
    /* Acquire second and third slots from child processes with ProcessCoordinator */
    for (i = 0; i < 2; i++) {
        pc_result_t prep_result = pc_prepare_fork(pcs[i]);
        if (prep_result != PC_SUCCESS) {
            release_lock();
            /* Cleanup all coordinators */
            for (int j = 0; j < 2; j++) {
                pc_destroy(pcs[j]);
            }
            opts = saved_opts;
            TEST_ASSERT(0, "ProcessCoordinator prepare_fork failed");
            return 1;
        }
        
        child_pids[i] = fork();
        if (child_pids[i] == 0) {
            /* Child process */
            pc_result_t child_result = pc_after_fork_child(pcs[i]);
            if (child_result != PC_SUCCESS) {
                printf("Child: ProcessCoordinator setup failed: %s\n", pc_get_error_string(pcs[i]));
                pc_destroy(pcs[i]);
                exit(1);
            }
            
            /* Reset global state for child (critical for avoiding state corruption) */
            extern struct global_state g_state;
            g_state.lock_fd = -1;
            g_state.lock_path[0] = '\0';
            g_state.child_pid = 0;
            
            int acquire_result = acquire_lock(opts.descriptor, opts.max_holders, 2.0);
            
            /* Signal parent about result using ProcessCoordinator */
            char status_msg[32];
            snprintf(status_msg, sizeof(status_msg), "%s:%d", 
                     (acquire_result == 0) ? "SUCCESS" : "FAILED", acquire_result);
            
            pc_result_t send_result = pc_child_send(pcs[i], status_msg, strlen(status_msg));
            if (send_result != PC_SUCCESS) {
                printf("Child: Failed to send status: %s\n", pc_get_error_string(pcs[i]));
            }
            
            if (acquire_result == 0) {
                /* Hold the slot for parent to test 4th slot */
                sleep(3);
                release_lock();
                exit(0);
            }
            
            pc_destroy(pcs[i]);
            exit(1);
        } else if (child_pids[i] < 0) {
            release_lock();
            /* Cleanup all coordinators */
            for (int j = 0; j < 2; j++) {
                pc_destroy(pcs[j]);
            }
            opts = saved_opts;
            TEST_ASSERT(0, "Failed to fork child process");
            return 1;
        }
        
        /* Parent: complete fork coordination */
        pc_result_t parent_result = pc_after_fork_parent(pcs[i], child_pids[i]);
        if (parent_result != PC_SUCCESS) {
            release_lock();
            /* Cleanup all coordinators */
            for (int j = 0; j < 2; j++) {
                pc_destroy(pcs[j]);
            }
            opts = saved_opts;
            TEST_ASSERT(0, "ProcessCoordinator after_fork_parent failed");
            return 1;
        }
    }
    
    /* Wait for all children to acquire their slots using ProcessCoordinator */
    int successful_child_acquisitions = 0;
    for (i = 0; i < 2; i++) {
        char child_status[32];
        
        /* Receive status from each child */
        pc_result_t recv_result = pc_parent_receive(pcs[i], child_status, sizeof(child_status) - 1, 10000);
        if (recv_result == PC_SUCCESS) {
            child_status[sizeof(child_status) - 1] = '\0';
            if (strstr(child_status, "SUCCESS:") != NULL) {
                successful_child_acquisitions++;
            }
        }
    }
    
    TEST_ASSERT(successful_child_acquisitions == 2, "Both children should acquire slots");
    
    /* Test 3: Try to acquire fourth slot (should fail) - give children time to establish */
    sleep(1);
    
    pid_t fourth_child = fork();
    if (fourth_child == 0) {
        /* Child process - try to get 4th slot with longer timeout */
        int child_result = acquire_lock(opts.descriptor, opts.max_holders, 2.0);
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
    
    /* Cleanup all ProcessCoordinators */
    for (i = 0; i < 2; i++) {
        pc_destroy(pcs[i]);
    }
    opts = saved_opts;
    
    return 0;
}

int main() {
    test_semaphore_slots();
    test_end_to_end_semaphore();
    return 0;
}