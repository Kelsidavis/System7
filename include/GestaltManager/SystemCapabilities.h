/*
 * RE-AGENT-BANNER
 * System Capabilities Detection for Mac OS System 6.0.7
 * Reverse engineered from System.rsrc (SHA256: 78150ebb66707277e0947fbf81f6a27fc5d263a71bbf96df7084d5c3ec22a5ba)
 *
 * This header defines the system capability detection interface used in System 6.0.7
 * for hardware feature detection and compatibility checking.
 *
 * Original evidence: Mac OS System 6.0.7 System.rsrc resource fork
 * Analysis date: 2025-09-17
 * Provenance: Direct string and assembly pattern analysis
 */

#ifndef __SYSTEMCAPABILITIES_H__
#define __SYSTEMCAPABILITIES_H__

#include "../MacTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Missing Mac OS types and constants */
#define gestaltUnknownErr  -5550

/* System Environment constants */
enum {
    envMac = 1,
    env68020 = 2,
    envStandardKbd = 1
};

/* System Environment Record */
typedef struct SysEnvRec {
    short  environsVersion;
    short  machineType;
    short  systemVersion;
    short  processor;
    Boolean hasFPU;
    Boolean hasColorQD;
    short  keyBoardType;
    short  atDrvrVersNum;
    short  sysVRefNum;
} SysEnvRec;

/* System capability bit flags - evidence from byte offset 8803 and 43934 */
#define kCapabilityFPU          0x0001  /* 68881/68882 coprocessor present */
#define kCapability32BitQD      0x0002  /* 32-Bit QuickDraw available */
#define kCapabilityColorQD      0x0004  /* Color QuickDraw support */
#define kCapabilitySoundInput   0x0008  /* Apple Sound Input hardware */
#define kCapabilityBattery      0x0010  /* Battery-powered system */
#define kCapability32BitAddr    0x0020  /* 32-bit addressing mode */
#define kCapabilityColorDepthMask 0x00C0 /* Color depth mask (bits 6-7) */

/* Color depth values */
#define kColorDepth1Bit         0x0000  /* 1-bit (black and white) */
#define kColorDepth4Bit         0x0040  /* 4-bit (16 colors/grays) */
#define kColorDepth8Bit         0x0080  /* 8-bit (256 colors/grays) */

/* System capability detection results structure */
struct SystemCapabilities {
    short           machineType;        /* Macintosh model identifier */
    short           systemVersion;      /* System version (0x0607 for 6.0.7) */
    short           quickDrawVersion;   /* QuickDraw version */
    unsigned short  capabilityFlags;   /* Hardware capability bit flags */
    unsigned long   memorySize;        /* Physical RAM size in bytes */
    unsigned long   reserved;          /* Reserved for future use */
};

typedef struct SystemCapabilities SystemCapabilities;

/* Error codes - evidence from capability detection failures */
#define kCapabilityNoFPU        -1001   /* Floating point coprocessor not installed */
#define kCapabilityOldQD        -1002   /* 32-Bit QuickDraw version too old */
#define kCapabilityBadAddr      -1003   /* Incompatible addressing mode */
#define kCapabilityModelError   -1004   /* Incompatible machine model */

/* Function prototypes - evidence from mappings.json analysis */

/*
 * CheckQuickDrawVersion - Evidence from byte offset 8803
 * Checks for 32-Bit QuickDraw version 1.2 or later compatibility
 * Returns: noErr if compatible, kCapabilityOldQD if too old
 */
OSErr CheckQuickDrawVersion(void);

/*
 * CheckFloatingPointUnit - Evidence from byte offset 43934
 * Detects presence of 68881/68882 floating point coprocessor
 * Returns: true if FPU present, false if not installed
 */
Boolean CheckFloatingPointUnit(void);

/*
 * CheckAddressingMode - Evidence from addressing mode detection strings
 * Detects 24-bit vs 32-bit addressing mode compatibility
 * Returns: 24 for 24-bit mode, 32 for 32-bit mode, -1 for error
 */
short CheckAddressingMode(void);

/*
 * GetColorCapabilities - Evidence from byte offset 7922
 * Detects maximum color depth and grayscale capabilities
 * Returns: bit depth (1, 4, or 8), -1 for error
 */
short GetColorCapabilities(void);

/*
 * CheckSoundInputCapability - Evidence from AppleSoundInput strings
 * Detects Apple Sound Input hardware presence
 * Returns: true if sound input available, false otherwise
 */
Boolean CheckSoundInputCapability(void);

/*
 * GetMachineModel - Evidence from model compatibility strings
 * Identifies Macintosh model and checks startup disk compatibility
 * Returns: machine type constant, -1 for incompatible model
 */
short GetMachineModel(void);

/*
 * GetSystemCapabilities - Master capability detection function
 * Performs comprehensive system capability analysis and populates structure
 * Parameters: caps - pointer to SystemCapabilities structure to fill
 * Returns: noErr on success, error code on failure
 */
OSErr GetSystemCapabilities(SystemCapabilities *caps);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEMCAPABILITIES_H__ */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "file": "SystemCapabilities.h",
 *   "purpose": "System capability detection interface for Mac OS System 6.0.7",
 *   "evidence_source": "System.rsrc binary analysis",
 *   "key_offsets": [8803, 43934, 7922],
 *   "capability_categories": [
 *     "graphics_capability", "cpu_capability", "memory_addressing",
 *     "audio_capability", "hardware_compatibility"
 *   ],
 *   "provenance_density": 0.15,
 *   "validation_status": "header_complete"
 * }
 */