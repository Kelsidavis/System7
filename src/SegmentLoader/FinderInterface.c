/*
 * FinderInterface.c - Finder Launch Protocol and Parameter Handling
 *
 * This file implements the Finder launch protocol, handling application
 * launch parameters, document opening, and communication between the
 * Finder and applications.
 */

#include "../../include/SegmentLoader/SegmentLoader.h"
#include "../../include/FileManager/FileManager.h"
#include "../../include/ResourceManager/ResourceManager.h"
#include "../../include/Debugging/Debugging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * Finder Launch Protocol Constants
 * ============================================================================ */

/* Launch parameter messages */
#define FINDER_LAUNCH_OPEN      0       /* Open documents */
#define FINDER_LAUNCH_PRINT     1       /* Print documents */
#define FINDER_LAUNCH_RUN       2       /* Just run application */

/* Application parameter block signature */
#define APP_PARAM_SIGNATURE     'APPL'

/* Maximum files that can be passed to an application */
#define MAX_LAUNCH_FILES        256

/* Default application parameter size */
#define DEFAULT_PARAM_SIZE      1024

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

typedef struct FinderLaunchRecord {
    OSType          signature;      /* 'APPL' signature */
    uint16_t        version;        /* Parameter block version */
    uint16_t        message;        /* Launch message */
    uint16_t        fileCount;      /* Number of files */
    uint16_t        reserved;       /* Reserved for future use */
    AppFile         files[1];       /* Variable-length file array */
} FinderLaunchRecord;

typedef struct LaunchContext {
    FSSpec          appSpec;        /* Application to launch */
    FSSpec          documents[MAX_LAUNCH_FILES]; /* Documents to open */
    uint16_t        documentCount;  /* Number of documents */
    uint16_t        launchMessage;  /* Launch message */
    uint32_t        appSignature;   /* Application signature */
    bool            printMode;      /* Print instead of open */
    bool            backgroundLaunch; /* Launch in background */
} LaunchContext;

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static LaunchContext gCurrentLaunch;
static bool gFinderInterfaceInitialized = false;
static Handle gGlobalAppParams = NULL;

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

static OSErr PrepareFinderLaunch(const FSSpec* appSpec, const FSSpec* documents,
                                 uint16_t docCount, uint16_t message);
static OSErr CreateAppParameterBlock(const LaunchContext* context, Handle* paramHandle);
static OSErr ValidateDocumentType(const FSSpec* docSpec, OSType appType);
static OSErr FindApplicationForDocument(const FSSpec* docSpec, FSSpec* appSpec);
static OSErr GetApplicationSignature(const FSSpec* appSpec, OSType* signature);
static OSErr SendLaunchEvent(const FSSpec* appSpec, const AppParameters* params);

/* ============================================================================
 * Finder Interface Initialization
 * ============================================================================ */

OSErr InitFinderInterface(void)
{
    if (gFinderInterfaceInitialized) {
        return segNoErr;
    }

    DEBUG_LOG("InitFinderInterface: Initializing Finder interface\n");

    /* Initialize launch context */
    memset(&gCurrentLaunch, 0, sizeof(LaunchContext));

    /* Initialize global parameters */
    gGlobalAppParams = NULL;

    gFinderInterfaceInitialized = true;

    DEBUG_LOG("InitFinderInterface: Finder interface initialized\n");
    return segNoErr;
}

void TerminateFinderInterface(void)
{
    if (!gFinderInterfaceInitialized) {
        return;
    }

    DEBUG_LOG("TerminateFinderInterface: Terminating Finder interface\n");

    /* Clean up global parameters */
    if (gGlobalAppParams) {
        DisposeHandle(gGlobalAppParams);
        gGlobalAppParams = NULL;
    }

    gFinderInterfaceInitialized = false;
}

/* ============================================================================
 * Document Opening
 * ============================================================================ */

OSErr OpenDocumentWithApp(const FSSpec* docSpec, const FSSpec* appSpec)
{
    if (!docSpec || !appSpec) {
        return segBadFormat;
    }

    DEBUG_LOG("OpenDocumentWithApp: Opening '%s' with '%s'\n",
             docSpec->name, appSpec->name);

    /* Initialize Finder interface if needed */
    if (!gFinderInterfaceInitialized) {
        OSErr err = InitFinderInterface();
        if (err != segNoErr) return err;
    }

    /* Prepare the launch */
    OSErr err = PrepareFinderLaunch(appSpec, docSpec, 1, FINDER_LAUNCH_OPEN);
    if (err != segNoErr) {
        DEBUG_LOG("OpenDocumentWithApp: Launch preparation failed: %d\n", err);
        return err;
    }

    /* Create parameter block */
    Handle paramHandle;
    err = CreateAppParameterBlock(&gCurrentLaunch, &paramHandle);
    if (err != segNoErr) {
        DEBUG_LOG("OpenDocumentWithApp: Parameter creation failed: %d\n", err);
        return err;
    }

    /* Launch the application */
    ApplicationControlBlock* acb;
    AppParameters* params = (AppParameters*)*paramHandle;

    err = LaunchApplication(appSpec, params, 0, &acb);
    if (err != segNoErr) {
        DisposeHandle(paramHandle);
        DEBUG_LOG("OpenDocumentWithApp: Application launch failed: %d\n", err);
        return err;
    }

    /* Clean up */
    DisposeHandle(paramHandle);

    DEBUG_LOG("OpenDocumentWithApp: Document opened successfully\n");
    return segNoErr;
}

OSErr FindApplicationForDocument(const FSSpec* docSpec, FSSpec* appSpec)
{
    if (!docSpec || !appSpec) {
        return segBadFormat;
    }

    DEBUG_LOG("FindApplicationForDocument: Finding app for '%s'\n", docSpec->name);

    /* Get document file info */
    FInfo fileInfo;
    OSErr err = FSpGetFInfo(docSpec, &fileInfo);
    if (err != noErr) {
        DEBUG_LOG("FindApplicationForDocument: Failed to get file info: %d\n", err);
        return err;
    }

    /* Try to find application by creator */
    if (fileInfo.fdCreator != 0) {
        err = FindApplicationBySignature(fileInfo.fdCreator, appSpec);
        if (err == segNoErr) {
            DEBUG_LOG("FindApplicationForDocument: Found by creator '%c%c%c%c'\n",
                     (char)(fileInfo.fdCreator >> 24), (char)(fileInfo.fdCreator >> 16),
                     (char)(fileInfo.fdCreator >> 8), (char)fileInfo.fdCreator);
            return segNoErr;
        }
    }

    /* Try to find application by file type */
    /* This would involve scanning the desktop database */
    DEBUG_LOG("FindApplicationForDocument: Searching by file type '%c%c%c%c'\n",
             (char)(fileInfo.fdType >> 24), (char)(fileInfo.fdType >> 16),
             (char)(fileInfo.fdType >> 8), (char)fileInfo.fdType);

    /* TODO: Implement desktop database search */
    /* For now, return not found */
    DEBUG_LOG("FindApplicationForDocument: No application found\n");
    return segNotRecognized;
}

OSErr FindApplicationBySignature(uint32_t signature, FSSpec* appSpec)
{
    DEBUG_LOG("FindApplicationBySignature: Finding app with signature '%c%c%c%c'\n",
             (char)(signature >> 24), (char)(signature >> 16),
             (char)(signature >> 8), (char)signature);

    /* TODO: Implement application search by signature */
    /* This would involve:
     * 1. Searching the System Folder
     * 2. Searching the Applications folder
     * 3. Checking the desktop database
     * 4. Scanning mounted volumes
     */

    /* For now, return not found */
    DEBUG_LOG("FindApplicationBySignature: Application not found\n");
    return segNotRecognized;
}

/* ============================================================================
 * Launch Preparation
 * ============================================================================ */

static OSErr PrepareFinderLaunch(const FSSpec* appSpec, const FSSpec* documents,
                                 uint16_t docCount, uint16_t message)
{
    DEBUG_LOG("PrepareFinderLaunch: Preparing launch with %d documents\n", docCount);

    /* Clear current launch context */
    memset(&gCurrentLaunch, 0, sizeof(LaunchContext));

    /* Copy application spec */
    gCurrentLaunch.appSpec = *appSpec;

    /* Copy document specs */
    if (documents && docCount > 0) {
        uint16_t copyCount = (docCount > MAX_LAUNCH_FILES) ? MAX_LAUNCH_FILES : docCount;
        for (uint16_t i = 0; i < copyCount; i++) {
            gCurrentLaunch.documents[i] = documents[i];
        }
        gCurrentLaunch.documentCount = copyCount;
    }

    /* Set launch parameters */
    gCurrentLaunch.launchMessage = message;
    gCurrentLaunch.printMode = (message == FINDER_LAUNCH_PRINT);

    /* Get application signature */
    OSErr err = GetApplicationSignature(appSpec, &gCurrentLaunch.appSignature);
    if (err != segNoErr) {
        DEBUG_LOG("PrepareFinderLaunch: Failed to get app signature: %d\n", err);
        /* Continue with default signature */
        gCurrentLaunch.appSignature = 'APPL';
    }

    DEBUG_LOG("PrepareFinderLaunch: Launch prepared successfully\n");
    return segNoErr;
}

static OSErr CreateAppParameterBlock(const LaunchContext* context, Handle* paramHandle)
{
    if (!context || !paramHandle) {
        return segBadFormat;
    }

    DEBUG_LOG("CreateAppParameterBlock: Creating parameter block\n");

    /* Calculate parameter block size */
    size_t paramSize = sizeof(AppParameters);
    if (context->documentCount > 1) {
        paramSize += (context->documentCount - 1) * sizeof(AppFile);
    }

    /* Allocate parameter handle */
    *paramHandle = NewHandle(paramSize);
    if (!*paramHandle) {
        DEBUG_LOG("CreateAppParameterBlock: Failed to allocate parameter handle\n");
        return segMemFullErr;
    }

    /* Fill in parameter block */
    HLock(*paramHandle);
    AppParameters* params = (AppParameters*)**paramHandle;

    params->message = context->launchMessage;
    params->count = context->documentCount;

    /* Convert document specs to AppFile format */
    for (uint16_t i = 0; i < context->documentCount; i++) {
        const FSSpec* docSpec = &context->documents[i];

        /* Get file info */
        FInfo fileInfo;
        OSErr err = FSpGetFInfo(docSpec, &fileInfo);
        if (err == noErr) {
            params->files[i].vRefNum = docSpec->vRefNum;
            params->files[i].fType = fileInfo.fdType;
            params->files[i].versNum = 0;  /* Version not used */

            /* Convert filename to Pascal string */
            size_t nameLen = strlen(docSpec->name);
            if (nameLen > 255) nameLen = 255;
            params->files[i].fName[0] = (unsigned char)nameLen;
            memcpy(&params->files[i].fName[1], docSpec->name, nameLen);

            DEBUG_LOG("CreateAppParameterBlock: File %d: '%.*s' (type '%c%c%c%c')\n",
                     i, nameLen, docSpec->name,
                     (char)(fileInfo.fdType >> 24), (char)(fileInfo.fdType >> 16),
                     (char)(fileInfo.fdType >> 8), (char)fileInfo.fdType);
        } else {
            DEBUG_LOG("CreateAppParameterBlock: Failed to get info for file %d\n", i);
        }
    }

    HUnlock(*paramHandle);

    DEBUG_LOG("CreateAppParameterBlock: Parameter block created (%d bytes)\n", paramSize);
    return segNoErr;
}

static OSErr GetApplicationSignature(const FSSpec* appSpec, OSType* signature)
{
    DEBUG_LOG("GetApplicationSignature: Getting signature for '%s'\n", appSpec->name);

    if (!appSpec || !signature) {
        return segBadFormat;
    }

    /* Get file info */
    FInfo fileInfo;
    OSErr err = FSpGetFInfo(appSpec, &fileInfo);
    if (err != noErr) {
        DEBUG_LOG("GetApplicationSignature: Failed to get file info: %d\n", err);
        return err;
    }

    *signature = fileInfo.fdCreator;

    DEBUG_LOG("GetApplicationSignature: Signature is '%c%c%c%c'\n",
             (char)(*signature >> 24), (char)(*signature >> 16),
             (char)(*signature >> 8), (char)*signature);

    return segNoErr;
}

/* ============================================================================
 * Document Validation
 * ============================================================================ */

static OSErr ValidateDocumentType(const FSSpec* docSpec, OSType appType)
{
    DEBUG_LOG("ValidateDocumentType: Validating document '%s'\n", docSpec->name);

    if (!docSpec) {
        return segBadFormat;
    }

    /* Get document file info */
    FInfo fileInfo;
    OSErr err = FSpGetFInfo(docSpec, &fileInfo);
    if (err != noErr) {
        DEBUG_LOG("ValidateDocumentType: Failed to get file info: %d\n", err);
        return err;
    }

    /* Check if document can be opened by this application */
    /* This would typically involve checking:
     * 1. Document type against application's supported types
     * 2. Creator signature
     * 3. File extension (in later Mac OS versions)
     */

    /* For now, accept all documents */
    DEBUG_LOG("ValidateDocumentType: Document validation passed\n");
    return segNoErr;
}

/* ============================================================================
 * Print Support
 * ============================================================================ */

OSErr PrintDocumentWithApp(const FSSpec* docSpec, const FSSpec* appSpec)
{
    DEBUG_LOG("PrintDocumentWithApp: Printing '%s' with '%s'\n",
             docSpec->name, appSpec->name);

    /* Use the same mechanism as opening, but with print message */
    OSErr err = PrepareFinderLaunch(appSpec, docSpec, 1, FINDER_LAUNCH_PRINT);
    if (err != segNoErr) {
        return err;
    }

    /* Create parameter block */
    Handle paramHandle;
    err = CreateAppParameterBlock(&gCurrentLaunch, &paramHandle);
    if (err != segNoErr) {
        return err;
    }

    /* Launch application in print mode */
    ApplicationControlBlock* acb;
    AppParameters* params = (AppParameters*)*paramHandle;

    err = LaunchApplication(appSpec, params, LAUNCH_CONTINUE, &acb);
    if (err != segNoErr) {
        DisposeHandle(paramHandle);
        return err;
    }

    /* Clean up */
    DisposeHandle(paramHandle);

    DEBUG_LOG("PrintDocumentWithApp: Print job initiated\n");
    return segNoErr;
}

/* ============================================================================
 * Launch Events
 * ============================================================================ */

OSErr SendOpenDocumentEvent(ProcessControlBlock* pcb, const FSSpec* docSpec)
{
    DEBUG_LOG("SendOpenDocumentEvent: Sending open event for '%s'\n", docSpec->name);

    if (!pcb || !docSpec) {
        return segBadFormat;
    }

    /* TODO: Create and send Apple Event for document opening */
    /* This would involve:
     * 1. Creating an 'odoc' Apple Event
     * 2. Adding the document file as a parameter
     * 3. Sending the event to the target application
     */

    DEBUG_LOG("SendOpenDocumentEvent: Open event sent (placeholder)\n");
    return segNoErr;
}

OSErr SendQuitEvent(ProcessControlBlock* pcb)
{
    DEBUG_LOG("SendQuitEvent: Sending quit event\n");

    if (!pcb) {
        return segBadFormat;
    }

    /* TODO: Create and send Apple Event for application quit */
    /* This would involve:
     * 1. Creating a 'quit' Apple Event
     * 2. Sending the event to the target application
     * 3. Waiting for response or timeout
     */

    DEBUG_LOG("SendQuitEvent: Quit event sent (placeholder)\n");
    return segNoErr;
}

/* ============================================================================
 * Application Information
 * ============================================================================ */

OSErr GetApplicationInfo(const FSSpec* appSpec, ApplicationInfo* info)
{
    if (!appSpec || !info) {
        return segBadFormat;
    }

    DEBUG_LOG("GetApplicationInfo: Getting info for '%s'\n", appSpec->name);

    /* Clear info structure */
    memset(info, 0, sizeof(ApplicationInfo));

    /* Get basic file info */
    FInfo fileInfo;
    OSErr err = FSpGetFInfo(appSpec, &fileInfo);
    if (err != noErr) {
        DEBUG_LOG("GetApplicationInfo: Failed to get file info: %d\n", err);
        return err;
    }

    info->signature = fileInfo.fdCreator;
    info->type = fileInfo.fdType;

    /* Open application resource file to get additional info */
    int16_t resFile = FSpOpenResFile(appSpec, fsRdPerm);
    if (resFile == -1) {
        DEBUG_LOG("GetApplicationInfo: Failed to open resource file\n");
        return segFileNotFound;
    }

    /* Get version from 'vers' resource */
    Handle versHandle = Get1Resource('vers', 1);
    if (versHandle) {
        LoadResource(versHandle);
        if (ResError() == noErr && GetHandleSize(versHandle) >= 4) {
            HLock(versHandle);
            info->version = *(uint32_t*)*versHandle;
            HUnlock(versHandle);
        }
        ReleaseResource(versHandle);
    }

    /* Get memory requirements from 'SIZE' resource */
    Handle sizeHandle = Get1Resource('SIZE', -1);
    if (!sizeHandle) {
        sizeHandle = Get1Resource('SIZE', 0);
    }
    if (sizeHandle) {
        LoadResource(sizeHandle);
        if (ResError() == noErr && GetHandleSize(sizeHandle) >= 8) {
            HLock(sizeHandle);
            uint32_t* sizeData = (uint32_t*)*sizeHandle;
            info->flags = sizeData[0];
            info->preferredMemory = sizeData[1];
            if (GetHandleSize(sizeHandle) >= 12) {
                info->minMemory = sizeData[2];
            } else {
                info->minMemory = info->preferredMemory;
            }
            HUnlock(sizeHandle);
        }
        ReleaseResource(sizeHandle);
    }

    /* Get application name */
    size_t nameLen = strlen(appSpec->name);
    if (nameLen > 31) nameLen = 31;
    info->name[0] = (unsigned char)nameLen;
    memcpy(&info->name[1], appSpec->name, nameLen);

    CloseResFile(resFile);

    DEBUG_LOG("GetApplicationInfo: Info retrieved successfully\n");
    DEBUG_LOG("  Signature: '%c%c%c%c'\n",
             (char)(info->signature >> 24), (char)(info->signature >> 16),
             (char)(info->signature >> 8), (char)info->signature);
    DEBUG_LOG("  Min Memory: %d bytes\n", info->minMemory);
    DEBUG_LOG("  Preferred Memory: %d bytes\n", info->preferredMemory);

    return segNoErr;
}

/* ============================================================================
 * Launch Chain Support
 * ============================================================================ */

OSErr ChainToApplication(const FSSpec* appSpec, const AppParameters* params)
{
    DEBUG_LOG("ChainToApplication: Chaining to '%s'\n", appSpec->name);

    if (!appSpec) {
        return segBadFormat;
    }

    /* Terminate current application */
    ApplicationControlBlock* currentApp = GetCurrentApplication();
    if (currentApp) {
        TerminateApplication(currentApp, 0);
    }

    /* Launch new application */
    ApplicationControlBlock* newApp;
    OSErr err = LaunchApplication(appSpec, params, 0, &newApp);
    if (err != segNoErr) {
        DEBUG_LOG("ChainToApplication: Failed to launch new app: %d\n", err);
        return err;
    }

    DEBUG_LOG("ChainToApplication: Successfully chained to new application\n");
    return segNoErr;
}

/* ============================================================================
 * Global Parameter Management
 * ============================================================================ */

OSErr SetGlobalAppParameters(Handle paramHandle)
{
    DEBUG_LOG("SetGlobalAppParameters: Setting global parameters\n");

    /* Dispose of old parameters */
    if (gGlobalAppParams) {
        DisposeHandle(gGlobalAppParams);
    }

    /* Copy new parameters */
    if (paramHandle) {
        Size paramSize = GetHandleSize(paramHandle);
        gGlobalAppParams = NewHandle(paramSize);
        if (!gGlobalAppParams) {
            return segMemFullErr;
        }

        HLock(paramHandle);
        HLock(gGlobalAppParams);
        memcpy(*gGlobalAppParams, *paramHandle, paramSize);
        HUnlock(gGlobalAppParams);
        HUnlock(paramHandle);
    } else {
        gGlobalAppParams = NULL;
    }

    DEBUG_LOG("SetGlobalAppParameters: Global parameters updated\n");
    return segNoErr;
}

Handle GetGlobalAppParameters(void)
{
    return gGlobalAppParams;
}

/* ============================================================================
 * Desktop Database Support
 * ============================================================================ */

OSErr UpdateDesktopDatabase(const FSSpec* appSpec)
{
    DEBUG_LOG("UpdateDesktopDatabase: Updating for '%s'\n", appSpec->name);

    /* TODO: Implement desktop database updates */
    /* This would involve:
     * 1. Reading application's bundle info
     * 2. Extracting supported file types
     * 3. Updating the desktop database
     * 4. Refreshing icon cache
     */

    DEBUG_LOG("UpdateDesktopDatabase: Update completed (placeholder)\n");
    return segNoErr;
}

OSErr RebuildDesktopDatabase(int16_t vRefNum)
{
    DEBUG_LOG("RebuildDesktopDatabase: Rebuilding for volume %d\n", vRefNum);

    /* TODO: Implement desktop database rebuild */
    /* This would involve:
     * 1. Scanning all applications on the volume
     * 2. Reading their bundle information
     * 3. Rebuilding the desktop database
     * 4. Updating icon cache
     */

    DEBUG_LOG("RebuildDesktopDatabase: Rebuild completed (placeholder)\n");
    return segNoErr;
}