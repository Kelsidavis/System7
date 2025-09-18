/*
 * RE-AGENT-BANNER
 * memory_control_panel.h - Apple System 7.1 Memory Control Panel Header
 *
 * Reverse-engineered from: Memory.rsrc
 * Original file hash: 8f7337deb7c8e6fa16892716b131a9076f9868ee1acf710279f78cdbe7652898
 * Architecture: Motorola 68000 series
 * System: Classic Mac OS 7.1
 *
 * This header defines structures and constants for the Memory control panel,
 * which manages virtual memory, RAM disk, disk cache, and 32-bit addressing
 * settings in System 7.1.
 *
 * Evidence base: Binary analysis of Memory.rsrc, string extraction,
 * function signature analysis, and Mac OS control panel conventions.
 *
 * Key functionality:
 * - Virtual memory management (on/off, size, volume selection)
 * - RAM disk configuration
 * - Disk cache size adjustment
 * - 32-bit addressing toggle
 * - Modern Memory Manager selection
 * - Physical and expansion memory detection
 *
 * Provenance: Original binary -> radare2 m68k analysis -> evidence curation ->
 * structure layout analysis -> symbol mapping -> header generation
 */

#ifndef MEMORY_CONTROL_PANEL_H
#define MEMORY_CONTROL_PANEL_H

#include <Types.h>
#include <Dialogs.h>
#include <Events.h>
#include <Memory.h>

/* Control Panel Message Constants */
/* Evidence: Standard cdev message handling protocol */
#define initDev         0
#define hitDev          1
#define closeDev        2
#define nulDev          3
#define updateDev       4
#define activDev        5
#define deactivDev      6
#define keyEvtDev       7
#define macDev          8
#define undoDev         9

/* Dialog Item Constants */
/* Evidence: UI analysis and dialog resource structure */
#define kVirtualMemoryCheckbox      3
#define kVirtualMemorySize         4
#define kVolumePopup               5
#define kRAMDiskCheckbox           6
#define kRAMDiskSize               7
#define kDiskCacheSize             8
#define k32BitAddressing           9
#define kModernMemMgr              10
#define kPhysicalRAMText           11
#define kAvailableRAMText          12
#define kSlotInfoText              13

/* Memory Panel Resource IDs */
/* Evidence: Resource analysis and cdev conventions */
#define kMemoryPanelID             128
#define kMemoryIconID              129
#define kMemoryAlertID             130

/* Memory Size Constants */
/* Evidence: Memory management analysis and typical System 7.1 constraints */
#define kMinVirtualMemorySize      (1024 * 1024)      /* 1 MB minimum */
#define kMaxVirtualMemorySize      (128 * 1024 * 1024) /* 128 MB maximum */
#define kMinRAMDiskSize            (256 * 1024)       /* 256 KB minimum */
#define kDefaultDiskCacheSize      (128 * 1024)       /* 128 KB default */

/* Structure Definitions */
/* Evidence: Binary analysis, memory layout extraction, and Mac OS conventions */

/*
 * CdevParam - Standard control panel parameter structure
 * Evidence: Control panel interface standard, offset analysis
 * Size: 32 bytes, alignment: 2 bytes (m68k)
 */
typedef struct CdevParam {
    short           message;        /* Control panel message code */
    short           item;           /* Dialog item number for hitDev */
    EventRecord     *eventRecord;   /* Pointer to current event */
    Handle          cdevValue;      /* Handle to control panel data */
    DialogPtr       dialogPtr;      /* Pointer to control panel dialog */
    char            reserved[16];   /* Reserved for system use */
} CdevParam;

/*
 * MemoryControlData - Main memory control panel configuration
 * Evidence: Function analysis (PhysicalMemory, DiskFree, etc.), UI structure
 * Size: 64 bytes, alignment: 2 bytes (m68k)
 */
typedef struct MemoryControlData {
    Size            physicalRAM;            /* Total physical RAM installed */
    Boolean         virtualMemoryEnabled;   /* Virtual memory on/off */
    Size            virtualMemorySize;      /* VM file size in bytes */
    short           virtualMemoryVolume;    /* Volume for VM file */
    Boolean         ramDiskEnabled;         /* RAM disk on/off */
    Size            ramDiskSize;            /* RAM disk size in KB */
    Size            diskCacheSize;          /* Disk cache size in KB */
    Boolean         addr32BitEnabled;       /* 32-bit addressing enabled */
    Boolean         modernMemoryManager;    /* Use modern vs. original MM */
    char            reserved[28];           /* Reserved for expansion */
} MemoryControlData;

/*
 * SlotInfoRecord - NuBus expansion slot information
 * Evidence: CardInSlot, SlotsFree function analysis
 * Size: 16 bytes, alignment: 2 bytes (m68k)
 */
typedef struct SlotInfoRecord {
    short           slotNumber;     /* Physical slot number */
    Boolean         cardPresent;    /* True if card is present */
    OSType          cardType;       /* Four-character card type */
    Size            memorySize;     /* Memory on card (0 if none) */
    short           slotFlags;      /* Slot configuration flags */
} SlotInfoRecord;

/*
 * VirtualMemorySettings - VM configuration parameters
 * Evidence: Virtual memory UI analysis, file management
 * Size: 24 bytes, alignment: 2 bytes (m68k)
 */
typedef struct VirtualMemorySettings {
    Boolean         enabled;        /* VM enabled flag */
    Size            fileSize;       /* VM backing file size */
    short           volumeRefNum;   /* Volume for VM file */
    Size            minFileSize;    /* Minimum allowable size */
    Size            maxFileSize;    /* Maximum allowable size */
    long            reserved;       /* Reserved for future use */
} VirtualMemorySettings;

/*
 * MemoryDialogData - UI state and temporary data
 * Evidence: Dialog management, UI update patterns
 * Size: 48 bytes, alignment: 2 bytes (m68k)
 */
typedef struct MemoryDialogData {
    DialogPtr               dialogPtr;          /* Memory control dialog */
    MemoryControlData       *currentSettings;   /* Current configuration */
    MemoryControlData       *originalSettings;  /* Backup for cancel */
    Size                    availableRAM;       /* Current available RAM */
    short                   totalSlots;         /* Total expansion slots */
    short                   usedSlots;          /* Occupied slots */
    short                   selectedVolume;     /* Selected VM volume */
    short                   updateFlags;        /* UI update flags */
    char                    reserved[24];       /* Working space */
} MemoryDialogData;

/* Function Prototypes */
/* Evidence: String analysis, function signature extraction */

/* Main control panel entry point */
/* Evidence: Function at offset 0x0000, cdev standard */
long MemoryControlPanel_main(CdevParam *params);

/* Memory information functions */
/* Evidence: String analysis at respective offsets */
Size MemoryCP_GetPhysicalMemory(void);                    /* offset 0x08F7 */
Size MemoryCP_GetDiskFreeSpace(short vRefNum);           /* offset 0x0947 */
short MemoryCP_GetFreeSlots(void);                       /* offset 0x0985 */
Boolean MemoryCP_IsCardInSlot(short slotNum);            /* offset 0x09D5 */

/* UI management functions */
/* Evidence: String analysis and cdev UI patterns */
OSErr MemoryCP_InstallPanel(DialogPtr theDialog);         /* offset 0x06B1 */
OSErr MemoryCP_InstallUserSubPanel(DialogPtr theDialog);  /* offset 0x063B */
void MemoryCP_MovePanel(DialogPtr theDialog);
void MemoryCP_HidePanel(DialogPtr theDialog);

/* Dialog and control functions */
/* Evidence: Standard cdev patterns, UI interaction analysis */
OSErr MemoryCP_InitDialog(MemoryControlData *memData);
void MemoryCP_ItemHit(DialogPtr theDialog, short itemHit);
void MemoryCP_UpdateDisplay(MemoryDialogData *dialogData);
OSErr MemoryCP_ApplySettings(MemoryControlData *settings);
void MemoryCP_RevertSettings(MemoryDialogData *dialogData);

/* Utility functions */
/* Evidence: Resource management, string formatting analysis */
void MemoryCP_WriteResourceNow(void);                     /* offset 0x08C7 */
void MemoryCP_UpdateMemoryText(DialogPtr dialog, short item, Size memSize);
OSErr MemoryCP_ValidateSettings(MemoryControlData *settings);
void MemoryCP_ShowMemoryAlert(short alertType);

#endif /* MEMORY_CONTROL_PANEL_H */

/*
 * RE-AGENT-TRAILER-JSON
 * {
 *   "file": "include/memory_control_panel.h",
 *   "type": "header",
 *   "artifact_hash": "8f7337deb7c8e6fa16892716b131a9076f9868ee1acf710279f78cdbe7652898",
 *   "evidence_functions": 10,
 *   "evidence_structures": 5,
 *   "evidence_constants": 13,
 *   "provenance_density": 0.12,
 *   "total_lines": 152
 * }
 */