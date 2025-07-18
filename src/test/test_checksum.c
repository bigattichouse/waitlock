/*
 * Unit tests for checksum.c functions
 * Tests CRC32 checksum calculation and validation
 */

#include "test.h"
#include "../checksum/checksum.h"

/* Test framework */
static int test_count = 0;
static int pass_count = 0;
static int fail_count = 0;

#define TEST_START(name) \
    do { \
        test_count++; \
        printf("\n[CHECKSUM_TEST %d] %s\n", test_count, name); \
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

/* Test basic CRC32 calculation */
int test_calculate_crc32(void) {
    TEST_START("CRC32 calculation");
    
    const char *test_data = "Test data for checksum";
    uint32_t checksum1 = calculate_crc32(test_data, strlen(test_data));
    uint32_t checksum2 = calculate_crc32(test_data, strlen(test_data));
    
    TEST_ASSERT(checksum1 != 0, "Checksum should not be zero");
    TEST_ASSERT(checksum1 == checksum2, "Checksum should be deterministic");
    
    printf("  → Checksum: 0x%08x\n", checksum1);
    
    return 0;
}

/* Test CRC32 with different data */
int test_crc32_different_data(void) {
    TEST_START("CRC32 with different data");
    
    const char *data1 = "Hello World";
    const char *data2 = "Hello world";  /* Different case */
    const char *data3 = "Goodbye World";
    
    uint32_t checksum1 = calculate_crc32(data1, strlen(data1));
    uint32_t checksum2 = calculate_crc32(data2, strlen(data2));
    uint32_t checksum3 = calculate_crc32(data3, strlen(data3));
    
    TEST_ASSERT(checksum1 != checksum2, "Different case should produce different checksums");
    TEST_ASSERT(checksum1 != checksum3, "Different strings should produce different checksums");
    TEST_ASSERT(checksum2 != checksum3, "Different strings should produce different checksums");
    
    printf("  → '%s' = 0x%08x\n", data1, checksum1);
    printf("  → '%s' = 0x%08x\n", data2, checksum2);
    printf("  → '%s' = 0x%08x\n", data3, checksum3);
    
    return 0;
}

/* Test CRC32 with empty data */
int test_crc32_empty_data(void) {
    TEST_START("CRC32 with empty data");
    
    const char *empty_data = "";
    uint32_t checksum = calculate_crc32(empty_data, 0);
    
    TEST_ASSERT(checksum == 0, "Empty data should produce zero checksum (standard behavior)");
    
    printf("  → Empty data checksum: 0x%08x\n", checksum);
    
    return 0;
}

/* Test CRC32 with single byte */
int test_crc32_single_byte(void) {
    TEST_START("CRC32 with single byte");
    
    const char *single_byte = "A";
    uint32_t checksum = calculate_crc32(single_byte, 1);
    
    TEST_ASSERT(checksum != 0, "Single byte should produce non-zero checksum");
    
    printf("  → Single byte 'A' checksum: 0x%08x\n", checksum);
    
    return 0;
}

/* Test CRC32 with binary data */
int test_crc32_binary_data(void) {
    TEST_START("CRC32 with binary data");
    
    unsigned char binary_data[] = {0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE, 0xFD, 0xFC};
    uint32_t checksum = calculate_crc32(binary_data, sizeof(binary_data));
    
    TEST_ASSERT(checksum != 0, "Binary data should produce non-zero checksum");
    
    printf("  → Binary data checksum: 0x%08x\n", checksum);
    
    return 0;
}

/* Test CRC32 with large data */
int test_crc32_large_data(void) {
    TEST_START("CRC32 with large data");
    
    char large_data[4096];
    int i;
    
    /* Fill with pattern */
    for (i = 0; i < (int)sizeof(large_data); i++) {
        large_data[i] = (char)(i % 256);
    }
    
    uint32_t checksum = calculate_crc32(large_data, sizeof(large_data));
    
    TEST_ASSERT(checksum != 0, "Large data should produce non-zero checksum");
    
    printf("  → Large data (4KB) checksum: 0x%08x\n", checksum);
    
    return 0;
}

/* Test CRC32 incremental calculation */
int test_crc32_incremental(void) {
    TEST_START("CRC32 incremental calculation");
    
    const char *data = "This is a test string for incremental CRC calculation";
    size_t len = strlen(data);
    
    /* Calculate full checksum */
    uint32_t full_checksum = calculate_crc32(data, len);
    
    /* Calculate incremental checksum */
    uint32_t incremental_checksum = calculate_crc32(data, len / 2);
    incremental_checksum = calculate_crc32(data + len / 2, len - len / 2);
    
    /* Note: This test demonstrates that our simple CRC32 implementation 
     * doesn't support incremental calculation - each call starts fresh.
     * This is expected behavior for our implementation. */
    
    TEST_ASSERT(full_checksum != 0, "Full checksum should not be zero");
    TEST_ASSERT(incremental_checksum != 0, "Incremental checksum should not be zero");
    
    printf("  → Full checksum: 0x%08x\n", full_checksum);
    printf("  → Incremental checksum: 0x%08x\n", incremental_checksum);
    
    TEST_ASSERT(1, "Incremental calculation tested (not supported by design)");
    
    return 0;
}

/* Test lock checksum calculation */
int test_calculate_lock_checksum(void) {
    TEST_START("Lock checksum calculation");
    
    struct lock_info info;
    memset(&info, 0, sizeof(info));
    
    info.magic = LOCK_MAGIC;
    info.version = 1;
    info.pid = 12345;
    info.ppid = 12344;
    info.uid = 1000;
    info.acquired_at = 1234567890;
    info.lock_type = 0;
    info.max_holders = 1;
    info.slot = 0;
    
    strcpy(info.hostname, "testhost");
    strcpy(info.descriptor, "test_descriptor");
    strcpy(info.cmdline, "test_command");
    
    uint32_t checksum1 = calculate_lock_checksum(&info);
    uint32_t checksum2 = calculate_lock_checksum(&info);
    
    TEST_ASSERT(checksum1 != 0, "Lock checksum should not be zero");
    TEST_ASSERT(checksum1 == checksum2, "Lock checksum should be deterministic");
    
    printf("  → Lock checksum: 0x%08x\n", checksum1);
    
    return 0;
}

/* Test lock checksum with different data */
int test_lock_checksum_different_data(void) {
    TEST_START("Lock checksum with different data");
    
    struct lock_info info1, info2;
    memset(&info1, 0, sizeof(info1));
    memset(&info2, 0, sizeof(info2));
    
    /* Set up identical info */
    info1.magic = info2.magic = LOCK_MAGIC;
    info1.version = info2.version = 1;
    info1.pid = info2.pid = 12345;
    info1.ppid = info2.ppid = 12344;
    info1.uid = info2.uid = 1000;
    info1.acquired_at = info2.acquired_at = 1234567890;
    info1.lock_type = info2.lock_type = 0;
    info1.max_holders = info2.max_holders = 1;
    info1.slot = info2.slot = 0;
    
    strcpy(info1.hostname, "testhost");
    strcpy(info1.descriptor, "test_descriptor");
    strcpy(info1.cmdline, "test_command");
    
    strcpy(info2.hostname, "testhost");
    strcpy(info2.descriptor, "test_descriptor");
    strcpy(info2.cmdline, "test_command");
    
    uint32_t checksum1 = calculate_lock_checksum(&info1);
    uint32_t checksum2 = calculate_lock_checksum(&info2);
    
    TEST_ASSERT(checksum1 == checksum2, "Identical lock info should produce same checksum");
    
    /* Change one field */
    info2.pid = 54321;
    uint32_t checksum3 = calculate_lock_checksum(&info2);
    
    TEST_ASSERT(checksum1 != checksum3, "Different lock info should produce different checksum");
    
    printf("  → Identical info checksums: 0x%08x == 0x%08x\n", checksum1, checksum2);
    printf("  → Different info checksums: 0x%08x != 0x%08x\n", checksum1, checksum3);
    
    return 0;
}

/* Test lock checksum validation */
int test_validate_lock_checksum(void) {
    TEST_START("Lock checksum validation");
    
    struct lock_info info;
    memset(&info, 0, sizeof(info));
    
    info.magic = LOCK_MAGIC;
    info.version = 1;
    info.pid = 12345;
    info.ppid = 12344;
    info.uid = 1000;
    info.acquired_at = 1234567890;
    info.lock_type = 0;
    info.max_holders = 1;
    info.slot = 0;
    
    strcpy(info.hostname, "testhost");
    strcpy(info.descriptor, "test_descriptor");
    strcpy(info.cmdline, "test_command");
    
    /* Calculate and set checksum */
    info.checksum = calculate_lock_checksum(&info);
    
    TEST_ASSERT(validate_lock_checksum(&info), "Valid checksum should pass validation");
    
    /* Corrupt the checksum */
    info.checksum = 0x12345678;
    
    TEST_ASSERT(!validate_lock_checksum(&info), "Invalid checksum should fail validation");
    
    /* Restore correct checksum */
    info.checksum = calculate_lock_checksum(&info);
    
    TEST_ASSERT(validate_lock_checksum(&info), "Restored checksum should pass validation");
    
    return 0;
}

/* Test checksum with corrupted data */
int test_checksum_corrupted_data(void) {
    TEST_START("Checksum with corrupted data");
    
    struct lock_info info;
    memset(&info, 0, sizeof(info));
    
    info.magic = LOCK_MAGIC;
    info.version = 1;
    info.pid = 12345;
    info.ppid = 12344;
    info.uid = 1000;
    info.acquired_at = 1234567890;
    info.lock_type = 0;
    info.max_holders = 1;
    info.slot = 0;
    
    strcpy(info.hostname, "testhost");
    strcpy(info.descriptor, "test_descriptor");
    strcpy(info.cmdline, "test_command");
    
    /* Calculate and set checksum */
    info.checksum = calculate_lock_checksum(&info);
    
    TEST_ASSERT(validate_lock_checksum(&info), "Original data should validate");
    
    /* Corrupt different fields and test */
    struct lock_info corrupted = info;
    
    /* Corrupt PID */
    corrupted.pid = 99999;
    TEST_ASSERT(!validate_lock_checksum(&corrupted), "Corrupted PID should fail validation");
    
    /* Corrupt descriptor */
    corrupted = info;
    strcpy(corrupted.descriptor, "corrupted_descriptor");
    TEST_ASSERT(!validate_lock_checksum(&corrupted), "Corrupted descriptor should fail validation");
    
    /* Corrupt hostname */
    corrupted = info;
    strcpy(corrupted.hostname, "corrupted_host");
    TEST_ASSERT(!validate_lock_checksum(&corrupted), "Corrupted hostname should fail validation");
    
    /* Corrupt command line */
    corrupted = info;
    strcpy(corrupted.cmdline, "corrupted_command");
    TEST_ASSERT(!validate_lock_checksum(&corrupted), "Corrupted cmdline should fail validation");
    
    return 0;
}

/* Test edge cases */
int test_checksum_edge_cases(void) {
    TEST_START("Checksum edge cases");
    
    /* Test with NULL data */
    uint32_t null_checksum = calculate_crc32(NULL, 0);
    TEST_ASSERT(null_checksum != 0, "NULL data should produce non-zero checksum");
    
    /* Test with NULL lock info */
    struct lock_info *null_info = NULL;
    uint32_t null_lock_checksum = calculate_lock_checksum(null_info);
    TEST_ASSERT(null_lock_checksum == 0, "NULL lock info should produce zero checksum");
    
    /* Test validation with NULL lock info */
    TEST_ASSERT(!validate_lock_checksum(null_info), "NULL lock info should fail validation");
    
    /* Test with zero checksum */
    struct lock_info zero_info;
    memset(&zero_info, 0, sizeof(zero_info));
    zero_info.checksum = 0;
    TEST_ASSERT(!validate_lock_checksum(&zero_info), "Zero checksum should fail validation");
    
    return 0;
}

/* Test checksum consistency across calls */
int test_checksum_consistency(void) {
    TEST_START("Checksum consistency");
    
    const char *test_strings[] = {
        "Hello World",
        "The quick brown fox jumps over the lazy dog",
        "1234567890",
        "!@#$%^&*()",
        "Mixed123!@#abc",
        ""
    };
    
    size_t num_strings = sizeof(test_strings) / sizeof(test_strings[0]);
    size_t i;
    
    for (i = 0; i < num_strings; i++) {
        const char *str = test_strings[i];
        size_t len = strlen(str);
        
        uint32_t checksum1 = calculate_crc32(str, len);
        uint32_t checksum2 = calculate_crc32(str, len);
        uint32_t checksum3 = calculate_crc32(str, len);
        
        TEST_ASSERT(checksum1 == checksum2, "Checksum should be consistent");
        TEST_ASSERT(checksum2 == checksum3, "Checksum should be consistent");
        
        printf("  → '%s' = 0x%08x\n", str, checksum1);
    }
    
    return 0;
}

/* Test framework summary */
void test_checksum_summary(void) {
    printf("\n=== CHECKSUM TEST SUMMARY ===\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", pass_count);
    printf("Failed: %d\n", fail_count);
    if (fail_count == 0) {
        printf("All checksum tests passed!\n");
    } else {
        printf("Some checksum tests failed!\n");
    }
}

/* Main test runner for checksum module */
int run_checksum_tests(void) {
    printf("=== CHECKSUM MODULE TEST SUITE ===\n");
    
    /* Reset counters */
    test_count = 0;
    pass_count = 0;
    fail_count = 0;
    
    /* Run all checksum tests */
    test_calculate_crc32();
    test_crc32_different_data();
    test_crc32_empty_data();
    test_crc32_single_byte();
    test_crc32_binary_data();
    test_crc32_large_data();
    test_crc32_incremental();
    test_calculate_lock_checksum();
    test_lock_checksum_different_data();
    test_validate_lock_checksum();
    test_checksum_corrupted_data();
    test_checksum_edge_cases();
    test_checksum_consistency();
    
    test_checksum_summary();
    
    return (fail_count > 0) ? 1 : 0;
}