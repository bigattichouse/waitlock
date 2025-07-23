#include "pipe_coordinator.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

PipeCoordinator *pipe_coordinator_create() {
    PipeCoordinator *pc = (PipeCoordinator *)malloc(sizeof(PipeCoordinator));
    if (pc == NULL) {
        perror("Failed to allocate PipeCoordinator");
        return NULL;
    }

    if (pipe(pc->pipefd) == -1) {
        perror("Failed to create pipe");
        free(pc);
        return NULL;
    }

    pc->child_pid = -1; // Initialize child PID to -1 (no child yet)
    return pc;
}

int pipe_coordinator_get_read_fd(PipeCoordinator *pc) {
    if (pc == NULL) return -1;
    return pc->pipefd[0];
}

int pipe_coordinator_get_write_fd(PipeCoordinator *pc) {
    if (pc == NULL) return -1;
    return pc->pipefd[1];
}

pid_t pipe_coordinator_get_child_pid(PipeCoordinator *pc) {
    if (pc == NULL) return -1;
    return pc->child_pid;
}

void pipe_coordinator_set_child_pid(PipeCoordinator *pc, pid_t pid) {
    if (pc != NULL) {
        pc->child_pid = pid;
    }
}

void pipe_coordinator_parent_close_read(PipeCoordinator *pc) {
    if (pc != NULL && pc->pipefd[0] != -1) {
        close(pc->pipefd[0]);
        pc->pipefd[0] = -1; // Mark as closed
    }
}

void pipe_coordinator_child_close_write(PipeCoordinator *pc) {
    if (pc != NULL && pc->pipefd[1] != -1) {
        close(pc->pipefd[1]);
        pc->pipefd[1] = -1; // Mark as closed
    }
}

void pipe_coordinator_close_read_end(PipeCoordinator *pc) {
    if (pc != NULL && pc->pipefd[0] != -1) {
        close(pc->pipefd[0]);
        pc->pipefd[0] = -1; // Mark as closed
    }
}

void pipe_coordinator_close_write_end(PipeCoordinator *pc) {
    if (pc != NULL && pc->pipefd[1] != -1) {
        close(pc->pipefd[1]);
        pc->pipefd[1] = -1; // Mark as closed
    }
}

ssize_t pipe_coordinator_write(PipeCoordinator *pc, const void *buf, size_t count) {
    if (pc == NULL || pc->pipefd[1] == -1) {
        errno = EBADF; // Bad file descriptor
        return -1;
    }
    return write(pc->pipefd[1], buf, count);
}

ssize_t pipe_coordinator_read(PipeCoordinator *pc, void *buf, size_t count) {
    if (pc == NULL || pc->pipefd[0] == -1) {
        errno = EBADF; // Bad file descriptor
        return -1;
    }
    return read(pc->pipefd[0], buf, count);
}

int pipe_coordinator_wait_for_child(PipeCoordinator *pc, int *status) {
    if (pc == NULL || pc->child_pid == -1) {
        errno = EINVAL; // Invalid argument
        return -1;
    }
    return waitpid(pc->child_pid, status, 0);
}

void pipe_coordinator_destroy(PipeCoordinator *pc) {
    if (pc != NULL) {
        // Close any open file descriptors
        pipe_coordinator_close_read_end(pc);
        pipe_coordinator_close_write_end(pc);
        free(pc);
    }
}
