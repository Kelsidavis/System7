/*
 * PrintResources.h
 *
 * Print Resource management for System 7.1 Portable
 * Handles PDEF resources, printer drivers, and print-related resources
 *
 * Based on Apple's Print Manager resources from Mac OS System 7.1
 */

#ifndef __PRINTRESOURCES__
#define __PRINTRESOURCES__

#include "PrintTypes.h"
#include "Resources.h"
#include "Files.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resource Types */
enum {
    kPDEFResourceType = 'PDEF',         /* Print definition resource */
    kPRECResourceType = 'PREC',         /* Print record resource */
    kPDLGResourceType = 'PDLG',         /* Print dialog resource */
    kPSTLResourceType = 'PSTL',         /* Print style resource */
    kPDRVResourceType = 'PDRV',         /* Print driver resource */
    kPPATResourceType = 'PPAT',         /* Print pattern resource */
    kPFNTResourceType = 'PFNT',         /* Print font resource */
    kPICNResourceType = 'PICN',         /* Print icon resource */
    kPSTRResourceType = 'PSTR'          /* Print string resource */
};

/* Resource IDs */
enum {
    kPDEFDraftID = 0,                   /* Draft PDEF resource */
    kPDEFSpoolID = 1,                   /* Spool PDEF resource */
    kPDEFUser1ID = 2,                   /* User PDEF 1 resource */
    kPDEFUser2ID = 3,                   /* User PDEF 2 resource */
    kPDEFDialogsID = 4,                 /* Dialogs PDEF resource */
    kPDEFPicFileID = 5,                 /* Picture file PDEF resource */
    kPDEFConfigID = 6,                  /* Configuration PDEF resource */
    kPDEFHackID = 7                     /* Hack PDEF resource */
};

/* PDEF Function Offsets */
enum {
    kPDEFOpenDocOffset = 0x0000,        /* PrOpenDoc function offset */
    kPDEFCloseDocOffset = 0x0004,       /* PrCloseDoc function offset */
    kPDEFOpenPageOffset = 0x0008,       /* PrOpenPage function offset */
    kPDEFClosePageOffset = 0x000C,      /* PrClosePage function offset */
    kPDEFDefaultOffset = 0x0000,        /* PrintDefault function offset */
    kPDEFStlDialogOffset = 0x0004,      /* PrStlDialog function offset */
    kPDEFJobDialogOffset = 0x0008,      /* PrJobDialog function offset */
    kPDEFStlInitOffset = 0x000C,        /* PrStlInit function offset */
    kPDEFJobInitOffset = 0x0010,        /* PrJobInit function offset */
    kPDEFDlgMainOffset = 0x0014,        /* PrDlgMain function offset */
    kPDEFValidateOffset = 0x0018,       /* PrValidate function offset */
    kPDEFJobMergeOffset = 0x001C        /* PrJobMerge function offset */
};

/* Print Resource File Structure */
struct TPrintResourceFile {
    short fileRefNum;                   /* Resource file reference number */
    StringPtr fileName;                 /* Resource file name */
    FSSpec fileSpec;                    /* File specification */
    Boolean isOpen;                     /* File is open */
    Boolean weOpened;                   /* We opened the file */
    short printerType;                  /* Printer type */
    long version;                       /* Resource version */
    Handle printerInfo;                 /* Printer information */
};
typedef struct TPrintResourceFile TPrintResourceFile;
typedef TPrintResourceFile *TPrintResourceFilePtr;

/* PDEF Resource Structure */
struct TPDEFResource {
    Handle resourceHandle;              /* Resource handle */
    short resourceID;                   /* Resource ID */
    OSType resourceType;                /* Resource type */
    Boolean locked;                     /* Resource is locked */
    long codeOffset;                    /* Code offset */
    long dataOffset;                    /* Data offset */
    short version;                      /* PDEF version */
    long jumpTable[16];                 /* Function jump table */
};
typedef struct TPDEFResource TPDEFResource;
typedef TPDEFResource *TPDEFResourcePtr;

/* Print Driver Resource */
struct TPrintDriverResource {
    StringPtr driverName;               /* Driver name */
    short driverVersion;                /* Driver version */
    long driverFlags;                   /* Driver capability flags */
    Handle driverCode;                  /* Driver code */
    Handle driverData;                  /* Driver data */
    TPrinterCaps capabilities;          /* Printer capabilities */
    Handle iconSuite;                   /* Icon suite */
    StringPtr description;              /* Driver description */
};
typedef struct TPrintDriverResource TPrintDriverResource;
typedef TPrintDriverResource *TPrintDriverResourcePtr;

/* Resource Management Functions */

/* Print Resource File Management */
OSErr OpenPrintResourceFile(StringPtr fileName, short *fileRefNum);
OSErr ClosePrintResourceFile(short fileRefNum);
OSErr CreatePrintResourceFile(FSSpec *fileSpec, StringPtr printerName);
OSErr ValidatePrintResourceFile(short fileRefNum);
OSErr GetPrintResourceFileInfo(short fileRefNum, TPrintResourceFilePtr info);

/* PDEF Resource Management */
OSErr LoadPDEFResource(short resourceID, TPDEFResourcePtr *pdefResource);
OSErr ReleasePDEFResource(TPDEFResourcePtr pdefResource);
OSErr CallPDEFFunction(TPDEFResourcePtr pdefResource, short functionOffset, Ptr params);
OSErr GetPDEFJumpTable(TPDEFResourcePtr pdefResource, long jumpTable[], short *count);
OSErr ValidatePDEFResource(TPDEFResourcePtr pdefResource);

/* Print Record Resources */
OSErr SavePrintRecordResource(THPrint hPrint, short resourceID);
OSErr LoadPrintRecordResource(short resourceID, THPrint *hPrint);
OSErr DeletePrintRecordResource(short resourceID);
OSErr GetPrintRecordResourceList(short resourceIDs[], short *count);

/* Driver Resource Management */
OSErr LoadPrintDriverResource(StringPtr driverName, TPrintDriverResourcePtr *driver);
OSErr ReleasePrintDriverResource(TPrintDriverResourcePtr driver);
OSErr InstallPrintDriverResource(TPrintDriverResourcePtr driver);
OSErr RemovePrintDriverResource(StringPtr driverName);
OSErr GetDriverResourceList(StringPtr driverNames[], short *count);

/* Dialog Resource Management */
OSErr LoadPrintDialogResource(short dialogID, DialogPtr *dialog);
OSErr GetPrintDialogResourceInfo(short dialogID, short *itemCount, Rect *bounds);
OSErr LoadPrintDialogItemList(short dialogID, Handle *itemList);
OSErr CreateCustomPrintDialog(short dialogID, DialogPtr *dialog);

/* Icon and Image Resources */
OSErr LoadPrinterIconSuite(StringPtr printerName, Handle *iconSuite);
OSErr GetPrinterIcon(StringPtr printerName, short iconSize, Handle *icon);
OSErr LoadPrintPatternResource(short patternID, PatHandle *pattern);
OSErr LoadPrintBitmapResource(short bitmapID, BitMap **bitmap);

/* String Resources */
OSErr LoadPrintStringResource(short stringID, StringPtr string);
OSErr GetPrintErrorString(short errorCode, StringPtr errorString);
OSErr GetPrinterStatusString(short statusCode, StringPtr statusString);
OSErr LoadLocalizedPrintString(short stringID, short languageCode, StringPtr string);

/* Font Resources */
OSErr LoadPrintFontResource(short fontID, Handle *fontHandle);
OSErr GetPrintFontList(short fontIDs[], StringPtr fontNames[], short *count);
OSErr InstallPrintFont(Handle fontHandle, short *fontID);
OSErr RemovePrintFont(short fontID);

/* Modern Resource Extensions */
OSErr LoadResourceFromFile(FSSpec *fileSpec, OSType resourceType, short resourceID, Handle *resource);
OSErr SaveResourceToFile(FSSpec *fileSpec, OSType resourceType, short resourceID, Handle resource);
OSErr CompressResourceData(Handle resource, Handle *compressedResource);
OSErr DecompressResourceData(Handle compressedResource, Handle *resource);

/* Resource Caching */
struct TResourceCache {
    short cacheSize;                    /* Cache size */
    short itemCount;                    /* Number of cached items */
    Handle cacheData;                   /* Cache data */
    long accessTime[32];                /* Access times */
    Boolean dirty[32];                  /* Dirty flags */
};
typedef struct TResourceCache TResourceCache;
typedef TResourceCache *TResourceCachePtr;

OSErr CreateResourceCache(short cacheSize, TResourceCachePtr *cache);
OSErr DisposeResourceCache(TResourceCachePtr cache);
OSErr AddResourceToCache(TResourceCachePtr cache, OSType resourceType, short resourceID, Handle resource);
OSErr GetResourceFromCache(TResourceCachePtr cache, OSType resourceType, short resourceID, Handle *resource);
OSErr FlushResourceCache(TResourceCachePtr cache);
OSErr PurgeResourceCache(TResourceCachePtr cache);

/* Resource Validation */
OSErr ValidateResourceIntegrity(Handle resource, OSType resourceType);
OSErr CheckResourceVersion(Handle resource, short requiredVersion);
OSErr VerifyResourceChecksum(Handle resource);
OSErr RepairResourceData(Handle resource, Handle *repairedResource);

/* Resource Utilities */
OSErr GetResourceTypeList(OSType resourceTypes[], short *count);
OSErr GetResourceIDList(OSType resourceType, short resourceIDs[], short *count);
OSErr DuplicateResource(Handle sourceResource, Handle *destResource);
OSErr MergeResourceFiles(short sourceFile, short destFile);
OSErr OptimizeResourceFile(short fileRefNum);

/* Print Resource Templates */
struct TPrintResourceTemplate {
    OSType resourceType;                /* Resource type */
    short resourceID;                   /* Resource ID */
    StringPtr name;                     /* Resource name */
    long size;                          /* Resource size */
    Handle templateData;                /* Template data */
    StringPtr description;              /* Description */
};
typedef struct TPrintResourceTemplate TPrintResourceTemplate;

OSErr CreateResourceFromTemplate(TPrintResourceTemplate *template, Handle *resource);
OSErr LoadResourceTemplate(OSType resourceType, TPrintResourceTemplate **template);
OSErr SaveResourceTemplate(TPrintResourceTemplate *template, FSSpec *file);

/* Legacy Resource Support */
OSErr ConvertLegacyPrintResource(Handle legacyResource, Handle *modernResource);
OSErr UpdateResourceToCurrentVersion(Handle resource, short currentVersion);
OSErr MigratePrintResources(short oldFileRefNum, short newFileRefNum);

/* Resource Security */
OSErr EncryptResourceData(Handle resource, StringPtr password, Handle *encryptedResource);
OSErr DecryptResourceData(Handle encryptedResource, StringPtr password, Handle *resource);
OSErr SignResourceData(Handle resource, Handle signature);
OSErr VerifyResourceSignature(Handle resource, Handle signature);

/* Resource Monitoring */
typedef pascal void (*ResourceChangeNotifyProcPtr)(OSType resourceType, short resourceID, short changeType);

enum {
    kResourceAdded = 1,                 /* Resource was added */
    kResourceChanged = 2,               /* Resource was modified */
    kResourceRemoved = 3                /* Resource was removed */
};

OSErr InstallResourceChangeNotify(ResourceChangeNotifyProcPtr notifyProc);
OSErr RemoveResourceChangeNotify(ResourceChangeNotifyProcPtr notifyProc);
OSErr NotifyResourceChange(OSType resourceType, short resourceID, short changeType);

/* Resource Debugging */
OSErr DumpResourceInfo(short fileRefNum, Handle *infoHandle);
OSErr ValidateAllResources(short fileRefNum, short *errorCount);
OSErr AnalyzeResourceUsage(short fileRefNum, Handle *analysisHandle);
OSErr OptimizeResourceAccess(short fileRefNum);

#ifdef __cplusplus
}
#endif

#endif /* __PRINTRESOURCES__ */