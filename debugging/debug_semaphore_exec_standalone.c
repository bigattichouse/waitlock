#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

/* Include waitlock headers */
#include "src/waitlock.h"
#include "src/core/core.h"
#include "src/lock/lock.h"
#include "src/process/process.h"
#include "src/signal/signal.h"
#include "src/checksum/checksum.h"

/* Global state and options - we need to define these since they're extern in headers */
struct options opts = {
    .descriptor = NULL,
    .max_holders = 1,
    .timeout = -1.0,
    .check_only = FALSE,
    .list_mode = FALSE,
    .done_mode = FALSE,
    .exec_argv = NULL,
    .output_format = FMT_HUMAN,
    .show_all = FALSE,
    .stale_only = FALSE,
    .lock_dir = NULL,
    .one_per_cpu = FALSE,
    .exclude_cpus = 0,
    .test_mode = FALSE,
    .preferred_slot = -1
};

struct global_state g_state = {
    .lock_fd = -1,
    .lock_path = "",
    .should_exit = 0,
    .quiet = FALSE,
    .verbose = FALSE,
    .use_syslog = FALSE,
    .syslog_facility = LOG_DAEMON,
    .child_pid = 0,
    .received_signal = 0,
    .cleanup_needed = 0
};

/* Debug functions */
void debug_print_lock_files(const char *lock_dir, const char *descriptor) {
    printf("[DEBUG] Lock files for descriptor '%s' in %s:\n", descriptor, lock_dir);
    
    DIR *dir = opendir(lock_dir);
    if (!dir) {
        printf("[DEBUG] Cannot open lock directory: %s\n", strerror(errno));
        return;
    }
    
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, descriptor) && strstr(entry->d_name, ".lock")) {
            printf("[DEBUG]   %s\n", entry->d_name);
            count++;
        }
    }
    closedir(dir);
    
    if (count == 0) {
        printf("[DEBUG]   (no lock files found)\n");
    }
    printf("[DEBUG] Total lock files: %d\n", count);
}

void debug_print_process_info(const char *label, pid_t pid) {
    printf("[DEBUG] %s - PID: %d, exists: %s\n", 
           label, pid, process_exists(pid) ? "YES" : "NO");
}

/* Test semaphore race condition with detailed debugging */
void test_semaphore_race_condition_detailed() {
    printf("\n=== DETAILED SEMAPHORE RACE CONDITION TEST ===\n");
    
    const char *descriptor = "test_semaphore_detailed";
    int max_holders = 3;
    double timeout = 2.0;
    char lock_dir[256];
    snprintf(lock_dir, sizeof(lock_dir), "/tmp/waitlock_debug_%d", getpid());
    
    /* Create lock directory */
    if (mkdir(lock_dir, 0755) != 0 && errno != EEXIST) {
        printf("FAIL: Cannot create lock directory %s: %s\n", lock_dir, strerror(errno));
        return;
    }
    
    printf("Testing semaphore with max_holders=%d, timeout=%.1f\n", max_holders, timeout);
    printf("Lock directory: %s\n", lock_dir);
    
    /* Set up global options */
    opts.descriptor = descriptor;
    opts.max_holders = max_holders;
    opts.timeout = timeout;
    opts.lock_dir = lock_dir;
    
    debug_print_lock_files(lock_dir, descriptor);
    
    /* Step 1: Parent acquires first slot */
    printf("\n[Parent] Acquiring first slot...\n");
    int parent_result = acquire_lock(descriptor, max_holders, timeout);
    if (parent_result != 0) {
        printf("FAIL: Parent couldn't acquire first slot (result=%d)\n", parent_result);
        return;
    }
    printf("PASS: Parent acquired first slot\n");
    printf("[Parent] Lock fd: %d, lock path: %s\n", g_state.lock_fd, 
           g_state.lock_path[0] ? g_state.lock_path : "NULL");
    
    debug_print_lock_files(lock_dir, descriptor);
    
    /* Step 2: Fork children with detailed coordination */
    pid_t child_pids[4];
    int child_pipes[4][2]; /* For detailed communication */
    
    printf("\n[Parent] Creating coordination pipes...\n");
    for (int i = 0; i < 4; i++) {
        if (pipe(child_pipes[i]) != 0) {
            printf("FAIL: Could not create pipe %d: %s\n", i, strerror(errno));
            return;
        }
    }
    
    printf("[Parent] Forking 4 children to test semaphore limits...\n");
    
    for (int i = 0; i < 4; i++) {
        child_pids[i] = fork();
        if (child_pids[i] == 0) {
            /* Child process */
            close(child_pipes[i][0]); /* Close read end */
            
            /* Reset global state for child */
            g_state.lock_fd = -1;
            g_state.lock_path[0] = '\0';
            g_state.child_pid = 0;
            
            printf("[Child %d] PID=%d, attempting to acquire slot...\n", i + 2, getpid());
            
            struct timespec start, end;
            clock_gettime(CLOCK_MONOTONIC, &start);
            
            int child_result = acquire_lock(descriptor, max_holders, timeout);
            
            clock_gettime(CLOCK_MONOTONIC, &end);
            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
            
            char status_msg[512];
            if (child_result == 0) {
                snprintf(status_msg, sizeof(status_msg), 
                         "SUCCESS:PID=%d:FD=%d:TIME=%.3fs:PATH=%s", 
                         getpid(), g_state.lock_fd, elapsed,
                         g_state.lock_path[0] ? g_state.lock_path : "NULL");
                printf("[Child %d] SUCCESS: Acquired lock (fd=%d) in %.3fs\n", 
                       i + 2, g_state.lock_fd, elapsed);
                
                /* Hold the lock for analysis */
                sleep(5);
                
                printf("[Child %d] Releasing lock (fd=%d)\n", i + 2, g_state.lock_fd);
                release_lock();
                
            } else {
                snprintf(status_msg, sizeof(status_msg), 
                         "FAILED:PID=%d:RESULT=%d:TIME=%.3fs", 
                         getpid(), child_result, elapsed);
                printf("[Child %d] FAILED: Could not acquire slot (result=%d, time=%.3fs)\n", 
                       i + 2, child_result, elapsed);
            }
            
            /* Send detailed status to parent */
            ssize_t written = write(child_pipes[i][1], status_msg, strlen(status_msg));
            if (written < 0) {
                printf("[Child %d] Warning: Could not write to pipe: %s\n", i + 2, strerror(errno));
            }
            close(child_pipes[i][1]);
            
            exit(child_result == 0 ? 0 : 1);
            
        } else if (child_pids[i] < 0) {
            printf("FAIL: Could not fork child %d: %s\n", i, strerror(errno));
            return;
        }
    }
    
    /* Step 3: Monitor children acquisition */
    printf("\n[Parent] Monitoring children acquisition...\n");
    sleep(1); /* Give children time to start */
    
    debug_print_lock_files(lock_dir, descriptor);
    
    /* Step 4: Collect detailed results */
    printf("\n[Parent] Collecting results from children...\n");
    char child_results[4][512];
    int successful_acquisitions = 1; /* Parent has one */
    
    for (int i = 0; i < 4; i++) {
        close(child_pipes[i][1]); /* Close write end */
        
        fd_set readfds;
        struct timeval timeout_tv = {10, 0}; /* 10 second timeout */
        FD_ZERO(&readfds);
        FD_SET(child_pipes[i][0], &readfds);
        
        int ready = select(child_pipes[i][0] + 1, &readfds, NULL, NULL, &timeout_tv);
        if (ready > 0) {
            ssize_t bytes_read = read(child_pipes[i][0], child_results[i], sizeof(child_results[i]) - 1);
            if (bytes_read > 0) {
                child_results[i][bytes_read] = '\0';
                printf("[Parent] Child %d result: %s\n", i + 2, child_results[i]);
                
                if (strstr(child_results[i], "SUCCESS:") != NULL) {
                    successful_acquisitions++;
                }
            }
        } else {
            printf("[Parent] Child %d timed out or error\n", i + 2);
            strcpy(child_results[i], "TIMEOUT");
        }
        close(child_pipes[i][0]);
    }
    
    printf("\n[Parent] Analysis:\n");
    printf("  Parent + Children successful acquisitions: %d\n", successful_acquisitions);
    printf("  Maximum allowed (max_holders): %d\n", max_holders);
    
    debug_print_lock_files(lock_dir, descriptor);
    
    /* Step 5: Analyze results */
    if (successful_acquisitions == max_holders) {
        printf("PASS: Semaphore correctly limited to %d holders\n", max_holders);
    } else if (successful_acquisitions > max_holders) {
        printf("FAIL: SEMAPHORE BUG - Too many processes acquired locks (%d > %d)\n", 
               successful_acquisitions, max_holders);
        printf("      This indicates a race condition in acquire_lock()\n");
    } else {
        printf("UNEXPECTED: Fewer processes acquired locks than expected (%d < %d)\n",
               successful_acquisitions, max_holders);
    }
    
    /* Step 6: Release parent lock and wait for children */
    printf("\n[Parent] Releasing parent slot and waiting for children...\n");
    release_lock();
    
    for (int i = 0; i < 4; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
        printf("[Parent] Child %d exited with status %d\n", i + 2, 
               WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
    
    /* Step 7: Final cleanup verification */
    sleep(1);
    debug_print_lock_files(lock_dir, descriptor);
    
    int final_check = check_lock(descriptor);
    if (final_check == 0) {
        printf("PASS: All semaphore slots properly released\n");
    } else {
        printf("FAIL: Semaphore slots not properly released (check_result=%d)\n", final_check);
    }
    
    /* Cleanup */
    char cleanup_cmd[512];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf %s", lock_dir);
    system(cleanup_cmd);
}

/* Test opts.timeout integration issue specifically */
void test_opts_timeout_integration_bug() {
    printf("\n=== OPTS.TIMEOUT INTEGRATION BUG TEST ===\n");
    printf("This reproduces the exact issue in integration test 4\n");
    
    const char *descriptor = "test_opts_timeout_bug";
    char lock_dir[256];
    snprintf(lock_dir, sizeof(lock_dir), "/tmp/waitlock_opts_%d", getpid());
    
    if (mkdir(lock_dir, 0755) != 0 && errno != EEXIST) {
        printf("FAIL: Cannot create lock directory %s: %s\n", lock_dir, strerror(errno));
        return;
    }
    
    printf("Testing opts.timeout not being set before exec_with_lock()\n");
    printf("Lock directory: %s\n", lock_dir);
    
    opts.lock_dir = lock_dir;
    
    /* Test Case 1: Reproduce the integration test bug exactly */
    printf("\n[BUG TEST] Reproducing integration test 4 exactly...\n");
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process - EXACTLY like integration test */
        printf("[Child] Setting up opts like integration test...\n");
        opts.descriptor = descriptor;
        opts.max_holders = 1;
        char *argv[] = {"echo", "Hello from exec", NULL};
        opts.exec_argv = argv;
        
        /* BUG: opts.timeout is NOT set here, just like in integration test! */
        printf("[Child] opts.timeout = %.1f (uninitialized!)\n", opts.timeout);
        printf("[Child] Calling exec_with_lock() with uninitialized timeout...\n");
        
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        int result = exec_with_lock(descriptor, argv);
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        printf("[Child] exec_with_lock returned %d after %.3fs\n", result, elapsed);
        printf("[Child] Expected: Should fail with timeout error\n");
        exit(result);
    } else if (child_pid > 0) {
        int status;
        waitpid(child_pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("[Parent] Child exited with code %d\n", exit_code);
            
            if (exit_code == 2) { /* E_TIMEOUT */
                printf("CONFIRMED BUG: exec_with_lock failed due to uninitialized timeout\n");
                printf("             This explains integration test 4 failure!\n");
            } else if (exit_code == 0) {
                printf("UNEXPECTED: exec_with_lock succeeded despite uninitialized timeout\n");
            } else {
                printf("DIFFERENT ERROR: exec_with_lock failed with code %d\n", exit_code);
            }
        }
    }
    
    /* Test Case 2: Show the fix */
    printf("\n[FIX TEST] Testing with properly initialized timeout...\n");
    
    child_pid = fork();
    if (child_pid == 0) {
        /* Child process - WITH PROPER TIMEOUT */
        printf("[Child] Setting up opts with PROPER timeout...\n");
        opts.descriptor = descriptor;
        opts.max_holders = 1;
        opts.timeout = 5.0; /* FIX: Set proper timeout */
        char *argv[] = {"echo", "Hello from FIXED exec", NULL};
        opts.exec_argv = argv;
        
        printf("[Child] opts.timeout = %.1f (properly set!)\n", opts.timeout);
        printf("[Child] Calling exec_with_lock() with proper timeout...\n");
        
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        int result = exec_with_lock(descriptor, argv);
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        printf("[Child] exec_with_lock returned %d after %.3fs\n", result, elapsed);
        exit(result);
    } else if (child_pid > 0) {
        int status;
        waitpid(child_pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("[Parent] Child exited with code %d\n", exit_code);
            
            if (exit_code == 0) {
                printf("PASS: exec_with_lock succeeded with proper timeout\n");
                printf("      This confirms the fix for integration test 4!\n");
            } else {
                printf("FAIL: exec_with_lock still failed even with proper timeout (code %d)\n", exit_code);
            }
        }
    }
    
    /* Cleanup */
    char cleanup_cmd[512];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf %s", lock_dir);
    system(cleanup_cmd);
}

/* Test mutex race condition with same stress pattern as semaphore test */
void test_mutex_race_condition_detailed() {
    printf("\n=== DETAILED MUTEX RACE CONDITION TEST ===\n");
    printf("Testing mutex (max_holders=1) with same stress pattern as semaphore test\n");
    
    const char *descriptor = "test_mutex_detailed";
    int max_holders = 1; /* MUTEX: only 1 holder allowed */
    double timeout = 2.0;
    char lock_dir[256];
    snprintf(lock_dir, sizeof(lock_dir), "/tmp/waitlock_mutex_%d", getpid());
    
    /* Create lock directory */
    if (mkdir(lock_dir, 0755) != 0 && errno != EEXIST) {
        printf("FAIL: Cannot create lock directory %s: %s\n", lock_dir, strerror(errno));
        return;
    }
    
    printf("Testing mutex with max_holders=%d, timeout=%.1f\n", max_holders, timeout);
    printf("Lock directory: %s\n", lock_dir);
    
    /* Set up global options */
    opts.descriptor = descriptor;
    opts.max_holders = max_holders;
    opts.timeout = timeout;
    opts.lock_dir = lock_dir;
    
    debug_print_lock_files(lock_dir, descriptor);
    
    /* Step 1: Parent acquires mutex lock */
    printf("\n[Parent] Acquiring mutex lock...\n");
    int parent_result = acquire_lock(descriptor, max_holders, timeout);
    if (parent_result != 0) {
        printf("FAIL: Parent couldn't acquire mutex lock (result=%d)\n", parent_result);
        return;
    }
    printf("PASS: Parent acquired mutex lock\n");
    printf("[Parent] Lock fd: %d, lock path: %s\n", g_state.lock_fd, 
           g_state.lock_path[0] ? g_state.lock_path : "NULL");
    
    debug_print_lock_files(lock_dir, descriptor);
    
    /* Step 2: Fork 4 children to test mutex exclusion */
    pid_t child_pids[4];
    int child_pipes[4][2]; /* For detailed communication */
    
    printf("\n[Parent] Creating coordination pipes...\n");
    for (int i = 0; i < 4; i++) {
        if (pipe(child_pipes[i]) != 0) {
            printf("FAIL: Could not create pipe %d: %s\n", i, strerror(errno));
            return;
        }
    }
    
    printf("[Parent] Forking 4 children to test mutex exclusion...\n");
    printf("[Parent] Expected: All 4 children should fail to acquire (mutex is exclusive)\n");
    
    for (int i = 0; i < 4; i++) {
        child_pids[i] = fork();
        if (child_pids[i] == 0) {
            /* Child process */
            close(child_pipes[i][0]); /* Close read end */
            
            /* Reset global state for child */
            g_state.lock_fd = -1;
            g_state.lock_path[0] = '\0';
            g_state.child_pid = 0;
            
            printf("[Child %d] PID=%d, attempting to acquire mutex (should fail)...\n", i + 1, getpid());
            
            struct timespec start, end;
            clock_gettime(CLOCK_MONOTONIC, &start);
            
            int child_result = acquire_lock(descriptor, max_holders, timeout);
            
            clock_gettime(CLOCK_MONOTONIC, &end);
            double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
            
            char status_msg[512];
            if (child_result == 0) {
                snprintf(status_msg, sizeof(status_msg), 
                         "BUG_SUCCESS:PID=%d:FD=%d:TIME=%.3fs:PATH=%s", 
                         getpid(), g_state.lock_fd, elapsed,
                         g_state.lock_path[0] ? g_state.lock_path : "NULL");
                printf("[Child %d] BUG: Successfully acquired mutex when parent holds it! (fd=%d) in %.3fs\n", 
                       i + 1, g_state.lock_fd, elapsed);
                
                /* Release the incorrectly acquired lock */
                sleep(1);
                printf("[Child %d] Releasing incorrectly acquired mutex (fd=%d)\n", i + 1, g_state.lock_fd);
                release_lock();
                
            } else {
                snprintf(status_msg, sizeof(status_msg), 
                         "EXPECTED_FAIL:PID=%d:RESULT=%d:TIME=%.3fs", 
                         getpid(), child_result, elapsed);
                printf("[Child %d] EXPECTED: Failed to acquire mutex (result=%d, time=%.3fs)\n", 
                       i + 1, child_result, elapsed);
            }
            
            /* Send detailed status to parent */
            ssize_t written = write(child_pipes[i][1], status_msg, strlen(status_msg));
            if (written < 0) {
                printf("[Child %d] Warning: Could not write to pipe: %s\n", i + 1, strerror(errno));
            }
            close(child_pipes[i][1]);
            
            exit(child_result == 0 ? 0 : 1);
            
        } else if (child_pids[i] < 0) {
            printf("FAIL: Could not fork child %d: %s\n", i, strerror(errno));
            return;
        }
    }
    
    /* Step 3: Monitor children attempts */
    printf("\n[Parent] Monitoring children attempts...\n");
    sleep(1); /* Give children time to attempt */
    
    debug_print_lock_files(lock_dir, descriptor);
    
    /* Step 4: Collect detailed results */
    printf("\n[Parent] Collecting results from children...\n");
    char child_results[4][512];
    int successful_acquisitions = 1; /* Parent has one */
    int race_condition_detected = 0;
    
    for (int i = 0; i < 4; i++) {
        close(child_pipes[i][1]); /* Close write end */
        
        fd_set readfds;
        struct timeval timeout_tv = {10, 0}; /* 10 second timeout */
        FD_ZERO(&readfds);
        FD_SET(child_pipes[i][0], &readfds);
        
        int ready = select(child_pipes[i][0] + 1, &readfds, NULL, NULL, &timeout_tv);
        if (ready > 0) {
            ssize_t bytes_read = read(child_pipes[i][0], child_results[i], sizeof(child_results[i]) - 1);
            if (bytes_read > 0) {
                child_results[i][bytes_read] = '\0';
                printf("[Parent] Child %d result: %s\n", i + 1, child_results[i]);
                
                if (strstr(child_results[i], "BUG_SUCCESS:") != NULL) {
                    successful_acquisitions++;
                    race_condition_detected = 1;
                }
            }
        } else {
            printf("[Parent] Child %d timed out or error\n", i + 1);
            strcpy(child_results[i], "TIMEOUT");
        }
        close(child_pipes[i][0]);
    }
    
    printf("\n[Parent] Mutex Analysis:\n");
    printf("  Parent + Children successful acquisitions: %d\n", successful_acquisitions);
    printf("  Maximum allowed (max_holders): %d\n", max_holders);
    printf("  Race condition detected: %s\n", race_condition_detected ? "YES" : "NO");
    
    debug_print_lock_files(lock_dir, descriptor);
    
    /* Step 5: Analyze mutex results compared to semaphore */
    if (successful_acquisitions == max_holders) {
        printf("PASS: Mutex correctly limited to %d holder\n", max_holders);
        printf("      This suggests race condition may be semaphore-specific\n");
    } else if (successful_acquisitions > max_holders) {
        printf("FAIL: MUTEX RACE CONDITION DETECTED - Multiple processes acquired exclusive lock!\n");
        printf("      This indicates the race condition affects BOTH mutex and semaphore\n");
        printf("      The bug is in the core atomic rename mechanism in acquire_lock()\n");
    } else {
        printf("UNEXPECTED: No mutex acquisitions by children (all timed out?)\n");
    }
    
    /* Step 6: Release parent lock and wait for children */
    printf("\n[Parent] Releasing parent mutex and waiting for children...\n");
    release_lock();
    
    for (int i = 0; i < 4; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
        printf("[Parent] Child %d exited with status %d\n", i + 1, 
               WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
    
    /* Step 7: Final cleanup verification */
    sleep(1);
    debug_print_lock_files(lock_dir, descriptor);
    
    int final_check = check_lock(descriptor);
    if (final_check == 0) {
        printf("PASS: Mutex properly released\n");
    } else {
        printf("FAIL: Mutex not properly released (check_result=%d)\n", final_check);
    }
    
    /* Cleanup */
    char cleanup_cmd[512];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf %s", lock_dir);
    system(cleanup_cmd);
}

/* Test exec timeout issues with debugging */
void test_exec_timeout_detailed() {
    printf("\n=== DETAILED EXEC TIMEOUT TEST ===\n");
    
    const char *descriptor = "test_exec_detailed";
    char lock_dir[256];
    snprintf(lock_dir, sizeof(lock_dir), "/tmp/waitlock_exec_%d", getpid());
    
    /* Create lock directory */
    if (mkdir(lock_dir, 0755) != 0 && errno != EEXIST) {
        printf("FAIL: Cannot create lock directory %s: %s\n", lock_dir, strerror(errno));
        return;
    }
    
    printf("Testing exec_with_lock timeout handling\n");
    printf("Lock directory: %s\n", lock_dir);
    
    opts.lock_dir = lock_dir;
    
    /* Test 1: Simple exec with proper timeout */
    printf("\n[Test 1] Simple exec with timeout=5.0...\n");
    opts.descriptor = descriptor;
    opts.max_holders = 1;
    opts.timeout = 5.0;
    
    debug_print_lock_files(lock_dir, descriptor);
    
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        printf("[Child] Calling exec_with_lock with timeout=%.1f\n", opts.timeout);
        printf("[Child] Current working directory: ");
        system("pwd");
        
        char *argv[] = {"echo", "Hello from exec test", NULL};
        
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        int result = exec_with_lock(descriptor, argv);
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        printf("[Child] exec_with_lock returned %d after %.3fs\n", result, elapsed);
        exit(result);
    } else if (child_pid > 0) {
        int status;
        waitpid(child_pid, &status, 0);
        
        debug_print_lock_files(lock_dir, descriptor);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("[Parent] Child exited with code %d\n", exit_code);
            if (exit_code == 0) {
                printf("PASS: Simple exec succeeded\n");
            } else {
                printf("FAIL: Simple exec failed with exit code %d\n", exit_code);
            }
        } else {
            printf("FAIL: Child did not exit normally\n");
        }
    }
    
    /* Test 2: Exec with zero timeout (reproducing the bug) */
    printf("\n[Test 2] Exec with timeout=0.0 (should fail immediately)...\n");
    opts.timeout = 0.0;
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        printf("[Child] Calling exec_with_lock with timeout=%.1f\n", opts.timeout);
        
        char *argv[] = {"echo", "Should fail immediately", NULL};
        int result = exec_with_lock(descriptor, argv);
        
        printf("[Child] exec_with_lock returned %d\n", result);
        exit(result);
    } else if (child_pid > 0) {
        int status;
        waitpid(child_pid, &status, 0);
        
        clock_gettime(CLOCK_MONOTONIC, &end);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        debug_print_lock_files(lock_dir, descriptor);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            printf("[Parent] Child exited with code %d after %.3fs\n", exit_code, elapsed);
            
            if (exit_code != 0 && elapsed < 1.0) {
                printf("PASS: timeout=0.0 failed quickly as expected\n");
            } else if (exit_code == 0) {
                printf("FAIL: timeout=0.0 should have failed but succeeded\n");
            } else {
                printf("FAIL: timeout=0.0 took too long (%.3fs)\n", elapsed);
            }
        } else {
            printf("FAIL: Child did not exit normally\n");
        }
    }
    
    /* Final cleanup check */
    sleep(1);
    debug_print_lock_files(lock_dir, descriptor);
    
    int final_check = check_lock(descriptor);
    if (final_check == 0) {
        printf("PASS: All exec test locks properly cleaned up\n");
    } else {
        printf("FAIL: Exec test locks not properly cleaned up (check_result=%d)\n", final_check);
    }
    
    /* Cleanup */
    char cleanup_cmd[512];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -rf %s", lock_dir);
    system(cleanup_cmd);
}

int main() {
    printf("=== WAITLOCK SEMAPHORE AND EXEC DEBUG TOOL ===\n");
    printf("This standalone program tests the specific issues found in integration tests\n");
    
    /* Initialize logging/state */
    g_state.lock_fd = -1;
    g_state.lock_path[0] = '\0';
    g_state.child_pid = 0;
    g_state.received_signal = 0;
    g_state.cleanup_needed = 0;
    
    /* Test the semaphore race condition with detailed debugging */
    test_semaphore_race_condition_detailed();
    
    /* Test the mutex race condition to compare with semaphore behavior */
    test_mutex_race_condition_detailed();
    
    /* Test the specific opts.timeout integration bug */
    test_opts_timeout_integration_bug();
    
    /* Test the exec timeout issue with detailed debugging */
    test_exec_timeout_detailed();
    
    printf("\n=== DEBUG COMPLETE ===\n");
    printf("This program should help identify the exact cause of the integration test failures\n");
    
    return 0;
}
