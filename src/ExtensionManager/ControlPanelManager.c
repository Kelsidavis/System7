/*
 * ControlPanelManager.c - System 7.1 Control Panel Manager Implementation
 *
 * Manages Control Panel (CDEV) resources and provides enumeration and loading
 */

#include <stdlib.h>
#include <string.h>
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "ExtensionManager/ControlPanelManager.h"
#include "ExtensionManager/ExtensionTypes.h"
#include "ResourceManager.h"
#include "MemoryMgr/MemoryManager.h"
#include "System/SystemLogging.h"

/* ========================================================================
 * GLOBAL STATE
 * ======================================================================== */

static Boolean g_controlPanelMgrInitialized = false;
static ControlPanelEntry *g_firstControlPanel = NULL;
static ControlPanelEntry *g_lastControlPanel = NULL;
static SInt16 g_discoveredCount = 0;
static SInt16 g_loadedCount = 0;
static Boolean g_debugMode = false;

/* Debug logging macro */
#define CPPANEL_LOG(...) do { \
    if (g_debugMode) { \
        SYSTEM_LOG_DEBUG("[ControlPanelMgr] " __VA_ARGS__); \
    } \
} while(0)

/* ========================================================================
 * INTERNAL FUNCTION PROTOTYPES
 * ======================================================================== */

static ControlPanelEntry* ControlPanelEntry_Allocate(void);
static void ControlPanelEntry_Free(ControlPanelEntry *entry);
static OSErr ControlPanelEntry_LoadCode(ControlPanelEntry *entry, Handle resourceHandle);
static void ControlPanelEntry_UnloadCode(ControlPanelEntry *entry);

/* ========================================================================
 * PUBLIC API
 * ======================================================================== */

/**
 * Initialize Control Panel Manager
 */
OSErr ControlPanelManager_Initialize(void)
{
    if (g_controlPanelMgrInitialized) {
        return extNoErr;
    }

    g_firstControlPanel = NULL;
    g_lastControlPanel = NULL;
    g_discoveredCount = 0;
    g_loadedCount = 0;
    g_debugMode = false;

    SYSTEM_LOG_DEBUG("Control Panel Manager initialized\n");
    g_controlPanelMgrInitialized = true;

    return extNoErr;
}

/**
 * Shutdown Control Panel Manager
 */
void ControlPanelManager_Shutdown(void)
{
    if (!g_controlPanelMgrInitialized) {
        return;
    }

    /* Unload all control panels in reverse order */
    ControlPanelEntry *current = g_lastControlPanel;
    while (current) {
        ControlPanelEntry *prev = current->prev;
        ControlPanelManager_UnloadControlPanel(current->resourceID);
        current = prev;
    }

    g_controlPanelMgrInitialized = false;
    SYSTEM_LOG_DEBUG("Control Panel Manager shutdown\n");
}

/**
 * Check if Control Panel Manager is initialized
 */
Boolean ControlPanelManager_IsInitialized(void)
{
    return g_controlPanelMgrInitialized;
}

/**
 * Scan System file for CDEV resources
 */
SInt16 ControlPanelManager_ScanForControlPanels(Boolean rescan)
{
    if (!g_controlPanelMgrInitialized) {
        return 0;
    }

    SInt16 discovered = 0;
    SInt16 cdevCount = CountResources(CDEV_TYPE);

    CPPANEL_LOG("Scanning for CDEV resources, found %d\n", cdevCount);

    for (SInt16 i = 1; i <= cdevCount; i++) {
        Handle cdevResource = GetIndResource(CDEV_TYPE, i);
        if (!cdevResource) {
            CPPANEL_LOG("Failed to load CDEV resource %d\n", i);
            continue;
        }

        /* Get resource info */
        ResID resourceID = 0;
        ResType resourceType = 0;
        char resourceName[256] = {0};
        GetResInfo(cdevResource, &resourceID, &resourceType, resourceName);

        /* Allocate control panel entry */
        ControlPanelEntry *entry = ControlPanelEntry_Allocate();
        if (!entry) {
            CPPANEL_LOG("Failed to allocate control panel entry\n");
            continue;
        }

        /* Fill in entry info */
        entry->resourceID = resourceID;
        entry->resourceHandle = cdevResource;

        strncpy(entry->name, resourceName, MAX_CDEV_NAME - 1);
        entry->name[MAX_CDEV_NAME - 1] = '\0';

        entry->version = 0;
        entry->flags = 0;
        entry->loaded = false;
        entry->active = false;

        /* Add to linked list */
        if (!g_firstControlPanel) {
            g_firstControlPanel = entry;
            g_lastControlPanel = entry;
        } else {
            entry->prev = g_lastControlPanel;
            g_lastControlPanel->next = entry;
            g_lastControlPanel = entry;
        }

        discovered++;
        g_discoveredCount++;

        CPPANEL_LOG("Discovered control panel %s (id=%d)\n",
                    entry->name, resourceID);
    }

    CPPANEL_LOG("Scanned for control panels, found %d\n", discovered);
    return discovered;
}

/**
 * Get count of discovered control panels
 */
SInt16 ControlPanelManager_GetDiscoveredCount(void)
{
    if (!g_controlPanelMgrInitialized) {
        return 0;
    }
    return g_discoveredCount;
}

/**
 * Get count of loaded control panels
 */
SInt16 ControlPanelManager_GetLoadedCount(void)
{
    if (!g_controlPanelMgrInitialized) {
        return 0;
    }
    return g_loadedCount;
}

/**
 * Load a specific control panel by resource ID
 */
OSErr ControlPanelManager_LoadControlPanel(SInt16 resourceID)
{
    if (!g_controlPanelMgrInitialized) {
        return extNotFound;
    }

    ControlPanelEntry *entry = ControlPanelManager_GetByResourceID(resourceID);
    if (!entry) {
        return extNotFound;
    }

    if (entry->loaded) {
        return extAlreadyLoaded;
    }

    /* Load code */
    OSErr err = ControlPanelEntry_LoadCode(entry, entry->resourceHandle);
    if (err != extNoErr) {
        CPPANEL_LOG("Failed to load code for control panel %s\n", entry->name);
        return err;
    }

    entry->loaded = true;
    g_loadedCount++;

    CPPANEL_LOG("Loaded control panel %s (id=%d)\n", entry->name, resourceID);

    return extNoErr;
}

/**
 * Get control panel entry by resource ID
 */
ControlPanelEntry* ControlPanelManager_GetByResourceID(SInt16 resourceID)
{
    if (!g_controlPanelMgrInitialized) {
        return NULL;
    }

    ControlPanelEntry *current = g_firstControlPanel;
    while (current) {
        if (current->resourceID == resourceID) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Get control panel entry by name
 */
ControlPanelEntry* ControlPanelManager_GetByName(const char *name)
{
    if (!g_controlPanelMgrInitialized || !name) {
        return NULL;
    }

    ControlPanelEntry *current = g_firstControlPanel;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/**
 * Get first control panel
 */
ControlPanelEntry* ControlPanelManager_GetFirstControlPanel(void)
{
    if (!g_controlPanelMgrInitialized) {
        return NULL;
    }
    return g_firstControlPanel;
}

/**
 * Get next control panel
 */
ControlPanelEntry* ControlPanelManager_GetNextControlPanel(ControlPanelEntry *current)
{
    if (!g_controlPanelMgrInitialized || !current) {
        return NULL;
    }
    return current->next;
}

/**
 * Unload a control panel
 */
OSErr ControlPanelManager_UnloadControlPanel(SInt16 resourceID)
{
    ControlPanelEntry *entry = ControlPanelManager_GetByResourceID(resourceID);
    if (!entry) {
        return extNotFound;
    }

    if (entry->loaded) {
        ControlPanelEntry_UnloadCode(entry);
        entry->loaded = false;
        g_loadedCount--;
    }

    /* Remove from linked list */
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        g_firstControlPanel = entry->next;
    }

    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        g_lastControlPanel = entry->prev;
    }

    CPPANEL_LOG("Unloaded control panel %s\n", entry->name);

    ControlPanelEntry_Free(entry);
    g_discoveredCount--;

    return extNoErr;
}

/**
 * Dump control panel registry (debug)
 */
void ControlPanelManager_DumpRegistry(void)
{
    if (!g_controlPanelMgrInitialized) {
        SYSTEM_LOG_DEBUG("Control Panel Manager not initialized\n");
        return;
    }

    SYSTEM_LOG_DEBUG("\n===== CONTROL PANEL REGISTRY DUMP =====\n");
    SYSTEM_LOG_DEBUG("Discovered: %d, Loaded: %d\n",
                     g_discoveredCount, g_loadedCount);

    ControlPanelEntry *current = g_firstControlPanel;
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
 * Set debug mode
 */
void ControlPanelManager_SetDebugMode(Boolean enable)
{
    if (g_controlPanelMgrInitialized) {
        g_debugMode = enable;
    }
}

/* ========================================================================
 * INTERNAL FUNCTIONS
 * ======================================================================== */

/**
 * Allocate control panel entry
 */
static ControlPanelEntry* ControlPanelEntry_Allocate(void)
{
    ControlPanelEntry *entry = NewPtr(sizeof(ControlPanelEntry));
    if (entry) {
        memset(entry, 0, sizeof(ControlPanelEntry));
    }
    return entry;
}

/**
 * Free control panel entry
 */
static void ControlPanelEntry_Free(ControlPanelEntry *entry)
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
 * Load control panel code
 */
static OSErr ControlPanelEntry_LoadCode(ControlPanelEntry *entry, Handle resourceHandle)
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

    /* Store entry point references */
    entry->initProc = codePtr;  /* Entry point at start of resource */

    CPPANEL_LOG("Loaded code for control panel %s\n", entry->name);
    return extNoErr;
}

/**
 * Unload control panel code
 */
static void ControlPanelEntry_UnloadCode(ControlPanelEntry *entry)
{
    if (!entry || !entry->resourceHandle) {
        return;
    }

    /* Unlock the handle */
    HUnlock(entry->resourceHandle);

    entry->initProc = NULL;
    entry->itemProc = NULL;
    entry->filterProc = NULL;

    CPPANEL_LOG("Unloaded code for control panel %s\n", entry->name);
}
