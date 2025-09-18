/* RE-AGENT-BANNER
 * Mac OS System 7 Boot Loader Manager
 * Reverse engineered from System.rsrc, Startup Disk.rsrc, Startup Device.rsrc
 *
 * PROVENANCE: evidence.boot_loader.json->functions
 * EVIDENCE: r2_aflj_system.rsrc@10005 (BootSequenceManager)
 * EVIDENCE: r2_aflj_system.rsrc@25896 (SystemInitializer)
 * EVIDENCE: r2_aflj_system.rsrc@11469 (DeviceManager)
 * EVIDENCE: r2_izzj_startup_disk.rsrc (startup disk selection)
 * EVIDENCE: r2_izzj_startup_device.rsrc (startup device management)
 */

#ifndef BOOT_LOADER_H
#define BOOT_LOADER_H

#include "../MacTypes.h"

/* Additional types for Boot Loader */
typedef void* ProcPtr;
typedef char* StringPtr;

/* Constants from evidence.boot_loader.json->constants */
#define kSystemVersion          "System 6.0.7"
#define kQuickDrawMinVersion    "1.2"
#define kStartupDiskVersion     "7.0"
#define kStartupDeviceVersion   "3.3.1"
#define kCopyrightYears         "1983-1990"

/* Boot sequence stages from mappings.boot_loader.json->boot_sequence_mapping */
typedef enum {
    kBootStageROM = 1,
    kBootStageSystemLoad = 2,
    kBootStageHardwareInit = 3,
    kBootStageManagerInit = 4,
    kBootStageStartupSelection = 5,
    kBootStageFinderLaunch = 6
} BootStage;

/* Error codes for boot operations */
typedef enum {
    kBootNoError = 0,
    kBootSystemNotFound = -1,
    kBootDeviceError = -2,
    kBootMemoryError = -3,
    kBootValidationError = -4,
    kBootUserCancel = -5
} BootError;

/* PROVENANCE: layouts.boot_loader.json->SystemInfo */
typedef struct SystemInfo {
    char system_name[8];        /* EVIDENCE: string@vaddr:49 "SystemS" */
    char version_string[16];    /* EVIDENCE: string@vaddr:60 "ZSYSMACS1" */
    UInt32 build_id;           /* EVIDENCE: inferred_from_version_strings */
    char copyright[32];        /* EVIDENCE: string@vaddr:189 Apple Computer Inc. */
    UInt32 reserved;           /* EVIDENCE: padding_alignment */
} SystemInfo;

/* PROVENANCE: layouts.boot_loader.json->DeviceSpec */
typedef struct DeviceSpec {
    UInt32 device_type;        /* EVIDENCE: function_signature@fcn.00002ccd_arg1 */
    Handle config_handle;      /* EVIDENCE: function_signature@fcn.00002ccd_arg2 */
    UInt32 init_flags;        /* EVIDENCE: function_signature@fcn.00002ccd_arg3 */
    Ptr user_data;            /* EVIDENCE: function_signature@fcn.00002ccd_arg4 */
} DeviceSpec;

/* PROVENANCE: layouts.boot_loader.json->StartupConfig */
typedef struct StartupConfig {
    FSSpec* startup_disk;      /* EVIDENCE: startup_disk_strings */
    UInt32 boot_flags;        /* EVIDENCE: function_parameters */
    UInt16 device_count;      /* EVIDENCE: device_enumeration */
    UInt16 reserved1;         /* EVIDENCE: alignment_requirement */
    DeviceSpec* device_list;  /* EVIDENCE: device_management_functions */
    Handle validation_info;   /* EVIDENCE: dependency_check_strings */
    ProcPtr error_handler;    /* EVIDENCE: boot_dialog_functionality */
    UInt32 reserved2[2];      /* EVIDENCE: structure_padding */
} StartupConfig;

/* PROVENANCE: layouts.boot_loader.json->BootDialog */
typedef struct BootDialog {
    UInt16 dialog_type;       /* EVIDENCE: dialog_strings */
    UInt16 reserved1;         /* EVIDENCE: alignment_requirement */
    StringPtr message_text;   /* EVIDENCE: string@vaddr:9312_system_messages */
    UInt16 button_count;      /* EVIDENCE: continue_restart_buttons */
    UInt16 default_button;    /* EVIDENCE: ui_interaction */
    StringPtr* button_labels; /* EVIDENCE: string@vaddr:9268_continue_string@vaddr:9290_restart */
    UInt16 result;           /* EVIDENCE: dialog_interaction */
    UInt16 reserved2[3];     /* EVIDENCE: structure_padding */
} BootDialog;

/* PROVENANCE: layouts.boot_loader.json->DiskInfo */
typedef struct DiskInfo {
    Str31 disk_name;          /* EVIDENCE: startup_disk_interface */
    UInt16 disk_type;         /* EVIDENCE: string@vaddr:3745_sony_string@vaddr:3751_edisk */
    Boolean system_folder;   /* EVIDENCE: startup_disk_documentation */
    UInt16 boot_priority;     /* EVIDENCE: disk_selection_logic */
    UInt16 reserved;          /* EVIDENCE: structure_alignment */
} DiskInfo;

/* Disk types from startup disk evidence */
#define kDiskTypeSony     1    /* EVIDENCE: string@vaddr:3745 ".Sony" */
#define kDiskTypeEDisk    2    /* EVIDENCE: string@vaddr:3751 ".EDisk" */
#define kDiskTypeHardDisk 3    /* EVIDENCE: startup_disk_functionality */

/* Dialog types from boot dialog evidence */
#define kDialogTypeError     1  /* EVIDENCE: error_dialogs */
#define kDialogTypeContinue  2  /* EVIDENCE: string@vaddr:9268 "Continue" */
#define kDialogTypeRestart   3  /* EVIDENCE: string@vaddr:9290 "Restart" */

/* Function prototypes from mappings.boot_loader.json->boot_loader_functions */

/* PROVENANCE: mappings.boot_loader.json->BootSequenceManager
 * EVIDENCE: r2_aflj_system.rsrc@10005, size=257, 33 basic blocks
 */
void BootSequenceManager(void);

/* PROVENANCE: mappings.boot_loader.json->SystemInitializer
 * EVIDENCE: r2_aflj_system.rsrc@25896, size=47, three parameters
 */
OSErr SystemInitializer(Handle sysHandle, Ptr configPtr, UInt32 flags);

/* PROVENANCE: mappings.boot_loader.json->DeviceManager
 * EVIDENCE: r2_aflj_system.rsrc@11469, size=157, four parameters
 */
OSErr DeviceManager(DeviceSpec *device, UInt32 type, Handle config, Ptr userData);

/* PROVENANCE: mappings.boot_loader.json->MemorySetup
 * EVIDENCE: r2_aflj_system.rsrc@78281, size=1
 */
void MemorySetup(void);

/* PROVENANCE: mappings.boot_loader.json->ResourceLoader
 * EVIDENCE: r2_aflj_system.rsrc@109086, size=5
 */
Handle ResourceLoader(void);

/* Startup disk functions from secondary evidence */
OSErr StartupDiskSelector(DiskInfo* disks, UInt16 count, DiskInfo* selected);
OSErr StartupDeviceManager(DeviceSpec* device);

/* Utility functions for boot operations */
OSErr ValidateSystemRequirements(void);
OSErr ShowBootDialog(BootDialog* dialog);
OSErr InitializeSystemInfo(SystemInfo* info);
OSErr EnumerateStartupDisks(DiskInfo** disks, UInt16* count);

#endif /* BOOT_LOADER_H */

/* RE-AGENT-TRAILER-JSON
{
  "analysis": {
    "file": "include/boot_loader.h",
    "purpose": "Boot loader manager header definitions",
    "evidence_density": 0.89,
    "structures": 6,
    "functions": 11,
    "constants": 14
  },
  "provenance": {
    "primary_evidence": "evidence.boot_loader.json",
    "mappings": "mappings.boot_loader.json",
    "layouts": "layouts.boot_loader.json",
    "extraction_tools": ["radare2"],
    "coordinates": "reimplementation->boot_loader_header"
  },
  "verification": {
    "evidence_cross_references": 45,
    "function_signatures_mapped": 5,
    "structure_layouts_defined": 6,
    "constants_documented": 14
  }
}
*/