/*
 * FKEYLoader.c - FKEY (Function Key) Resource Loader Implementation
 *
 * Loads FKEY resources from System file for extended keyboard support
 */

#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "ExtensionManager/FKEYLoader.h"
#include "ExtensionManager/ExtensionTypes.h"
#include "ResourceMgr/ResourceMgr.h"
#include "MemoryMgr/MemoryManager.h"
#include "System/SystemLogging.h"

/* ========================================================================
 * GLOBAL STATE
 * ======================================================================== */

static Boolean g_fkeyLoaderInitialized = false;
static FKEYEntry *g_firstFKEY = NULL;
static FKEYEntry *g_lastFKEY = NULL;
static SInt16 g_fkeyCount = 0;

/* Debug logging macro */
#define FKEY_LOG(...) do { \
    SYSTEM_LOG_DEBUG("[FKEYLoader] " __VA_ARGS__); \
} while(0)

/* ========================================================================
 * INTERNAL FUNCTION PROTOTYPES
 * ======================================================================== */

static FKEYEntry* FKEYEntry_Allocate(void);
static void FKEYEntry_Free(FKEYEntry *entry);
static OSErr FKEYEntry_LoadCode(FKEYEntry *entry, Handle resourceHandle);
static void FKEYEntry_UnloadCode(FKEYEntry *entry);

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

/**
 * Initialize FKEY Loader subsystem
 */
OSErr FKEYLoader_Initialize(void)
{
    if (g_fkeyLoaderInitialized) {
        return extNoErr;
    }

    g_firstFKEY = NULL;
    g_lastFKEY = NULL;
    g_fkeyCount = 0;

    FKEY_LOG("FKEY Loader initialized\n");
    g_fkeyLoaderInitialized = true;

    return extNoErr;
}

/**
 * Shutdown FKEY Loader
 */
void FKEYLoader_Shutdown(void)
{
    if (!g_fkeyLoaderInitialized) {
        return;
    }

    /* Unload all FKEYs in reverse order */
    FKEYEntry *current = g_lastFKEY;
    while (current) {
        FKEYEntry *prev = current->prev;
        FKEYLoader_UnloadFKEY(current->resourceID);
        current = prev;
    }

    g_fkeyLoaderInitialized = false;
    FKEY_LOG("FKEY Loader shutdown\n");
}

/**
 * Load an FKEY resource
 */
OSErr FKEYLoader_LoadFKEY(SInt16 resourceID)
{
    if (!g_fkeyLoaderInitialized) {
        return extNotFound;
    }

    /* Check if already loaded */
    FKEYEntry *existing = FKEYLoader_GetByResourceID(resourceID);
    if (existing) {
        return extAlreadyLoaded;
    }

    /* Load resource from file */
    Handle resourceHandle = GetResource(FKEY_TYPE, resourceID);
    if (!resourceHandle) {
        FKEY_LOG("Failed to get FKEY resource %d\n", resourceID);
        return extBadResource;
    }

    /* Load resource data into memory */
    LoadResource(resourceHandle);
    if (!*resourceHandle) {
        FKEY_LOG("Failed to load FKEY resource %d into memory\n", resourceID);
        ReleaseResource(resourceHandle);
        return extBadResource;
    }

    /* Get resource info */
    ResID resID = 0;
    ResType resType = 0;
    char resName[256] = {0};
    GetResInfo(resourceHandle, &resID, &resType, resName);

    /* Allocate FKEY entry */
    FKEYEntry *entry = FKEYEntry_Allocate();
    if (!entry) {
        ReleaseResource(resourceHandle);
        return extMemError;
    }

    /* Fill in entry info */
    entry->resourceID = resourceID;
    entry->resourceHandle = resourceHandle;
    entry->resourceSize = GetResourceSizeOnDisk(resourceHandle);
    entry->loaded = false;
    entry->active = false;

    strncpy(entry->name, resName, MAX_FKEY_NAME - 1);
    entry->name[MAX_FKEY_NAME - 1] = '\0';

    /* Load code */
    OSErr err = FKEYEntry_LoadCode(entry, resourceHandle);
    if (err != extNoErr) {
        FKEY_LOG("Failed to load code for FKEY %d\n", resourceID);
        FKEYEntry_Free(entry);
        return err;
    }

    entry->loaded = true;
    entry->active = true;

    /* Add to linked list */
    if (!g_firstFKEY) {
        g_firstFKEY = entry;
        g_lastFKEY = entry;
    } else {
        entry->prev = g_lastFKEY;
        g_lastFKEY->next = entry;
        g_lastFKEY = entry;
    }

    g_fkeyCount++;

    FKEY_LOG("Loaded FKEY %s (id=%d, size=%d)\n",
            entry->name, resourceID, (int)entry->resourceSize);

    return extNoErr;
}

/**
 * Load all FKEY resources from System file
 */
SInt16 FKEYLoader_LoadAllFKEYs(void)
{
    if (!g_fkeyLoaderInitialized) {
        return 0;
    }

    SInt16 loaded = 0;
    SInt16 fkeyCount = CountResources(FKEY_TYPE);

    FKEY_LOG("Scanning for FKEY resources, found %d\n", fkeyCount);

    for (SInt16 i = 1; i <= fkeyCount; i++) {
        Handle fkeyResource = GetIndResource(FKEY_TYPE, i);
        if (!fkeyResource) {
            continue;
        }

        /* Get resource ID */
        ResID resID = 0;
        ResType resType = 0;
        char resName[256] = {0};
        GetResInfo(fkeyResource, &resID, &resType, resName);

        /* Try to load this FKEY */
        OSErr err = FKEYLoader_LoadFKEY(resID);
        if (err == extNoErr) {
            loaded++;
        } else {
            FKEY_LOG("Failed to load FKEY %d: error %d\n", resID, err);
        }
    }

    FKEY_LOG("Loaded %d FKEYs\n", loaded);
    return loaded;
}

/**
 * Get FKEY entry by resource ID
 */
FKEYEntry* FKEYLoader_GetByResourceID(SInt16 resourceID)
{
    if (!g_fkeyLoaderInitialized) {
        return NULL;
    }

    FKEYEntry *current = g_firstFKEY;
    while (current) {
        if (current->resourceID == resourceID) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Unload an FKEY
 */
OSErr FKEYLoader_UnloadFKEY(SInt16 resourceID)
{
    FKEYEntry *entry = FKEYLoader_GetByResourceID(resourceID);
    if (!entry) {
        return extNotFound;
    }

    if (entry->loaded) {
        FKEYEntry_UnloadCode(entry);
        entry->loaded = false;
    }

    /* Remove from linked list */
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        g_firstFKEY = entry->next;
    }

    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        g_lastFKEY = entry->prev;
    }

    FKEY_LOG("Unloaded FKEY %s\n", entry->name);

    FKEYEntry_Free(entry);
    g_fkeyCount--;

    return extNoErr;
}

/**
 * Get count of loaded FKEYs
 */
SInt16 FKEYLoader_GetLoadedCount(void)
{
    if (!g_fkeyLoaderInitialized) {
        return 0;
    }
    return g_fkeyCount;
}

/**
 * Dump FKEY registry (debug)
 */
void FKEYLoader_DumpRegistry(void)
{
    if (!g_fkeyLoaderInitialized) {
        SYSTEM_LOG_DEBUG("FKEY Loader not initialized\n");
        return;
    }

    SYSTEM_LOG_DEBUG("\n========== FKEY REGISTRY DUMP ==========\n");
    SYSTEM_LOG_DEBUG("Total FKEYs: %d\n", g_fkeyCount);

    FKEYEntry *current = g_firstFKEY;
    int index = 1;
    while (current) {
        SYSTEM_LOG_DEBUG("%d. %s (id=%d, loaded=%s)\n",
                        index++, current->name, current->resourceID,
                        current->loaded ? "yes" : "no");
        current = current->next;
    }
    SYSTEM_LOG_DEBUG("=======================================\n\n");
}

/**
 * Check if FKEY Loader is initialized
 */
Boolean FKEYLoader_IsInitialized(void)
{
    return g_fkeyLoaderInitialized;
}

/* ========================================================================
 * INTERNAL FUNCTIONS
 * ======================================================================== */

/**
 * Allocate FKEY entry
 */
static FKEYEntry* FKEYEntry_Allocate(void)
{
    FKEYEntry *entry = NewPtr(sizeof(FKEYEntry));
    if (entry) {
        memset(entry, 0, sizeof(FKEYEntry));
    }
    return entry;
}

/**
 * Free FKEY entry
 */
static void FKEYEntry_Free(FKEYEntry *entry)
{
    if (!entry) {
        return;
    }

    if (entry->resourceHandle && entry->loaded) {
        HUnlock(entry->resourceHandle);
    }

    DisposePtr((Ptr)entry);
}

/**
 * Load FKEY code
 */
static OSErr FKEYEntry_LoadCode(FKEYEntry *entry, Handle resourceHandle)
{
    if (!entry || !resourceHandle) {
        return extBadResource;
    }

    /* Lock the handle */
    HLock(resourceHandle);

    /* Get pointer to code */
    Ptr codePtr = *resourceHandle;
    if (!codePtr) {
        HUnlock(resourceHandle);
        return extMemError;
    }

    /* Store entry point */
    entry->fkeyProc = codePtr;

    FKEY_LOG("Loaded code for FKEY %s\n", entry->name);
    return extNoErr;
}

/**
 * Unload FKEY code
 */
static void FKEYEntry_UnloadCode(FKEYEntry *entry)
{
    if (!entry || !entry->resourceHandle) {
        return;
    }

    /* Unlock the handle */
    HUnlock(entry->resourceHandle);

    entry->fkeyProc = NULL;
    FKEY_LOG("Unloaded code for FKEY %s\n", entry->name);
}
