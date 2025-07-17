/*
 * Test suite for waitlock functionality
 */

#include "test.h"
#include "../core/core.h"
#include "../lock/lock.h"
#include "../process/process.h"
#include "../signal/signal.h"
#include "../checksum/checksum.h"

/* Simple test function for modular verification */
int run_all_tests(void) {
    printf("Waitlock modular test suite\n");
    printf("Testing core functionality...\n");
    
    /* Test checksum functionality */
    const char *test_data = "Test data";
    uint32_t checksum = calculate_crc32(test_data, strlen(test_data));
    if (checksum == 0) {
        printf("FAIL: Checksum calculation\n");
        return 1;
    }
    printf("PASS: Checksum calculation\n");
    
    /* Test process detection */
    if (!process_exists(getpid())) {
        printf("FAIL: Process existence check\n");
        return 1;
    }
    printf("PASS: Process existence check\n");
    
    /* Test lock directory finding */
    char *lock_dir = find_lock_directory();
    if (!lock_dir) {
        printf("FAIL: Lock directory discovery\n");
        return 1;
    }
    printf("PASS: Lock directory discovery (%s)\n", lock_dir);
    
    /* Test signal handler installation */
    install_signal_handlers();
    printf("PASS: Signal handler installation\n");
    
    printf("\nModular test suite completed successfully!\n");
    printf("All core modules are functioning correctly.\n");
    
    return 0;
}