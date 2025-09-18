/*
 * DeviceTypes.h
 * System 7.1 Device Manager - Core Types and Structures
 *
 * Portable C implementation of the Mac OS System 7.1 Device Manager
 * data structures and constants.
 */

#ifndef DEVICE_TYPES_H
#define DEVICE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "ResourceManager.h"  /* For Handle type */

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Forward Declarations
 * ============================================================================= */

typedef struct DeviceControlEntry DeviceControlEntry;
typedef struct DeviceControlEntry *DCEPtr;
typedef struct DeviceControlEntry **DCEHandle;

typedef struct AuxiliaryDCE AuxiliaryDCE;
typedef struct AuxiliaryDCE *AuxDCEPtr;
typedef struct AuxiliaryDCE **AuxDCEHandle;

typedef struct DriverHeader DriverHeader;
typedef struct DriverHeader *DriverHeaderPtr;
typedef struct DriverHeader **DriverHeaderHandle;

typedef struct IOParam IOParam;
typedef struct IOParam *IOParamPtr;

typedef struct CntrlParam CntrlParam;
typedef struct CntrlParam *CntrlParamPtr;

/* =============================================================================
 * Device Manager Constants
 * ============================================================================= */

/* Device Manager result codes */
#define noErr              0      /* No error */
#define ioInProgress       1      /* I/O operation in progress */
#define abortErr           -27    /* I/O operation aborted */
#define notOpenErr         -28    /* Driver not open */
#define unitEmptyErr       -29    /* No driver in unit table */
#define badUnitErr         -21    /* Driver reference number out of range */
#define openErr            -23    /* Requested read/write permission not granted */
#define controlErr         -17    /* Driver does not respond to Control calls */
#define statusErr          -18    /* Driver does not respond to Status calls */
#define readErr            -19    /* Driver does not respond to Read calls */
#define writErr            -20    /* Driver does not respond to Write calls */
#define badReqErr          -22    /* Invalid I/O request */
#define dInstErr           -26    /* DrvrInstall couldn't find driver in resources */
#define dRemovErr          -25    /* DrvrRemove couldn't find driver to remove */

/* Trap word constants */
#define aRdCmd             2      /* Low byte of ioTrap for Read calls */
#define aWrCmd             3      /* Low byte of ioTrap for Write calls */

/* Trap word modifier bits */
#define asyncTrpBit        10     /* Asynchronous trap bit */
#define noQueueBit         9      /* No queue bit */

/* Special control codes */
#define killCode           1      /* Control code to kill pending I/O */
#define goodBye            -1     /* Special goodbye control code */
#define accRun             65     /* Accessory run control code */

/* Maximum unit table size */
#define MAX_UNIT_TABLE_SIZE 256

/* =============================================================================
 * DCE Flags - Driver Control Entry Status Flags
 * ============================================================================= */

/* High byte flags (driver capabilities) */
#define dReadEnable        0      /* Driver can respond to Read calls */
#define dWritEnable        1      /* Driver can respond to Write calls */
#define dCtlEnable         2      /* Driver can respond to Control calls */
#define dStatEnable        3      /* Driver can respond to Status calls */
#define dNeedGoodBye       4      /* Driver needs goodbye call */
#define dNeedTime          5      /* Driver needs time for periodic action */
#define dNeedLock          6      /* Driver should be locked in memory */

/* Low byte flags (DCE status) */
#define dOpened            5      /* Driver is open */
#define dRAMBased          6      /* Driver is resource-based (in RAM) */
#define drvrActive         7      /* Driver has pending I/O operations */

/* New extended flags (word-based) */
#define Is_AppleTalk       0      /* AppleTalk driver */
#define Is_Agent           1      /* Agent driver */
#define FollowsNewRules    2      /* Follows new driver rules */
#define Is_Open            5      /* Driver is open */
#define Is_Ram_Based       6      /* Driver is RAM-based */
#define Is_Active          7      /* Driver is active */
#define Read_Enable        8      /* Read capability */
#define Write_Enable       9      /* Write capability */
#define Control_Enable     10     /* Control capability */
#define Status_Enable      11     /* Status capability */
#define Needs_Goodbye      12     /* Needs goodbye call */
#define Needs_Time         13     /* Needs periodic time */
#define Needs_Lock         14     /* Needs to stay locked */

/* Flag masks */
#define Is_AppleTalk_Mask      (1 << Is_AppleTalk)
#define Is_Agent_Mask          (1 << Is_Agent)
#define FollowsNewRules_Mask   (1 << FollowsNewRules)
#define Is_Open_Mask           (1 << Is_Open)
#define Is_Ram_Based_Mask      (1 << Is_Ram_Based)
#define Is_Active_Mask         (1 << Is_Active)
#define Read_Enable_Mask       (1 << Read_Enable)
#define Write_Enable_Mask      (1 << Write_Enable)
#define Control_Enable_Mask    (1 << Control_Enable)
#define Status_Enable_Mask     (1 << Status_Enable)
#define Needs_Goodbye_Mask     (1 << Needs_Goodbye)
#define Needs_Time_Mask        (1 << Needs_Time)
#define Needs_Lock_Mask        (1 << Needs_Lock)

/* =============================================================================
 * Queue Header Structure
 * ============================================================================= */

typedef struct QueueHeader {
    int16_t         qFlags;        /* Queue flags */
    void           *qHead;         /* Pointer to first queue element */
    void           *qTail;         /* Pointer to last queue element */
} QueueHeader, *QueueHeaderPtr;

typedef struct QueueElement {
    struct QueueElement *qLink;    /* Link to next queue element */
    int16_t              qType;    /* Queue element type */
    int16_t              qData[1]; /* Queue element data */
} QueueElement, *QueueElementPtr;

/* =============================================================================
 * Parameter Block Header
 * ============================================================================= */

typedef struct ParamBlockHeader {
    void           *qLink;         /* Queue link in header */
    int16_t         qType;         /* Type byte for safety check */
    int16_t         ioTrap;        /* The trap word */
    void           *ioCmdAddr;     /* Address to dispatch to */
    void           *ioCompletion;  /* Completion routine address */
    int16_t         ioResult;      /* Result code */
    void           *ioNamePtr;     /* Pointer to name string */
    int16_t         ioVRefNum;     /* Volume reference number */
} ParamBlockHeader;

/* =============================================================================
 * Device Control Entry (DCE) Structure
 * ============================================================================= */

struct DeviceControlEntry {
    void           *dCtlDriver;    /* Pointer to driver */
    int16_t         dCtlFlags;     /* Driver flags */
    QueueHeader     dCtlQHdr;      /* Driver's I/O queue header */
    int32_t         dCtlPosition;  /* Current file position */
    Handle          dCtlStorage;   /* Handle to driver's storage */
    int16_t         dCtlRefNum;    /* Driver reference number */
    int32_t         dCtlCurTicks;  /* Current tick count */
    void           *dCtlWindow;    /* Window pointer (for desk accessories) */
    int16_t         dCtlDelay;     /* Tick delay for periodic action */
    int16_t         dCtlEMask;     /* Event mask */
    int16_t         dCtlMenu;      /* Menu ID */
};

/* =============================================================================
 * Auxiliary DCE Structure (Extended for Slot Manager)
 * ============================================================================= */

struct AuxiliaryDCE {
    void           *dCtlDriver;    /* Pointer to driver */
    int16_t         dCtlFlags;     /* Driver flags */
    QueueHeader     dCtlQHdr;      /* Driver's I/O queue header */
    int32_t         dCtlPosition;  /* Current file position */
    Handle          dCtlStorage;   /* Handle to driver's storage */
    int16_t         dCtlRefNum;    /* Driver reference number */
    int32_t         dCtlCurTicks;  /* Current tick count */
    void           *dCtlWindow;    /* Window pointer */
    int16_t         dCtlDelay;     /* Tick delay for periodic action */
    int16_t         dCtlEMask;     /* Event mask */
    int16_t         dCtlMenu;      /* Menu ID */
    int8_t          dCtlSlot;      /* Slot number */
    int8_t          dCtlSlotId;    /* Slot ID */
    int32_t         dCtlDevBase;   /* Device base address */
    void           *dCtlOwner;     /* Owner pointer */
    int8_t          dCtlExtDev;    /* External device */
    int8_t          fillByte;      /* Padding byte */
};

/* =============================================================================
 * Driver Header Structure
 * ============================================================================= */

struct DriverHeader {
    int16_t         drvrFlags;     /* Driver capability flags */
    int16_t         drvrDelay;     /* Tick delay for periodic action */
    int16_t         drvrEMask;     /* Event mask */
    int16_t         drvrMenu;      /* Menu ID */
    int16_t         drvrOpen;      /* Offset to Open routine */
    int16_t         drvrPrime;     /* Offset to Prime routine */
    int16_t         drvrCtl;       /* Offset to Control routine */
    int16_t         drvrStatus;    /* Offset to Status routine */
    int16_t         drvrClose;     /* Offset to Close routine */
    uint8_t         drvrName[256]; /* Driver name (Pascal string) */
};

/* =============================================================================
 * I/O Parameter Block Structure
 * ============================================================================= */

struct IOParam {
    ParamBlockHeader pb;           /* Parameter block header */
    int16_t         ioRefNum;      /* Driver reference number */
    int8_t          ioVersNum;     /* Version number */
    int8_t          ioPermssn;     /* Permission */
    void           *ioMisc;        /* Miscellaneous pointer */
    void           *ioBuffer;      /* I/O buffer pointer */
    int32_t         ioReqCount;    /* Requested byte count */
    int32_t         ioActCount;    /* Actual byte count */
    int16_t         ioPosMode;     /* Positioning mode */
    int32_t         ioPosOffset;   /* Position offset */
};

/* =============================================================================
 * Control Parameter Block Structure
 * ============================================================================= */

struct CntrlParam {
    ParamBlockHeader pb;           /* Parameter block header */
    int16_t         ioCRefNum;     /* Driver reference number */
    int16_t         csCode;        /* Control/status code */
    int16_t         csParam[11];   /* Control/status parameters */
};

/* =============================================================================
 * Driver Function Pointer Types
 * ============================================================================= */

/* Driver entry point function type */
typedef int16_t (*DriverEntryProc)(IOParamPtr pb, DCEPtr dce);

/* Completion routine function type */
typedef void (*CompletionProc)(IOParamPtr pb);

/* =============================================================================
 * Unit Table Entry
 * ============================================================================= */

typedef struct UnitTableEntry {
    DCEHandle       dceHandle;     /* Handle to DCE */
    bool            inUse;         /* True if entry is in use */
} UnitTableEntry;

/* =============================================================================
 * Driver Gestalt Support
 * ============================================================================= */

#define driverGestaltCode          43
#define driverGestaltSync          'sync'  /* Driver synchronous behavior */
#define driverGestaltPrefetch      'prft'  /* Prefetch characteristics */
#define driverGestaltBoot          'boot'  /* Boot drive value */

typedef struct DriverGestaltSyncResponse {
    bool            behavesSynchronously;
} DriverGestaltSyncResponse;

typedef struct DriverGestaltBootResponse {
    uint8_t         extDev;        /* External device (target+LUN) */
    uint8_t         partition;     /* Partition (unused) */
    uint8_t         SIMSlot;       /* Slot number */
    uint8_t         SIMsRSRC;      /* sRsrcID */
} DriverGestaltBootResponse;

typedef union DriverGestaltInfo {
    DriverGestaltSyncResponse sync;
    DriverGestaltBootResponse boot;
} DriverGestaltInfo;

typedef struct DriverGestaltParam {
    ParamBlockHeader pb;           /* Parameter block header */
    int16_t         ioCRefNum;     /* Reference number */
    int16_t         csCode;        /* Control code (driverGestaltCode) */
    uint32_t        driverGestaltSelector;   /* Gestalt selector */
    DriverGestaltInfo *driverGestaltResponse; /* Response buffer */
} DriverGestaltParam;

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_TYPES_H */