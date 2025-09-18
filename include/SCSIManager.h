#ifndef __SCSIMANAGER_H__
#define __SCSIMANAGER_H__

/*
 * SCSIManager.h
 *
 * Portable implementation of Mac OS System 7.1 SCSI Manager
 * Based on Apple's SCSI Manager 4.3 with CAM (Common Access Method) API
 *
 * Provides complete SCSI device management including:
 * - SCSI command execution (6, 10, and 12-byte commands)
 * - Device enumeration and identification
 * - Bus management and arbitration
 * - Synchronous and asynchronous I/O operations
 * - Error handling and retry logic
 * - Hardware abstraction for modern storage interfaces
 */

#include "MacTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SCSI Manager Version */
#define SCSI_VERSION 43

/* Maximum values */
#define MAX_CDB_LENGTH          16      /* Maximum Command Descriptor Block length */
#define MAX_SCSI_BUSES          8       /* Maximum number of SCSI buses */
#define MAX_SCSI_TARGETS        8       /* Maximum targets per bus */
#define MAX_SCSI_LUNS           8       /* Maximum LUNs per target */
#define VENDOR_ID_LENGTH        16      /* ASCII string length for Vendor ID */
#define HANDSHAKE_DATA_LENGTH   8       /* Handshake data length */

/* SCSI Manager Function Codes */
typedef enum {
    /* Common Functions */
    SCSINop                   = 0x00,   /* Execute nothing */
    SCSIExecIO               = 0x01,   /* Execute the specified IO */
    SCSIBusInquiry           = 0x03,   /* Get parameters for entire path */
    SCSIReleaseQ             = 0x04,   /* Release the frozen queue for LUN */
    SCSISetAsyncCallback     = 0x05,   /* Set async event callback */

    /* Control Functions */
    SCSIAbortCommand         = 0x10,   /* Abort the selected command */
    SCSIResetBus             = 0x11,   /* Reset the SCSI bus */
    SCSIResetDevice          = 0x12,   /* Reset the SCSI device */
    SCSITerminateIO          = 0x13,   /* Terminate any pending IO */

    /* Target Mode Functions */
    SCSIEnableLUN            = 0x30,   /* Enable LUN, Target mode support */
    SCSITargetIO             = 0x31,   /* Execute the target IO request */

    /* Apple Extensions */
    SCSIGetVirtualIDInfo     = 0x80,   /* Find out which bus old ID is on */
    SCSIGenerateInterleaveID = 0x81,   /* Generate a new interleave factor */
    SCSILoadDriver           = 0x82,   /* Load a driver for a device */
    SCSIOldCall              = 0x84,   /* XPT->SIM private call for old-API */
    SCSICreateRefNumXref     = 0x85,   /* Register DeviceIdent to driver RefNum */
    SCSILookupRefNumXref     = 0x86,   /* Get DeviceIdent to driver RefNum */
    SCSIRemoveRefNumXref     = 0x87,   /* Remove DeviceIdent to driver RefNum */
    SCSIRegisterWithNewXPT   = 0x88    /* XPT changed - SIM re-register */
} SCSIFunctionCode;

/* SCSI Device Identifier */
typedef struct {
    uint8_t reserved;       /* Reserved */
    uint8_t bus;           /* SCSI Bus number */
    uint8_t targetID;      /* SCSI Target ID */
    uint8_t LUN;           /* Logical Unit Number */
} DeviceIdent;

/* Command Descriptor Block */
typedef union {
    uint8_t *cdbPtr;                    /* Pointer to CDB bytes */
    uint8_t cdbBytes[MAX_CDB_LENGTH];   /* Actual CDB to send */
} CDB;

/* Scatter/Gather Record */
typedef struct {
    void *SGAddr;           /* Scatter/gather address */
    uint32_t SGCount;       /* Scatter/gather count */
} SGRecord;

/* SCSI Parameter Block Header */
typedef struct SCSIHdr {
    struct SCSIHdr *qLink;          /* Queue link to next PB */
    int16_t scsiReserved1;          /* Reserved for input */
    uint16_t scsiPBLength;          /* Length of the entire PB */
    uint8_t scsiFunctionCode;       /* Function selector */
    uint8_t scsiReserved2;          /* Reserved for output */
    OSErr scsiResult;               /* Returned result */
    DeviceIdent scsiDevice;         /* Device Identifier */
    void (*scsiCompletion)(void *); /* Callback completion function */
    uint32_t scsiFlags;             /* Assorted flags */
    uint8_t *scsiDriverStorage;     /* Pointer for driver private use */
    void *scsiXPTprivate;          /* Private field for XPT */
    int32_t scsiReserved3;         /* Reserved */
} SCSIHdr;

/* SCSI I/O Parameter Block */
typedef struct SCSI_IO {
    /* Header */
    struct SCSI_IO *qLink;          /* Queue link */
    int16_t scsiReserved1;          /* Reserved */
    uint16_t scsiPBLength;          /* PB length */
    uint8_t scsiFunctionCode;       /* Function code */
    uint8_t scsiReserved2;          /* Reserved */
    OSErr scsiResult;               /* Result */
    DeviceIdent scsiDevice;         /* Device identifier */
    void (*scsiCompletion)(void *); /* Completion routine */
    uint32_t scsiFlags;             /* Flags */
    uint8_t *scsiDriverStorage;     /* Driver storage */
    void *scsiXPTprivate;          /* XPT private */
    int32_t scsiReserved3;         /* Reserved */

    /* I/O specific fields */
    uint16_t scsiResultFlags;       /* Flags which modify scsiResult */
    uint16_t scsiInterleaveID;      /* Interleave designation */
    uint8_t *scsiDataPtr;          /* Data buffer or S/G list pointer */
    uint32_t scsiDataLength;        /* Data transfer length */
    uint8_t *scsiSensePtr;         /* Autosense data buffer pointer */
    uint8_t scsiSenseLength;        /* Autosense buffer size */
    uint8_t scsiCDBLength;          /* CDB length */
    uint16_t scsiSGListCount;       /* Scatter/gather list entries */
    uint32_t scsiReserved4;         /* Reserved */
    uint8_t scsiSCSIstatus;         /* Returned SCSI device status */
    int8_t scsiSenseResidual;       /* Autosense residual length */
    uint16_t scsiReserved5;         /* Reserved */
    int32_t scsiDataResidual;       /* Transfer residual length */
    CDB scsiCDB;                    /* Command Descriptor Block */
    int32_t scsiTimeout;            /* Timeout value */
    uint8_t *scsiMessagePtr;        /* Message buffer pointer */
    uint16_t scsiMessageLen;        /* Message buffer length */
    uint16_t scsiIOFlags;           /* Additional I/O flags */
    uint8_t scsiTagAction;          /* Tag queuing action */
    uint8_t scsiReserved6;          /* Reserved */
    uint16_t scsiReserved7;         /* Reserved */
    uint16_t scsiSelectTimeout;     /* Select timeout value */
    uint8_t scsiDataType;           /* Data description type */
    uint8_t scsiTransferType;       /* Transfer type */
    uint32_t scsiReserved8;         /* Reserved */
    uint32_t scsiReserved9;         /* Reserved */
    uint16_t scsiHandshake[HANDSHAKE_DATA_LENGTH]; /* Handshaking points */
    uint32_t scsiReserved10;        /* Reserved */
    uint32_t scsiReserved11;        /* Reserved */
    struct SCSI_IO *scsiCommandLink; /* Linked command chain pointer */

    /* Extension areas */
    uint8_t scsiSIMpublics[8];      /* Reserved for 3rd-party SIMs */
    uint8_t scsiAppleReserved6[8];  /* Apple reserved */

    /* XPT layer privates */
    uint16_t scsiCurrentPhase;      /* Phase upon completing old call */
    int16_t scsiSelector;           /* Selector for old calls */
    OSErr scsiOldCallResult;        /* Result of old call */
    uint8_t scsiSCSImessage;        /* Returned SCSI message */
    uint8_t XPTprivateFlags;        /* Various flags */
    uint8_t XPTextras[12];          /* Extra space */
} SCSI_IO;

/* Bus Inquiry Parameter Block */
typedef struct {
    /* Header */
    struct SCSIHdr *qLink;          /* Queue link */
    int16_t scsiReserved1;          /* Reserved */
    uint16_t scsiPBLength;          /* PB length */
    uint8_t scsiFunctionCode;       /* Function code */
    uint8_t scsiReserved2;          /* Reserved */
    OSErr scsiResult;               /* Result */
    DeviceIdent scsiDevice;         /* Device identifier */
    void (*scsiCompletion)(void *); /* Completion routine */
    uint32_t scsiFlags;             /* Flags */
    uint8_t *scsiDriverStorage;     /* Driver storage */
    void *scsiXPTprivate;          /* XPT private */
    int32_t scsiReserved3;         /* Reserved */

    /* Bus inquiry specific */
    uint16_t scsiEngineCount;       /* Number of engines on HBA */
    uint16_t scsiMaxTransferType;   /* Number of transfer types */
    uint32_t scsiDataTypes;         /* Supported data types */
    uint16_t scsiIOpbSize;          /* SCSI_IO PB size for this SIM/HBA */
    uint16_t scsiMaxIOpbSize;       /* Max SCSI_IO PB size for all */
    uint32_t scsiFeatureFlags;      /* Supported features */
    uint8_t scsiVersionNumber;      /* SIM/HBA version number */
    uint8_t scsiHBAInquiry;         /* HBA inquiry byte 7 mimic */
    uint8_t scsiTargetModeFlags;    /* Target mode support flags */
    uint8_t scsiScanFlags;          /* Scan related flags */
    uint32_t scsiSIMPrivatesPtr;    /* SIM private data area pointer */
    uint32_t scsiSIMPrivatesSize;   /* SIM private data area size */
    uint32_t scsiAsyncFlags;        /* Async callback event capability */
    uint8_t scsiHiBusID;            /* Highest path ID in subsystem */
    uint8_t scsiInitiatorID;        /* HBA ID on SCSI bus */
    uint16_t scsiBIReserved0;       /* Reserved */
    uint32_t scsiBIReserved1;       /* Reserved */
    uint32_t scsiFlagsSupported;    /* Supported scsiFlags */
    uint16_t scsiIOFlagsSupported;  /* Supported scsiIOFlags */
    uint16_t scsiWeirdStuff;        /* Weird stuff flags */
    uint16_t scsiMaxTarget;         /* Maximum target number */
    uint16_t scsiMaxLUN;            /* Maximum LUN number */
    char scsiSIMVendor[VENDOR_ID_LENGTH];      /* SIM vendor ID */
    char scsiHBAVendor[VENDOR_ID_LENGTH];      /* HBA vendor ID */
    char scsiControllerFamily[VENDOR_ID_LENGTH]; /* Controller family */
    char scsiControllerType[VENDOR_ID_LENGTH];   /* Controller type */
    char scsiXPTversion[4];         /* XPT version */
    char scsiSIMversion[4];         /* SIM version */
    char scsiHBAversion[4];         /* HBA version */
    uint8_t scsiHBAslotType;        /* HBA slot type */
    uint8_t scsiHBAslotNumber;      /* HBA slot number */
    uint16_t scsiSIMsRsrcID;        /* SIM resource ID */
    uint16_t scsiBIReserved3;       /* Reserved */
    uint16_t scsiAdditionalLength;  /* Additional BusInquiry PB length */
} SCSIBusInquiryPB;

/* Abort Command Parameter Block */
typedef struct {
    SCSIHdr header;                 /* Standard header */
    SCSI_IO *scsiIOptr;            /* Pointer to PB to abort */
} SCSIAbortCommandPB;

/* Terminate I/O Parameter Block */
typedef struct {
    SCSIHdr header;                 /* Standard header */
    SCSI_IO *scsiIOptr;            /* Pointer to PB to terminate */
} SCSITerminateIOPB;

/* Reset Bus Parameter Block */
typedef struct {
    SCSIHdr header;                 /* Standard header */
} SCSIResetBusPB;

/* Reset Device Parameter Block */
typedef struct {
    SCSIHdr header;                 /* Standard header */
} SCSIResetDevicePB;

/* SCSI Flags */
#define scsiDirectionMask      0x80000000L  /* Data direction mask */
#define scsiDirectionOut       0x80000000L  /* Data out (write) */
#define scsiDirectionIn        0x00000000L  /* Data in (read) */
#define scsiDirectionNone      0x40000000L  /* No data transfer */

#define scsiSIMQHead           0x20000000L  /* Queue at head */
#define scsiSIMQFreeze         0x10000000L  /* Freeze queue */
#define scsiSIMQNoFreeze       0x08000000L  /* Don't freeze on error */
#define scsiDoDisconnect       0x04000000L  /* Enable disconnect */
#define scsiDontDisconnect     0x02000000L  /* Disable disconnect */
#define scsiInitiateWide       0x01000000L  /* Initiate wide negotiation */

/* Result Flags */
#define scsiSIMQFrozen         0x0001       /* SIM queue is frozen */
#define scsiAutosenseValid     0x0002       /* Autosense data valid */
#define scsiBusNotFree         0x0004       /* Bus not free */

/* I/O Flags */
#define scsiNoParityCheck      0x0001       /* Disable parity checking */
#define scsiDisableSelectWAtn  0x0002       /* Disable select with ATN */
#define scsiSavePtrOnDisconnect 0x0004      /* Save pointers on disconnect */
#define scsiNoBucketIn         0x0008       /* No bit bucket in */
#define scsiNoBucketOut        0x0010       /* No bit bucket out */
#define scsiDisableWide        0x0020       /* Disable wide negotiation */
#define scsiInitiateSync       0x0040       /* Initiate sync negotiation */
#define scsiDisableSync        0x0080       /* Disable sync negotiation */

/* Tag Action Values */
#define scsiSimpleQTag         0x20         /* Simple queue tag */
#define scsiHeadQTag           0x21         /* Head of queue tag */
#define scsiOrderedQTag        0x22         /* Ordered queue tag */

/* Data Types */
#define scsiDataBuffer         0x00         /* Regular data buffer */
#define scsiDataTIB            0x01         /* Transfer Information Block */
#define scsiDataSG             0x02         /* Scatter/Gather list */

/* Transfer Types */
#define scsiTransferPolled     0x00         /* Polled transfer */
#define scsiTransferBlind      0x01         /* Blind transfer */
#define scsiTransferDMA        0x02         /* DMA transfer */

/* Error Codes */
#define scsiErrorBase          -7936
#define scsiRequestInProgress  1            /* Request in progress */
#define scsiRequestAborted     (scsiErrorBase + 2)
#define scsiUnableToAbort      (scsiErrorBase + 3)
#define scsiNonZeroStatus      (scsiErrorBase + 4)
#define scsiUnused05           (scsiErrorBase + 5)
#define scsiUnused06           (scsiErrorBase + 6)
#define scsiUnused07           (scsiErrorBase + 7)
#define scsiUnused08           (scsiErrorBase + 8)
#define scsiUnableToTerminate  (scsiErrorBase + 9)
#define scsiSelectTimeout      (scsiErrorBase + 10)
#define scsiCommandTimeout     (scsiErrorBase + 11)
#define scsiIdentifyMessageRejected (scsiErrorBase + 12)
#define scsiMessageRejectReceived   (scsiErrorBase + 13)
#define scsiSCSIBusReset       (scsiErrorBase + 14)
#define scsiParityError        (scsiErrorBase + 15)
#define scsiAutosenseFailed    (scsiErrorBase + 16)
#define scsiUnused11           (scsiErrorBase + 17)
#define scsiDataRunError       (scsiErrorBase + 18)
#define scsiUnexpectedBusFree  (scsiErrorBase + 19)
#define scsiSequenceFail       (scsiErrorBase + 20)
#define scsiWrongDirection     (scsiErrorBase + 21)
#define scsiUnused16           (scsiErrorBase + 22)
#define scsiBDRsent            (scsiErrorBase + 23)
#define scsiTerminated         (scsiErrorBase + 24)
#define scsiNoNexus            (scsiErrorBase + 25)
#define scsiCDBReceived        (scsiErrorBase + 26)
#define scsiTooManyBuses       (scsiErrorBase + 48)
#define scsiNoSuchXref         (scsiErrorBase + 49)
#define scsiXrefNotFound       (scsiErrorBase + 50)
#define scsiBadFunction        (scsiErrorBase + 64)
#define scsiBadParameter       (scsiErrorBase + 65)
#define scsiTIDInvalid         (scsiErrorBase + 66)
#define scsiLUNInvalid         (scsiErrorBase + 67)
#define scsiIDInvalid          (scsiErrorBase + 68)
#define scsiDataTypeInvalid    (scsiErrorBase + 69)
#define scsiTransferTypeInvalid (scsiErrorBase + 70)
#define scsiCDBLengthInvalid   (scsiErrorBase + 71)

/* Function Prototypes */

/* Main SCSI Manager Entry Point */
OSErr SCSIAction(SCSI_IO *ioPtr);

/* Synchronous wrapper for common operations */
OSErr SCSIExecIOSync(SCSI_IO *ioPtr);

/* Bus Management */
OSErr SCSIBusInquirySync(SCSIBusInquiryPB *inquiry);
OSErr SCSIResetBusSync(SCSIResetBusPB *resetBus);
OSErr SCSIResetDeviceSync(SCSIResetDevicePB *resetDevice);

/* I/O Management */
OSErr SCSIAbortCommandSync(SCSIAbortCommandPB *abort);
OSErr SCSITerminateIOSync(SCSITerminateIOPB *terminate);

/* Parameter Block Management */
SCSI_IO *NewSCSI_PB(void);
void DisposeSCSI_PB(SCSI_IO *pb);

/* Device Management */
OSErr SCSIGetVirtualIDInfo(DeviceIdent *device, uint8_t *virtualID);
OSErr SCSICreateRefNumXref(DeviceIdent *device, int16_t refNum);
OSErr SCSILookupRefNumXref(DeviceIdent *device, int16_t *refNum);
OSErr SCSIRemoveRefNumXref(DeviceIdent *device);

/* Initialization and Cleanup */
OSErr InitSCSIManager(void);
void ShutdownSCSIManager(void);

/* Hardware Abstraction Layer Interface */
typedef struct {
    void *baseAddress;              /* Base address of SCSI controller */
    uint8_t initiatorID;           /* Initiator ID */
    uint8_t maxTarget;             /* Maximum target ID */
    uint8_t maxLUN;                /* Maximum LUN */
    bool supportsSynchronous;       /* Supports synchronous transfers */
    bool supportsWide;             /* Supports wide transfers */
    bool supportsTaggedQueuing;    /* Supports tagged command queuing */
    uint32_t maxTransferSize;      /* Maximum transfer size */
} SCSIHardwareInfo;

/* Hardware abstraction callbacks */
typedef struct {
    OSErr (*initHardware)(SCSIHardwareInfo *info);
    OSErr (*resetBus)(uint8_t busID);
    OSErr (*executeCommand)(uint8_t busID, uint8_t target, uint8_t lun,
                           uint8_t *cdb, uint8_t cdbLen,
                           void *dataPtr, uint32_t dataLen,
                           bool dataOut, uint32_t timeout);
    OSErr (*getDeviceInfo)(uint8_t busID, uint8_t target, uint8_t lun,
                          uint8_t *inquiry, uint8_t inquiryLen);
    void (*shutdownHardware)(uint8_t busID);
} SCSIHardwareCallbacks;

/* Register hardware abstraction layer */
OSErr SCSIRegisterHAL(SCSIHardwareCallbacks *callbacks, SCSIHardwareInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* __SCSIMANAGER_H__ */