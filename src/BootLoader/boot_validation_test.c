/*
 * Boot Loader Validation Test
 * Comprehensive validation of System 7.1 Boot Sequence Manager
 */

#include "../../include/BootLoader/boot_loader.h"
#include <stdio.h>
#include <assert.h>

/* Test Results */
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} TestResults;

static TestResults results = {0, 0, 0};

#define TEST_ASSERT(condition, test_name) do { \
    results.total_tests++; \
    if (condition) { \
        printf("✓ PASS: %s\n", test_name); \
        results.passed_tests++; \
    } else { \
        printf("✗ FAIL: %s\n", test_name); \
        results.failed_tests++; \
    } \
} while(0)

/* Test Boot Sequence Stages */
static void test_boot_sequence_stages(void) {
    printf("\n=== Testing Boot Sequence Stages ===\n");

    /* Test that all boot stages are defined */
    TEST_ASSERT(kBootStageROM == 1, "Boot Stage ROM has correct value");
    TEST_ASSERT(kBootStageSystemLoad == 2, "Boot Stage System Load has correct value");
    TEST_ASSERT(kBootStageHardwareInit == 3, "Boot Stage Hardware Init has correct value");
    TEST_ASSERT(kBootStageManagerInit == 4, "Boot Stage Manager Init has correct value");
    TEST_ASSERT(kBootStageStartupSelection == 5, "Boot Stage Startup Selection has correct value");
    TEST_ASSERT(kBootStageFinderLaunch == 6, "Boot Stage Finder Launch has correct value");

    printf("Boot sequence stages validation completed.\n");
}

/* Test Device Management */
static void test_device_management(void) {
    printf("\n=== Testing Device Management ===\n");

    /* Test Sony drive device */
    DeviceSpec sonyDevice = {0};
    OSErr err = DeviceManager(&sonyDevice, kDiskTypeSony, NULL, NULL);
    TEST_ASSERT(err == noErr, "Sony device manager initialization");
    TEST_ASSERT(sonyDevice.device_type == kDiskTypeSony, "Sony device type set correctly");
    TEST_ASSERT(sonyDevice.init_flags & 0x01, "Sony device initialization flag set");

    /* Test EDisk device */
    DeviceSpec ediskDevice = {0};
    err = DeviceManager(&ediskDevice, kDiskTypeEDisk, NULL, NULL);
    TEST_ASSERT(err == noErr, "EDisk device manager initialization");
    TEST_ASSERT(ediskDevice.device_type == kDiskTypeEDisk, "EDisk device type set correctly");
    TEST_ASSERT(ediskDevice.init_flags & 0x01, "EDisk device initialization flag set");

    /* Test Hard Disk device */
    DeviceSpec hardDiskDevice = {0};
    err = DeviceManager(&hardDiskDevice, kDiskTypeHardDisk, NULL, NULL);
    TEST_ASSERT(err == noErr, "Hard Disk device manager initialization");
    TEST_ASSERT(hardDiskDevice.device_type == kDiskTypeHardDisk, "Hard Disk device type set correctly");
    TEST_ASSERT(hardDiskDevice.init_flags & 0x01, "Hard Disk device initialization flag set");

    /* Test invalid device type */
    DeviceSpec invalidDevice = {0};
    err = DeviceManager(&invalidDevice, 999, NULL, NULL);
    TEST_ASSERT(err == paramErr, "Invalid device type returns error");

    printf("Device management validation completed.\n");
}

/* Test System Initialization */
static void test_system_initialization(void) {
    printf("\n=== Testing System Initialization ===\n");

    SystemInfo sysInfo = {0};
    OSErr err = InitializeSystemInfo(&sysInfo);
    TEST_ASSERT(err == noErr, "System initialization successful");
    TEST_ASSERT(strncmp(sysInfo.system_name, "SystemS", 7) == 0, "System name set correctly");
    TEST_ASSERT(strncmp(sysInfo.version_string, "ZSYSMACS1", 9) == 0, "Version string set correctly");
    TEST_ASSERT(sysInfo.build_id == 0x07010000, "Build ID set correctly");

    /* Test null parameter */
    err = InitializeSystemInfo(NULL);
    TEST_ASSERT(err == paramErr, "Null parameter returns error");

    printf("System initialization validation completed.\n");
}

/* Test Startup Disk Management */
static void test_startup_disk_management(void) {
    printf("\n=== Testing Startup Disk Management ===\n");

    DiskInfo* disks = NULL;
    UInt16 count = 0;

    /* Test disk enumeration */
    OSErr err = EnumerateStartupDisks(&disks, &count);
    TEST_ASSERT(err == noErr, "Startup disk enumeration successful");
    TEST_ASSERT(count == 2, "Expected number of startup disks found");
    TEST_ASSERT(disks != NULL, "Disk array allocated");

    if (disks && count >= 2) {
        /* Validate first disk (Hard Disk) */
        TEST_ASSERT(disks[0].disk_type == kDiskTypeHardDisk, "First disk is hard disk");
        TEST_ASSERT(disks[0].system_folder == true, "First disk has system folder");
        TEST_ASSERT(disks[0].boot_priority == 1, "First disk has priority 1");

        /* Validate second disk (Floppy) */
        TEST_ASSERT(disks[1].disk_type == kDiskTypeSony, "Second disk is Sony floppy");
        TEST_ASSERT(disks[1].system_folder == false, "Second disk has no system folder");
        TEST_ASSERT(disks[1].boot_priority == 2, "Second disk has priority 2");

        /* Test disk selection */
        DiskInfo selected = {0};
        err = StartupDiskSelector(disks, count, &selected);
        TEST_ASSERT(err == noErr, "Startup disk selection successful");
        TEST_ASSERT(selected.disk_type == kDiskTypeHardDisk, "Hard disk selected (highest priority with system)");
        TEST_ASSERT(selected.boot_priority == 1, "Selected disk has correct priority");
    }

    /* Test null parameters */
    err = EnumerateStartupDisks(NULL, &count);
    TEST_ASSERT(err == paramErr, "Null disks parameter returns error");

    err = EnumerateStartupDisks(&disks, NULL);
    TEST_ASSERT(err == paramErr, "Null count parameter returns error");

    /* Clean up */
    if (disks) {
        free(disks);
    }

    printf("Startup disk management validation completed.\n");
}

/* Test Error Handling */
static void test_error_handling(void) {
    printf("\n=== Testing Error Handling ===\n");

    /* Test null parameters for various functions */
    OSErr err = DeviceManager(NULL, kDiskTypeSony, NULL, NULL);
    TEST_ASSERT(err == paramErr, "DeviceManager null device parameter");

    BootDialog* nullDialog = NULL;
    err = ShowBootDialog(nullDialog);
    TEST_ASSERT(err == paramErr, "ShowBootDialog null dialog parameter");

    DiskInfo* nullDisks = NULL;
    err = StartupDiskSelector(nullDisks, 0, NULL);
    TEST_ASSERT(err == paramErr, "StartupDiskSelector null parameters");

    printf("Error handling validation completed.\n");
}

/* Test System Requirements Validation */
static void test_system_requirements(void) {
    printf("\n=== Testing System Requirements Validation ===\n");

    OSErr err = ValidateSystemRequirements();
    TEST_ASSERT(err == noErr, "System requirements validation successful");

    printf("System requirements validation completed.\n");
}

/* Main validation test */
int main(void) {
    printf("System 7.1 Boot Loader Validation Test\n");
    printf("======================================\n");

    test_boot_sequence_stages();
    test_device_management();
    test_system_initialization();
    test_startup_disk_management();
    test_error_handling();
    test_system_requirements();

    printf("\n=== VALIDATION SUMMARY ===\n");
    printf("Total Tests: %d\n", results.total_tests);
    printf("Passed: %d\n", results.passed_tests);
    printf("Failed: %d\n", results.failed_tests);
    printf("Success Rate: %.1f%%\n",
           results.total_tests > 0 ? (float)results.passed_tests / results.total_tests * 100.0 : 0.0);

    if (results.failed_tests == 0) {
        printf("\n🎉 ALL TESTS PASSED! Boot Loader integration is successful.\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed. Please review the implementation.\n");
        return 1;
    }
}