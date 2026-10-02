/*
 * ControlPanelManager.h - System 7.1 Control Panel Manager
 *
 * Manages Control Panel resources (CDEV) and their execution
 * Control Panels provide system customization interfaces
 */

#ifndef CONTROL_PANEL_MANAGER_H
#define CONTROL_PANEL_MANAGER_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * CONTROL PANEL CONSTANTS
 * ======================================================================== */

#define MAX_CONTROL_PANELS          32      /* Maximum control panels */
#define MAX_CDEV_NAME               64      /* Max control panel name */

/* Control Panel resource type */
#define CDEV_TYPE                   FOURCC('C', 'D', 'E', 'V') /* Control device resource */

/* Control Panel item types */
#define CDEV_ITEM_SLIDER            1
#define CDEV_ITEM_CHECKBOX          2
#define CDEV_ITEM_POPUP             3
#define CDEV_ITEM_BUTTON            4
#define CDEV_ITEM_LISTBOX           5

/* ========================================================================
 * CONTROL PANEL ENTRY
 * ======================================================================== */

typedef struct ControlPanelEntry {
    SInt16          resourceID;             /* Resource ID in System file */
    Handle          resourceHandle;         /* Loaded resource handle */

    /* Metadata */
    char            name[MAX_CDEV_NAME];   /* Control panel name */
    SInt16          version;                /* CDEV version */
    SInt32          flags;                  /* CDEV flags */

    /* Entry points */
    void*           initProc;               /* Initialization entry point */
    void*           itemProc;               /* Item handling entry point */
    void*           filterProc;             /* Event filter entry point */

    /* State */
    Boolean         loaded;                 /* Currently loaded */
    Boolean         active;                 /* Currently active */

    /* Linked list */
    struct ControlPanelEntry *next;
    struct ControlPanelEntry *prev;
} ControlPanelEntry;

/* ========================================================================
 * CONTROL PANEL MANAGER API
 * ======================================================================== */

/**
 * Initialize Control Panel Manager
 * @return extNoErr on success, error code on failure
 */
OSErr ControlPanelManager_Initialize(void);

/**
 * Shutdown Control Panel Manager
 */
void ControlPanelManager_Shutdown(void);

/**
 * Check if Control Panel Manager is initialized
 * @return true if initialized, false otherwise
 */
Boolean ControlPanelManager_IsInitialized(void);

/**
 * Scan System file for CDEV resources
 * @param rescan If true, ignore cache and re-scan
 * @return Number of control panels found
 */
SInt16 ControlPanelManager_ScanForControlPanels(Boolean rescan);

/**
 * Get count of discovered control panels
 * @return Number of control panel resources found
 */
SInt16 ControlPanelManager_GetDiscoveredCount(void);

/**
 * Get count of loaded control panels
 * @return Number of loaded control panels
 */
SInt16 ControlPanelManager_GetLoadedCount(void);

/**
 * Load a specific control panel by resource ID
 * @param resourceID Resource ID of CDEV to load
 * @return extNoErr on success, error code on failure
 */
OSErr ControlPanelManager_LoadControlPanel(SInt16 resourceID);

/**
 * Get control panel entry by resource ID
 * @param resourceID Resource ID to find
 * @return Pointer to control panel entry or NULL if not found
 */
ControlPanelEntry* ControlPanelManager_GetByResourceID(SInt16 resourceID);

/**
 * Get control panel entry by name
 * @param name Control panel name to find
 * @return Pointer to control panel entry or NULL if not found
 */
ControlPanelEntry* ControlPanelManager_GetByName(const char *name);

/**
 * Get first control panel (for iteration)
 * @return Pointer to first control panel or NULL if none
 */
ControlPanelEntry* ControlPanelManager_GetFirstControlPanel(void);

/**
 * Get next control panel (for iteration)
 * @param current Current control panel entry
 * @return Pointer to next control panel or NULL if at end
 */
ControlPanelEntry* ControlPanelManager_GetNextControlPanel(ControlPanelEntry *current);

/**
 * Unload a control panel
 * @param resourceID Resource ID of control panel to unload
 * @return extNoErr on success, error code on failure
 */
OSErr ControlPanelManager_UnloadControlPanel(SInt16 resourceID);

/**
 * Dump control panel registry (debug)
 */
void ControlPanelManager_DumpRegistry(void);

/**
 * Set debug mode
 * @param enable If true, enable debug logging
 */
void ControlPanelManager_SetDebugMode(Boolean enable);

#ifdef __cplusplus
}
#endif

#endif /* CONTROL_PANEL_MANAGER_H */
