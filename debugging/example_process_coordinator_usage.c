/*
 * Example showing how to use ProcessCoordinator properly
 * This demonstrates the correct pattern to replace problematic PipeCoordinator usage
 */

#include "../process_coordinator/process_coordinator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Example 1: Simple parent-child coordination */
int example_simple_coordination(void) {
    printf("=== Example 1: Simple Parent-Child Coordination ===\n");
    
    ProcessCoordinator *pc = pc_create();
    if (!pc) {
        fprintf(stderr, "Failed to create ProcessCoordinator\n");
        return 1;
    }
    
    pc_result_t result = pc_prepare_fork(pc);
    if (result != PC_SUCCESS) {
        fprintf(stderr, "Failed to prepare fork: %s\n", pc_get_error_string(pc));
        pc_destroy(pc);
        return 1;
    }
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        result = pc_after_fork_child(pc);
        if (result != PC_SUCCESS) {
            fprintf(stderr, "Child: Failed setup: %s\n", pc_get_error_string(pc));
            pc_destroy(pc);
            exit(1);
        }
        
        /* Do some work */
        printf("Child: Starting work...\n");
        sleep(1);
        
        /* Signal completion to parent */
        char completion_signal = 'C';
        result = pc_child_send(pc, &completion_signal, 1);
        if (result != PC_SUCCESS) {
            fprintf(stderr, "Child: Failed to send completion: %s\n", pc_get_error_string(pc));
            pc_destroy(pc);
            exit(1);
        }
        
        printf("Child: Work completed, exiting\n");
        pc_destroy(pc);
        exit(0);
        
    } else if (child_pid > 0) {
        /* Parent process */
        result = pc_after_fork_parent(pc, child_pid);
        if (result != PC_SUCCESS) {
            fprintf(stderr, "Parent: Failed setup: %s\n", pc_get_error_string(pc));
            pc_destroy(pc);
            return 1;
        }
        
        printf("Parent: Waiting for child completion...\n");
        
        /* Wait for child completion signal */
        char received_signal;
        result = pc_parent_receive(pc, &received_signal, 1, 5000); /* 5 second timeout */
        if (result != PC_SUCCESS) {
            fprintf(stderr, "Parent: Failed to receive completion: %s\n", pc_get_error_string(pc));
        } else if (received_signal == 'C') {
            printf("Parent: Child completed successfully!\n");
        }
        
        /* Wait for child to exit */
        int exit_status;
        result = pc_parent_wait_for_child_exit(pc, &exit_status);
        if (result == PC_SUCCESS) {
            printf("Parent: Child exited with status %d\n", WEXITSTATUS(exit_status));
        }
        
        pc_destroy(pc);
        
    } else {
        fprintf(stderr, "Fork failed\n");
        pc_destroy(pc);
        return 1;
    }
    
    printf("Example 1 completed successfully!\n\n");
    return 0;
}

/* Example 2: Ready signaling pattern */
int example_ready_signaling(void) {
    printf("=== Example 2: Ready Signaling Pattern ===\n");
    
    ProcessCoordinator *pc = pc_create();
    if (!pc) {
        fprintf(stderr, "Failed to create ProcessCoordinator\n");
        return 1;
    }
    
    pc_result_t result = pc_prepare_fork(pc);
    if (result != PC_SUCCESS) {
        fprintf(stderr, "Failed to prepare fork: %s\n", pc_get_error_string(pc));
        pc_destroy(pc);
        return 1;
    }
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        result = pc_after_fork_child(pc);
        if (result != PC_SUCCESS) {
            pc_destroy(pc);
            exit(1);
        }
        
        /* Simulate initialization work */
        printf("Child: Initializing...\n");
        sleep(1);
        
        /* Signal ready to parent */
        result = pc_child_signal_ready(pc);
        if (result != PC_SUCCESS) {
            fprintf(stderr, "Child: Failed to signal ready: %s\n", pc_get_error_string(pc));
            pc_destroy(pc);
            exit(1);
        }
        
        printf("Child: Ready signal sent\n");
        
        /* Wait for parent's acknowledgment */
        char ack;
        result = pc_child_receive(pc, &ack, 1, 5000);
        if (result == PC_SUCCESS && ack == 'A') {
            printf("Child: Received acknowledgment from parent\n");
        }
        
        pc_destroy(pc);
        exit(0);
        
    } else if (child_pid > 0) {
        /* Parent process */
        result = pc_after_fork_parent(pc, child_pid);
        if (result != PC_SUCCESS) {
            pc_destroy(pc);
            return 1;
        }
        
        printf("Parent: Waiting for child to be ready...\n");
        
        /* Wait for child ready signal */
        result = pc_parent_wait_for_child_ready(pc, 5000);
        if (result == PC_SUCCESS) {
            printf("Parent: Child is ready!\n");
            
            /* Send acknowledgment */
            char ack = 'A';
            pc_parent_send(pc, &ack, 1);
        } else {
            fprintf(stderr, "Parent: Child ready timeout: %s\n", pc_get_error_string(pc));
        }
        
        /* Wait for child to exit */
        int exit_status;
        pc_parent_wait_for_child_exit(pc, &exit_status);
        
        pc_destroy(pc);
        
    } else {
        fprintf(stderr, "Fork failed\n");
        pc_destroy(pc);
        return 1;
    }
    
    printf("Example 2 completed successfully!\n\n");
    return 0;
}

/* Example 3: Error handling and timeouts */
int example_error_handling(void) {
    printf("=== Example 3: Error Handling and Timeouts ===\n");
    
    ProcessCoordinator *pc = pc_create();
    if (!pc) {
        fprintf(stderr, "Failed to create ProcessCoordinator\n");
        return 1;
    }
    
    pc_result_t result = pc_prepare_fork(pc);
    if (result != PC_SUCCESS) {
        fprintf(stderr, "Failed to prepare fork: %s\n", pc_get_error_string(pc));
        pc_destroy(pc);
        return 1;
    }
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process - deliberately don't send anything to test timeout */
        result = pc_after_fork_child(pc);
        if (result != PC_SUCCESS) {
            pc_destroy(pc);
            exit(1);
        }
        
        printf("Child: Working without sending messages (testing timeout)...\n");
        sleep(3); /* Work for 3 seconds without communicating */
        
        printf("Child: Exiting without communication\n");
        pc_destroy(pc);
        exit(0);
        
    } else if (child_pid > 0) {
        /* Parent process */
        result = pc_after_fork_parent(pc, child_pid);
        if (result != PC_SUCCESS) {
            pc_destroy(pc);
            return 1;
        }
        
        printf("Parent: Trying to receive with 1 second timeout...\n");
        
        /* Try to receive with timeout */
        char data;
        result = pc_parent_receive(pc, &data, 1, 1000); /* 1 second timeout */
        if (result == PC_ERROR_TIMEOUT) {
            printf("Parent: Correctly timed out waiting for child data\n");
        } else if (result == PC_SUCCESS) {
            printf("Parent: Unexpectedly received data\n");
        } else {
            printf("Parent: Error receiving data: %s\n", pc_get_error_string(pc));
        }
        
        /* Wait for child to exit */
        int exit_status;
        pc_parent_wait_for_child_exit(pc, &exit_status);
        printf("Parent: Child exited\n");
        
        pc_destroy(pc);
        
    } else {
        fprintf(stderr, "Fork failed\n");
        pc_destroy(pc);
        return 1;
    }
    
    printf("Example 3 completed successfully!\n\n");
    return 0;
}

int main(void) {
    printf("ProcessCoordinator Usage Examples\n");
    printf("=================================\n\n");
    
    int result = 0;
    
    result |= example_simple_coordination();
    result |= example_ready_signaling();
    result |= example_error_handling();
    
    if (result == 0) {
        printf("All examples completed successfully!\n");
    } else {
        printf("Some examples failed.\n");
    }
    
    return result;
}