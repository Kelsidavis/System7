/*
 * DRVRLoader.c - DRVR (Device Driver) Resource Loader Implementation
 *
 * Loads DRVR resources from System file and registers them with the Device Manager
 */

#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "ExtensionManager/DRVRLoader.h"
#include "ExtensionManager/ExtensionTypes.h"
#include "ResourceMgr/ResourceMgr.h"
#include "MemoryMgr/MemoryManager.h"
#include "System/SystemLogging.h"

/* ========================================================================
 * GLOBAL STATE
 * ======================================================================== */

static Boolean g_drvrLoaderInitialized = false;
static DRVREntry *g_firstDriver = NULL;
static DRVREntry *g_lastDriver = NULL;
static SInt16 g_driverCount = 0;
static SInt16 g_nextUnitNumber = 0;  /* Assign unit numbers sequentially */
static Boolean g_unitAssigned[MAX_DRIVERS] = {false};  /* Track assigned units */

/* Debug logging macro */
#define DRVR_LOG(...) do { \
    SYSTEM_LOG_DEBUG("[DRVRLoader] " __VA_ARGS__); \
} while(0)

/* ========================================================================
 * INTERNAL FUNCTION PROTOTYPES
 * ======================================================================== */

static DRVREntry* DRVREntry_Allocate(void);
static void DRVREntry_Free(DRVREntry *entry);
static OSErr DRVREntry_LoadCode(DRVREntry *entry, Handle resourceHandle);
static void DRVREntry_UnloadCode(DRVREntry *entry);
static SInt16 DRVRLoader_AllocateUnitNumber(void);

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

/**
 * Initialize DRVR Loader subsystem
 */
OSErr DRVRLoader_Initialize(void)
{
    if (g_drvrLoaderInitialized) {
        return extNoErr;
    }

    g_firstDriver = NULL;
    g_lastDriver = NULL;
    g_driverCount = 0;
    g_nextUnitNumber = 0;
    memset(g_unitAssigned, 0, sizeof(g_unitAssigned));

    DRVR_LOG("DRVR Loader initialized\n");
    g_drvrLoaderInitialized = true;

    return extNoErr;
}

/**
 * Shutdown DRVR Loader (unload all registered drivers)
 */
void DRVRLoader_Shutdown(void)
{
    if (!g_drvrLoaderInitialized) {
        return;
    }

    /* Unload all drivers in reverse order */
    DRVREntry *current = g_lastDriver;
    while (current) {
        DRVREntry *prev = current->prev;
        DRVRLoader_UnloadDriver(current->unitNumber);
        current = prev;
    }

    g_drvrLoaderInitialized = false;
    DRVR_LOG("DRVR Loader shutdown\n");
}

/**
 * Load a DRVR resource and register with Device Manager
 */
OSErr DRVRLoader_LoadDriver(SInt16 resourceID, SInt16 *outUnitNumber)
{
    if (!g_drvrLoaderInitialized) {
        return extNotFound;
    }

    /* Check if already loaded */
    DRVREntry *existing = DRVRLoader_GetByResourceID(resourceID);
    if (existing) {
        if (outUnitNumber) {
            *outUnitNumber = existing->unitNumber;
        }
        return extAlreadyLoaded;
    }

    /* Load resource from file */
    Handle resourceHandle = GetResource(DRVR_TYPE, resourceID);
    if (!resourceHandle) {
        DRVR_LOG("Failed to get DRVR resource %d\n", resourceID);
        return extBadResource;
    }

    /* Load resource data into memory */
    LoadResource(resourceHandle);
    if (!*resourceHandle) {
        DRVR_LOG("Failed to load DRVR resource %d into memory\n", resourceID);
        ReleaseResource(resourceHandle);
        return extBadResource;
    }

    /* Get resource info */
    ResID resID = 0;
    ResType resType = 0;
    char resName[256] = {0};
    GetResInfo(resourceHandle, &resID, &resType, resName);

    /* Allocate DRVR entry */
    DRVREntry *entry = DRVREntry_Allocate();
    if (!entry) {
        ReleaseResource(resourceHandle);
        return extMemError;
    }

    /* Allocate unit number */
    SInt16 unitNumber = DRVRLoader_AllocateUnitNumber();
    if (unitNumber < 0) {
        DRVR_LOG("No available unit numbers for DRVR %d\n", resourceID);
        DRVREntry_Free(entry);
        ReleaseResource(resourceHandle);
        return extMaxExtensions;  /* Too many drivers */
    }

    /* Fill in entry info */
    entry->resourceID = resourceID;
    entry->resourceHandle = resourceHandle;
    entry->resourceSize = GetResourceSizeOnDisk(resourceHandle);
    entry->unitNumber = unitNumber;
    entry->registered = false;
    entry->openCount = 0;

    strncpy(entry->name, resName, MAX_DRIVER_NAME - 1);
    entry->name[MAX_DRIVER_NAME - 1] = '\0';

    /* Load code */
    OSErr err = DRVREntry_LoadCode(entry, resourceHandle);
    if (err != extNoErr) {
        DRVR_LOG("Failed to load code for DRVR %d\n", resourceID);
        DRVREntry_Free(entry);
        g_unitAssigned[unitNumber] = false;
        return err;
    }

    /* Mark as registered */
    entry->registered = true;

    /* Add to linked list */
    if (!g_firstDriver) {
        g_firstDriver = entry;
        g_lastDriver = entry;
    } else {
        entry->prev = g_lastDriver;
        g_lastDriver->next = entry;
        g_lastDriver = entry;
    }

    g_driverCount++;

    DRVR_LOG("Loaded DRVR %s (id=%d, unit=%d, size=%d)\n",
            entry->name, resourceID, unitNumber, (int)entry->resourceSize);

    if (outUnitNumber) {
        *outUnitNumber = unitNumber;
    }

    return extNoErr;
}

/**
 * Load all DRVR resources from System file
 */
SInt16 DRVRLoader_LoadAllDrivers(void)
{
    if (!g_drvrLoaderInitialized) {
        return 0;
    }

    SInt16 loaded = 0;
    SInt16 drvrCount = CountResources(DRVR_TYPE);

    DRVR_LOG("Scanning for DRVR resources, found %d\n", drvrCount);

    for (SInt16 i = 1; i <= drvrCount; i++) {
        Handle drvrResource = GetIndResource(DRVR_TYPE, i);
        if (!drvrResource) {
            continue;
        }

        /* Get resource ID */
        ResID resID = 0;
        ResType resType = 0;
        char resName[256] = {0};
        GetResInfo(drvrResource, &resID, &resType, resName);

        /* Try to load this driver */
        SInt16 unitNumber = 0;
        OSErr err = DRVRLoader_LoadDriver(resID, &unitNumber);
        if (err == extNoErr) {
            loaded++;
        } else {
            DRVR_LOG("Failed to load DRVR %d: error %d\n", resID, err);
        }
    }

    DRVR_LOG("Loaded %d drivers\n", loaded);
    return loaded;
}

/**
 * Get DRVR entry by unit number
 */
DRVREntry* DRVRLoader_GetByUnitNumber(SInt16 unitNumber)
{
    if (!g_drvrLoaderInitialized) {
        return NULL;
    }

    DRVREntry *current = g_firstDriver;
    while (current) {
        if (current->unitNumber == unitNumber) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Get DRVR entry by resource ID
 */
DRVREntry* DRVRLoader_GetByResourceID(SInt16 resourceID)
{
    if (!g_drvrLoaderInitialized) {
        return NULL;
    }

    DRVREntry *current = g_firstDriver;
    while (current) {
        if (current->resourceID == resourceID) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Get DRVR entry by name
 */
DRVREntry* DRVRLoader_GetByName(const char *name)
{
    if (!g_drvrLoaderInitialized || !name) {
        return NULL;
    }

    DRVREntry *current = g_firstDriver;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Unload a DRVR and unregister from Device Manager
 */
OSErr DRVRLoader_UnloadDriver(SInt16 unitNumber)
{
    DRVREntry *entry = DRVRLoader_GetByUnitNumber(unitNumber);
    if (!entry) {
        return extNotFound;
    }

    /* Unload code */
    DRVREntry_UnloadCode(entry);

    /* Remove from linked list */
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        g_firstDriver = entry->next;
    }

    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        g_lastDriver = entry->prev;
    }

    /* Free unit number */
    g_unitAssigned[unitNumber] = false;

    DRVR_LOG("Unloaded DRVR %s (unit=%d)\n", entry->name, unitNumber);

    DRVREntry_Free(entry);
    g_driverCount--;

    return extNoErr;
}

/**
 * Get count of loaded drivers
 */
SInt16 DRVRLoader_GetLoadedCount(void)
{
    if (!g_drvrLoaderInitialized) {
        return 0;
    }
    return g_driverCount;
}

/**
 * Call driver control code
 */
OSErr DRVRLoader_CallDriver(SInt16 unitNumber, SInt16 csCode, void *pb)
{
    DRVREntry *entry = DRVRLoader_GetByUnitNumber(unitNumber);
    if (!entry || !entry->driverProc) {
        return extNotFound;
    }

    /* Call the driver's entry point */
    OSErr err = (*entry->driverProc)(unitNumber, csCode, pb);

    return err;
}

/**
 * Dump DRVR registry (debug)
 */
void DRVRLoader_DumpRegistry(void)
{
    if (!g_drvrLoaderInitialized) {
        SYSTEM_LOG_DEBUG("DRVR Loader not initialized\n");
        return;
    }

    SYSTEM_LOG_DEBUG("\n========== DRVR REGISTRY DUMP ==========\n");
    SYSTEM_LOG_DEBUG("Total Drivers: %d\n", g_driverCount);

    DRVREntry *current = g_firstDriver;
    int index = 1;
    while (current) {
        SYSTEM_LOG_DEBUG("%d. %s (id=%d, unit=%d, opens=%d)\n",
                        index++, current->name, current->resourceID,
                        current->unitNumber, (int)current->openCount);
        current = current->next;
    }
    SYSTEM_LOG_DEBUG("=======================================\n\n");
}

/**
 * Check if DRVR Loader is initialized
 */
Boolean DRVRLoader_IsInitialized(void)
{
    return g_drvrLoaderInitialized;
}

/* ========================================================================
 * INTERNAL FUNCTIONS
 * ======================================================================== */

/**
 * Allocate DRVR entry
 */
static DRVREntry* DRVREntry_Allocate(void)
{
    DRVREntry *entry = NewPtr(sizeof(DRVREntry));
    if (entry) {
        memset(entry, 0, sizeof(DRVREntry));
    }
    return entry;
}

/**
 * Free DRVR entry
 */
static void DRVREntry_Free(DRVREntry *entry)
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
 * Load DRVR code
 */
static OSErr DRVREntry_LoadCode(DRVREntry *entry, Handle resourceHandle)
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

    /* DRVR resources contain the driver entry point
     * The entry point is typically at offset 0 in the resource
     */
    entry->driverProc = (DriverEntryProc)codePtr;

    DRVR_LOG("Loaded code for DRVR %s\n", entry->name);
    return extNoErr;
}

/**
 * Unload DRVR code
 */
static void DRVREntry_UnloadCode(DRVREntry *entry)
{
    if (!entry || !entry->resourceHandle) {
        return;
    }

    /* Unlock the handle */
    HUnlock(entry->resourceHandle);

    entry->driverProc = NULL;
    DRVR_LOG("Unloaded code for DRVR %s\n", entry->name);
}

/**
 * Allocate next available unit number
 */
static SInt16 DRVRLoader_AllocateUnitNumber(void)
{
    for (SInt16 i = DRIVER_UNIT_MIN; i < MAX_DRIVERS; i++) {
        if (!g_unitAssigned[i]) {
            g_unitAssigned[i] = true;
            if (i >= g_nextUnitNumber) {
                g_nextUnitNumber = i + 1;
            }
            return i;
        }
    }

    return -1;  /* No available unit numbers */
}
