/*
 * DefLoader.c - Definition Resource Loader Implementation
 *
 * Loads WDEF, LDEF, and MDEF resources from System file
 */

#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "ExtensionManager/DefLoader.h"
#include "ExtensionManager/ExtensionTypes.h"
#include "ResourceMgr/ResourceMgr.h"
#include "MemoryMgr/MemoryManager.h"
#include "System/SystemLogging.h"

/* ========================================================================
 * GLOBAL STATE
 * ======================================================================== */

static Boolean g_defLoaderInitialized = false;
static DefEntry *g_firstDef = NULL;
static DefEntry *g_lastDef = NULL;
static SInt16 g_defCount = 0;
static SInt16 g_wdefCount = 0;
static SInt16 g_ldefCount = 0;
static SInt16 g_mdefCount = 0;

/* Debug logging macro */
#define DEF_LOG(...) do { \
    SYSTEM_LOG_DEBUG("[DefLoader] " __VA_ARGS__); \
} while(0)

/* ========================================================================
 * INTERNAL FUNCTION PROTOTYPES
 * ======================================================================== */

static DefEntry* DefEntry_Allocate(void);
static void DefEntry_Free(DefEntry *entry);
static OSErr DefEntry_LoadCode(DefEntry *entry, Handle resourceHandle);
static void DefEntry_UnloadCode(DefEntry *entry);
static const char* DefTypeToString(ResType defType);

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

/**
 * Initialize Definition Loader subsystem
 */
OSErr DefLoader_Initialize(void)
{
    if (g_defLoaderInitialized) {
        return extNoErr;
    }

    g_firstDef = NULL;
    g_lastDef = NULL;
    g_defCount = 0;
    g_wdefCount = 0;
    g_ldefCount = 0;
    g_mdefCount = 0;

    DEF_LOG("Definition Loader initialized\n");
    g_defLoaderInitialized = true;

    return extNoErr;
}

/**
 * Shutdown Definition Loader
 */
void DefLoader_Shutdown(void)
{
    if (!g_defLoaderInitialized) {
        return;
    }

    /* Unload all definitions in reverse order */
    DefEntry *current = g_lastDef;
    while (current) {
        DefEntry *prev = current->prev;
        DefLoader_UnloadDefinition(current->defType, current->resourceID);
        current = prev;
    }

    g_defLoaderInitialized = false;
    DEF_LOG("Definition Loader shutdown\n");
}

/**
 * Load a definition resource
 */
OSErr DefLoader_LoadDefinition(ResType defType, SInt16 resourceID)
{
    if (!g_defLoaderInitialized) {
        return extNotFound;
    }

    /* Validate definition type */
    if (defType != WDEF_TYPE && defType != LDEF_TYPE && defType != MDEF_TYPE) {
        return extBadResource;
    }

    /* Check if already loaded */
    DefEntry *existing = DefLoader_GetByResourceID(defType, resourceID);
    if (existing) {
        return extAlreadyLoaded;
    }

    /* Load resource from file */
    Handle resourceHandle = GetResource(defType, resourceID);
    if (!resourceHandle) {
        DEF_LOG("Failed to get %s resource %d\n", DefTypeToString(defType), resourceID);
        return extBadResource;
    }

    /* Load resource data into memory */
    LoadResource(resourceHandle);
    if (!*resourceHandle) {
        DEF_LOG("Failed to load %s resource %d into memory\n", DefTypeToString(defType), resourceID);
        ReleaseResource(resourceHandle);
        return extBadResource;
    }

    /* Get resource info */
    ResID resID = 0;
    ResType resType = 0;
    char resName[256] = {0};
    GetResInfo(resourceHandle, &resID, &resType, resName);

    /* Allocate definition entry */
    DefEntry *entry = DefEntry_Allocate();
    if (!entry) {
        ReleaseResource(resourceHandle);
        return extMemError;
    }

    /* Fill in entry info */
    entry->defType = defType;
    entry->resourceID = resourceID;
    entry->defID = resourceID;  /* Use resource ID as definition ID */
    entry->resourceHandle = resourceHandle;
    entry->resourceSize = GetResourceSizeOnDisk(resourceHandle);
    entry->loaded = false;
    entry->registered = false;

    strncpy(entry->name, resName, MAX_DEF_NAME - 1);
    entry->name[MAX_DEF_NAME - 1] = '\0';

    /* Load code */
    OSErr err = DefEntry_LoadCode(entry, resourceHandle);
    if (err != extNoErr) {
        DEF_LOG("Failed to load code for %s %d\n", DefTypeToString(defType), resourceID);
        DefEntry_Free(entry);
        return err;
    }

    entry->loaded = true;

    /* Add to linked list */
    if (!g_firstDef) {
        g_firstDef = entry;
        g_lastDef = entry;
    } else {
        entry->prev = g_lastDef;
        g_lastDef->next = entry;
        g_lastDef = entry;
    }

    g_defCount++;

    /* Increment type-specific counter */
    if (defType == WDEF_TYPE) {
        g_wdefCount++;
    } else if (defType == LDEF_TYPE) {
        g_ldefCount++;
    } else if (defType == MDEF_TYPE) {
        g_mdefCount++;
    }

    DEF_LOG("Loaded %s %s (id=%d, size=%d)\n",
            DefTypeToString(defType), entry->name, resourceID, (int)entry->resourceSize);

    return extNoErr;
}

/**
 * Load all definition resources of a specific type
 */
SInt16 DefLoader_LoadAllDefinitions(ResType defType)
{
    if (!g_defLoaderInitialized) {
        return 0;
    }

    SInt16 loaded = 0;
    SInt16 defCount = CountResources(defType);

    DEF_LOG("Scanning for %s resources, found %d\n", DefTypeToString(defType), defCount);

    for (SInt16 i = 1; i <= defCount; i++) {
        Handle defResource = GetIndResource(defType, i);
        if (!defResource) {
            continue;
        }

        /* Get resource ID */
        ResID resID = 0;
        ResType resType = 0;
        char resName[256] = {0};
        GetResInfo(defResource, &resID, &resType, resName);

        /* Try to load this definition */
        OSErr err = DefLoader_LoadDefinition(defType, resID);
        if (err == extNoErr) {
            loaded++;
        } else {
            DEF_LOG("Failed to load %s %d: error %d\n", DefTypeToString(defType), resID, err);
        }
    }

    DEF_LOG("Loaded %d %s resources\n", loaded, DefTypeToString(defType));
    return loaded;
}

/**
 * Get definition entry by type and resource ID
 */
DefEntry* DefLoader_GetByResourceID(ResType defType, SInt16 resourceID)
{
    if (!g_defLoaderInitialized) {
        return NULL;
    }

    DefEntry *current = g_firstDef;
    while (current) {
        if (current->defType == defType && current->resourceID == resourceID) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Unload a definition
 */
OSErr DefLoader_UnloadDefinition(ResType defType, SInt16 resourceID)
{
    DefEntry *entry = DefLoader_GetByResourceID(defType, resourceID);
    if (!entry) {
        return extNotFound;
    }

    if (entry->loaded) {
        DefEntry_UnloadCode(entry);
        entry->loaded = false;
    }

    /* Remove from linked list */
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        g_firstDef = entry->next;
    }

    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        g_lastDef = entry->prev;
    }

    /* Decrement type-specific counter */
    if (defType == WDEF_TYPE) {
        g_wdefCount--;
    } else if (defType == LDEF_TYPE) {
        g_ldefCount--;
    } else if (defType == MDEF_TYPE) {
        g_mdefCount--;
    }

    DEF_LOG("Unloaded %s %s\n", DefTypeToString(defType), entry->name);

    DefEntry_Free(entry);
    g_defCount--;

    return extNoErr;
}

/**
 * Get count of loaded definitions by type
 */
SInt16 DefLoader_GetLoadedCount(ResType defType)
{
    if (!g_defLoaderInitialized) {
        return 0;
    }

    if (defType == WDEF_TYPE) {
        return g_wdefCount;
    } else if (defType == LDEF_TYPE) {
        return g_ldefCount;
    } else if (defType == MDEF_TYPE) {
        return g_mdefCount;
    }

    return 0;
}

/**
 * Get total count of loaded definitions
 */
SInt16 DefLoader_GetTotalCount(void)
{
    if (!g_defLoaderInitialized) {
        return 0;
    }
    return g_defCount;
}

/**
 * Dump definition registry (debug)
 */
void DefLoader_DumpRegistry(void)
{
    if (!g_defLoaderInitialized) {
        SYSTEM_LOG_DEBUG("Definition Loader not initialized\n");
        return;
    }

    SYSTEM_LOG_DEBUG("\n====== DEFINITION REGISTRY DUMP ======\n");
    SYSTEM_LOG_DEBUG("Total Definitions: %d (WDEF:%d LDEF:%d MDEF:%d)\n",
                     g_defCount, g_wdefCount, g_ldefCount, g_mdefCount);

    DefEntry *current = g_firstDef;
    int index = 1;
    while (current) {
        SYSTEM_LOG_DEBUG("%d. %s %s (id=%d, loaded=%s)\n",
                        index++, DefTypeToString(current->defType),
                        current->name, current->resourceID,
                        current->loaded ? "yes" : "no");
        current = current->next;
    }
    SYSTEM_LOG_DEBUG("=====================================\n\n");
}

/**
 * Check if Definition Loader is initialized
 */
Boolean DefLoader_IsInitialized(void)
{
    return g_defLoaderInitialized;
}

/* ========================================================================
 * INTERNAL FUNCTIONS
 * ======================================================================== */

/**
 * Allocate definition entry
 */
static DefEntry* DefEntry_Allocate(void)
{
    DefEntry *entry = NewPtr(sizeof(DefEntry));
    if (entry) {
        memset(entry, 0, sizeof(DefEntry));
    }
    return entry;
}

/**
 * Free definition entry
 */
static void DefEntry_Free(DefEntry *entry)
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
 * Load definition code
 */
static OSErr DefEntry_LoadCode(DefEntry *entry, Handle resourceHandle)
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
    entry->defProc = codePtr;

    DEF_LOG("Loaded code for %s\n", entry->name);
    return extNoErr;
}

/**
 * Unload definition code
 */
static void DefEntry_UnloadCode(DefEntry *entry)
{
    if (!entry || !entry->resourceHandle) {
        return;
    }

    /* Unlock the handle */
    HUnlock(entry->resourceHandle);

    entry->defProc = NULL;
    DEF_LOG("Unloaded code for %s\n", entry->name);
}

/**
 * Convert definition type to string
 */
static const char* DefTypeToString(ResType defType)
{
    if (defType == WDEF_TYPE) {
        return "WDEF";
    } else if (defType == LDEF_TYPE) {
        return "LDEF";
    } else if (defType == MDEF_TYPE) {
        return "MDEF";
    }
    return "UNKNOWN";
}
