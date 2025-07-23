#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

/* Include waitlock headers */
#include "../../src/waitlock.h"

/* We need to test if global state corruption is the issue */
int main() {
    printf("Testing global state corruption hypothesis...\n");
    
    extern struct global_state g_state;
    extern struct options opts;
    
    /* Set up test environment */
    const char *descriptor = "test_global_state";
    
    printf("Initial parent state: lock_fd=%d, lock_path='%s'\n", 
           g_state.lock_fd, g_state.lock_path);
    
    /* Parent tries to acquire a lock */
    printf("Parent acquiring lock...\n");
    int parent_result = acquire_lock(descriptor, 3, 2.0);
    
    printf("Parent result: %d, lock_fd=%d, lock_path='%s'\n", 
           parent_result, g_state.lock_fd, g_state.lock_path);
    
    if (parent_result != 0) {
        printf("ERROR: Parent failed to acquire lock!\n");
        return 1;
    }
    
    /* Now fork a child */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        printf("Child BEFORE reset: lock_fd=%d, lock_path='%s'\n", 
               g_state.lock_fd, g_state.lock_path);
        
        /* Reset global state */
        g_state.lock_fd = -1;
        g_state.lock_path[0] = '\0';
        g_state.child_pid = 0;
        
        printf("Child AFTER reset: lock_fd=%d, lock_path='%s'\n", 
               g_state.lock_fd, g_state.lock_path);
        
        /* Try to acquire another slot */
        printf("Child trying to acquire slot...\n");
        int child_result = acquire_lock(descriptor, 3, 2.0);
        
        printf("Child result: %d, lock_fd=%d, lock_path='%s'\n", 
               child_result, g_state.lock_fd, g_state.lock_path);
        
        if (child_result == 0) {
            printf("Child successfully acquired slot - will release in 2s\n");
            sleep(2);
            release_lock();
            printf("Child released lock\n");
        } else {
            printf("Child failed to acquire slot (expected if max reached)\n");
        }
        
        exit(child_result == 0 ? 0 : 1);
        
    } else if (child_pid > 0) {
        /* Parent waits */
        int status;
        waitpid(child_pid, &status, 0);
        
        printf("Parent: Child exited with status %d\n", 
               WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        
        /* Parent releases its lock */
        printf("Parent releasing lock...\n");
        release_lock();
        
        printf("Test complete\n");
        return 0;
        
    } else {
        printf("Fork failed\n");
        return 1;
    }
}
