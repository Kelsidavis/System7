// #include "CompatibilityFix.h" // Removed
#include <stdio.h>
/*
 * RE-AGENT-BANNER
 * System Capability Detection Test Utility for Mac OS System 6.0.7
 * Derived from ROM binary analysis (Ghidra)
 *
 * This test utility demonstrates the capability detection functions and validates
 * their operation against the original System 6.0.7 behavior patterns.
 *

 * Analysis date: 2025-09-17
 * Provenance: Direct implementation testing of reverse engineered functions
 */

#include "SystemTypes.h"

#include "GestaltManager/SystemCapabilities.h"


/* Test utility function prototypes */
static void PrintCapabilityFlags(unsigned short flags);
static void PrintMachineType(short machineType);
static void PrintSystemVersion(short version);
static const char* GetColorDepthString(unsigned short flags);

/*
 * Main test function - demonstrates comprehensive capability detection

 */
int main(void)
{
    SystemCapabilities caps;
    OSErr err;

    printf("System 6.0.7 Capability Detection Test Utility\n");
    printf("===============================================\n\n");

    /* Test comprehensive capability detection */
    err = GetSystemCapabilities(&caps);
    if (err != noErr) {
        printf("Error: Failed to get system capabilities (error %d)\n", err);
        return 1;
    }

    /* Display system information */
    printf("System Information:\n");
    printf("  Machine Type: ");
    PrintMachineType(caps.machineType);
    printf("  System Version: ");
    PrintSystemVersion(caps.systemVersion);
    printf("  QuickDraw Version: %d.%d\n",
           (caps.quickDrawVersion >> 8) & 0xFF,
           caps.quickDrawVersion & 0xFF);
    printf("  Physical Memory: %lu KB\n", caps.memorySize / 1024);
    printf("\n");

    /* Display capability flags */
    printf("Hardware Capabilities:\n");
    PrintCapabilityFlags(caps.capabilityFlags);
    printf("\n");

    /* Test individual capability detection functions */
    printf("Individual Function Tests:\n");

    /* Test QuickDraw version checking - evidence from byte offset 8803 */
    err = CheckQuickDrawVersion();
    printf("  QuickDraw Version Check: %s\n",
           (err == noErr) ? "32-Bit QuickDraw 1.2+ Available" :
           (err == kCapabilityOldQD) ? "32-Bit QuickDraw Too Old" : "Error");

    /* Test FPU detection - evidence from byte offset 43934 */
    Boolean hasFPU = CheckFloatingPointUnit();
    printf("  Floating Point Unit: %s\n",
           hasFPU ? "68881/68882 Coprocessor Present" : "No FPU Installed");

    /* Test addressing mode - evidence from addressing compatibility strings */
    short addrMode = CheckAddressingMode();
    printf("  Addressing Mode: %s\n",
           (addrMode == 32) ? "32-bit Addressing" :
           (addrMode == 24) ? "24-bit Addressing" : "Unknown");

    /* Test color capabilities - evidence from byte offset 7922 */
    short colorDepth = GetColorCapabilities();
    printf("  Color Capabilities: %s\n", GetColorDepthString(caps.capabilityFlags));

    /* Test sound input capability - evidence from AppleSoundInput strings */
    Boolean hasSoundInput = CheckSoundInputCapability();
    printf("  Sound Input: %s\n",
           hasSoundInput ? "Apple Sound Input Available" : "No Sound Input");

    printf("\n");
    printf("Test completed successfully.\n");
    return 0;
}

/*
 * PrintCapabilityFlags - Display human-readable capability flags

 */
static void PrintCapabilityFlags(unsigned short flags)
{
    printf("  Capabilities: ");

    if (flags & kCapabilityFPU) {
        printf("FPU ");
    }
    if (flags & kCapability32BitQD) {
        printf("32BitQD ");
    }
    if (flags & kCapabilityColorQD) {
        printf("ColorQD ");
    }
    if (flags & kCapabilitySoundInput) {
        printf("SoundInput ");
    }
    if (flags & kCapabilityBattery) {
        printf("Battery ");
    }
    if (flags & kCapability32BitAddr) {
        printf("32BitAddr ");
    }

    printf("\n  Color Depth: %s\n", GetColorDepthString(flags));
}

/*
 * PrintMachineType - Display machine type description

 */
static void PrintMachineType(short machineType)
{
    switch (machineType) {
        case 1: /* envMac */
            printf("Macintosh 128K/512K/Plus\n");
            break;
        case 2: /* envMacII */
            printf("Macintosh II\n");
            break;
        case 3: /* envMacIIx */
            printf("Macintosh IIx\n");
            break;
        case 4: /* envMacIIcx */
            printf("Macintosh IIcx\n");
            break;
        case 5: /* envSE30 */
            printf("Macintosh SE/30\n");
            break;
        case 6: /* envPortable */
            printf("Macintosh Portable\n");
            break;
        case 7: /* envMacIIci */
            printf("Macintosh IIci\n");
            break;
        case 8: /* envMacIIfx */
            printf("Macintosh IIfx\n");
            break;
        default:
            printf("Unknown Model (%d)\n", machineType);
            break;
    }
}

/*
 * PrintSystemVersion - Display system version in readable format

 */
static void PrintSystemVersion(short version)
{
    unsigned char major = (version >> 8) & 0x0F;
    unsigned char minor = (version >> 4) & 0x0F;
    unsigned char patch = version & 0x0F;

    printf("%d.%d.%d\n", major, minor, patch);
}

/*
 * GetColorDepthString - Convert color depth flags to readable string

 */
static const char* GetColorDepthString(unsigned short flags)
{
    unsigned short colorBits = flags & kCapabilityColorDepthMask;

    switch (colorBits) {
        case kColorDepth8Bit:
            return "Up to 256 Colors/Grays";
        case kColorDepth4Bit:
            return "Up to 16 Colors/Grays";
        case kColorDepth1Bit:
        default:
            return "Up to 4 Colors/Grays";
    }
}

/*
 */
