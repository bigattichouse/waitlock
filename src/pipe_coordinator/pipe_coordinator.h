#ifndef PIPE_COORDINATOR_H
#define PIPE_COORDINATOR_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int pipefd[2];
    pid_t child_pid;
} PipeCoordinator;

// Function to create and initialize a PipeCoordinator
// Returns NULL on failure, otherwise a pointer to the new PipeCoordinator
PipeCoordinator *pipe_coordinator_create();

// Function to get the read file descriptor
int pipe_coordinator_get_read_fd(PipeCoordinator *pc);

// Function to get the write file descriptor
int pipe_coordinator_get_write_fd(PipeCoordinator *pc);

// Function to get the child PID
pid_t pipe_coordinator_get_child_pid(PipeCoordinator *pc);

// Function to set the child PID
void pipe_coordinator_set_child_pid(PipeCoordinator *pc, pid_t pid);

// Function for parent to close its read end of the pipe
void pipe_coordinator_parent_close_read(PipeCoordinator *pc);

// Function for child to close its write end of the pipe
void pipe_coordinator_child_close_write(PipeCoordinator *pc);

// Function to close the read end of the pipe
void pipe_coordinator_close_read_end(PipeCoordinator *pc);

// Function to close the write end of the pipe
void pipe_coordinator_close_write_end(PipeCoordinator *pc);

// Function to write to the pipe
ssize_t pipe_coordinator_write(PipeCoordinator *pc, const void *buf, size_t count);

// Function to read from the pipe
ssize_t pipe_coordinator_read(PipeCoordinator *pc, void *buf, size_t count);

// Function to wait for the child process to terminate
// Returns the exit status of the child, or -1 on error
int pipe_coordinator_wait_for_child(PipeCoordinator *pc, int *status);

// Function to clean up and free the PipeCoordinator
void pipe_coordinator_destroy(PipeCoordinator *pc);

#endif // PIPE_COORDINATOR_H