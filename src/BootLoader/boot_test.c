/*
 * Boot Loader Test Program
 * Tests the System 7.1 Boot Sequence Manager implementation
 */

#include "../../include/BootLoader/boot_loader.h"
#include <stdio.h>

int main(void) {
    printf("System 7.1 Boot Loader Manager Test\n");
    printf("====================================\n\n");

    printf("Testing boot sequence manager...\n");
    BootSequenceManager();

    printf("\nTesting system initializer...\n");
    Handle testHandle = (Handle)0x12345678;  // Dummy handle
    Ptr testPtr = (Ptr)0x87654321;           // Dummy pointer
    UInt32 testFlags = 0x07;                 // Test all flags

    OSErr err = SystemInitializer(testHandle, testPtr, testFlags);
    printf("SystemInitializer returned: %d\n", err);

    printf("\nTesting device manager...\n");
    DeviceSpec testDevice = {0};
    err = DeviceManager(&testDevice, kDiskTypeSony, NULL, NULL);
    printf("DeviceManager returned: %d\n", err);
    printf("Device type set to: %u\n", testDevice.device_type);
    printf("Init flags: 0x%x\n", testDevice.init_flags);

    printf("\nTesting memory setup...\n");
    MemorySetup();

    printf("\nTesting resource loader...\n");
    Handle resourceHandle = ResourceLoader();
    if (resourceHandle) {
        printf("Resource loading successful\n");
    } else {
        printf("Resource loading failed\n");
    }

    printf("\nTesting system requirements validation...\n");
    err = ValidateSystemRequirements();
    printf("ValidateSystemRequirements returned: %d\n", err);

    printf("\nBoot loader test completed successfully!\n");
    return 0;
}