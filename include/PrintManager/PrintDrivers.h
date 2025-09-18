/*
 * PrintDrivers.h
 *
 * Print Driver interface for System 7.1 Portable
 * Handles printer driver communication and abstraction
 *
 * Based on Apple's Print Manager driver interface from Mac OS System 7.1
 */

#ifndef __PRINTDRIVERS__
#define __PRINTDRIVERS__

#include "PrintTypes.h"
#include "Devices.h"
#include "Files.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Print Driver Constants */
#define kPrintDriverName ".Print"
#define kMaxDriverName 32
#define kMaxPrinterName 64

/* Driver Control Codes */
enum {
    kPrDrvrOpenCode = 1,                /* Open driver */
    kPrDrvrCloseCode = 2,               /* Close driver */
    kPrDrvrStatusCode = 3,              /* Get status */
    kPrDrvrControlCode = 4,             /* Send control */
    kPrDrvrBitmapCode = 5,              /* Send bitmap */
    kPrDrvrTextCode = 6,                /* Send text */
    kPrDrvrResetCode = 7,               /* Reset printer */
    kPrDrvrConfigCode = 8               /* Configure printer */
};

/* Printer Capabilities */
struct TPrinterCaps {
    Boolean supportsColor;              /* Supports color printing */
    Boolean supportsDuplex;             /* Supports duplex printing */
    Boolean supportsCollation;          /* Supports collation */
    Boolean supportsScaling;            /* Supports scaling */
    short maxResolutionX;               /* Maximum X resolution */
    short maxResolutionY;               /* Maximum Y resolution */
    short minResolutionX;               /* Minimum X resolution */
    short minResolutionY;               /* Minimum Y resolution */
    short maxCopies;                    /* Maximum number of copies */
    short paperSizeCount;               /* Number of supported paper sizes */
    short paperSizes[16];               /* Supported paper size codes */
    Rect maxPaperSize;                  /* Maximum paper size */
    Rect minPaperSize;                  /* Minimum paper size */
    long memorySize;                    /* Printer memory size */
    Boolean supportsPostScript;         /* Supports PostScript */
    Boolean supportsPCL;                /* Supports PCL */
    StringPtr driverName;               /* Driver name */
    StringPtr driverVersion;            /* Driver version */
};
typedef struct TPrinterCaps TPrinterCaps;
typedef TPrinterCaps *TPrinterCapsPtr;

/* Printer Status */
struct TPrinterStatus {
    Boolean online;                     /* Printer is online */
    Boolean ready;                      /* Printer is ready */
    Boolean printing;                   /* Currently printing */
    Boolean paperOut;                   /* Out of paper */
    Boolean paperJam;                   /* Paper jam */
    Boolean tonerLow;                   /* Toner/ink low */
    Boolean error;                      /* General error */
    short errorCode;                    /* Specific error code */
    StringPtr errorMessage;             /* Error message */
    short jobsQueued;                   /* Number of jobs in queue */
    long memoryUsed;                    /* Memory currently used */
    long memoryTotal;                   /* Total memory available */
};
typedef struct TPrinterStatus TPrinterStatus;
typedef TPrinterStatus *TPrinterStatusPtr;

/* Driver Command Block */
struct TDriverCommand {
    short commandCode;                  /* Command code */
    long param1;                        /* Parameter 1 */
    long param2;                        /* Parameter 2 */
    long param3;                        /* Parameter 3 */
    Ptr dataPtr;                        /* Data pointer */
    long dataSize;                      /* Data size */
    short result;                       /* Result code */
};
typedef struct TDriverCommand TDriverCommand;
typedef TDriverCommand *TDriverCommandPtr;

/* Print Driver Functions */

/* Driver Management */
OSErr OpenPrintDriver(StringPtr driverName, short *driverRefNum);
OSErr ClosePrintDriver(short driverRefNum);
OSErr GetPrintDriverInfo(short driverRefNum, TPrinterCapsPtr caps);
OSErr GetPrintDriverStatus(short driverRefNum, TPrinterStatusPtr status);

/* Driver Communication */
OSErr SendDriverCommand(short driverRefNum, TDriverCommandPtr command);
OSErr SendControlToDriver(short driverRefNum, short controlCode, Ptr dataPtr, long dataSize);
OSErr GetStatusFromDriver(short driverRefNum, short statusCode, Ptr dataPtr, long *dataSize);

/* Print Data Transmission */
OSErr SendBitmapToDriver(short driverRefNum, BitMap *bitmap, Rect *srcRect, Rect *dstRect);
OSErr SendTextToDriver(short driverRefNum, Ptr textPtr, long textSize, short fontID, short fontSize);
OSErr SendPictureToDriver(short driverRefNum, PicHandle picture, Rect *dstRect);

/* Driver Configuration */
OSErr ConfigureDriver(short driverRefNum, THPrint hPrint);
OSErr ValidateDriverSettings(short driverRefNum, THPrint hPrint);
OSErr GetDriverDefaults(short driverRefNum, THPrint hPrint);

/* Modern Driver Abstraction */
struct TModernDriverInterface {
    OSErr (*initialize)(void);
    OSErr (*cleanup)(void);
    OSErr (*enumPrinters)(StringPtr printerList[], short *count);
    OSErr (*openPrinter)(StringPtr printerName, Handle *printerHandle);
    OSErr (*closePrinter)(Handle printerHandle);
    OSErr (*startDocument)(Handle printerHandle, StringPtr docName);
    OSErr (*endDocument)(Handle printerHandle);
    OSErr (*startPage)(Handle printerHandle);
    OSErr (*endPage)(Handle printerHandle);
    OSErr (*printBitmap)(Handle printerHandle, BitMap *bitmap, Rect *rect);
    OSErr (*printText)(Handle printerHandle, Ptr text, long size, short font, short size);
    OSErr (*getStatus)(Handle printerHandle, TPrinterStatusPtr status);
    OSErr (*cancelJob)(Handle printerHandle);
};
typedef struct TModernDriverInterface TModernDriverInterface;
typedef TModernDriverInterface *TModernDriverInterfacePtr;

/* Platform-Specific Driver Interfaces */
#ifdef PLATFORM_MACOS
OSErr InitMacOSPrintingSystem(TModernDriverInterfacePtr interface);
OSErr GetMacOSPrinterList(StringPtr printerList[], short *count);
OSErr PrintToMacOSPrinter(StringPtr printerName, PicHandle picture, THPrint hPrint);
#endif

#ifdef PLATFORM_WINDOWS
OSErr InitWindowsPrintingSystem(TModernDriverInterfacePtr interface);
OSErr GetWindowsPrinterList(StringPtr printerList[], short *count);
OSErr PrintToWindowsPrinter(StringPtr printerName, PicHandle picture, THPrint hPrint);
#endif

#ifdef PLATFORM_LINUX
OSErr InitCUPSPrintingSystem(TModernDriverInterfacePtr interface);
OSErr GetCUPSPrinterList(StringPtr printerList[], short *count);
OSErr PrintToCUPSPrinter(StringPtr printerName, PicHandle picture, THPrint hPrint);
#endif

/* Driver Resource Management */
OSErr LoadDriverResources(StringPtr driverName);
OSErr UnloadDriverResources(StringPtr driverName);
Handle GetDriverResource(OSType resourceType, short resourceID);
OSErr InstallDriverResource(OSType resourceType, short resourceID, Handle resource);

/* Legacy Driver Support */
struct TLegacyDriverInfo {
    StringPtr driverName;               /* Original driver name */
    short driverRefNum;                 /* Driver reference number */
    Boolean isSerial;                   /* Is serial printer */
    Boolean isParallel;                 /* Is parallel printer */
    Boolean isAppleTalk;                /* Is AppleTalk printer */
    Boolean isPostScript;               /* Is PostScript printer */
    short deviceType;                   /* Device type code */
    Handle driverHandle;                /* Driver code handle */
    DCEPtr driverDCE;                   /* Driver DCE pointer */
};
typedef struct TLegacyDriverInfo TLegacyDriverInfo;
typedef TLegacyDriverInfo *TLegacyDriverInfoPtr;

/* Legacy Driver Functions */
OSErr LoadLegacyDriver(StringPtr driverName, TLegacyDriverInfoPtr info);
OSErr UnloadLegacyDriver(TLegacyDriverInfoPtr info);
OSErr CallLegacyDriver(TLegacyDriverInfoPtr info, short command, Ptr params);

/* Print Job Management */
struct TPrintJob {
    long jobID;                         /* Unique job identifier */
    StringPtr documentName;             /* Document name */
    StringPtr printerName;              /* Target printer name */
    THPrint hPrint;                     /* Print settings */
    PicHandle documentPicture;          /* Document picture */
    short status;                       /* Current job status */
    short percentComplete;              /* Completion percentage */
    unsigned long startTime;            /* Job start time */
    unsigned long estimatedTime;        /* Estimated completion time */
    Handle driverHandle;                /* Driver handle */
};
typedef struct TPrintJob TPrintJob;
typedef TPrintJob *TPrintJobPtr;

/* Print Job Functions */
OSErr CreatePrintJob(StringPtr docName, StringPtr printerName, THPrint hPrint,
                     PicHandle picture, TPrintJobPtr *job);
OSErr SubmitPrintJob(TPrintJobPtr job);
OSErr CancelPrintJob(long jobID);
OSErr GetPrintJobStatus(long jobID, TPrintJob *jobInfo);
OSErr GetPrintJobList(TPrintJobPtr jobList[], short *count);

/* Driver Error Codes */
enum {
    kDriverNoErr = 0,                   /* No error */
    kDriverNotFound = -1,               /* Driver not found */
    kDriverInUse = -2,                  /* Driver already in use */
    kDriverNotOpen = -3,                /* Driver not open */
    kDriverError = -4,                  /* General driver error */
    kPrinterOffline = -5,               /* Printer is offline */
    kPrinterNotReady = -6,              /* Printer not ready */
    kPaperOutError = -7,                /* Out of paper */
    kPaperJamError = -8,                /* Paper jam */
    kTonerLowError = -9,                /* Toner/ink low */
    kMemoryFullError = -10,             /* Printer memory full */
    kInvalidCommand = -11,              /* Invalid command */
    kTimeoutError = -12,                /* Communication timeout */
    kNetworkError = -13                 /* Network error */
};

/* Driver Utilities */
OSErr GetDefaultPrinter(StringPtr printerName);
OSErr SetDefaultPrinter(StringPtr printerName);
Boolean IsPrinterAvailable(StringPtr printerName);
OSErr RefreshPrinterList(void);
OSErr TestPrinterConnection(StringPtr printerName);

#ifdef __cplusplus
}
#endif

#endif /* __PRINTDRIVERS__ */