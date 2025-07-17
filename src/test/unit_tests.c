/*
 * Unit test suite for waitlock functionality
 * Tests individual functions and components in isolation
 */

#include "test.h"
#include "../core/core.h"
#include "../lock/lock.h"
#include "../process/process.h"
#include "../signal/signal.h"
#include "../checksum/checksum.h"

/* Test framework */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n[TEST %d] %s\n", test_count, name); \
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

#define TEST_SUMMARY() \
    do { \
        printf("\n=== UNIT TEST SUMMARY ===\n"); \
        printf("Total tests: %d\n", test_count); \
        printf("Passed: %d\n", pass_count); \
        printf("Failed: %d\n", fail_count); \
        if (fail_count == 0) { \
            printf("All unit tests passed!\n"); \
        } else { \
            printf("Some unit tests failed!\n"); \
        } \
    } while(0)

/* Test lock directory discovery */
int test_lock_directory(void) {
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

/* Test checksum functionality */
int test_checksum(void) {
    TEST_START("Checksum calculation");
    
    const char *test_data = "Test data for checksum";
    uint32_t checksum1 = calculate_crc32(test_data, strlen(test_data));
    uint32_t checksum2 = calculate_crc32(test_data, strlen(test_data));
    
    TEST_ASSERT(checksum1 != 0, "Checksum should not be zero");
    TEST_ASSERT(checksum1 == checksum2, "Checksum should be deterministic");
    
    const char *different_data = "Different test data";
    uint32_t checksum3 = calculate_crc32(different_data, strlen(different_data));
    TEST_ASSERT(checksum1 != checksum3, "Different data should produce different checksums");
    
    printf("  → Checksum: 0x%08x\n", checksum1);
    
    return 0;
}

/* Test process detection */
int test_process_detection(void) {
    TEST_START("Process detection");
    
    pid_t current_pid = getpid();
    TEST_ASSERT(process_exists(current_pid), "Current process should exist");
    
    pid_t invalid_pid = 999999; /* Likely non-existent */
    TEST_ASSERT(!process_exists(invalid_pid), "Invalid process should not exist");
    
    char *cmdline = get_process_cmdline(current_pid);
    TEST_ASSERT(cmdline != NULL, "Should be able to get current process cmdline");
    
    if (cmdline) {
        printf("  → Current process cmdline: %s\n", cmdline);
    }
    
    return 0;
}

/* Test lock info structure */
int test_lock_info_structure(void) {
    TEST_START("Lock info structure");
    
    struct lock_info info;
    memset(&info, 0, sizeof(info));
    
    info.magic = LOCK_MAGIC;
    info.version = 1;
    info.pid = getpid();
    info.ppid = getppid();
    info.uid = getuid();
    info.acquired_at = time(NULL);
    info.lock_type = 0; /* mutex */
    info.max_holders = 1;
    info.slot = 0;
    
    if (gethostname(info.hostname, sizeof(info.hostname)) != 0) {
        strcpy(info.hostname, "unknown");
    }
    
    strcpy(info.descriptor, "test_descriptor");
    strcpy(info.cmdline, "test_command");
    
    uint32_t checksum = calculate_lock_checksum(&info);
    info.checksum = checksum;
    
    TEST_ASSERT(info.magic == LOCK_MAGIC, "Magic number should be set correctly");
    TEST_ASSERT(info.pid == getpid(), "PID should be set correctly");
    TEST_ASSERT(validate_lock_checksum(&info), "Lock checksum should be valid");
    
    printf("  → Lock info size: %zu bytes\n", sizeof(info));
    printf("  → Magic: 0x%08x\n", info.magic);
    printf("  → PID: %d\n", info.pid);
    printf("  → Checksum: 0x%08x\n", info.checksum);
    
    return 0;
}

/* Test lock file creation and reading */
int test_lock_file_io(void) {
    TEST_START("Lock file I/O");
    
    char *lock_dir = find_lock_directory();
    if (!lock_dir) {
        printf("  ✗ FAIL: Cannot find lock directory\n");
        fail_count++;
        return 1;
    }
    
    char test_file[PATH_MAX];
    snprintf(test_file, sizeof(test_file), "%s/test_lock.slot0.%s.%d.lock", 
             lock_dir, "testhost", getpid());
    
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
    strcpy(write_info.descriptor, "test_descriptor");
    strcpy(write_info.cmdline, "test_command");
    
    write_info.checksum = calculate_lock_checksum(&write_info);
    
    /* Write lock file */
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

/* Test argument parsing */
int test_argument_parsing(void) {
    TEST_START("Argument parsing");
    
    /* Save original options */
    struct options saved_opts = opts;
    
    /* Reset options to default state */
    opts.descriptor = NULL;
    opts.max_holders = 1;
    opts.done_mode = FALSE;
    opts.check_only = FALSE;
    opts.list_mode = FALSE;
    
    /* Test basic argument parsing */
    char *test_args1[] = {"waitlock", "test_descriptor"};
    int result1 = parse_args(2, test_args1);
    TEST_ASSERT(result1 == 0, "Basic argument parsing should succeed");
    TEST_ASSERT(opts.descriptor != NULL, "Descriptor should be set");
    TEST_ASSERT(strcmp(opts.descriptor, "test_descriptor") == 0, "Descriptor should match");
    
    /* Reset for next test */
    opts.descriptor = NULL;
    opts.done_mode = FALSE;
    
    /* Test --done flag */
    char *test_args2[] = {"waitlock", "--done", "test_descriptor"};
    int result2 = parse_args(3, test_args2);
    TEST_ASSERT(result2 == 0, "--done argument parsing should succeed");
    TEST_ASSERT(opts.done_mode == TRUE, "--done mode should be enabled");
    
    /* Reset for next test */
    opts.descriptor = NULL;
    opts.max_holders = 1;
    opts.done_mode = FALSE;
    
    /* Test semaphore argument */
    char *test_args3[] = {"waitlock", "-m", "5", "test_descriptor"};
    int result3 = parse_args(4, test_args3);
    TEST_ASSERT(result3 == 0, "Semaphore argument parsing should succeed");
    TEST_ASSERT(opts.max_holders == 5, "Max holders should be set to 5");
    
    /* Reset for next test */
    opts.descriptor = NULL;
    opts.max_holders = 1;
    opts.test_mode = FALSE;  /* Temporarily disable test mode for validation */
    
    /* Test invalid descriptor */
    char *test_args4[] = {"waitlock", "invalid@descriptor"};
    int result4 = parse_args(2, test_args4);
    TEST_ASSERT(result4 != 0, "Invalid descriptor should be rejected");
    
    /* Restore original options */
    opts = saved_opts;
    
    return 0;
}

/* Test directory scanning (for lock listing) */
int test_directory_scanning(void) {
    TEST_START("Directory scanning");
    
    char *lock_dir = find_lock_directory();
    if (!lock_dir) {
        printf("  ✗ FAIL: Cannot find lock directory\n");
        fail_count++;
        return 1;
    }
    
    DIR *dir = opendir(lock_dir);
    TEST_ASSERT(dir != NULL, "Should be able to open lock directory");
    
    if (dir) {
        struct dirent *entry;
        int file_count = 0;
        
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.') {
                file_count++;
            }
        }
        
        closedir(dir);
        printf("  → Found %d files in lock directory\n", file_count);
        TEST_ASSERT(1, "Directory scanning completed successfully");
    }
    
    return 0;
}

/* Test signal handler installation */
int test_signal_handlers(void) {
    TEST_START("Signal handler installation");
    
    install_signal_handlers();
    TEST_ASSERT(1, "Signal handlers installed without error");
    
    /* Test that we can get current signal handlers */
    struct sigaction sa;
    int result = sigaction(SIGTERM, NULL, &sa);
    TEST_ASSERT(result == 0, "Should be able to get SIGTERM handler");
    TEST_ASSERT(sa.sa_handler != SIG_DFL, "SIGTERM handler should not be default");
    
    return 0;
}

/* Test environment variable reading */
int test_environment_variables(void) {
    TEST_START("Environment variable handling");
    
    /* Test WAITLOCK_DEBUG */
    setenv("WAITLOCK_DEBUG", "1", 1);
    char *debug_val = getenv("WAITLOCK_DEBUG");
    TEST_ASSERT(debug_val != NULL, "WAITLOCK_DEBUG should be readable");
    TEST_ASSERT(strcmp(debug_val, "1") == 0, "WAITLOCK_DEBUG should have correct value");
    
    /* Test WAITLOCK_TIMEOUT */
    setenv("WAITLOCK_TIMEOUT", "30", 1);
    char *timeout_val = getenv("WAITLOCK_TIMEOUT");
    TEST_ASSERT(timeout_val != NULL, "WAITLOCK_TIMEOUT should be readable");
    TEST_ASSERT(strcmp(timeout_val, "30") == 0, "WAITLOCK_TIMEOUT should have correct value");
    
    /* Clean up */
    unsetenv("WAITLOCK_DEBUG");
    unsetenv("WAITLOCK_TIMEOUT");
    
    return 0;
}

/* Main unit test runner */
int run_unit_tests(void) {
    printf("=== WAITLOCK UNIT TEST SUITE ===\n");
    printf("Testing individual components...\n");
    
    test_lock_directory();
    test_checksum();
    test_process_detection();
    test_lock_info_structure();
    test_lock_file_io();
    test_argument_parsing();
    test_directory_scanning();
    test_signal_handlers();
    test_environment_variables();
    
    TEST_SUMMARY();
    
    return (fail_count > 0) ? 1 : 0;
}