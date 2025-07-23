#ifndef PROCESS_COORDINATOR_H
#define PROCESS_COORDINATOR_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdio.h>  
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

/* Simple ProcessCoordinator for inter-process communication */
/* Based on proven pipe + select pattern from debug_semaphore_exec.c */

#define PC_PARENT_TO_CHILD 0
#define PC_CHILD_TO_PARENT 1

#define PC_ROLE_UNSET  0
#define PC_ROLE_PARENT 1
#define PC_ROLE_CHILD  2

/* Simplified ProcessCoordinator structure */
typedef struct {
    int pipes[2][2];          /* [parent_to_child][2], [child_to_parent][2] */
    pid_t child_pid;
    int role;                 /* PC_ROLE_UNSET/PARENT/CHILD */
    char error_msg[256];
} ProcessCoordinator;

/* Core lifecycle functions */
ProcessCoordinator* pc_create(void);
int pc_prepare_fork(ProcessCoordinator *pc);
int pc_after_fork_parent(ProcessCoordinator *pc, pid_t child_pid);
int pc_after_fork_child(ProcessCoordinator *pc);
void pc_destroy(ProcessCoordinator *pc);

/* Communication functions - return bytes read/written or -1 on error */
ssize_t pc_send(ProcessCoordinator *pc, const void *data, size_t len);
ssize_t pc_receive(ProcessCoordinator *pc, void *data, size_t len, int timeout_ms);

/* Simple coordination helpers */
int pc_wait_for_signal(ProcessCoordinator *pc, char expected_signal, int timeout_ms);
int pc_send_signal(ProcessCoordinator *pc, char signal);

/* Status and error handling */
const char* pc_get_error_string(ProcessCoordinator *pc);
int pc_is_child_alive(ProcessCoordinator *pc);

/* Legacy API compatibility - maps to new simple functions */
typedef int pc_result_t;
#define PC_SUCCESS 0

pc_result_t pc_parent_send(ProcessCoordinator *pc, const void *data, size_t len);
pc_result_t pc_parent_receive(ProcessCoordinator *pc, void *data, size_t len, int timeout_ms);
pc_result_t pc_child_send(ProcessCoordinator *pc, const void *data, size_t len);
pc_result_t pc_child_receive(ProcessCoordinator *pc, void *data, size_t len, int timeout_ms);
pc_result_t pc_parent_wait_for_child_ready(ProcessCoordinator *pc, int timeout_ms);
pc_result_t pc_child_signal_ready(ProcessCoordinator *pc);
pc_result_t pc_parent_wait_for_child_exit(ProcessCoordinator *pc, int *exit_status);

#endif /* PROCESS_COORDINATOR_H */