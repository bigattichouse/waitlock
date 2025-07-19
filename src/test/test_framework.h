/*
 * Test framework utilities for better test isolation and coordination
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <sys/types.h>
#include <limits.h>

/* Test context for isolated test environments */
typedef struct {
    char test_dir[PATH_MAX];
    char lock_dir[PATH_MAX]; 
    char original_lock_dir[PATH_MAX];
    pid_t test_pid;
    int cleanup_needed;
} test_context_t;

/* Clean up any leftover test artifacts from previous runs */
void test_cleanup_global(void);

/* Initialize test context with unique directory */
int test_setup_context(test_context_t *ctx, const char *test_name);

/* Cleanup test context and restore environment */
int test_teardown_context(test_context_t *ctx);

/* Enhanced test macros with context and better error reporting */
#define TEST_START_CTX(ctx, name) \
    do { \
        if (test_setup_context(ctx, name) != 0) { \
            printf("  ✗ FAIL: Cannot setup test context for %s\n", name); \
            fail_count++; \
            return 1; \
        } \
        test_count++; \
        printf("\n[LOCK_TEST %d] %s\n", test_count, name); \
        printf("  → Test dir: %s\n", ctx->test_dir); \
    } while(0)

#define TEST_END_CTX(ctx) \
    do { \
        test_teardown_context(ctx); \
    } while(0)

/* Enhanced TEST_ASSERT with more context */
#define TEST_ASSERT_CTX(ctx, condition, message) \
    do { \
        if (condition) { \
            pass_count++; \
            printf("  ✓ PASS: %s\n", message); \
        } else { \
            fail_count++; \
            printf("  ✗ FAIL: %s\n", message); \
            printf("    Context: PID=%d, Test=%s\n", getpid(), ctx->test_dir); \
            printf("    Lock dir: %s\n", ctx->lock_dir); \
            printf("    Current locks:\n"); \
            char cmd[PATH_MAX + 20]; \
            snprintf(cmd, sizeof(cmd), "ls -la %s/ 2>/dev/null | head -5", ctx->lock_dir); \
            system(cmd); \
        } \
    } while(0)

#endif /* TEST_FRAMEWORK_H */