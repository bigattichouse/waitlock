/*
 * Unit tests for lock.c functions
 * Tests lock acquisition, release, checking, listing, and file I/O
 */

#include "test.h"
#include "../lock/lock.h"
#include "../core/core.h"
#include "../process/process.h"
#include "../pipe_coordinator/pipe_coordinator.h"
#include <time.h>

/* Test framework */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n[LOCK_TEST %d] %s\n", test_count, name); \
    } while(0)

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            pass_count++; \
            printf("  ✓ PASS: %s\n", message); \
        } else { \
            fail_count++; \
            printf("  ✗ FAIL: %s\n", message); \
        } \
    } while(0)

/* Test lock directory discovery */
int test_find_lock_directory(void) {
    TEST_START("Lock directory discovery");
    
    char *lock_dir = find_lock_directory();
    TEST_ASSERT(lock_dir != NULL, "Lock directory should be found");
    
    if (lock_dir) {
        struct stat st;
        int stat_result = stat(lock_dir, &st);
        TEST_ASSERT(stat_result == 0, "Lock directory should exist");
        TEST_ASSERT(S_ISDIR(st.st_mode), "Lock directory should be a directory");
        TEST_ASSERT(access(lock_dir, W_OK) == 0, "Lock directory should be writable");
        
        printf("  → Lock directory: %s\n", lock_dir);
    }
    
    return 0;
}

/* Test portable lock functionality */
int test_portable_lock(void) {
    TEST_START("Portable lock functionality");
    
    char *lock_dir = find_lock_directory();
    if (!lock_dir) {
        printf("  ✗ FAIL: Cannot find lock directory\n");
        fail_count++;
        return 1;
    }
    
    char test_file[PATH_MAX];
    snprintf(test_file, sizeof(test_file), "%s/test_portable_lock.tmp", lock_dir);
    
    /* Create test file */
    int fd = open(test_file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "Should be able to create test file");
    
    if (fd >= 0) {
        /* Test exclusive lock */
        int lock_result = portable_lock(fd, LOCK_EX | LOCK_NB);
        TEST_ASSERT(lock_result == 0, "Should be able to acquire exclusive lock");
        
        /* Test that another lock fails */
        int fd2 = open(test_file, O_RDONLY);
        if (fd2 >= 0) {
            int lock_result2 = portable_lock(fd2, LOCK_EX | LOCK_NB);
            TEST_ASSERT(lock_result2 != 0, "Second exclusive lock should fail");
            close(fd2);
        }
        
        /* Release lock */
        int unlock_result = portable_lock(fd, LOCK_UN);
        TEST_ASSERT(unlock_result == 0, "Should be able to release lock");
        
        close(fd);
        unlink(test_file);
    }
    
    return 0;
}

/* Test lock acquisition */
int test_acquire_lock(void) {
    TEST_START("Lock acquisition");
    
    char *lock_dir = find_lock_directory();
    if (!lock_dir) {
        printf("  ✗ FAIL: Cannot find lock directory\n");
        fail_count++;
        return 1;
    }
    
    /* Clean up any existing lock files from previous tests */
    char cleanup_cmd[PATH_MAX + 50];
    snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -f %s/test_*.lock 2>/dev/null || true", lock_dir);
    int sys_result = system(cleanup_cmd);
    (void)sys_result; /* Suppress unused variable warning */
    
    /* Test basic mutex acquisition */
    const char *test_descriptor = "test_acquire_lock";
    int result = acquire_lock(test_descriptor, 1, 2.0);
    TEST_ASSERT(result == 0, "Should be able to acquire mutex lock");
    
    /* Test that same lock cannot be acquired again */
    int result2 = acquire_lock(test_descriptor, 1, 0.1);
    TEST_ASSERT(result2 != 0, "Should not be able to acquire same mutex lock twice");
    
    /* Release the lock */
    release_lock();
    
    /* Test semaphore acquisition */
    int result3 = acquire_lock("test_semaphore", 3, 2.0);
    TEST_ASSERT(result3 == 0, "Should be able to acquire semaphore slot");
    
    /* Test second semaphore slot with proper coordination */
    PipeCoordinator *pc = pipe_coordinator_create();
    if (pc == NULL) {
        TEST_ASSERT(0, "Failed to create PipeCoordinator for semaphore test");
        release_lock();
        return 1;
    }

    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process */
        pipe_coordinator_close_read_end(pc); /* Child closes read end */
        
        int child_result = acquire_lock("test_semaphore", 3, 1.0);
        
        /* Signal parent about result */
        char signal = (child_result == 0) ? 'S' : 'F';
        pipe_coordinator_write(pc, &signal, 1);
        pipe_coordinator_close_write_end(pc); /* Close write end after signaling */
        
        if (child_result == 0) {
            /* Hold lock briefly for parent to test, then release */
            sleep(1);
            release_lock();
        }
        
        pipe_coordinator_destroy(pc);
        exit(child_result == 0 ? 0 : 1);
    } else if (child_pid > 0) {
        /* Parent process */
        pipe_coordinator_set_child_pid(pc, child_pid);
        pipe_coordinator_close_write_end(pc); /* Parent closes write end */
        
        /* Wait for child result */
        char child_signal;
        if (pipe_coordinator_read(pc, &child_signal, 1) == 1 && child_signal == 'S') {
            TEST_ASSERT(1, "Should be able to acquire second semaphore slot");
            
            /* Wait for child to complete */
            int status;
            pipe_coordinator_wait_for_child(pc, &status);
            TEST_ASSERT(WEXITSTATUS(status) == 0, "Child should exit successfully");
        } else {
            TEST_ASSERT(0, "Child failed to acquire semaphore slot");
            kill(child_pid, SIGTERM);
            int status;
            pipe_coordinator_wait_for_child(pc, &status);
        }
        pipe_coordinator_destroy(pc);
    } else {
        TEST_ASSERT(0, "Failed to fork child process for semaphore test");
        pipe_coordinator_destroy(pc); // Clean up if fork fails
    }
    
    release_lock();
    
    return 0;
}

/* Test lock release */
int test_release_lock(void) {
    TEST_START("Lock release");
    
    /* Clean up any existing lock files */
    char *lock_dir = find_lock_directory();
    if (lock_dir) {
        char cleanup_cmd[PATH_MAX + 50];
        snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -f %s/test_*.lock 2>/dev/null || true", lock_dir);
        int sys_result = system(cleanup_cmd);
    (void)sys_result; /* Suppress unused variable warning */
    }
    
    const char *test_descriptor = "test_release_lock";
    
    /* Acquire lock */
    int acquire_result = acquire_lock(test_descriptor, 1, 2.0);
    TEST_ASSERT(acquire_result == 0, "Should be able to acquire lock");
    
    /* Verify lock is held */
    int check_result = check_lock(test_descriptor);
    TEST_ASSERT(check_result != 0, "Lock should be held");
    
    /* Release lock */
    release_lock();
    
    /* Verify lock is released */
    int check_result2 = check_lock(test_descriptor);
    TEST_ASSERT(check_result2 == 0, "Lock should be released");
    
    /* Test releasing non-existent lock (should not crash) */
    release_lock(); /* Second release should be safe */
    TEST_ASSERT(1, "Multiple releases should be safe");
    
    return 0;
}

/* Test lock checking */
int test_check_lock(void) {
    TEST_START("Lock checking");
    
    const char *test_descriptor = "test_check_lock";
    
    /* Test checking non-existent lock */
    int result1 = check_lock(test_descriptor);
    TEST_ASSERT(result1 == 0, "Non-existent lock should be available");
    
    /* Acquire lock */
    int acquire_result = acquire_lock(test_descriptor, 1, 2.0);
    TEST_ASSERT(acquire_result == 0, "Should be able to acquire lock");
    
    /* Test checking held lock */
    int result2 = check_lock(test_descriptor);
    TEST_ASSERT(result2 != 0, "Held lock should not be available");
    
    /* Release lock */
    release_lock();
    
    /* Test checking released lock */
    int result3 = check_lock(test_descriptor);
    TEST_ASSERT(result3 == 0, "Released lock should be available");
    
    return 0;
}

/* Test lock listing */
int test_list_locks(void) {
    TEST_START("Lock listing");
    
    /* Create a test lock */
    const char *test_descriptor = "test_list_lock";
    int acquire_result = acquire_lock(test_descriptor, 1, 2.0);
    TEST_ASSERT(acquire_result == 0, "Should be able to acquire test lock");
    
    /* Test human format listing */
    printf("  → Testing human format listing\n");
    list_locks(FMT_HUMAN, FALSE, FALSE);
    
    /* Test CSV format listing */
    printf("  → Testing CSV format listing\n");
    list_locks(FMT_CSV, FALSE, FALSE);
    
    /* Test null format listing */
    printf("  → Testing null format listing\n");
    list_locks(FMT_NULL, FALSE, FALSE);
    
    /* Test show all locks */
    printf("  → Testing show all locks\n");
    list_locks(FMT_HUMAN, TRUE, FALSE);
    
    /* Test stale only locks */
    printf("  → Testing stale only locks\n");
    list_locks(FMT_HUMAN, FALSE, TRUE);
    
    TEST_ASSERT(1, "Lock listing completed without errors");
    
    /* Release test lock */
    release_lock();
    
    return 0;
}

/* Test done lock functionality */
int test_done_lock(void) {
    TEST_START("Done lock functionality");
    
    const char *test_descriptor = "test_done_lock";
    
    /* Test done on non-existent lock */
    int result1 = done_lock(test_descriptor);
    TEST_ASSERT(result1 != 0, "Done on non-existent lock should fail");
    
    /* Create pipe for parent-child coordination */
    PipeCoordinator *pc = pipe_coordinator_create();
    if (pc == NULL) {
        printf("  ✗ FAIL: Cannot create PipeCoordinator\n");
        fail_count++;
        return 1;
    }
    
    /* Create a child process that holds a lock */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process - acquire lock and signal parent */
        pipe_coordinator_close_read_end(pc); /* Child closes read end */
        
        int acquire_result = acquire_lock(test_descriptor, 1, 2.0);
        
        /* Signal parent about acquisition result */
        char signal = (acquire_result == 0) ? 'S' : 'F'; /* Success/Fail */
        pipe_coordinator_write(pc, &signal, 1);
        pipe_coordinator_close_write_end(pc); /* Close write end after signaling */
        
        if (acquire_result == 0) {
            /* Wait for done signal to release lock (implement signal handler) */
            while (1) {
                sleep(1); /* Wait for done signal or termination */
            }
        }
        
        pipe_coordinator_destroy(pc);
        exit(acquire_result);
    } else if (child_pid > 0) {
        /* Parent process */
        pipe_coordinator_set_child_pid(pc, child_pid);
        pipe_coordinator_close_write_end(pc); /* Parent closes write end */
        
        /* Wait for child to signal lock acquisition */
        char child_signal;
        ssize_t bytes_read = pipe_coordinator_read(pc, &child_signal, 1);
        
        if (bytes_read == 1 && child_signal == 'S') {
            TEST_ASSERT(1, "Child successfully acquired lock");
            
            /* Give child time to fully initialize */
            sleep(1);
            
            /* Verify lock is held */
            int check_result = check_lock(test_descriptor);
            TEST_ASSERT(check_result != 0, "Lock should be held by child");
            
            /* Send done signal */
            int done_result = done_lock(test_descriptor);
            TEST_ASSERT(done_result == 0, "Done signal should succeed");
            
            /* Wait for child to exit */
            int status;
            pipe_coordinator_wait_for_child(pc, &status);
            TEST_ASSERT(WEXITSTATUS(status) == 0, "Child should exit successfully");
            
            /* Give brief time for cleanup */
            usleep(100000); /* 100ms */
            
            /* Verify lock is released */
            int check_result2 = check_lock(test_descriptor);
            TEST_ASSERT(check_result2 == 0, "Lock should be released after done signal");
        } else {
            TEST_ASSERT(0, "Child failed to acquire lock or communication failed");
            pipe_coordinator_close_read_end(pc);
            
            /* Kill child if still running */
            kill(child_pid, SIGTERM);
            int status;
            pipe_coordinator_wait_for_child(pc, &status);
        }
        pipe_coordinator_destroy(pc); // Clean up parent's coordinator
    } else if (child_pid < 0) {
        /* Fork failed */
        pipe_coordinator_destroy(pc);
        printf("  ✗ FAIL: Fork failed\n");
        fail_count++;
        return 1;
    }
    
    return 0;
}

/* Test lock timeout functionality */
int test_lock_timeout(void) {
    TEST_START("Lock timeout functionality");
    
    const char *test_descriptor = "test_timeout_lock";
    
    /* Create pipe for coordination */
    int timeout_pipe[2];
    if (pipe(timeout_pipe) != 0) {
        printf("  ✗ FAIL: Cannot create coordination pipe\n");
        fail_count++;
        return 1;
    }
    
    /* Create a child process that holds a lock */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process - acquire lock and signal parent */
        close(timeout_pipe[0]); /* Close read end */
        
        int acquire_result = acquire_lock(test_descriptor, 1, 2.0);
        
        /* Signal parent about acquisition */
        char signal = (acquire_result == 0) ? 'S' : 'F';
        ssize_t bytes_written = write(timeout_pipe[1], &signal, 1);
        (void)bytes_written; /* Suppress unused variable warning */
        
        if (acquire_result == 0) {
            /* Hold lock for 3 seconds */
            sleep(3);
            release_lock();
        }
        
        close(timeout_pipe[1]);
        exit(acquire_result);
    } else if (child_pid > 0) {
        /* Parent process */
        close(timeout_pipe[1]); /* Close write end */
        
        /* Wait for child to acquire lock */
        char child_signal;
        if (read(timeout_pipe[0], &child_signal, 1) == 1 && child_signal == 'S') {
            close(timeout_pipe[0]);
            
            /* Now try to acquire with short timeout */
            struct timespec start, end;
            clock_gettime(CLOCK_MONOTONIC, &start);
            
            int timeout_result = acquire_lock(test_descriptor, 1, 1.0);
            
            clock_gettime(CLOCK_MONOTONIC, &end);
            
            double elapsed = (end.tv_sec - start.tv_sec) + 
                           (end.tv_nsec - start.tv_nsec) / 1000000000.0;
            
            TEST_ASSERT(timeout_result != 0, "Lock acquisition should timeout");
            TEST_ASSERT(elapsed >= 0.9, "Timeout should be respected");
            TEST_ASSERT(elapsed <= 1.5, "Timeout should not be too long");
            
            /* Wait for child to exit */
            int status;
            waitpid(child_pid, &status, 0);
            TEST_ASSERT(WEXITSTATUS(status) == 0, "Child should exit successfully");
        } else {
            TEST_ASSERT(0, "Child failed to acquire lock");
            close(timeout_pipe[0]);
            kill(child_pid, SIGTERM);
            int status;
            waitpid(child_pid, &status, 0);
        }
    } else {
        /* Fork failed */
        close(timeout_pipe[0]);
        close(timeout_pipe[1]);
        printf("  ✗ FAIL: Fork failed\n");
        fail_count++;
        return 1;
    }
    
    return 0;
}

/* Test text lock file I/O */
int test_text_lock_file(void) {
    TEST_START("Text lock file I/O");
    
    char *lock_dir = find_lock_directory();
    if (!lock_dir) {
        printf("  ✗ FAIL: Cannot find lock directory\n");
        fail_count++;
        return 1;
    }
    
    char test_file[PATH_MAX];
    snprintf(test_file, sizeof(test_file), "%s/test_text_lock.tmp", lock_dir);
    
    /* Create test lock info */
    struct lock_info write_info;
    memset(&write_info, 0, sizeof(write_info));
    
    write_info.magic = LOCK_MAGIC;
    write_info.version = 1;
    write_info.pid = getpid();
    write_info.ppid = getppid();
    write_info.uid = getuid();
    write_info.acquired_at = time(NULL);
    write_info.lock_type = 0;
    write_info.max_holders = 1;
    write_info.slot = 0;
    
    strcpy(write_info.hostname, "testhost");
    strcpy(write_info.descriptor, "test_text_descriptor");
    strcpy(write_info.cmdline, "test_text_command");
    
    write_info.checksum = calculate_lock_checksum(&write_info);
    
    /* Write text lock file */
    int write_result = write_text_lock_file(test_file, &write_info);
    TEST_ASSERT(write_result == 0, "Should be able to write text lock file");
    
    /* Read text lock file */
    struct lock_info read_info;
    int read_result = read_text_lock_file(test_file, &read_info);
    TEST_ASSERT(read_result == 0, "Should be able to read text lock file");
    
    if (read_result == 0) {
        TEST_ASSERT(read_info.pid == write_info.pid, "PID should match");
        TEST_ASSERT(strcmp(read_info.descriptor, write_info.descriptor) == 0, "Descriptor should match");
        TEST_ASSERT(strcmp(read_info.hostname, write_info.hostname) == 0, "Hostname should match");
    }
    
    /* Clean up */
    unlink(test_file);
    
    return 0;
}

/* Test binary lock file I/O */
int test_binary_lock_file(void) {
    TEST_START("Binary lock file I/O");
    
    char *lock_dir = find_lock_directory();
    if (!lock_dir) {
        printf("  ✗ FAIL: Cannot find lock directory\n");
        fail_count++;
        return 1;
    }
    
    char test_file[PATH_MAX];
    snprintf(test_file, sizeof(test_file), "%s/test_binary_lock.tmp", lock_dir);
    
    /* Create test lock info */
    struct lock_info write_info;
    memset(&write_info, 0, sizeof(write_info));
    
    write_info.magic = LOCK_MAGIC;
    write_info.version = 1;
    write_info.pid = getpid();
    write_info.ppid = getppid();
    write_info.uid = getuid();
    write_info.acquired_at = time(NULL);
    write_info.lock_type = 0;
    write_info.max_holders = 1;
    write_info.slot = 0;
    
    strcpy(write_info.hostname, "testhost");
    strcpy(write_info.descriptor, "test_binary_descriptor");
    strcpy(write_info.cmdline, "test_binary_command");
    
    write_info.checksum = calculate_lock_checksum(&write_info);
    
    /* Write binary lock file */
    int fd = open(test_file, O_CREAT | O_WRONLY | O_EXCL, 0644);
    TEST_ASSERT(fd >= 0, "Should be able to create lock file");
    
    if (fd >= 0) {
        ssize_t written = write(fd, &write_info, sizeof(write_info));
        TEST_ASSERT(written == sizeof(write_info), "Should write complete lock info");
        close(fd);
        
        /* Read lock file back */
        struct lock_info read_info;
        int read_result = read_lock_file_any_format(test_file, &read_info);
        TEST_ASSERT(read_result == 0, "Should be able to read lock file");
        
        if (read_result == 0) {
            TEST_ASSERT(read_info.magic == LOCK_MAGIC, "Magic should match");
            TEST_ASSERT(read_info.pid == write_info.pid, "PID should match");
            TEST_ASSERT(strcmp(read_info.descriptor, write_info.descriptor) == 0, "Descriptor should match");
            TEST_ASSERT(validate_lock_checksum(&read_info), "Checksum should be valid");
        }
        
        /* Clean up */
        unlink(test_file);
    }
    
    return 0;
}

/* Test stale lock detection */
int test_stale_lock_detection(void) {
    TEST_START("Stale lock detection");
    
    /* Create a child process that will die abruptly */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        /* Child process - acquire lock and exit without cleanup */
        int acquire_result = acquire_lock("test_stale_lock", 1, 2.0);
        if (acquire_result == 0) {
            /* Exit without calling release_lock() */
            _exit(0);
        }
        _exit(1);
    } else if (child_pid > 0) {
        /* Parent process */
        int status;
        waitpid(child_pid, &status, 0);
        
        /* Give some time for cleanup */
        sleep(1);
        
        /* Try to acquire the same lock - should work if stale detection works */
        int acquire_result = acquire_lock("test_stale_lock", 1, 1.0);
        TEST_ASSERT(acquire_result == 0, "Should be able to acquire lock after child died");
        
        if (acquire_result == 0) {
            release_lock();
        }
    }
    
    return 0;
}

/* Test semaphore slot allocation */
int test_semaphore_slots(void) {
    TEST_START("Semaphore slot allocation");
    
    const char *test_descriptor = "test_semaphore_slots";
    int max_holders = 3;
    
    /* Create pipes for coordination with each child */
    int pipes[max_holders][2];
    pid_t child_pids[max_holders];
    int i;
    
    /* Initialize all pipes */
    for (i = 0; i < max_holders; i++) {
        if (pipe(pipes[i]) != 0) {
            printf("  ✗ FAIL: Cannot create coordination pipe %d\n", i);
            fail_count++;
            return 1;
        }
    }
    
    /* Create multiple child processes to test slot allocation */
    for (i = 0; i < max_holders; i++) {
        child_pids[i] = fork();
        if (child_pids[i] == 0) {
            /* Child process - acquire semaphore slot */
            close(pipes[i][0]); /* Close read end - child only writes */
            
            int acquire_result = acquire_lock(test_descriptor, max_holders, 1.0);
            
            /* Signal parent about result */
            char signal = (acquire_result == 0) ? 'S' : 'F';
            ssize_t bytes_written = write(pipes[i][1], &signal, 1);
            (void)bytes_written; /* Suppress unused variable warning */
            close(pipes[i][1]); /* Close write end after signaling */
            
            if (acquire_result == 0) {
                /* Hold the lock for a reasonable time for parent to test */
                sleep(5); /* Hold lock for 5 seconds, then auto-release */
                release_lock();
            }
            
            exit(acquire_result == 0 ? 0 : 1);
        } else if (child_pids[i] < 0) {
            TEST_ASSERT(0, "Failed to fork child process");
            /* Cleanup pipes */
            for (int j = 0; j <= i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return 1;
        }
        
        /* Parent: close write end for this child */
        close(pipes[i][1]);
    }
    
    /* Wait for all children to acquire their slots */
    int successful_acquisitions = 0;
    for (i = 0; i < max_holders; i++) {
        char child_signal;
        if (read(pipes[i][0], &child_signal, 1) == 1 && child_signal == 'S') {
            successful_acquisitions++;
        }
        close(pipes[i][0]); /* Close read end after receiving signal */
    }
    
    printf("  → Successful acquisitions: %d/%d\n", successful_acquisitions, max_holders);
    TEST_ASSERT(successful_acquisitions == max_holders, 
                "All children should successfully acquire semaphore slots");
    
    /* Test that all slots are occupied - try to acquire one more */
    int fourth_result = acquire_lock(test_descriptor, max_holders, 0.5);
    TEST_ASSERT(fourth_result != 0, "Fourth slot should not be available");
    
    /* Wait for all children to complete naturally */
    for (i = 0; i < max_holders; i++) {
        if (child_pids[i] > 0) {
            int status;
            waitpid(child_pids[i], &status, 0);
            TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0, 
                       "Child should successfully acquire and release slot");
        }
    }
    
    return 0;
}

/* Test framework summary */
void test_lock_summary(void) {
    printf("\n=== LOCK TEST SUMMARY ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", pass_count);
    printf("Failed: %d\n", fail_count);
    if (fail_count == 0) {
        printf("All lock tests passed!\n");
    } else {
        printf("Some lock tests failed!\n");
    }
}

/* Main test runner for lock module */
int run_lock_tests(void) {
    printf("=== LOCK MODULE TEST SUITE ===\n");
    
    /* Clean up any existing test lock files from previous runs */
    char *lock_dir = find_lock_directory();
    if (lock_dir) {
        char cleanup_cmd[PATH_MAX + 50];
        snprintf(cleanup_cmd, sizeof(cleanup_cmd), "rm -f %s/test_*.lock 2>/dev/null || true", lock_dir);
        int sys_result = system(cleanup_cmd);
    (void)sys_result; /* Suppress unused variable warning */
        printf("  → Cleaned up existing test locks in %s\n", lock_dir);
    }
    
    /* Reset counters */
    test_count = 0;
    pass_count = 0;
    fail_count = 0;
    
    /* Run all lock tests */
    test_find_lock_directory();
    test_portable_lock();
    test_acquire_lock();
    test_release_lock();
    test_check_lock();
    test_list_locks();
    test_done_lock();
    test_lock_timeout();
    test_text_lock_file();
    test_binary_lock_file();
    test_stale_lock_detection();
    test_semaphore_slots();
    
    test_lock_summary();
    
    return (fail_count > 0) ? 1 : 0;
}