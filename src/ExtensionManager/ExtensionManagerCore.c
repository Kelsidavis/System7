/*
 * ExtensionManagerCore.c - System 7.1 Portable Extension Manager Core
 *
 * Implements the core Extension Manager functionality including:
 * - Extension registry management
 * - Discovery and enumeration
 * - Loading and initialization
 * - Extension lifecycle management
 *
 * Extension types supported:
 * - INIT: System extensions (boot-time loaded)
 * - CDEF: Control definitions
 * - DRVR: Device drivers
 * - FKEY/WDEF/LDEF/MDEF: Definition resources
 */

#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "ExtensionManager/ExtensionManager.h"
#include "ExtensionManager/ExtensionTypes.h"
#include "ResourceMgr/ResourceMgr.h"
#include "MemoryMgr/MemoryManager.h"
#include "System/SystemLogging.h"
#include "EventManager/event_manager.h"

/* ========================================================================
 * GLOBAL STATE
 * ======================================================================== */

static ExtensionRegistry g_extensionRegistry = {0};
static Boolean g_extensionMgrInitialized = false;

/* Debug logging */
#define EXT_LOG(...) do { \
    if (g_extensionRegistry.debugMode) { \
        SYSTEM_LOG_DEBUG("[ExtMgr] " __VA_ARGS__); \
    } \
} while(0)

/* ========================================================================
 * INTERNAL FUNCTION PROTOTYPES
 * ======================================================================== */

static Extension* Extension_Allocate(void);
static void Extension_Free(Extension *ext);
static OSErr Extension_LoadCode(Extension *ext, Handle resourceHandle);
static void Extension_UnloadCode(Extension *ext);
static OSErr Extension_CallInitEntry(Extension *ext);

/* ========================================================================
 * EXTENSION MANAGER LIFECYCLE
 * ======================================================================== */

/**
 * Initialize the Extension Manager
 */
OSErr ExtensionManager_Initialize(void)
{
    if (g_extensionMgrInitialized) {
        return extNoErr;
    }

    /* Initialize registry */
    memset(&g_extensionRegistry, 0, sizeof(ExtensionRegistry));
    g_extensionRegistry.signature = EXTENSION_SIGNATURE;
    g_extensionRegistry.nextRefNum = 1;
    g_extensionRegistry.autoLoadEnabled = true;
    g_extensionRegistry.debugMode = false;

    SYSTEM_LOG_DEBUG("Extension Manager initialized\n");
    g_extensionMgrInitialized = true;

    return extNoErr;
}

/**
 * Shutdown Extension Manager
 */
void ExtensionManager_Shutdown(void)
{
    if (!g_extensionMgrInitialized) {
        return;
    }

    /* Unload all extensions in reverse order */
    Extension *current = g_extensionRegistry.lastExtension;
    while (current) {
        Extension *prev = current->prev;
        ExtensionManager_Unload(current->refNum);
        current = prev;
    }

    /* Clear registry */
    memset(&g_extensionRegistry, 0, sizeof(ExtensionRegistry));
    g_extensionMgrInitialized = false;

    SYSTEM_LOG_DEBUG("Extension Manager shutdown\n");
}

/**
 * Check if Extension Manager is initialized
 */
Boolean ExtensionManager_IsInitialized(void)
{
    return g_extensionMgrInitialized;
}

/* ========================================================================
 * EXTENSION DISCOVERY
 * ======================================================================== */

/**
 * Scan System Folder for extensions
 */
SInt16 ExtensionManager_ScanForExtensions(Boolean rescan)
{
    if (!g_extensionMgrInitialized) {
        return 0;
    }

    SInt16 discovered = 0;
    SInt16 initCount = CountResources(INIT_TYPE);

    EXT_LOG("Scanning for INIT resources, found %d\n", initCount);

    for (SInt16 i = 1; i <= initCount; i++) {
        Handle initResource = GetIndResource(INIT_TYPE, i);
        if (!initResource) {
            EXT_LOG("Failed to load INIT resource %d\n", i);
            continue;
        }

        /* Get resource info */
        ResID resourceID = 0;
        ResType resourceType = 0;
        char resourceName[256] = {0};
        GetResInfo(initResource, &resourceID, &resourceType, resourceName);

        /* Allocate extension record */
        Extension *ext = Extension_Allocate();
        if (!ext) {
            EXT_LOG("Failed to allocate extension record\n");
            continue;
        }

        /* Fill in extension info */
        ext->type = EXTTYPE_INIT;
        ext->state = EXT_STATE_DISCOVERED;
        ext->resourceType = INIT_TYPE;
        ext->resourceID = resourceID;
        ext->codeHandle = initResource;
        ext->codeSize = GetResourceSizeOnDisk(initResource);

        /* Extract name from resource name (truncate to MAX_EXTENSION_NAME) */
        strncpy(ext->name, resourceName, MAX_EXTENSION_NAME - 1);
        ext->name[MAX_EXTENSION_NAME - 1] = '\0';

        /* Default priority for discovered extensions (will be read from header) */
        ext->priority = INIT_PRIORITY_NORMAL;
        ext->flags = EXT_FLAG_ENABLED;

        /* Register in registry */
        SInt16 refNum = ExtensionManager_RegisterExtension(ext);
        if (refNum > 0) {
            discovered++;
            EXT_LOG("Discovered INIT %s (id=%d, size=%d)\n",
                    ext->name, resourceID, (int)ext->codeSize);
        } else {
            EXT_LOG("Failed to register extension %s\n", ext->name);
            Extension_Free(ext);
        }
    }

    EXT_LOG("Scanned for extensions, found %d\n", discovered);
    g_extensionRegistry.lastScanTime = TickCount();

    return discovered;
}

/**
 * Get count of discovered extensions
 */
SInt16 ExtensionManager_GetDiscoveredCount(void)
{
    if (!g_extensionMgrInitialized) {
        return 0;
    }

    SInt16 count = 0;
    Extension *current = g_extensionRegistry.firstExtension;
    while (current) {
        if (current->state == EXT_STATE_DISCOVERED) {
            count++;
        }
        current = current->next;
    }
    return count;
}

/**
 * Get count of active extensions
 */
SInt16 ExtensionManager_GetActiveCount(void)
{
    if (!g_extensionMgrInitialized) {
        return 0;
    }
    return g_extensionRegistry.activeCount;
}

/* ========================================================================
 * EXTENSION LOADING
 * ======================================================================== */

/**
 * Load and initialize all discovered extensions
 */
OSErr ExtensionManager_LoadAllExtensions(void)
{
    if (!g_extensionMgrInitialized) {
        return extNotFound;
    }

    OSErr firstError = extNoErr;

    /* Build array of extensions for sorting */
    Extension *extArray[MAX_EXTENSIONS];
    SInt16 extCount = 0;

    Extension *current = g_extensionRegistry.firstExtension;
    while (current && extCount < MAX_EXTENSIONS) {
        if (current->state == EXT_STATE_DISCOVERED) {
            extArray[extCount++] = current;
        }
        current = current->next;
    }

    /* Sort by priority using simple bubble sort */
    for (SInt16 i = 0; i < extCount - 1; i++) {
        for (SInt16 j = 0; j < extCount - i - 1; j++) {
            if (extArray[j]->priority > extArray[j + 1]->priority) {
                Extension *temp = extArray[j];
                extArray[j] = extArray[j + 1];
                extArray[j + 1] = temp;
            }
        }
    }

    /* Load each extension in priority order */
    for (SInt16 i = 0; i < extCount; i++) {
        Extension *ext = extArray[i];

        /* Skip disabled extensions */
        if (!(ext->flags & EXT_FLAG_ENABLED)) {
            EXT_LOG("Skipping disabled extension %s\n", ext->name);
            ext->state = EXT_STATE_DISABLED;
            continue;
        }

        /* Load code */
        OSErr err = Extension_LoadCode(ext, ext->codeHandle);
        if (err != extNoErr) {
            EXT_LOG("Failed to load code for %s (err=%d)\n", ext->name, err);
            ext->state = EXT_STATE_ERROR;
            ext->lastError = err;
            if (firstError == extNoErr) {
                firstError = err;
            }
            if (ext->flags & EXT_FLAG_REQUIRED) {
                break; /* Stop loading if required extension fails */
            }
            continue;
        }

        ext->state = EXT_STATE_LOADED;
        EXT_LOG("Loaded extension %s\n", ext->name);

        /* Call initialization entry point */
        err = Extension_CallInitEntry(ext);
        if (err != extNoErr) {
            EXT_LOG("Init entry point failed for %s (err=%d)\n", ext->name, err);
            ext->state = EXT_STATE_ERROR;
            ext->lastError = err;
            Extension_UnloadCode(ext);
            if (firstError == extNoErr) {
                firstError = err;
            }
            if (ext->flags & EXT_FLAG_REQUIRED) {
                break; /* Stop loading if required extension fails */
            }
            continue;
        }

        /* Mark as active */
        ext->state = EXT_STATE_ACTIVE;
        g_extensionRegistry.activeCount++;
        EXT_LOG("Activated extension %s\n", ext->name);
    }

    EXT_LOG("Loaded %d extensions\n", g_extensionRegistry.activeCount);
    return firstError;
}

/**
 * Load extension by name
 */
OSErr ExtensionManager_LoadByName(const char *name, SInt16 *outRefNum)
{
    if (!g_extensionMgrInitialized || !name) {
        return extNotFound;
    }

    Extension *ext = ExtensionManager_GetByName(name);
    if (!ext) {
        return extNotFound;
    }

    OSErr err = ExtensionManager_LoadByID(ext->resourceType, ext->resourceID, outRefNum);
    return err;
}

/**
 * Load extension by resource ID
 */
OSErr ExtensionManager_LoadByID(OSType resourceType, SInt16 resourceID,
                                 SInt16 *outRefNum)
{
    if (!g_extensionMgrInitialized) {
        return extNotFound;
    }

    /* Check if already loaded */
    Extension *current = g_extensionRegistry.firstExtension;
    while (current) {
        if (current->resourceType == resourceType && current->resourceID == resourceID) {
            if (outRefNum) {
                *outRefNum = current->refNum;
            }
            if (current->state == EXT_STATE_ACTIVE) {
                return extAlreadyLoaded;
            }
            break;
        }
        current = current->next;
    }

    /* Load resource from file */
    Handle resourceHandle = GetResource(resourceType, resourceID);
    if (!resourceHandle) {
        EXT_LOG("Failed to get resource type=%c%c%c%c id=%d\n",
                (char)(resourceType >> 24), (char)(resourceType >> 16),
                (char)(resourceType >> 8), (char)resourceType, resourceID);
        return extBadResource;
    }

    /* Load resource data into memory */
    LoadResource(resourceHandle);
    if (!*resourceHandle) {
        EXT_LOG("Failed to load resource data for type=%c%c%c%c id=%d\n",
                (char)(resourceType >> 24), (char)(resourceType >> 16),
                (char)(resourceType >> 8), (char)resourceType, resourceID);
        ReleaseResource(resourceHandle);
        return extBadResource;
    }

    /* Get resource info */
    ResID resID = 0;
    ResType resType = 0;
    char resName[256] = {0};
    GetResInfo(resourceHandle, &resID, &resType, resName);

    /* Create extension record */
    Extension *ext = Extension_Allocate();
    if (!ext) {
        ReleaseResource(resourceHandle);
        return extMemError;
    }

    /* Fill in extension info */
    ext->type = (resourceType == INIT_TYPE) ? EXTTYPE_INIT :
                (resourceType == CDEF_TYPE) ? EXTTYPE_CDEF :
                (resourceType == DRVR_TYPE) ? EXTTYPE_DRVR : EXTTYPE_UNKNOWN;
    ext->state = EXT_STATE_LOADED;
    ext->resourceType = resourceType;
    ext->resourceID = resourceID;
    ext->codeHandle = resourceHandle;
    ext->codeSize = GetResourceSizeOnDisk(resourceHandle);

    strncpy(ext->name, resName, MAX_EXTENSION_NAME - 1);
    ext->name[MAX_EXTENSION_NAME - 1] = '\0';

    ext->priority = INIT_PRIORITY_NORMAL;
    ext->flags = EXT_FLAG_ENABLED;

    /* Register in registry */
    SInt16 refNum = ExtensionManager_RegisterExtension(ext);
    if (refNum <= 0) {
        Extension_Free(ext);
        return extMaxExtensions;
    }

    /* Load code */
    OSErr err = Extension_LoadCode(ext, resourceHandle);
    if (err != extNoErr) {
        EXT_LOG("Failed to load code for resource id=%d\n", resourceID);
        ExtensionManager_Unload(refNum);
        return err;
    }

    /* Call initialization entry point */
    err = Extension_CallInitEntry(ext);
    if (err != extNoErr) {
        EXT_LOG("Init entry point failed for resource id=%d\n", resourceID);
        Extension_UnloadCode(ext);
        ExtensionManager_Unload(refNum);
        return extInitFailed;
    }

    ext->state = EXT_STATE_ACTIVE;
    g_extensionRegistry.activeCount++;
    EXT_LOG("Loaded extension by ID: type=%c%c%c%c id=%d\n",
            (char)(resourceType >> 24), (char)(resourceType >> 16),
            (char)(resourceType >> 8), (char)resourceType, resourceID);

    if (outRefNum) {
        *outRefNum = refNum;
    }

    return extNoErr;
}

/* ========================================================================
 * EXTENSION QUERIES
 * ======================================================================== */

/**
 * Get extension by reference number
 */
Extension* ExtensionManager_GetByRefNum(SInt16 refNum)
{
    if (!g_extensionMgrInitialized) {
        return NULL;
    }

    Extension *current = g_extensionRegistry.firstExtension;
    while (current) {
        if (current->refNum == refNum) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * Get extension by name
 */
Extension* ExtensionManager_GetByName(const char *name)
{
    if (!g_extensionMgrInitialized || !name) {
        return NULL;
    }

    Extension *current = g_extensionRegistry.firstExtension;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * Get first extension
 */
Extension* ExtensionManager_GetFirstExtension(void)
{
    if (!g_extensionMgrInitialized) {
        return NULL;
    }
    return g_extensionRegistry.firstExtension;
}

/**
 * Get next extension
 */
Extension* ExtensionManager_GetNextExtension(Extension *current)
{
    if (!g_extensionMgrInitialized || !current) {
        return NULL;
    }
    return current->next;
}

/* ========================================================================
 * EXTENSION CONTROL
 * ======================================================================== */

/**
 * Enable or disable extension
 */
OSErr ExtensionManager_SetEnabled(SInt16 refNum, Boolean enable)
{
    Extension *ext = ExtensionManager_GetByRefNum(refNum);
    if (!ext) {
        return extNotFound;
    }

    if (enable) {
        ext->flags |= EXT_FLAG_ENABLED;
    } else {
        ext->flags &= ~EXT_FLAG_ENABLED;
    }

    EXT_LOG("Extension %s %s\n", ext->name, enable ? "enabled" : "disabled");
    return extNoErr;
}

/**
 * Check if extension is enabled
 */
Boolean ExtensionManager_IsEnabled(SInt16 refNum)
{
    Extension *ext = ExtensionManager_GetByRefNum(refNum);
    if (!ext) {
        return false;
    }
    return (ext->flags & EXT_FLAG_ENABLED) != 0;
}

/**
 * Unload an extension
 */
OSErr ExtensionManager_Unload(SInt16 refNum)
{
    Extension *ext = ExtensionManager_GetByRefNum(refNum);
    if (!ext) {
        return extNotFound;
    }

    if (ext->state == EXT_STATE_ACTIVE) {
        g_extensionRegistry.activeCount--;
    }

    Extension_UnloadCode(ext);

    /* Remove from linked list */
    if (ext->prev) {
        ext->prev->next = ext->next;
    } else {
        g_extensionRegistry.firstExtension = ext->next;
    }

    if (ext->next) {
        ext->next->prev = ext->prev;
    } else {
        g_extensionRegistry.lastExtension = ext->prev;
    }

    EXT_LOG("Unloaded extension %s\n", ext->name);
    Extension_Free(ext);

    return extNoErr;
}

/**
 * Reload an extension
 */
OSErr ExtensionManager_Reload(SInt16 refNum)
{
    Extension *ext = ExtensionManager_GetByRefNum(refNum);
    if (!ext) {
        return extNotFound;
    }

    /* Unload and re-load */
    ExtensionManager_Unload(refNum);
    return ExtensionManager_LoadByID(ext->resourceType, ext->resourceID, NULL);
}

/* ========================================================================
 * EXTENSION INFORMATION
 * ======================================================================== */

/**
 * Get extension name
 */
OSErr ExtensionManager_GetName(SInt16 refNum, char *outName,
                                UInt16 *outNameLen)
{
    Extension *ext = ExtensionManager_GetByRefNum(refNum);
    if (!ext || !outName) {
        return extNotFound;
    }

    strncpy(outName, ext->name, MAX_EXTENSION_NAME - 1);
    outName[MAX_EXTENSION_NAME - 1] = '\0';

    if (outNameLen) {
        *outNameLen = strlen(outName);
    }

    return extNoErr;
}

/**
 * Get extension type
 */
ExtensionType ExtensionManager_GetType(SInt16 refNum)
{
    Extension *ext = ExtensionManager_GetByRefNum(refNum);
    if (!ext) {
        return EXTTYPE_UNKNOWN;
    }
    return ext->type;
}

/**
 * Get extension state
 */
ExtensionState ExtensionManager_GetState(SInt16 refNum)
{
    Extension *ext = ExtensionManager_GetByRefNum(refNum);
    if (!ext) {
        return EXT_STATE_INVALID;
    }
    return ext->state;
}

/**
 * Get extension code size
 */
SInt32 ExtensionManager_GetCodeSize(SInt16 refNum)
{
    Extension *ext = ExtensionManager_GetByRefNum(refNum);
    if (!ext) {
        return 0;
    }
    return ext->codeSize;
}

/**
 * Get extension version
 */
OSErr ExtensionManager_GetVersion(SInt16 refNum, SInt16 *outMajor,
                                   SInt16 *outMinor)
{
    Extension *ext = ExtensionManager_GetByRefNum(refNum);
    if (!ext) {
        return extNotFound;
    }

    if (outMajor) {
        *outMajor = ext->majorVersion;
    }
    if (outMinor) {
        *outMinor = ext->minorVersion;
    }

    return extNoErr;
}

/* ========================================================================
 * STATISTICS AND DEBUGGING
 * ======================================================================== */

/**
 * Get total memory used
 */
SInt32 ExtensionManager_GetTotalMemoryUsed(void)
{
    if (!g_extensionMgrInitialized) {
        return 0;
    }

    SInt32 total = 0;
    Extension *current = g_extensionRegistry.firstExtension;
    while (current) {
        total += current->codeSize;
        current = current->next;
    }
    return total;
}

/**
 * Get load statistics
 */
OSErr ExtensionManager_GetLoadStatistics(SInt32 *outLoadTime,
                                         SInt32 *outInitTime)
{
    if (!g_extensionMgrInitialized) {
        return extNotFound;
    }

    if (outLoadTime) {
        *outLoadTime = g_extensionRegistry.totalLoadTime;
    }
    if (outInitTime) {
        *outInitTime = g_extensionRegistry.totalInitTime;
    }

    return extNoErr;
}

/**
 * Dump registry (debug)
 */
void ExtensionManager_DumpRegistry(void)
{
    if (!g_extensionMgrInitialized) {
        SYSTEM_LOG_DEBUG("Extension Manager not initialized\n");
        return;
    }

    SYSTEM_LOG_DEBUG("\n========== EXTENSION REGISTRY DUMP ==========\n");
    SYSTEM_LOG_DEBUG("Total Extensions: %d, Active: %d\n",
                     g_extensionRegistry.extensionCount,
                     g_extensionRegistry.activeCount);

    Extension *current = g_extensionRegistry.firstExtension;
    int index = 1;
    while (current) {
        SYSTEM_LOG_DEBUG("%d. %s (ref=%d, type=%d, state=%d)\n",
                         index++, current->name, current->refNum,
                         current->type, current->state);
        current = current->next;
    }
    SYSTEM_LOG_DEBUG("=============================================\n\n");
}

/**
 * Set debug mode
 */
void ExtensionManager_SetDebugMode(Boolean enable)
{
    if (g_extensionMgrInitialized) {
        g_extensionRegistry.debugMode = enable;
    }
}

/* ========================================================================
 * INTERNAL FUNCTIONS
 * ======================================================================== */

/**
 * Get registry (internal)
 */
ExtensionRegistry* ExtensionManager_GetRegistry(void)
{
    return g_extensionMgrInitialized ? &g_extensionRegistry : NULL;
}

/**
 * Register extension in registry (internal)
 */
SInt16 ExtensionManager_RegisterExtension(Extension *extension)
{
    if (!g_extensionMgrInitialized || !extension) {
        return 0;
    }

    if (g_extensionRegistry.extensionCount >= MAX_EXTENSIONS) {
        return extMaxExtensions;
    }

    /* Assign reference number */
    extension->refNum = g_extensionRegistry.nextRefNum++;

    /* Add to linked list */
    if (!g_extensionRegistry.firstExtension) {
        g_extensionRegistry.firstExtension = extension;
        g_extensionRegistry.lastExtension = extension;
    } else {
        extension->prev = g_extensionRegistry.lastExtension;
        g_extensionRegistry.lastExtension->next = extension;
        g_extensionRegistry.lastExtension = extension;
    }

    g_extensionRegistry.extensionCount++;

    EXT_LOG("Registered extension %s (refNum=%d)\n",
            extension->name, extension->refNum);

    return extension->refNum;
}

/* ========================================================================
 * EXTENSION MEMORY MANAGEMENT
 * ======================================================================== */

/**
 * Allocate extension record
 */
static Extension* Extension_Allocate(void)
{
    Extension *ext = NewPtr(sizeof(Extension));
    if (ext) {
        memset(ext, 0, sizeof(Extension));
    }
    return ext;
}

/**
 * Free extension record
 */
static void Extension_Free(Extension *ext)
{
    if (!ext) {
        return;
    }

    if (ext->codeHandle) {
        DisposeHandle(ext->codeHandle);
    }

    DisposePtr((Ptr)ext);
}

/**
 * Load extension code
 */
static OSErr Extension_LoadCode(Extension *ext, Handle resourceHandle)
{
    if (!ext || !resourceHandle) {
        return extBadResource;
    }

    /* Lock the handle to make code location fixed in memory */
    HLock(resourceHandle);

    /* Get pointer to code */
    Ptr codePtr = *resourceHandle;
    if (!codePtr) {
        HUnlock(resourceHandle);
        return extMemError;
    }

    /* Store code pointer in extension record */
    ext->codeHandle = resourceHandle;

    EXT_LOG("Loaded code for extension %s (size=%d)\n", ext->name, (int)ext->codeSize);
    return extNoErr;
}

/**
 * Unload extension code
 */
static void Extension_UnloadCode(Extension *ext)
{
    if (!ext || !ext->codeHandle) {
        return;
    }

    /* Unlock the handle */
    HUnlock(ext->codeHandle);

    EXT_LOG("Unloaded code for extension %s\n", ext->name);
}

/**
 * Call init entry point
 */
static OSErr Extension_CallInitEntry(Extension *ext)
{
    if (!ext || !ext->codeHandle) {
        return extBadResource;
    }

    /* Get pointer to code */
    Ptr codePtr = *ext->codeHandle;
    if (!codePtr) {
        return extMemError;
    }

    /* INIT resources contain executable code that we need to execute
     * The entry point is typically at the start of the resource
     * (or offset from the start depending on resource format)
     */

    /* Cast to function pointer (returns OSErr) */
    InitEntryProc initEntry = (InitEntryProc)codePtr;

    /* Record start time for statistics */
    SInt32 startTime = TickCount();

    /* Call extension's initialization entry point
     * In a real system, we'd need proper exception handling here
     * For now, we trust the extension to behave correctly
     */
    OSErr err = (*initEntry)();

    /* Record end time */
    SInt32 endTime = TickCount();
    ext->initTime = endTime - startTime;

    if (err != extNoErr) {
        EXT_LOG("Extension %s init entry returned error %d\n", ext->name, err);
        return err;
    }

    EXT_LOG("Extension %s initialized successfully (time=%d ticks)\n",
            ext->name, (int)ext->initTime);

    return extNoErr;
}

