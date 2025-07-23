#include "process_coordinator.h"
#include <sys/select.h>
#include <sys/time.h>

/* Internal helper functions */
static pc_result_t pc_set_error(ProcessCoordinator *pc, pc_result_t error, const char *msg);
static pc_result_t pc_validate_state(ProcessCoordinator *pc, pc_state_t expected_state);
static pc_result_t pc_close_pipe_safe(int *pipe_fd);
static pc_result_t pc_wait_for_data(int fd, int timeout_ms);

/* Create and initialize a ProcessCoordinator */
ProcessCoordinator* pc_create(void) {
    ProcessCoordinator *pc = malloc(sizeof(ProcessCoordinator));
    if (!pc) {
        return NULL;
    }
    
    /* Initialize all fields to safe defaults */
    pc->parent_to_child[0] = -1;
    pc->parent_to_child[1] = -1;
    pc->child_to_parent[0] = -1;
    pc->child_to_parent[1] = -1;
    
    pc->child_pid = -1;
    pc->role = PC_ROLE_UNSET;
    pc->state = PC_STATE_UNINITIALIZED;
    
    pc->pipes_created = 0;
    pc->forks_done = 0;
    
    pc->cleanup_in_progress = 0;
    pc->child_exited = 0;
    
    pc->last_error = PC_SUCCESS;
    memset(pc->error_msg, 0, sizeof(pc->error_msg));
    
    return pc;
}

/* Prepare for fork by creating communication pipes */
pc_result_t pc_prepare_fork(ProcessCoordinator *pc) {
    if (!pc) return PC_ERROR_INVALID_STATE;
    
    if (pc->state != PC_STATE_UNINITIALIZED) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "ProcessCoordinator already initialized");
    }
    
    /* Create parent->child pipe */
    if (pipe(pc->parent_to_child) != 0) {
        return pc_set_error(pc, PC_ERROR_PIPE_FAILED, "Failed to create parent->child pipe");
    }
    
    /* Create child->parent pipe */
    if (pipe(pc->child_to_parent) != 0) {
        pc_close_pipe_safe(&pc->parent_to_child[0]);
        pc_close_pipe_safe(&pc->parent_to_child[1]);
        return pc_set_error(pc, PC_ERROR_PIPE_FAILED, "Failed to create child->parent pipe");
    }
    
    pc->pipes_created = 1;
    pc->state = PC_STATE_READY;
    
    return PC_SUCCESS;
}

/* Parent calls this after successful fork */
pc_result_t pc_after_fork_parent(ProcessCoordinator *pc, pid_t child_pid) {
    if (!pc) return PC_ERROR_INVALID_STATE;
    
    pc_result_t result = pc_validate_state(pc, PC_STATE_READY);
    if (result != PC_SUCCESS) return result;
    
    if (child_pid <= 0) {
        return pc_set_error(pc, PC_ERROR_FORK_FAILED, "Invalid child PID");
    }
    
    /* Set up parent role */
    pc->role = PC_ROLE_PARENT;
    pc->child_pid = child_pid;
    pc->forks_done = 1;
    
    /* Parent closes read end of parent->child pipe and write end of child->parent pipe */
    pc_close_pipe_safe(&pc->parent_to_child[0]);
    pc_close_pipe_safe(&pc->child_to_parent[1]);
    
    pc->state = PC_STATE_FORKED;
    return PC_SUCCESS;
}

/* Child calls this after fork */
pc_result_t pc_after_fork_child(ProcessCoordinator *pc) {
    if (!pc) return PC_ERROR_INVALID_STATE;
    
    pc_result_t result = pc_validate_state(pc, PC_STATE_READY);
    if (result != PC_SUCCESS) return result;
    
    /* Set up child role */
    pc->role = PC_ROLE_CHILD;
    pc->child_pid = getpid();
    pc->forks_done = 1;
    
    /* Child closes write end of parent->child pipe and read end of child->parent pipe */
    pc_close_pipe_safe(&pc->parent_to_child[1]);
    pc_close_pipe_safe(&pc->child_to_parent[0]);
    
    pc->state = PC_STATE_FORKED;
    return PC_SUCCESS;
}

/* Parent sends data to child */
pc_result_t pc_parent_send(ProcessCoordinator *pc, const void *data, size_t len) {
    if (!pc || !data) return PC_ERROR_INVALID_STATE;
    
    if (pc->role != PC_ROLE_PARENT) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Only parent can send to child");
    }
    
    if (pc->parent_to_child[1] == -1) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Parent->child pipe not available");
    }
    
    ssize_t written = write(pc->parent_to_child[1], data, len);
    if (written != (ssize_t)len) {
        return pc_set_error(pc, PC_ERROR_IO_FAILED, "Failed to write to child");
    }
    
    return PC_SUCCESS;
}

/* Parent receives data from child */
pc_result_t pc_parent_receive(ProcessCoordinator *pc, void *data, size_t len, int timeout_ms) {
    if (!pc || !data) return PC_ERROR_INVALID_STATE;
    
    if (pc->role != PC_ROLE_PARENT) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Only parent can receive from child");
    }
    
    if (pc->child_to_parent[0] == -1) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Child->parent pipe not available");
    }
    
    /* Wait for data with timeout */
    pc_result_t wait_result = pc_wait_for_data(pc->child_to_parent[0], timeout_ms);
    if (wait_result != PC_SUCCESS) {
        return wait_result;
    }
    
    ssize_t bytes_read = read(pc->child_to_parent[0], data, len);
    if (bytes_read != (ssize_t)len) {
        return pc_set_error(pc, PC_ERROR_IO_FAILED, "Failed to read from child");
    }
    
    return PC_SUCCESS;
}

/* Child sends data to parent */
pc_result_t pc_child_send(ProcessCoordinator *pc, const void *data, size_t len) {
    if (!pc || !data) return PC_ERROR_INVALID_STATE;
    
    if (pc->role != PC_ROLE_CHILD) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Only child can send to parent");
    }
    
    if (pc->child_to_parent[1] == -1) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Child->parent pipe not available");
    }
    
    ssize_t written = write(pc->child_to_parent[1], data, len);
    if (written != (ssize_t)len) {
        return pc_set_error(pc, PC_ERROR_IO_FAILED, "Failed to write to parent");
    }
    
    return PC_SUCCESS;
}

/* Child receives data from parent */
pc_result_t pc_child_receive(ProcessCoordinator *pc, void *data, size_t len, int timeout_ms) {
    if (!pc || !data) return PC_ERROR_INVALID_STATE;
    
    if (pc->role != PC_ROLE_CHILD) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Only child can receive from parent");
    }
    
    if (pc->parent_to_child[0] == -1) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Parent->child pipe not available");
    }
    
    /* Wait for data with timeout */
    pc_result_t wait_result = pc_wait_for_data(pc->parent_to_child[0], timeout_ms);
    if (wait_result != PC_SUCCESS) {
        return wait_result;
    }
    
    ssize_t bytes_read = read(pc->parent_to_child[0], data, len);
    if (bytes_read != (ssize_t)len) {
        return pc_set_error(pc, PC_ERROR_IO_FAILED, "Failed to read from parent");
    }
    
    return PC_SUCCESS;
}

/* Parent waits for child to signal ready */
pc_result_t pc_parent_wait_for_child_ready(ProcessCoordinator *pc, int timeout_ms) {
    if (!pc) return PC_ERROR_INVALID_STATE;
    
    if (pc->role != PC_ROLE_PARENT) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Only parent can wait for child ready");
    }
    
    char ready_signal;
    pc_result_t result = pc_parent_receive(pc, &ready_signal, 1, timeout_ms);
    if (result != PC_SUCCESS) {
        return result;
    }
    
    if (ready_signal != 'R') {
        return pc_set_error(pc, PC_ERROR_IO_FAILED, "Invalid ready signal from child");
    }
    
    pc->state = PC_STATE_COORDINATING;
    return PC_SUCCESS;
}

/* Child signals ready to parent */
pc_result_t pc_child_signal_ready(ProcessCoordinator *pc) {
    if (!pc) return PC_ERROR_INVALID_STATE;
    
    if (pc->role != PC_ROLE_CHILD) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Only child can signal ready");
    }
    
    char ready_signal = 'R';
    pc_result_t result = pc_child_send(pc, &ready_signal, 1);
    if (result == PC_SUCCESS) {
        pc->state = PC_STATE_COORDINATING;
    }
    
    return result;
}

/* Parent waits for child to exit */
pc_result_t pc_parent_wait_for_child_exit(ProcessCoordinator *pc, int *exit_status) {
    if (!pc) return PC_ERROR_INVALID_STATE;
    
    if (pc->role != PC_ROLE_PARENT) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Only parent can wait for child exit");
    }
    
    if (pc->child_pid <= 0) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "No child process to wait for");
    }
    
    int status;
    pid_t result = waitpid(pc->child_pid, &status, 0);
    if (result != pc->child_pid) {
        return pc_set_error(pc, PC_ERROR_CHILD_DIED, "waitpid failed");
    }
    
    if (exit_status) {
        *exit_status = status;
    }
    
    pc->child_exited = 1;
    pc->state = PC_STATE_COMPLETED;
    return PC_SUCCESS;
}

/* Child exits cleanly */
pc_result_t pc_child_exit(ProcessCoordinator *pc, int exit_code) {
    if (!pc) return PC_ERROR_INVALID_STATE;
    
    if (pc->role != PC_ROLE_CHILD) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "Only child can exit");
    }
    
    /* Clean up child resources before exit */
    pc_close_pipe_safe(&pc->parent_to_child[0]);
    pc_close_pipe_safe(&pc->child_to_parent[1]);
    
    pc->state = PC_STATE_COMPLETED;
    
    /* Note: pc_destroy should be called before this, but we clean up just in case */
    exit(exit_code);
}

/* Get error string */
const char* pc_get_error_string(ProcessCoordinator *pc) {
    if (!pc) return "Invalid ProcessCoordinator";
    return pc->error_msg[0] ? pc->error_msg : "No error";
}

/* Get current state */
pc_state_t pc_get_state(ProcessCoordinator *pc) {
    if (!pc) return PC_STATE_ERROR;
    return pc->state;
}

/* Check if child is alive */
int pc_is_child_alive(ProcessCoordinator *pc) {
    if (!pc || pc->role != PC_ROLE_PARENT || pc->child_pid <= 0) {
        return 0;
    }
    
    /* Use kill(pid, 0) to check if process exists */
    return (kill(pc->child_pid, 0) == 0);
}

/* Clean up and free ProcessCoordinator */
void pc_destroy(ProcessCoordinator *pc) {
    if (!pc) return;
    
    /* Prevent re-entrant cleanup */
    if (pc->cleanup_in_progress) return;
    pc->cleanup_in_progress = 1;
    
    /* Close all pipe file descriptors */
    pc_close_pipe_safe(&pc->parent_to_child[0]);
    pc_close_pipe_safe(&pc->parent_to_child[1]);
    pc_close_pipe_safe(&pc->child_to_parent[0]);
    pc_close_pipe_safe(&pc->child_to_parent[1]);
    
    /* Clean up child process if we're the parent and child is still alive */
    if (pc->role == PC_ROLE_PARENT && pc->child_pid > 0 && !pc->child_exited) {
        if (pc_is_child_alive(pc)) {
            kill(pc->child_pid, SIGTERM);
            /* Give child time to clean up */
            usleep(100000); /* 100ms */
            if (pc_is_child_alive(pc)) {
                kill(pc->child_pid, SIGKILL);
            }
            /* Wait for child to avoid zombies */
            int status;
            waitpid(pc->child_pid, &status, WNOHANG);
        }
    }
    
    free(pc);
}

/* Emergency cleanup for signal handlers */
void pc_emergency_cleanup(ProcessCoordinator *pc) {
    if (!pc) return;
    
    /* Quick, signal-safe cleanup */
    if (pc->parent_to_child[0] != -1) close(pc->parent_to_child[0]);
    if (pc->parent_to_child[1] != -1) close(pc->parent_to_child[1]);
    if (pc->child_to_parent[0] != -1) close(pc->child_to_parent[0]);
    if (pc->child_to_parent[1] != -1) close(pc->child_to_parent[1]);
}

/* Internal helper functions */

static pc_result_t pc_set_error(ProcessCoordinator *pc, pc_result_t error, const char *msg) {
    if (!pc) return error;
    
    pc->last_error = error;
    pc->state = PC_STATE_ERROR;
    
    if (msg) {
        snprintf(pc->error_msg, sizeof(pc->error_msg), "%s", msg);
    }
    
    return error;
}

static pc_result_t pc_validate_state(ProcessCoordinator *pc, pc_state_t expected_state) {
    if (!pc) return PC_ERROR_INVALID_STATE;
    
    if (pc->state != expected_state) {
        return pc_set_error(pc, PC_ERROR_INVALID_STATE, "ProcessCoordinator in invalid state");
    }
    
    return PC_SUCCESS;
}

static pc_result_t pc_close_pipe_safe(int *pipe_fd) {
    if (!pipe_fd || *pipe_fd == -1) return PC_SUCCESS;
    
    close(*pipe_fd);
    *pipe_fd = -1;
    
    return PC_SUCCESS;
}

static pc_result_t pc_wait_for_data(int fd, int timeout_ms) {
    if (fd == -1) return PC_ERROR_INVALID_STATE;
    
    fd_set readfds;
    struct timeval timeout;
    
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    if (timeout_ms > 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
    }
    
    int result = select(fd + 1, &readfds, NULL, NULL, timeout_ms > 0 ? &timeout : NULL);
    
    if (result == 0) {
        return PC_ERROR_TIMEOUT;
    } else if (result < 0) {
        return PC_ERROR_IO_FAILED;
    }
    
    return PC_SUCCESS;
}