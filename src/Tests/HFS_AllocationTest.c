/*
 * HFS_AllocationTest.c - Test suite for HFS allocation and I/O
 *
 * Tests the newly integrated HFS_Allocation.c functionality:
 * - Block allocation and deallocation
 * - Cache management
 * - Extent mapping
 * - File I/O operations
 */

#include "SystemTypes.h"
#include "FileManager.h"
#include "FileManager_Internal.h"
#include "System71StdLib.h"
#include <string.h>

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            tests_passed++; \
            serial_printf("[PASS] %s\n", message); \
        } else { \
            tests_failed++; \
            serial_printf("[FAIL] %s\n", message); \
        } \
    } while(0)

/* Mock VCB for testing */
static VCBExt test_vcb;
static UInt8 test_bitmap[1024];  /* 8192 blocks */

/* Mock device I/O functions */
static OSErr mock_device_read(UInt16 device, uint64_t offset, UInt32 bytes, void* buffer) {
    /* Simulate successful read */
    memset(buffer, 0, bytes);
    return noErr;
}

static OSErr mock_device_write(UInt16 device, uint64_t offset, UInt32 bytes, const void* buffer) {
    /* Simulate successful write */
    return noErr;
}

/* Initialize test VCB */
static void init_test_vcb(void) {
    memset(&test_vcb, 0, sizeof(VCBExt));

    /* Set up basic VCB fields */
    test_vcb.base.vcbSigWord = 0x4244;  /* HFS signature */
    test_vcb.base.vcbNmAlBlks = 8192;   /* Total allocation blocks */
    test_vcb.base.vcbFreeBks = 8192;    /* All free initially */
    test_vcb.base.vcbAlBlkSiz = 4096;   /* 4KB allocation blocks */
    test_vcb.base.vcbVBMSt = 3;         /* Bitmap starts at block 3 */
    test_vcb.base.vcbAllocPtr = 0;      /* Start allocation at beginning */
    test_vcb.vcbDevice = 0;             /* Device 0 */

    /* Set up bitmap cache */
    test_vcb.vcbVBMCache = test_bitmap;
    memset(test_bitmap, 0, sizeof(test_bitmap));
}

/* Test 1: Cache initialization */
static void test_cache_init(void) {
    OSErr err;

    serial_printf("\n=== Test 1: Cache Initialization ===\n");

    err = Cache_Init(32);
    TEST_ASSERT(err == noErr, "Cache_Init should succeed");
    TEST_ASSERT(g_FSGlobals.cacheBuffers != NULL, "Cache buffers should be allocated");
    TEST_ASSERT(g_FSGlobals.cacheSize == 32, "Cache size should be 32");
    TEST_ASSERT(g_FSGlobals.cacheFreeList != NULL, "Cache free list should exist");
}

/* Test 2: Allocation bitmap operations */
static void test_allocation_bitmap(void) {
    OSErr err;
    UInt32 actualStart, actualCount;

    serial_printf("\n=== Test 2: Allocation Bitmap Operations ===\n");

    /* Initialize allocation bitmap */
    err = Alloc_Init(&test_vcb);
    TEST_ASSERT(err == noErr, "Alloc_Init should succeed");

    /* Test allocating blocks */
    err = Alloc_Blocks(&test_vcb, 0, 10, 10, &actualStart, &actualCount);
    TEST_ASSERT(err == noErr, "Alloc_Blocks should succeed");
    TEST_ASSERT(actualCount == 10, "Should allocate 10 blocks");

    /* Test free block counting */
    UInt32 freeBlocks = Alloc_CountFree(&test_vcb);
    TEST_ASSERT(freeBlocks == (8192 - 10), "Free block count should be reduced");

    /* Test checking allocated blocks */
    Boolean allocated = Alloc_Check(&test_vcb, actualStart, actualCount);
    TEST_ASSERT(allocated == true, "Allocated blocks should be marked as allocated");

    /* Test freeing blocks */
    err = Alloc_Free(&test_vcb, actualStart, actualCount);
    TEST_ASSERT(err == noErr, "Alloc_Free should succeed");

    freeBlocks = Alloc_CountFree(&test_vcb);
    TEST_ASSERT(freeBlocks == 8192, "All blocks should be free after deallocation");

    /* Cleanup */
    Alloc_Close(&test_vcb);
}

/* Test 3: Extent operations */
static void test_extent_operations(void) {
    OSErr err;
    FCBExt test_fcb;
    UInt32 physBlock, contiguous;

    serial_printf("\n=== Test 3: Extent Operations ===\n");

    memset(&test_fcb, 0, sizeof(FCBExt));

    /* Set up test FCB with extents */
    test_fcb.base.fcbVPtr = (VCB*)&test_vcb;
    test_fcb.base.extent[0].startBlock = 100;
    test_fcb.base.extent[0].blockCount = 50;
    test_fcb.base.extent[1].startBlock = 200;
    test_fcb.base.extent[1].blockCount = 30;
    test_fcb.base.extent[2].startBlock = 0;
    test_fcb.base.extent[2].blockCount = 0;

    /* Test extent mapping - first extent */
    err = Ext_Map(&test_vcb, (FCB*)&test_fcb, 0, &physBlock, &contiguous);
    TEST_ASSERT(err == noErr, "Ext_Map should succeed for first block");
    TEST_ASSERT(physBlock == 100, "First block should map to physical block 100");
    TEST_ASSERT(contiguous == 50, "Should have 50 contiguous blocks");

    /* Test extent mapping - middle of first extent */
    err = Ext_Map(&test_vcb, (FCB*)&test_fcb, 25, &physBlock, &contiguous);
    TEST_ASSERT(err == noErr, "Ext_Map should succeed for middle block");
    TEST_ASSERT(physBlock == 125, "Block 25 should map to physical block 125");
    TEST_ASSERT(contiguous == 25, "Should have 25 contiguous blocks remaining");

    /* Test extent mapping - second extent */
    err = Ext_Map(&test_vcb, (FCB*)&test_fcb, 50, &physBlock, &contiguous);
    TEST_ASSERT(err == noErr, "Ext_Map should succeed for second extent");
    TEST_ASSERT(physBlock == 200, "Block 50 should map to physical block 200");
    TEST_ASSERT(contiguous == 30, "Should have 30 contiguous blocks");

    /* Test extent mapping - beyond extents */
    err = Ext_Map(&test_vcb, (FCB*)&test_fcb, 100, &physBlock, &contiguous);
    TEST_ASSERT(err == fxRangeErr, "Ext_Map should fail for block beyond extents");
}

/* Test 4: File extension */
static void test_file_extension(void) {
    OSErr err;
    FCBExt test_fcb;

    serial_printf("\n=== Test 4: File Extension ===\n");

    /* Initialize allocation bitmap */
    err = Alloc_Init(&test_vcb);
    TEST_ASSERT(err == noErr, "Alloc_Init should succeed");

    memset(&test_fcb, 0, sizeof(FCBExt));
    test_fcb.base.fcbVPtr = (VCB*)&test_vcb;
    test_fcb.base.fcbPLen = 0;
    test_fcb.base.fcbEOF = 0;
    test_fcb.base.fcbClpSiz = 4096;  /* 1 block clump size */

    /* Extend file to 20KB (5 blocks) */
    err = Ext_Extend(&test_vcb, (FCB*)&test_fcb, 20480);
    TEST_ASSERT(err == noErr, "Ext_Extend should succeed");
    TEST_ASSERT(test_fcb.base.fcbEOF == 20480, "EOF should be 20480");
    TEST_ASSERT(test_fcb.base.extent[0].blockCount > 0, "Should have allocated blocks");

    /* Verify blocks were allocated */
    UInt32 freeBlocks = Alloc_CountFree(&test_vcb);
    TEST_ASSERT(freeBlocks < 8192, "Some blocks should be allocated");

    /* Cleanup */
    Alloc_Close(&test_vcb);
}

/* Test 5: File truncation */
static void test_file_truncation(void) {
    OSErr err;
    FCBExt test_fcb;

    serial_printf("\n=== Test 5: File Truncation ===\n");

    /* Initialize allocation bitmap */
    err = Alloc_Init(&test_vcb);
    TEST_ASSERT(err == noErr, "Alloc_Init should succeed");

    memset(&test_fcb, 0, sizeof(FCBExt));
    test_fcb.base.fcbVPtr = (VCB*)&test_vcb;
    test_fcb.base.fcbClpSiz = 4096;

    /* Extend file first */
    err = Ext_Extend(&test_vcb, (FCB*)&test_fcb, 40960);  /* 10 blocks */
    TEST_ASSERT(err == noErr, "Ext_Extend should succeed");

    UInt32 blocksAfterExtend = 8192 - Alloc_CountFree(&test_vcb);

    /* Truncate file to smaller size */
    err = Ext_Truncate(&test_vcb, (FCB*)&test_fcb, 8192);  /* 2 blocks */
    TEST_ASSERT(err == noErr, "Ext_Truncate should succeed");
    TEST_ASSERT(test_fcb.base.fcbEOF == 8192, "EOF should be 8192");

    UInt32 blocksAfterTruncate = 8192 - Alloc_CountFree(&test_vcb);
    TEST_ASSERT(blocksAfterTruncate < blocksAfterExtend, "Should have freed some blocks");

    /* Cleanup */
    Alloc_Close(&test_vcb);
}

/* Test 6: Block I/O operations */
static void test_block_io(void) {
    OSErr err;
    UInt8 writeBuffer[512];
    UInt8 readBuffer[512];

    serial_printf("\n=== Test 6: Block I/O Operations ===\n");

    /* Set up platform hooks */
    g_PlatformHooks.DeviceRead = mock_device_read;
    g_PlatformHooks.DeviceWrite = mock_device_write;

    /* Prepare test data */
    for (int i = 0; i < 512; i++) {
        writeBuffer[i] = (UInt8)(i & 0xFF);
    }

    /* Test write blocks */
    err = IO_WriteBlocks(&test_vcb, 10, 1, writeBuffer);
    TEST_ASSERT(err == noErr, "IO_WriteBlocks should succeed");

    /* Test read blocks */
    err = IO_ReadBlocks(&test_vcb, 10, 1, readBuffer);
    TEST_ASSERT(err == noErr, "IO_ReadBlocks should succeed");
}

/* Test 7: Cache operations */
static void test_cache_operations(void) {
    OSErr err;
    CacheBuffer* buffer;

    serial_printf("\n=== Test 7: Cache Operations ===\n");

    /* Set up platform hooks */
    g_PlatformHooks.DeviceRead = mock_device_read;
    g_PlatformHooks.DeviceWrite = mock_device_write;

    /* Get a cache block */
    err = Cache_GetBlock(&test_vcb, 5, &buffer);
    TEST_ASSERT(err == noErr, "Cache_GetBlock should succeed");
    TEST_ASSERT(buffer != NULL, "Cache buffer should not be NULL");
    TEST_ASSERT(buffer->cbVCB == &test_vcb, "Buffer VCB should match");
    TEST_ASSERT(buffer->cbBlkNum == 5, "Buffer block number should be 5");

    /* Release cache block */
    err = Cache_ReleaseBlock(buffer, false);
    TEST_ASSERT(err == noErr, "Cache_ReleaseBlock should succeed");

    /* Test cache statistics */
    TEST_ASSERT(g_FSGlobals.cacheMisses > 0, "Should have cache misses");

    /* Flush cache */
    err = Cache_FlushAll();
    TEST_ASSERT(err == noErr, "Cache_FlushAll should succeed");
}

/* Main test runner */
void HFS_AllocationTest_Run(void) {
    serial_puts("[HFS_TEST] Test function entered\n");
    serial_puts("\n");
    serial_puts("=====================================\n");
    serial_puts("  HFS Allocation & I/O Test Suite\n");
    serial_puts("=====================================\n");

    /* Initialize test environment */
    serial_puts("[HFS_TEST] Initializing test VCB\n");
    init_test_vcb();
    serial_puts("[HFS_TEST] Test VCB initialized\n");

    /* Run all tests */
    test_cache_init();
    test_allocation_bitmap();
    test_extent_operations();
    test_file_extension();
    test_file_truncation();
    test_block_io();
    test_cache_operations();

    /* Cleanup */
    Cache_Shutdown();

    /* Print summary */
    serial_printf("\n");
    serial_printf("=====================================\n");
    serial_printf("  Test Results\n");
    serial_printf("=====================================\n");
    serial_printf("  Tests passed: %d\n", tests_passed);
    serial_printf("  Tests failed: %d\n", tests_failed);
    serial_printf("  Total tests:  %d\n", tests_passed + tests_failed);
    serial_printf("=====================================\n");

    if (tests_failed == 0) {
        serial_printf("  ✓ ALL TESTS PASSED!\n");
    } else {
        serial_printf("  ✗ SOME TESTS FAILED\n");
    }
    serial_printf("=====================================\n\n");
}
