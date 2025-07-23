#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

/* Minimal includes for testing - we'll link against the built objects */
extern int acquire_lock(const char *descriptor, int max_holders, double timeout);
extern void release_lock(void);
extern int check_lock(const char *descriptor);
extern int exec_with_lock(const char *descriptor, char *argv[]);

/* Global options structure (defined in waitlock.c) */
extern struct {
    const char *descriptor;
    int max_holders;
    double timeout;
    int check_only;
    int list_mode;
    int done_mode;
    char **exec_argv;
} opts;

void test_semaphore_race_condition() {
    printf("\n=== SEMAPHORE RACE CONDITION TEST ===\n");
    
    const char *descriptor = "test_semaphore_race";
    int max_holders = 3;
    double timeout = 2.0;
    
    printf("Testing semaphore with max_holders=%d\n", max_holders);
    
    /* Set up options */
    opts.descriptor = descriptor;
    opts.max_holders = max_holders;
    opts.timeout = timeout;
    
    /* Step 1: Parent acquires first slot */
    printf("[Parent] Acquiring first slot...\n");
    int parent_result = acquire_lock(descriptor, max_holders, timeout);
    if (parent_result != 0) {
        printf("FAIL: Parent couldn't acquire first slot\n");
        return;
    }
    printf("PASS: Parent acquired first slot\n");
    
    /* Step 2: Fork children to acquire remaining slots */
    pid_t child_pids[3];
    int pipe_fds[3][2]; /* Pipes for coordination */
    
    for (int i = 0; i < 3; i++) {
        if (pipe(pipe_fds[i]) != 0) {
            printf("FAIL: Could not create pipe %d\n", i);
            return;
        }
    }
    
    printf("[Parent] Forking 3 children to test slot acquisition...\n");
    
    for (int i = 0; i < 3; i++) {
        child_pids[i] = fork();
        if (child_pids[i] == 0) {
            /* Child process */
            close(pipe_fds[i][0]); /* Close read end */
            
            printf("[Child %d] Attempting to acquire slot...\n", i + 2);
            int child_result = acquire_lock(descriptor, max_holders, timeout);
            
            if (child_result == 0) {
                printf("[Child %d] SUCCESS: Acquired slot\n", i + 2);
                /* Signal parent that we got the lock */
                char signal = 'Y';
                write(pipe_fds[i][1], &signal, 1);
                
                /* Hold the lock for a while */
                sleep(3);
                
                printf("[Child %d] Releasing slot\n", i + 2);
                release_lock();
                exit(0);
            } else {
                printf("[Child %d] FAILED: Could not acquire slot (expected for child 4)\n", i + 2);
                char signal = 'N';
                write(pipe_fds[i][1], &signal, 1);
                exit(1);
            }
        } else if (child_pids[i] < 0) {
            printf("FAIL: Could not fork child %d\n", i);
            return;
        }
    }
    
    /* Step 3: Wait for children to attempt acquisition */
    sleep(1);
    
    /* Step 4: Check which children succeeded */
    int successful_acquisitions = 1; /* Parent has one */
    char results[3];
    
    for (int i = 0; i < 3; i++) {
        close(pipe_fds[i][1]); /* Close write end */
        
        fd_set readfds;
        struct timeval timeout_tv = {5, 0}; /* 5 second timeout */
        FD_ZERO(&readfds);
        FD_SET(pipe_fds[i][0], &readfds);
        
        int ready = select(pipe_fds[i][0] + 1, &readfds, NULL, NULL, &timeout_tv);
        if (ready > 0) {
            read(pipe_fds[i][0], &results[i], 1);
            if (results[i] == 'Y') {
                successful_acquisitions++;
                printf("[Parent] Child %d successfully acquired slot\n", i + 2);
            } else {
                printf("[Parent] Child %d failed to acquire slot\n", i + 2);
            }
        } else {
            printf("[Parent] Child %d timed out or error\n", i + 2);
            results[i] = '?';
        }
        close(pipe_fds[i][0]);
    }
    
    printf("[Parent] Total successful acquisitions: %d/%d\n", successful_acquisitions, max_holders);
    
    /* Step 5: Analyze results */
    if (successful_acquisitions == max_holders) {
        if (results[2] == 'N') { /* Third child (index 2) should fail */
            printf("PASS: Semaphore correctly limited to %d holders\n", max_holders);
        } else {
            printf("FAIL: Expected child 4 to fail, but it succeeded\n");
        }
    } else if (successful_acquisitions > max_holders) {
        printf("FAIL: Too many processes acquired locks (%d > %d)\n", 
               successful_acquisitions, max_holders);
    } else {
        printf("UNEXPECTED: Fewer processes acquired locks than expected\n");
    }
    
    /* Step 6: Release parent lock and wait for children */
    printf("[Parent] Releasing parent slot\n");
    release_lock();
    
    for (int i = 0; i < 3; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
        printf("[Parent] Child %d exited with status %d\n", i + 2, 
               WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
    
    /* Step 7: Verify cleanup */
    sleep(1);
    int final_check = check_lock(descriptor);
    if (final_check == 0) {
        printf("PASS: All semaphore slots released\n");
    } else {
        printf("FAIL: Semaphore slots not properly released\n");
    }
}

void test_exec_timeout_issue() {
    printf("\n=== EXEC TIMEOUT ISSUE TEST ===\n");
    
    const char *descriptor = "test_exec_timeout";
    
    printf("Testing exec_with_lock timeout handling...\n");
    
    /* Test 1: Simple exec with proper timeout */
    printf("[Test 1] Testing simple exec with timeout=5.0...\n");
    opts.descriptor = descriptor;
    opts.max_holders = 1;
    opts.timeout = 5.0; /* Set proper timeout */
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        char *argv[] = {"echo", "Hello from exec test", NULL};
        printf("[Child] Calling exec_with_lock with timeout=%.1f\n", opts.timeout);
        int result = exec_with_lock(descriptor, argv);
        printf("[Child] exec_with_lock returned %d\n", result);
        exit(result);
    } else if (child_pid > 0) {
        int status;
        waitpid(child_pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("PASS: Simple exec succeeded\n");
        } else {
            printf("FAIL: Simple exec failed with status %d\n", 
                   WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        }
    }
    
    /* Test 2: Exec with lock contention */
    printf("[Test 2] Testing exec with lock contention...\n");
    
    /* First, create a long-running holder */
    pid_t holder_pid = fork();
    if (holder_pid == 0) {
        /* Holder process */
        printf("[Holder] Acquiring lock for 4 seconds...\n");
        int result = acquire_lock(descriptor, 1, 5.0);
        if (result == 0) {
            printf("[Holder] Lock acquired, sleeping...\n");
            sleep(4);
            printf("[Holder] Releasing lock\n");
            release_lock();
            exit(0);
        } else {
            printf("[Holder] Failed to acquire lock\n");
            exit(1);
        }
    }
    
    /* Give holder time to acquire lock */
    sleep(1);
    
    /* Now test exec with contention */
    pid_t exec_child = fork();
    if (exec_child == 0) {
        /* Exec child process */
        char *argv[] = {"echo", "Should succeed after wait", NULL};
        opts.timeout = 6.0; /* Should be enough to wait for holder */
        printf("[ExecChild] Calling exec_with_lock with timeout=%.1f (holder should release in ~3 sec)\n", 
               opts.timeout);
        int result = exec_with_lock(descriptor, argv);
        printf("[ExecChild] exec_with_lock returned %d\n", result);
        exit(result);
    }
    
    /* Wait for both processes */
    int holder_status, exec_status;
    waitpid(holder_pid, &holder_status, 0);
    waitpid(exec_child, &exec_status, 0);
    
    printf("[Parent] Holder exited with status %d\n", 
           WIFEXITED(holder_status) ? WEXITSTATUS(holder_status) : -1);
    printf("[Parent] ExecChild exited with status %d\n", 
           WIFEXITED(exec_status) ? WEXITSTATUS(exec_status) : -1);
    
    if (WIFEXITED(exec_status) && WEXITSTATUS(exec_status) == 0) {
        printf("PASS: Exec with contention succeeded\n");
    } else {
        printf("FAIL: Exec with contention failed\n");
    }
    
    /* Verify cleanup */
    sleep(1);
    int final_check = check_lock(descriptor);
    if (final_check == 0) {
        printf("PASS: Lock properly released after exec\n");
    } else {
        printf("FAIL: Lock not properly released after exec\n");
    }
}

int main() {
    printf("=== STANDALONE SEMAPHORE AND EXEC TEST ===\n");
    printf("This test isolates the failing integration test issues\n");
    
    /* Test the semaphore race condition */
    test_semaphore_race_condition();
    
    /* Test the exec timeout issue */
    test_exec_timeout_issue();
    
    printf("\n=== TEST COMPLETE ===\n");
    return 0;
}