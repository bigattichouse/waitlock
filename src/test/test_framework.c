/*
 * Test framework utilities implementation
 */

#include "test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

/* Clean up any leftover test artifacts from previous runs */
void test_cleanup_global(void) {
    char cmd[512];
    
    /* Kill any lingering waitlock test processes */
    snprintf(cmd, sizeof(cmd), "pkill -f 'waitlock.*test_' 2>/dev/null || true");
    system(cmd);
    
    /* Remove any test lock files */
    snprintf(cmd, sizeof(cmd), "rm -f /var/lock/waitlock/test_*.lock 2>/dev/null || true");
    system(cmd);
    
    /* Small delay to ensure cleanup completes */
    usleep(200000); /* 200ms */
}

/* Initialize test context with unique directory */
int test_setup_context(test_context_t *ctx, const char *test_name) {
    /* Save original lock directory */
    const char *original_dir = getenv("WAITLOCK_DIR");
    if (original_dir) {
        strncpy(ctx->original_lock_dir, original_dir, sizeof(ctx->original_lock_dir) - 1);
        ctx->original_lock_dir[sizeof(ctx->original_lock_dir) - 1] = '\0';
    } else {
        ctx->original_lock_dir[0] = '\0';
    }
    
    /* Create unique test directory */
    snprintf(ctx->test_dir, sizeof(ctx->test_dir), 
             "/tmp/waitlock_test_%s_%d_%ld", 
             test_name, getpid(), time(NULL));
    
    snprintf(ctx->lock_dir, sizeof(ctx->lock_dir), 
             "%s/locks", ctx->test_dir);
    
    /* Create directories */
    if (mkdir(ctx->test_dir, 0755) != 0) {
        printf("Failed to create test directory %s: %s\n", ctx->test_dir, strerror(errno));
        return -1;
    }
    
    if (mkdir(ctx->lock_dir, 0755) != 0) {
        printf("Failed to create lock directory %s: %s\n", ctx->lock_dir, strerror(errno));
        rmdir(ctx->test_dir);
        return -1;
    }
    
    /* Set environment for this test */
    if (setenv("WAITLOCK_DIR", ctx->lock_dir, 1) != 0) {
        printf("Failed to set WAITLOCK_DIR environment variable\n");
        rmdir(ctx->lock_dir);
        rmdir(ctx->test_dir);
        return -1;
    }
    
    ctx->test_pid = getpid();
    ctx->cleanup_needed = 1;
    
    return 0;
}

/* Cleanup test context and restore environment */
int test_teardown_context(test_context_t *ctx) {
    if (!ctx->cleanup_needed) {
        return 0;
    }
    
    /* Kill any child processes spawned during this test */
    /* Note: This is a simple approach - in production we'd track child PIDs */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "pkill -P %d 2>/dev/null || true", ctx->test_pid);
    system(cmd);
    
    /* Small delay to let processes cleanup */
    usleep(100000); /* 100ms */
    
    /* Clean up any remaining test lock files */
    snprintf(cmd, sizeof(cmd), "rm -f /var/lock/waitlock/test_*.lock 2>/dev/null || true");
    system(cmd);
    
    /* Remove test directory recursively */
    snprintf(cmd, sizeof(cmd), "rm -rf %s", ctx->test_dir);
    system(cmd);
    
    /* Restore original environment */
    if (ctx->original_lock_dir[0] != '\0') {
        setenv("WAITLOCK_DIR", ctx->original_lock_dir, 1);
    } else {
        unsetenv("WAITLOCK_DIR");
    }
    
    ctx->cleanup_needed = 0;
    return 0;
}