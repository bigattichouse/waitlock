#define _XOPEN_SOURCE 500 // Required for usleep on some systems
#include "process_coordinator.h"
#include "../debug_utils.h"
#include <unistd.h> // For usleep
#include <signal.h> // For kill
#include <errno.h> // For errno
#include <sys/types.h> // For pid_t

/* Simple ProcessCoordinator implementation based on debug_semaphore_exec.c pattern */
/* No complex state machine, just reliable pipes + select() */

static int pc_set_error(ProcessCoordinator *pc, const char *msg);
static int pc_wait_for_data(int fd, int timeout_ms);
static void pc_close_pipe_safe(int *fd);

/* Create and initialize a ProcessCoordinator */
ProcessCoordinator* pc_create(void) {
    ProcessCoordinator *pc = malloc(sizeof(ProcessCoordinator));
    if (!pc) {
        return NULL;
    }
    
    /* Initialize all pipe FDs to -1 (closed) */
    pc->pipes[PC_PARENT_TO_CHILD][0] = -1;  /* parent writes, child reads */
    pc->pipes[PC_PARENT_TO_CHILD][1] = -1;
    pc->pipes[PC_CHILD_TO_PARENT][0] = -1;  /* child writes, parent reads */
    pc->pipes[PC_CHILD_TO_PARENT][1] = -1;
    
    pc->child_pid = -1;
    pc->role = PC_ROLE_UNSET;
    pc->error_msg[0] = '\0';
    
    return pc;
}

/* Prepare for fork by creating communication pipes */
int pc_prepare_fork(ProcessCoordinator *pc) {
    if (!pc) return -1;
    
    if (pc->role != PC_ROLE_UNSET) {
        return pc_set_error(pc, "ProcessCoordinator already initialized");
    }
    
    /* Create parent->child pipe */
    if (pipe(pc->pipes[PC_PARENT_TO_CHILD]) != 0) {
        return pc_set_error(pc, "Failed to create parent->child pipe");
    }
    
    /* Create child->parent pipe */
    if (pipe(pc->pipes[PC_CHILD_TO_PARENT]) != 0) {
        pc_close_pipe_safe(&pc->pipes[PC_PARENT_TO_CHILD][0]);
        pc_close_pipe_safe(&pc->pipes[PC_PARENT_TO_CHILD][1]);
        return pc_set_error(pc, "Failed to create child->parent pipe");
    }
    
    /* Mark as prepared to prevent double preparation */
    pc->role = PC_ROLE_PREPARED;
    
    return 0;
}

/* Parent calls this after successful fork */
int pc_after_fork_parent(ProcessCoordinator *pc, pid_t child_pid) {
    if (!pc || child_pid <= 0) return -1;
    
    if (pc->role != PC_ROLE_PREPARED) {
        return pc_set_error(pc, "ProcessCoordinator not prepared for fork");
    }
    
    pc->role = PC_ROLE_PARENT;
    pc->child_pid = child_pid;
    
    /* Parent closes unused pipe ends */
    pc_close_pipe_safe(&pc->pipes[PC_PARENT_TO_CHILD][0]); /* Close read end of parent->child */
    pc_close_pipe_safe(&pc->pipes[PC_CHILD_TO_PARENT][1]); /* Close write end of child->parent */
    
    return 0;
}

/* Child calls this after fork */
int pc_after_fork_child(ProcessCoordinator *pc) {
    if (!pc) return -1;
    
    if (pc->role != PC_ROLE_PREPARED) {
        return pc_set_error(pc, "ProcessCoordinator not prepared for fork");
    }
    
    pc->role = PC_ROLE_CHILD;
    pc->child_pid = getpid();
    
    /* Child closes unused pipe ends */
    pc_close_pipe_safe(&pc->pipes[PC_PARENT_TO_CHILD][1]); /* Close write end of parent->child */
    pc_close_pipe_safe(&pc->pipes[PC_CHILD_TO_PARENT][0]); /* Close read end of child->parent */
    
    return 0;
}

/* Send data - automatically chooses correct pipe based on role */
ssize_t pc_send(ProcessCoordinator *pc, const void *data, size_t len) {
    if (!pc || !data) return -1;
    
    int write_fd = -1;
    
    if (pc->role == PC_ROLE_PARENT) {
        write_fd = pc->pipes[PC_PARENT_TO_CHILD][1]; /* Parent writes to child */
    } else if (pc->role == PC_ROLE_CHILD) {
        write_fd = pc->pipes[PC_CHILD_TO_PARENT][1]; /* Child writes to parent */
    } else {
        pc_set_error(pc, "Invalid role for sending");
        return -1;
    }
    
    if (write_fd == -1) {
        pc_set_error(pc, "Write pipe not available");
        return -1;
    }
    
    ssize_t written = write(write_fd, data, len);
    if (written != (ssize_t)len) {
        pc_set_error(pc, "Failed to write all data");
        return -1;
    }
    
    /* Ensure data is flushed to the pipe immediately */
    fsync(write_fd);
    
    return written;
}

/* Receive data - automatically chooses correct pipe based on role */
ssize_t pc_receive(ProcessCoordinator *pc, void *data, size_t len, int timeout_ms) {
    if (!pc || !data) return -1;
    
    int read_fd = -1;
    
    if (pc->role == PC_ROLE_PARENT) {
        read_fd = pc->pipes[PC_CHILD_TO_PARENT][0]; /* Parent reads from child */
    } else if (pc->role == PC_ROLE_CHILD) {
        read_fd = pc->pipes[PC_PARENT_TO_CHILD][0]; /* Child reads from parent */
    } else {
        pc_set_error(pc, "Invalid role for receiving");
        return -1;
    }
    
    if (read_fd == -1) {
        pc_set_error(pc, "Read pipe not available");
        return -1;
    }
    
    /* Wait for data with timeout */
    if (pc_wait_for_data(read_fd, timeout_ms) != 0) {
        pc_set_error(pc, "Timeout waiting for data");
        return -1;
    }
    
    ssize_t bytes_read = read(read_fd, data, len);
    debug("pc_receive: read_fd=%d, len=%zu, bytes_read=%zd, errno=%d\n", read_fd, len, bytes_read, errno);
    if (bytes_read <= 0) {
        pc_set_error(pc, bytes_read == 0 ? "Pipe closed unexpectedly" : "Read failed");
        return -1;
    }
    
    return bytes_read;
}

/* Wait for a specific signal character */
int pc_wait_for_signal(ProcessCoordinator *pc, char expected_signal, int timeout_ms) {
    char signal;
    ssize_t result = pc_receive(pc, &signal, 1, timeout_ms);
    
    if (result != 1) {
        return -1;
    }
    
    if (signal != expected_signal) {
        pc_set_error(pc, "Received unexpected signal");
        return -1;
    }
    
    return 0;
}

/* Send a signal character */
int pc_send_signal(ProcessCoordinator *pc, char signal) {
    ssize_t result = pc_send(pc, &signal, 1);
    return (result == 1) ? 0 : -1;
}

/* Check if child is alive */
int pc_is_child_alive(ProcessCoordinator *pc) {
    if (!pc || pc->role != PC_ROLE_PARENT || pc->child_pid <= 0) {
        return 0;
    }
    
    /* Use kill(pid, 0) to check if process exists */
    return (kill(pc->child_pid, 0) == 0);
}

/* Get error string */
const char* pc_get_error_string(ProcessCoordinator *pc) {
    if (!pc) return "Invalid ProcessCoordinator";
    return pc->error_msg[0] ? pc->error_msg : "No error";
}

/* Clean up and free ProcessCoordinator */
void pc_destroy(ProcessCoordinator *pc) {
    if (!pc) return;
    
    /* Close all pipe file descriptors */
    pc_close_pipe_safe(&pc->pipes[PC_PARENT_TO_CHILD][0]);
    pc_close_pipe_safe(&pc->pipes[PC_PARENT_TO_CHILD][1]);
    pc_close_pipe_safe(&pc->pipes[PC_CHILD_TO_PARENT][0]);
    pc_close_pipe_safe(&pc->pipes[PC_CHILD_TO_PARENT][1]);
    
    /* Clean up child process if we're the parent and child is still alive */
    if (pc->role == PC_ROLE_PARENT && pc->child_pid > 0) {
        if (pc_is_child_alive(pc)) {
            kill(pc->child_pid, SIGTERM);
            usleep(100000); /* 100ms grace period */
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

/* Legacy API compatibility functions */

pc_result_t pc_parent_send(ProcessCoordinator *pc, const void *data, size_t len) {
    if (!pc || pc->role != PC_ROLE_PARENT) return -1;
    return (pc_send(pc, data, len) == (ssize_t)len) ? PC_SUCCESS : -1;
}

pc_result_t pc_parent_receive(ProcessCoordinator *pc, void *data, size_t len, int timeout_ms) {
    if (!pc || pc->role != PC_ROLE_PARENT) return -1;
    ssize_t result = pc_receive(pc, data, len, timeout_ms);
    if (result > 0 && result < (ssize_t)len) {
        ((char*)data)[result] = '\0'; /* Null terminate if space allows */
    }
    return result; /* Return actual bytes read (positive) or -1 on error */
}

pc_result_t pc_child_send(ProcessCoordinator *pc, const void *data, size_t len) {
    if (!pc || pc->role != PC_ROLE_CHILD) return -1;
    return (pc_send(pc, data, len) == (ssize_t)len) ? PC_SUCCESS : -1;
}

pc_result_t pc_child_receive(ProcessCoordinator *pc, void *data, size_t len, int timeout_ms) {
    if (!pc || pc->role != PC_ROLE_CHILD) return -1;
    ssize_t result = pc_receive(pc, data, len, timeout_ms);
    if (result > 0 && result < (ssize_t)len) {
        ((char*)data)[result] = '\0'; /* Null terminate if space allows */
    }
    return result; /* Return actual bytes read (positive) or -1 on error */
}

pc_result_t pc_parent_wait_for_child_ready(ProcessCoordinator *pc, int timeout_ms) {
    if (!pc || pc->role != PC_ROLE_PARENT) return -1;
    return pc_wait_for_signal(pc, 'R', timeout_ms);
}

pc_result_t pc_child_signal_ready(ProcessCoordinator *pc) {
    if (!pc || pc->role != PC_ROLE_CHILD) return -1;
    return pc_send_signal(pc, 'R');
}

pc_result_t pc_parent_wait_for_child_exit(ProcessCoordinator *pc, int *exit_status) {
    if (!pc || pc->role != PC_ROLE_PARENT || pc->child_pid <= 0) return -1;
    
    int status;
    pid_t result = waitpid(pc->child_pid, &status, 0);
    if (result != pc->child_pid) {
        pc_set_error(pc, "waitpid failed");
        return -1;
    }
    
    if (exit_status) {
        *exit_status = status;
    }
    
    return PC_SUCCESS;
}

/* Internal helper functions */

static int pc_set_error(ProcessCoordinator *pc, const char *msg) {
    if (!pc || !msg) return -1;
    
    snprintf(pc->error_msg, sizeof(pc->error_msg), "%s", msg);
    return -1;
}

static void pc_close_pipe_safe(int *fd) {
    if (fd && *fd != -1) {
        close(*fd);
        *fd = -1;
    }
}

static int pc_wait_for_data(int fd, int timeout_ms) {
    if (fd == -1) return -1;
    
    fd_set readfds;
    struct timeval timeout;
    int result;
    
    do {
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        
        if (timeout_ms > 0) {
            timeout.tv_sec = timeout_ms / 1000;
            timeout.tv_usec = (timeout_ms % 1000) * 1000;
        }
        
        result = select(fd + 1, &readfds, NULL, NULL, timeout_ms > 0 ? &timeout : NULL);
        
        if (result == 0) {
            return -1; /* Timeout */
        } else if (result < 0 && errno != EINTR) {
            return -1; /* Error (but not interrupted) */
        }
        /* If EINTR, retry the select */
    } while (result < 0 && errno == EINTR);
    
    return 0; /* Data available */
}