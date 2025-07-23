#ifndef PROCESS_COORDINATOR_H
#define PROCESS_COORDINATOR_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>  
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

/* Status codes for ProcessCoordinator operations */
typedef enum {
    PC_SUCCESS = 0,
    PC_ERROR_INVALID_STATE = -1,
    PC_ERROR_PIPE_FAILED = -2,
    PC_ERROR_FORK_FAILED = -3,
    PC_ERROR_IO_FAILED = -4,
    PC_ERROR_TIMEOUT = -5,
    PC_ERROR_CHILD_DIED = -6
} pc_result_t;

/* Process roles */
typedef enum {
    PC_ROLE_UNSET = 0,
    PC_ROLE_PARENT,
    PC_ROLE_CHILD
} pc_role_t;

/* ProcessCoordinator state */
typedef enum {
    PC_STATE_UNINITIALIZED = 0,
    PC_STATE_READY,
    PC_STATE_FORKED,
    PC_STATE_COORDINATING,
    PC_STATE_COMPLETED,
    PC_STATE_ERROR
} pc_state_t;

/* ProcessCoordinator structure */
typedef struct {
    /* Communication pipes: parent->child and child->parent */
    int parent_to_child[2];    /* Parent writes, child reads */
    int child_to_parent[2];    /* Child writes, parent reads */
    
    /* Process information */
    pid_t child_pid;
    pc_role_t role;
    pc_state_t state;
    
    /* Resource tracking */
    int pipes_created;
    int forks_done;
    
    /* Safety flags */
    volatile sig_atomic_t cleanup_in_progress;
    volatile sig_atomic_t child_exited;
    
    /* Error tracking */
    int last_error;
    char error_msg[256];
} ProcessCoordinator;

/* Core lifecycle functions */
ProcessCoordinator* pc_create(void);
pc_result_t pc_prepare_fork(ProcessCoordinator *pc);
pc_result_t pc_after_fork_parent(ProcessCoordinator *pc, pid_t child_pid);
pc_result_t pc_after_fork_child(ProcessCoordinator *pc);
void pc_destroy(ProcessCoordinator *pc);

/* Communication functions */
pc_result_t pc_parent_send(ProcessCoordinator *pc, const void *data, size_t len);
pc_result_t pc_parent_receive(ProcessCoordinator *pc, void *data, size_t len, int timeout_ms);
pc_result_t pc_child_send(ProcessCoordinator *pc, const void *data, size_t len);
pc_result_t pc_child_receive(ProcessCoordinator *pc, void *data, size_t len, int timeout_ms);

/* Synchronization helpers */
pc_result_t pc_parent_wait_for_child_ready(ProcessCoordinator *pc, int timeout_ms);
pc_result_t pc_child_signal_ready(ProcessCoordinator *pc);
pc_result_t pc_parent_wait_for_child_exit(ProcessCoordinator *pc, int *exit_status);
pc_result_t pc_child_exit(ProcessCoordinator *pc, int exit_code);

/* Status and error handling */
const char* pc_get_error_string(ProcessCoordinator *pc);
pc_state_t pc_get_state(ProcessCoordinator *pc);
int pc_is_child_alive(ProcessCoordinator *pc);

/* Safety functions */
void pc_emergency_cleanup(ProcessCoordinator *pc);
pc_result_t pc_set_nonblocking(ProcessCoordinator *pc);

#endif /* PROCESS_COORDINATOR_H */