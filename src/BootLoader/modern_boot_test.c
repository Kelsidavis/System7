/*
 * System 7.1 Boot Loader - Modern Boot Test
 *
 * Comprehensive test suite for the modernized System 7.1 boot loader
 * on x86_64 and ARM64 targets. Tests HAL, storage, and boot sequence.
 */

#include "hal/boot_hal.h"
#include "storage/modern_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Forward declaration */
OSErr ModernBootSequenceManager(void);
Boolean IsModernBootInitialized(void);
OSErr GetModernPlatformInfo(PlatformInfo* info);
OSErr GetModernSystemInfo(SystemInfo* info);
OSErr ModernShutdownSequence(void);

/* ============================================================================
 * Test Framework
 * ============================================================================ */

typedef struct TestResults {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
} TestResults;

static TestResults g_test_results = {0, 0, 0, 0};

#define TEST_ASSERT(condition, test_name) do { \
    g_test_results.total_tests++; \
    if (condition) { \
        printf("✓ PASS: %s\n", test_name); \
        g_test_results.passed_tests++; \
    } else { \
        printf("✗ FAIL: %s\n", test_name); \
        g_test_results.failed_tests++; \
    } \
} while(0)

#define TEST_SKIP(test_name, reason) do { \
    g_test_results.total_tests++; \
    g_test_results.skipped_tests++; \
    printf("⊘ SKIP: %s (%s)\n", test_name, reason); \
} while(0)

/* ============================================================================
 * Platform Detection Tests
 * ============================================================================ */

static void test_platform_detection(void) {
    printf("\n=== Testing Platform Detection ===\n");

    PlatformType platform;
    BootMode boot_mode;
    OSErr err = HAL_DetectPlatform(&platform, &boot_mode);
    TEST_ASSERT(err == noErr, "Platform detection succeeds");

    TEST_ASSERT(platform == kPlatformX86_64 || platform == kPlatformARM64,
                "Platform is supported (x86_64 or ARM64)");

    const char* platform_name = (platform == kPlatformX86_64) ? "x86_64" :
                               (platform == kPlatformARM64) ? "ARM64" : "Unknown";
    printf("  Detected platform: %s\n", platform_name);

    const char* boot_mode_name = (boot_mode == kBootModeUEFI) ? "UEFI" :
                                (boot_mode == kBootModeBIOS) ? "BIOS" :
                                (boot_mode == kBootModeLinux) ? "Linux" : "Unknown";
    printf("  Boot mode: %s\n", boot_mode_name);
}

/* ============================================================================
 * HAL Tests
 * ============================================================================ */

static void test_hal_initialization(void) {
    printf("\n=== Testing HAL Initialization ===\n");

    OSErr err = HAL_Initialize();
    TEST_ASSERT(err == noErr, "HAL initialization succeeds");

    BootServices* services = NULL;
    err = HAL_GetBootServices(&services);
    TEST_ASSERT(err == noErr, "Boot services retrieval succeeds");
    TEST_ASSERT(services != NULL, "Boot services pointer is valid");

    if (services) {
        TEST_ASSERT(services->GetPlatformInfo != NULL, "GetPlatformInfo function available");
        TEST_ASSERT(services->AllocateMemory != NULL, "AllocateMemory function available");
        TEST_ASSERT(services->FreeMemory != NULL, "FreeMemory function available");
        TEST_ASSERT(services->EnumerateStorageDevices != NULL, "EnumerateStorageDevices function available");
    }
}

static void test_platform_info(void) {
    printf("\n=== Testing Platform Information ===\n");

    BootServices* services = NULL;
    OSErr err = HAL_GetBootServices(&services);
    if (err != noErr || !services) {
        TEST_SKIP("Platform info tests", "HAL not initialized");
        return;
    }

    PlatformInfo info = {0};
    err = services->GetPlatformInfo(&info);
    TEST_ASSERT(err == noErr, "Platform info retrieval succeeds");

    TEST_ASSERT(info.platform != kPlatformUnknown, "Platform type is known");
    TEST_ASSERT(info.cpu_count > 0, "CPU count is positive");
    TEST_ASSERT(info.memory_size > 0, "Memory size is positive");
    TEST_ASSERT(info.page_size > 0, "Page size is positive");
    TEST_ASSERT(strlen(info.vendor) > 0, "Vendor string is not empty");

    printf("  Platform: %s %s\n", info.vendor, info.model);
    printf("  CPUs: %u cores\n", info.cpu_count);
    printf("  Memory: %llu MB\n", info.memory_size / (1024 * 1024));
    printf("  Page size: %u bytes\n", info.page_size);
    printf("  Features: UEFI=%s, ACPI=%s, DeviceTree=%s\n",
           info.has_uefi ? "Yes" : "No",
           info.has_acpi ? "Yes" : "No",
           info.has_devicetree ? "Yes" : "No");
}

static void test_memory_management(void) {
    printf("\n=== Testing Memory Management ===\n");

    BootServices* services = NULL;
    OSErr err = HAL_GetBootServices(&services);
    if (err != noErr || !services) {
        TEST_SKIP("Memory management tests", "HAL not initialized");
        return;
    }

    /* Test memory allocation */
    void* ptr1 = NULL;
    err = services->AllocateMemory(1024, kMemoryFlagZero, &ptr1);
    TEST_ASSERT(err == noErr, "Memory allocation succeeds");
    TEST_ASSERT(ptr1 != NULL, "Allocated pointer is valid");

    /* Test memory with different flags */
    void* ptr2 = NULL;
    err = services->AllocateMemory(2048, kMemoryFlagZero, &ptr2);
    TEST_ASSERT(err == noErr, "Flagged memory allocation succeeds");
    TEST_ASSERT(ptr2 != NULL, "Flagged allocated pointer is valid");

    /* Test memory deallocation */
    err = services->FreeMemory(ptr1);
    TEST_ASSERT(err == noErr, "Memory deallocation succeeds");

    err = services->FreeMemory(ptr2);
    TEST_ASSERT(err == noErr, "Flagged memory deallocation succeeds");

    /* Test null pointer handling */
    err = services->FreeMemory(NULL);
    TEST_ASSERT(err == paramErr, "NULL pointer free returns parameter error");

    /* Test memory info */
    MemoryInfo mem_info = {0};
    err = services->GetMemoryInfo(&mem_info);
    TEST_ASSERT(err == noErr, "Memory info retrieval succeeds");
    TEST_ASSERT(mem_info.total_memory > 0, "Total memory is positive");
    TEST_ASSERT(mem_info.page_size > 0, "Memory page size is positive");

    printf("  Memory info: %llu MB total, %llu MB available\n",
           mem_info.total_memory / (1024 * 1024),
           mem_info.available_memory / (1024 * 1024));
}

/* ============================================================================
 * Storage Tests
 * ============================================================================ */

static void test_storage_enumeration(void) {
    printf("\n=== Testing Storage Enumeration ===\n");

    BootServices* services = NULL;
    OSErr err = HAL_GetBootServices(&services);
    if (err != noErr || !services) {
        TEST_SKIP("Storage enumeration tests", "HAL not initialized");
        return;
    }

    ModernDeviceInfo* devices = NULL;
    UInt32 count = 0;
    err = services->EnumerateStorageDevices(&devices, &count);
    TEST_ASSERT(err == noErr, "Storage device enumeration succeeds");
    TEST_ASSERT(devices != NULL || count == 0, "Device array is valid or count is zero");

    if (count > 0 && devices) {
        printf("  Found %u storage devices:\n", count);
        for (UInt32 i = 0; i < count; i++) {
            ModernDeviceInfo* device = &devices[i];
            const char* type_name = (device->type == kModernDeviceNVMe) ? "NVMe" :
                                   (device->type == kModernDeviceSATA) ? "SATA" :
                                   (device->type == kModernDeviceUSB) ? "USB" :
                                   (device->type == kModernDeviceRAM) ? "RAM" : "Unknown";

            printf("    %u: %s (%s) - %llu GB, bootable=%s\n",
                   i, device->label, type_name,
                   device->size_bytes / (1024 * 1024 * 1024),
                   device->is_bootable ? "Yes" : "No");

            TEST_ASSERT(device->type != kModernDeviceUnknown, "Device type is known");
            TEST_ASSERT(strlen(device->label) > 0, "Device label is not empty");
            TEST_ASSERT(device->size_bytes > 0, "Device size is positive");
        }

        /* Clean up */
        free(devices);
    } else {
        TEST_SKIP("Storage device validation", "No storage devices found");
    }
}

/* ============================================================================
 * Boot Sequence Tests
 * ============================================================================ */

static void test_modern_boot_sequence(void) {
    printf("\n=== Testing Modern Boot Sequence ===\n");

    /* Test boot sequence execution */
    OSErr err = ModernBootSequenceManager();
    TEST_ASSERT(err == noErr, "Modern boot sequence completes successfully");

    /* Test boot status */
    Boolean is_initialized = IsModernBootInitialized();
    TEST_ASSERT(is_initialized == true, "Boot initialization status is correct");

    /* Test platform info retrieval after boot */
    PlatformInfo platform_info = {0};
    err = GetModernPlatformInfo(&platform_info);
    TEST_ASSERT(err == noErr, "Platform info retrieval after boot succeeds");
    TEST_ASSERT(platform_info.platform != kPlatformUnknown, "Platform info is valid");

    /* Test system info retrieval after boot */
    SystemInfo system_info = {0};
    err = GetModernSystemInfo(&system_info);
    TEST_ASSERT(err == noErr, "System info retrieval after boot succeeds");
    TEST_ASSERT(strcmp(system_info.system_name, "SystemS") == 0, "System name is correct");
    TEST_ASSERT(strcmp(system_info.version_string, "ZSYSMACS1") == 0, "Version string is correct");
    TEST_ASSERT(system_info.build_id == 0x07010000, "Build ID is correct");

    printf("  System: %s %s (Build 0x%08X)\n",
           system_info.system_name, system_info.version_string, system_info.build_id);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

static void test_error_handling(void) {
    printf("\n=== Testing Error Handling ===\n");

    BootServices* services = NULL;
    OSErr err = HAL_GetBootServices(&services);
    if (err != noErr || !services) {
        TEST_SKIP("Error handling tests", "HAL not initialized");
        return;
    }

    /* Test null parameter handling */
    err = services->GetPlatformInfo(NULL);
    TEST_ASSERT(err == paramErr, "NULL platform info pointer returns parameter error");

    err = services->AllocateMemory(0, 0, NULL);
    TEST_ASSERT(err == paramErr, "NULL allocation pointer returns parameter error");

    err = services->EnumerateStorageDevices(NULL, NULL);
    TEST_ASSERT(err == paramErr, "NULL storage enumeration parameters return parameter error");

    /* Test invalid size allocation */
    void* ptr = NULL;
    err = services->AllocateMemory(0, 0, &ptr);
    TEST_ASSERT(err == paramErr, "Zero size allocation returns parameter error");
}

/* ============================================================================
 * Platform-Specific Tests
 * ============================================================================ */

static void test_platform_specific_features(void) {
    printf("\n=== Testing Platform-Specific Features ===\n");

    PlatformInfo info = {0};
    OSErr err = GetModernPlatformInfo(&info);
    if (err != noErr) {
        TEST_SKIP("Platform-specific tests", "Platform info not available");
        return;
    }

    switch (info.platform) {
        case kPlatformX86_64:
            printf("  Testing x86_64 specific features...\n");
            TEST_ASSERT(info.has_acpi || info.has_uefi, "x86_64 has ACPI or UEFI support");

            /* Test x86_64 specific functions if available */
            #ifdef PLATFORM_X86_64
            /* Add x86_64 specific tests here */
            #endif
            break;

        case kPlatformARM64:
            printf("  Testing ARM64 specific features...\n");
            /* ARM64 systems typically have device tree */
            if (!info.has_devicetree && !info.has_acpi) {
                printf("    Warning: No device tree or ACPI detected\n");
            }

            /* Test ARM64 specific functions if available */
            #ifdef PLATFORM_ARM64
            /* Add ARM64 specific tests here */
            #endif
            break;

        default:
            TEST_SKIP("Platform-specific tests", "Unknown platform");
            break;
    }
}

/* ============================================================================
 * Cleanup Tests
 * ============================================================================ */

static void test_shutdown_sequence(void) {
    printf("\n=== Testing Shutdown Sequence ===\n");

    OSErr err = ModernShutdownSequence();
    TEST_ASSERT(err == noErr, "Shutdown sequence completes successfully");

    Boolean is_initialized = IsModernBootInitialized();
    TEST_ASSERT(is_initialized == false, "Boot initialization status cleared after shutdown");
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

static void run_diagnostics(void) {
    printf("\n=== Running Diagnostics ===\n");

    OSErr err = HAL_RunDiagnostics();
    if (err == noErr) {
        printf("  HAL diagnostics: PASS\n");
    } else {
        printf("  HAL diagnostics: FAIL (%d)\n", err);
    }

    /* Platform-specific diagnostics */
    PlatformInfo info = {0};
    err = GetModernPlatformInfo(&info);
    if (err == noErr) {
        switch (info.platform) {
            case kPlatformX86_64:
                #ifdef PLATFORM_X86_64
                /* Run x86_64 diagnostics if available */
                #endif
                break;

            case kPlatformARM64:
                #ifdef PLATFORM_ARM64
                /* Run ARM64 diagnostics if available */
                #endif
                break;

            default:
                break;
        }
    }
}

/* ============================================================================
 * Main Test Function
 * ============================================================================ */

int main(int argc, char* argv[]) {
    printf("System 7.1 Modern Boot Loader Test Suite\n");
    printf("========================================\n");

    /* Check for specific test flags */
    Boolean run_platform_test = false;
    Boolean run_verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--platform-test") == 0) {
            run_platform_test = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            run_verbose = true;
        }
    }

    if (run_verbose) {
        HAL_SetLogLevel(kLogLevelDebug);
    }

    /* Run test suites */
    test_platform_detection();
    test_hal_initialization();
    test_platform_info();
    test_memory_management();
    test_storage_enumeration();
    test_modern_boot_sequence();
    test_error_handling();

    if (run_platform_test) {
        test_platform_specific_features();
    }

    run_diagnostics();
    test_shutdown_sequence();

    /* Print test summary */
    printf("\n=== TEST SUMMARY ===\n");
    printf("Total Tests: %d\n", g_test_results.total_tests);
    printf("Passed: %d\n", g_test_results.passed_tests);
    printf("Failed: %d\n", g_test_results.failed_tests);
    printf("Skipped: %d\n", g_test_results.skipped_tests);

    if (g_test_results.total_tests > 0) {
        float success_rate = (float)g_test_results.passed_tests / g_test_results.total_tests * 100.0f;
        printf("Success Rate: %.1f%%\n", success_rate);
    }

    if (g_test_results.failed_tests == 0) {
        printf("\n🎉 ALL TESTS PASSED! Modern boot loader is working correctly.\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed. Please review the implementation.\n");
        return 1;
    }
}