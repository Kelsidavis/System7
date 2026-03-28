/*
 * CDEFLoader.c - CDEF (Control Definition) Resource Loader Implementation
 *
 * Loads CDEF resources from System file and registers them with the ControlManager
 */

#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "ExtensionManager/CDEFLoader.h"
#include "ResourceMgr/ResourceMgr.h"
#include "MemoryMgr/MemoryManager.h"
#include "ControlManager/ControlManager.h"
#include "System/SystemLogging.h"

/* ========================================================================
 * GLOBAL STATE
 * ======================================================================== */

static Boolean g_cdefLoaderInitialized = false;
static CDEFEntry *g_firstCDEF = NULL;
static CDEFEntry *g_lastCDEF = NULL;
static SInt16 g_cdefCount = 0;
static SInt16 g_nextProcID = 100;  /* Start custom proc IDs at 100 */

/* Debug logging macro */
#define CDEF_LOG(...) do { \
    SYSTEM_LOG_DEBUG("[CDEFLoader] " __VA_ARGS__); \
} while(0)

/* ========================================================================
 * INTERNAL FUNCTION PROTOTYPES
 * ======================================================================== */

static CDEFEntry* CDEFEntry_Allocate(void);
static void CDEFEntry_Free(CDEFEntry *entry);
static OSErr CDEFEntry_LoadCode(CDEFEntry *entry, Handle resourceHandle);
static void CDEFEntry_UnloadCode(CDEFEntry *entry);
static OSErr CDEFEntry_RegisterWithControlManager(CDEFEntry *entry);

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

/**
 * Initialize CDEF Loader subsystem
 */
OSErr CDEFLoader_Initialize(void)
{
    if (g_cdefLoaderInitialized) {
        return extNoErr;
    }

    g_firstCDEF = NULL;
    g_lastCDEF = NULL;
    g_cdefCount = 0;
    g_nextProcID = 100;

    CDEF_LOG("CDEF Loader initialized\n");
    g_cdefLoaderInitialized = true;

    return extNoErr;
}

/**
 * Shutdown CDEF Loader (unload all registered CDEFs)
 */
void CDEFLoader_Shutdown(void)
{
    if (!g_cdefLoaderInitialized) {
        return;
    }

    /* Unload all CDEFs in reverse order */
    CDEFEntry *current = g_lastCDEF;
    while (current) {
        CDEFEntry *prev = current->prev;
        CDEFLoader_UnloadCDEF(current->procID);
        current = prev;
    }

    g_cdefLoaderInitialized = false;
    CDEF_LOG("CDEF Loader shutdown\n");
}

/**
 * Load a CDEF resource and register with ControlManager
 */
OSErr CDEFLoader_LoadCDEF(SInt16 resourceID, SInt16 *outProcID)
{
    if (!g_cdefLoaderInitialized) {
        return extNotFound;
    }

    /* Check if already loaded */
    CDEFEntry *existing = CDEFLoader_GetByResourceID(resourceID);
    if (existing) {
        if (outProcID) {
            *outProcID = existing->procID;
        }
        return extAlreadyLoaded;
    }

    /* Load resource from file */
    Handle resourceHandle = GetResource(CDEF_TYPE, resourceID);
    if (!resourceHandle) {
        CDEF_LOG("Failed to get CDEF resource %d\n", resourceID);
        return extBadResource;
    }

    /* Load resource data into memory */
    LoadResource(resourceHandle);
    if (!*resourceHandle) {
        CDEF_LOG("Failed to load CDEF resource %d into memory\n", resourceID);
        ReleaseResource(resourceHandle);
        return extBadResource;
    }

    /* Get resource info */
    ResID resID = 0;
    ResType resType = 0;
    char resName[256] = {0};
    GetResInfo(resourceHandle, &resID, &resType, resName);

    /* Allocate CDEF entry */
    CDEFEntry *entry = CDEFEntry_Allocate();
    if (!entry) {
        ReleaseResource(resourceHandle);
        return extMemError;
    }

    /* Fill in entry info */
    entry->resourceID = resourceID;
    entry->resourceHandle = resourceHandle;
    entry->resourceSize = GetResourceSizeOnDisk(resourceHandle);
    entry->procID = g_nextProcID++;
    entry->registered = false;

    strncpy(entry->name, resName, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';

    /* Load code */
    OSErr err = CDEFEntry_LoadCode(entry, resourceHandle);
    if (err != extNoErr) {
        CDEF_LOG("Failed to load code for CDEF %d\n", resourceID);
        CDEFEntry_Free(entry);
        return err;
    }

    /* Register with ControlManager */
    err = CDEFEntry_RegisterWithControlManager(entry);
    if (err != extNoErr) {
        CDEF_LOG("Failed to register CDEF %d with ControlManager\n", resourceID);
        CDEFEntry_UnloadCode(entry);
        CDEFEntry_Free(entry);
        return err;
    }

    /* Add to linked list */
    if (!g_firstCDEF) {
        g_firstCDEF = entry;
        g_lastCDEF = entry;
    } else {
        entry->prev = g_lastCDEF;
        g_lastCDEF->next = entry;
        g_lastCDEF = entry;
    }

    g_cdefCount++;

    CDEF_LOG("Loaded CDEF %s (id=%d, procID=%d, size=%d)\n",
            entry->name, resourceID, entry->procID, (int)entry->resourceSize);

    if (outProcID) {
        *outProcID = entry->procID;
    }

    return extNoErr;
}

/**
 * Load all CDEF resources from System file
 */
SInt16 CDEFLoader_LoadAllCDEFs(void)
{
    if (!g_cdefLoaderInitialized) {
        return 0;
    }

    SInt16 loaded = 0;
    SInt16 cdefCount = CountResources(CDEF_TYPE);

    CDEF_LOG("Scanning for CDEF resources, found %d\n", cdefCount);

    for (SInt16 i = 1; i <= cdefCount; i++) {
        Handle cdefResource = GetIndResource(CDEF_TYPE, i);
        if (!cdefResource) {
            continue;
        }

        /* Get resource ID */
        ResID resID = 0;
        ResType resType = 0;
        char resName[256] = {0};
        GetResInfo(cdefResource, &resID, &resType, resName);

        /* Try to load this CDEF */
        SInt16 procID = 0;
        OSErr err = CDEFLoader_LoadCDEF(resID, &procID);
        if (err == extNoErr) {
            loaded++;
        } else {
            CDEF_LOG("Failed to load CDEF %d: error %d\n", resID, err);
        }
    }

    CDEF_LOG("Loaded %d CDEFs\n", loaded);
    return loaded;
}

/**
 * Get CDEF entry by proc ID
 */
CDEFEntry* CDEFLoader_GetByProcID(SInt16 procID)
{
    if (!g_cdefLoaderInitialized) {
        return NULL;
    }

    CDEFEntry *current = g_firstCDEF;
    while (current) {
        if (current->procID == procID) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Get CDEF entry by resource ID
 */
CDEFEntry* CDEFLoader_GetByResourceID(SInt16 resourceID)
{
    if (!g_cdefLoaderInitialized) {
        return NULL;
    }

    CDEFEntry *current = g_firstCDEF;
    while (current) {
        if (current->resourceID == resourceID) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Unload a CDEF and unregister from ControlManager
 */
OSErr CDEFLoader_UnloadCDEF(SInt16 procID)
{
    CDEFEntry *entry = CDEFLoader_GetByProcID(procID);
    if (!entry) {
        return extNotFound;
    }

    /* Unload code */
    CDEFEntry_UnloadCode(entry);

    /* Remove from linked list */
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        g_firstCDEF = entry->next;
    }

    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        g_lastCDEF = entry->prev;
    }

    CDEF_LOG("Unloaded CDEF %s (procID=%d)\n", entry->name, procID);

    CDEFEntry_Free(entry);
    g_cdefCount--;

    return extNoErr;
}

/**
 * Get count of loaded CDEFs
 */
SInt16 CDEFLoader_GetLoadedCount(void)
{
    if (!g_cdefLoaderInitialized) {
        return 0;
    }
    return g_cdefCount;
}

/**
 * Dump CDEF registry (debug)
 */
void CDEFLoader_DumpRegistry(void)
{
    if (!g_cdefLoaderInitialized) {
        SYSTEM_LOG_DEBUG("CDEF Loader not initialized\n");
        return;
    }

    SYSTEM_LOG_DEBUG("\n========== CDEF REGISTRY DUMP ==========\n");
    SYSTEM_LOG_DEBUG("Total CDEFs: %d\n", g_cdefCount);

    CDEFEntry *current = g_firstCDEF;
    int index = 1;
    while (current) {
        SYSTEM_LOG_DEBUG("%d. %s (id=%d, procID=%d, registered=%s)\n",
                        index++, current->name, current->resourceID,
                        current->procID, current->registered ? "yes" : "no");
        current = current->next;
    }
    SYSTEM_LOG_DEBUG("=======================================\n\n");
}

/**
 * Check if CDEF Loader is initialized
 */
Boolean CDEFLoader_IsInitialized(void)
{
    return g_cdefLoaderInitialized;
}

/* ========================================================================
 * INTERNAL FUNCTIONS
 * ======================================================================== */

/**
 * Allocate CDEF entry
 */
static CDEFEntry* CDEFEntry_Allocate(void)
{
    CDEFEntry *entry = NewPtr(sizeof(CDEFEntry));
    if (entry) {
        memset(entry, 0, sizeof(CDEFEntry));
    }
    return entry;
}

/**
 * Free CDEF entry
 */
static void CDEFEntry_Free(CDEFEntry *entry)
{
    if (!entry) {
        return;
    }

    if (entry->resourceHandle) {
        DisposeHandle(entry->resourceHandle);
    }

    DisposePtr((Ptr)entry);
}

/**
 * Load CDEF code
 */
static OSErr CDEFEntry_LoadCode(CDEFEntry *entry, Handle resourceHandle)
{
    if (!entry || !resourceHandle) {
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

    /* CDEF resources contain a routine descriptor or direct code
     * For now, assume it's a direct function pointer to the CDEF procedure
     * The entry point is at offset 0 in the resource
     */
    entry->defProc = (ControlDefProcPtr)codePtr;

    CDEF_LOG("Loaded code for CDEF %s\n", entry->name);
    return extNoErr;
}

/**
 * Unload CDEF code
 */
static void CDEFEntry_UnloadCode(CDEFEntry *entry)
{
    if (!entry || !entry->resourceHandle) {
        return;
    }

    /* Unlock the handle */
    HUnlock(entry->resourceHandle);

    entry->defProc = NULL;
    CDEF_LOG("Unloaded code for CDEF %s\n", entry->name);
}

/**
 * Register CDEF with ControlManager
 */
static OSErr CDEFEntry_RegisterWithControlManager(CDEFEntry *entry)
{
    if (!entry || !entry->defProc) {
        return extBadResource;
    }

    /* Register control definition procedure with ControlManager */
    RegisterControlType(entry->procID, entry->defProc);

    entry->registered = true;
    CDEF_LOG("Registered CDEF %s with ControlManager (procID=%d)\n",
            entry->name, entry->procID);

    return extNoErr;
}
