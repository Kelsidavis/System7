/*
 * FinderInterface.h - Finder Launch Protocol Definitions
 *
 * This file defines the Finder launch protocol, document handling,
 * and application-Finder communication for Mac OS 7.1.
 */

#ifndef _FINDER_INTERFACE_H
#define _FINDER_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include "ApplicationTypes.h"
#include "../FileManager/FileManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Finder Launch Protocol Constants
 * ============================================================================ */

/* Launch parameter messages */
enum {
    kFinderLaunchOpen       = 0,        /* Open documents */
    kFinderLaunchPrint      = 1,        /* Print documents */
    kFinderLaunchRun        = 2         /* Just run application */
};

/* Application parameter block signature */
#define kAppParamSignature      'APPL'
#define kAppParamVersion        1

/* Document types */
#define kDocumentTypeGeneric    'docu'
#define kDocumentTypeText       'TEXT'
#define kDocumentTypeApplication 'APPL'
#define kDocumentTypeFolder     'fold'

/* Creator signatures */
#define kFinderSignature        'MACS'
#define kSystemSignature        'ERIK'

/* Maximum files in launch */
#define kMaxLaunchFiles         256

/* Launch flags */
#define kLaunchInBackground     0x0001
#define kLaunchDontSwitch       0x0002
#define kLaunchNoFileFlags      0x0004
#define kLaunchAsync            0x0008

/* ============================================================================
 * Finder Launch Data Structures
 * ============================================================================ */

typedef struct FinderLaunchRecord {
    OSType      signature;              /* 'APPL' signature */
    uint16_t    version;                /* Parameter block version */
    uint16_t    message;                /* Launch message */
    uint16_t    fileCount;              /* Number of files */
    uint16_t    reserved;               /* Reserved for future use */
    AppFile     files[1];               /* Variable-length file array */
} FinderLaunchRecord;

typedef struct DocumentInfo {
    FSSpec      docSpec;                /* Document file spec */
    OSType      docType;                /* Document type */
    OSType      creator;                /* Document creator */
    uint32_t    fileSize;               /* File size */
    uint32_t    modDate;                /* Modification date */
    uint16_t    flags;                  /* Document flags */
    bool        canOpen;                /* Can be opened */
    bool        canPrint;               /* Can be printed */
} DocumentInfo;

typedef struct ApplicationLaunchInfo {
    FSSpec      appSpec;                /* Application file spec */
    OSType      signature;              /* Application signature */
    uint32_t    version;                /* Application version */
    uint32_t    minMemory;              /* Minimum memory */
    uint32_t    preferredMemory;        /* Preferred memory */
    uint16_t    supportedTypes[16];     /* Supported document types */
    uint16_t    typeCount;              /* Number of supported types */
    bool        canBackground;          /* Can run in background */
    bool        canPrint;               /* Can print documents */
} ApplicationLaunchInfo;

/* ============================================================================
 * Desktop Database Structures
 * ============================================================================ */

typedef struct DesktopInfo {
    OSType      signature;              /* Application signature */
    OSType      fileType;               /* File type */
    OSType      creator;                /* File creator */
    FSSpec      appSpec;                /* Application location */
    Str255      appName;                /* Application name */
    Str255      comment;                /* File comment */
    uint16_t    iconID;                 /* Icon ID */
    uint32_t    version;                /* Version info */
} DesktopInfo;

typedef struct FileTypeInfo {
    OSType      fileType;               /* File type */
    OSType      creator;                /* Creator signature */
    Str255      description;            /* Type description */
    uint16_t    iconID;                 /* Icon resource ID */
    OSType      defaultApp;             /* Default application */
    bool        alwaysShowExtension;    /* Always show extension */
} FileTypeInfo;

/* ============================================================================
 * Finder Interface Initialization
 * ============================================================================ */

/* Initialize Finder interface */
OSErr InitFinderInterface(void);

/* Terminate Finder interface */
void TerminateFinderInterface(void);

/* Set global application parameters */
OSErr SetGlobalAppParameters(Handle paramHandle);

/* Get global application parameters */
Handle GetGlobalAppParameters(void);

/* ============================================================================
 * Document Opening
 * ============================================================================ */

/* Open document with specific application */
OSErr OpenDocumentWithApp(const FSSpec* docSpec, const FSSpec* appSpec);

/* Find application for document */
OSErr FindApplicationForDocument(const FSSpec* docSpec, FSSpec* appSpec);

/* Find application by signature */
OSErr FindApplicationBySignature(uint32_t signature, FSSpec* appSpec);

/* Get document information */
OSErr GetDocumentInfo(const FSSpec* docSpec, DocumentInfo* info);

/* Validate document can be opened */
OSErr ValidateDocumentForApp(const FSSpec* docSpec, const FSSpec* appSpec);

/* ============================================================================
 * Printing Support
 * ============================================================================ */

/* Print document with application */
OSErr PrintDocumentWithApp(const FSSpec* docSpec, const FSSpec* appSpec);

/* Print multiple documents */
OSErr PrintDocuments(const FSSpec* docSpecs, uint16_t docCount,
                    const FSSpec* appSpec);

/* Check if application can print */
bool CanApplicationPrint(const FSSpec* appSpec);

/* Get print capabilities */
OSErr GetApplicationPrintCapabilities(const FSSpec* appSpec, uint32_t* caps);

/* ============================================================================
 * Application Launch Support
 * ============================================================================ */

/* Get application launch information */
OSErr GetApplicationLaunchInfo(const FSSpec* appSpec, ApplicationLaunchInfo* info);

/* Create launch parameter block */
OSErr CreateLaunchParameters(const FSSpec* appSpec, const FSSpec* documents,
                            uint16_t docCount, uint16_t message,
                            Handle* paramHandle);

/* Parse launch parameters */
OSErr ParseLaunchParameters(Handle paramHandle, uint16_t* message,
                           uint16_t* docCount, AppFile** files);

/* Validate launch parameters */
OSErr ValidateLaunchParameters(Handle paramHandle);

/* ============================================================================
 * Application Information
 * ============================================================================ */

/* Get application information from file */
OSErr GetApplicationInfo(const FSSpec* appSpec, ApplicationInfo* info);

/* Get application name */
OSErr GetApplicationName(const FSSpec* appSpec, Str255 name);

/* Get application version */
OSErr GetApplicationVersion(const FSSpec* appSpec, uint32_t* version);

/* Get application memory requirements */
OSErr GetApplicationMemoryRequirements(const FSSpec* appSpec,
                                      uint32_t* minMem, uint32_t* prefMem);

/* Check application permissions */
OSErr CheckApplicationPermissions(const FSSpec* appSpec);

/* ============================================================================
 * File Type Management
 * ============================================================================ */

/* Register file type with application */
OSErr RegisterFileType(OSType fileType, OSType creator, const FSSpec* appSpec);

/* Unregister file type */
OSErr UnregisterFileType(OSType fileType, OSType creator);

/* Get file type information */
OSErr GetFileTypeInfo(OSType fileType, OSType creator, FileTypeInfo* info);

/* Set default application for file type */
OSErr SetDefaultApplication(OSType fileType, const FSSpec* appSpec);

/* Get default application for file type */
OSErr GetDefaultApplication(OSType fileType, FSSpec* appSpec);

/* ============================================================================
 * Desktop Database Management
 * ============================================================================ */

/* Update desktop database entry */
OSErr UpdateDesktopDatabase(const FSSpec* appSpec);

/* Remove desktop database entry */
OSErr RemoveDesktopEntry(const FSSpec* appSpec);

/* Rebuild desktop database */
OSErr RebuildDesktopDatabase(int16_t vRefNum);

/* Get desktop database info */
OSErr GetDesktopInfo(const FSSpec* appSpec, DesktopInfo* info);

/* Set desktop database info */
OSErr SetDesktopInfo(const FSSpec* appSpec, const DesktopInfo* info);

/* ============================================================================
 * Launch Chain Support
 * ============================================================================ */

/* Chain to another application */
OSErr ChainToApplication(const FSSpec* appSpec, const AppParameters* params);

/* Setup launch chain */
OSErr SetupLaunchChain(ApplicationControlBlock* acb, const FSSpec* nextApp);

/* Execute launch chain */
OSErr ExecuteLaunchChain(ApplicationControlBlock* acb);

/* Cancel launch chain */
OSErr CancelLaunchChain(ApplicationControlBlock* acb);

/* ============================================================================
 * Apple Events Support
 * ============================================================================ */

/* Send open document event */
OSErr SendOpenDocumentEvent(ProcessControlBlock* pcb, const FSSpec* docSpec);

/* Send print document event */
OSErr SendPrintDocumentEvent(ProcessControlBlock* pcb, const FSSpec* docSpec);

/* Send quit event */
OSErr SendQuitEvent(ProcessControlBlock* pcb);

/* Send reopen application event */
OSErr SendReopenApplicationEvent(ProcessControlBlock* pcb);

/* Handle required Apple Events */
OSErr HandleRequiredAppleEvents(ApplicationControlBlock* acb);

/* ============================================================================
 * Alias Support
 * ============================================================================ */

/* Resolve application alias */
OSErr ResolveApplicationAlias(FSSpec* appSpec);

/* Resolve document alias */
OSErr ResolveDocumentAlias(FSSpec* docSpec);

/* Create application alias */
OSErr CreateApplicationAlias(const FSSpec* appSpec, const FSSpec* aliasSpec);

/* Update alias target */
OSErr UpdateAliasTarget(const FSSpec* aliasSpec, const FSSpec* newTarget);

/* ============================================================================
 * Bundle Support (for future compatibility)
 * ============================================================================ */

/* Get bundle information */
OSErr GetBundleInfo(const FSSpec* appSpec, Handle* bundleInfo);

/* Get bundle resources */
OSErr GetBundleResources(const FSSpec* appSpec, OSType resType, int16_t resID,
                        Handle* resource);

/* Get localized bundle resource */
OSErr GetLocalizedBundleResource(const FSSpec* appSpec, OSType resType,
                                int16_t resID, int16_t language,
                                Handle* resource);

/* ============================================================================
 * Launch Validation
 * ============================================================================ */

/* Validate application file */
OSErr ValidateApplicationFile(const FSSpec* appSpec);

/* Check application compatibility */
OSErr CheckApplicationCompatibility(const FSSpec* appSpec);

/* Check system requirements */
OSErr CheckSystemRequirements(const FSSpec* appSpec);

/* Check memory requirements */
OSErr CheckMemoryRequirements(const FSSpec* appSpec, uint32_t availableMemory);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

/* Get launch error string */
OSErr GetLaunchErrorString(OSErr error, Str255 errorString);

/* Log launch event */
void LogLaunchEvent(const FSSpec* appSpec, OSErr result, const char* event);

/* Report launch failure */
OSErr ReportLaunchFailure(const FSSpec* appSpec, OSErr error);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Convert C string to Pascal string for app name */
void ConvertAppNameToPascal(const char* cName, Str255 pascalName);

/* Convert Pascal string to C string for app name */
void ConvertAppNameToC(const Str255 pascalName, char* cName, uint32_t maxLen);

/* Compare application signatures */
bool CompareApplicationSignatures(OSType sig1, OSType sig2);

/* Get application from process serial number */
OSErr GetApplicationFromPSN(const ProcessSerialNumber* psn, FSSpec* appSpec);

/* Get process serial number from application */
OSErr GetPSNFromApplication(const FSSpec* appSpec, ProcessSerialNumber* psn);

/* ============================================================================
 * Constants for Error Reporting
 * ============================================================================ */

enum {
    kLaunchNoErr                = 0,
    kLaunchMemErr              = -1,
    kLaunchFileNotFound        = -43,
    kLaunchBadFormat           = -192,
    kLaunchResNotFound         = -193,
    kLaunchDupPSN              = -194,
    kLaunchAppInTrash          = -195,
    kLaunchNoLaunchData        = -196,
    kLaunchNotRecognized       = -197,
    kLaunchAppDamaged          = -198,
    kLaunchIncompatible        = -199,
    kLaunchInsufficientMemory  = -200,
    kLaunchPermissionDenied    = -201,
    kLaunchApplicationBusy     = -202,
    kLaunchCannotLocateApp     = -203
};

#ifdef __cplusplus
}
#endif

#endif /* _FINDER_INTERFACE_H */