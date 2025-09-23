// #include "CompatibilityFix.h" // Removed
#define DESKMANAGER_INCLUDED
#include <stdlib.h>
#include <string.h>
/*
 * DeskManagerCore.c - Core Desk Manager Implementation
 *
 * Provides the core functionality for managing desk accessories (DAs) in
 * System 7.1. Handles DA loading, system integration, and event routing.
 *
 * Based on Apple's Macintosh Toolbox Desk Manager from System 7.1
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeskManager/DeskManager.h"
#include "DeskManager/DeskAccessory.h"


/* Global Desk Manager State */
static DeskManagerState g_deskMgr = {0};
static Boolean g_deskMgrInitialized = false;

/* Internal Function Prototypes */
static int DA_AllocateRefNum(void);
static void DA_FreeRefNum(SInt16 refNum);
static DeskAccessory *DA_AllocateInstance(void);
static void DA_FreeInstance(DeskAccessory *da);
static int DA_LoadFromRegistry(DeskAccessory *da, const char *name);
static void DA_AddToList(DeskAccessory *da);
static void DA_RemoveFromList(DeskAccessory *da);

/*
 * Initialize the Desk Manager
 */
int DeskManager_Initialize(void)
{
    if (g_deskMgrInitialized) {
        return DESK_ERR_NONE;
    }

    /* Initialize state */
    memset(&g_deskMgr, 0, sizeof(g_deskMgr));
    g_deskMgr.nextRefNum = 1;
    g_deskMgr.systemMenuEnabled = true;

    /* Register built-in desk accessories */
    if (DeskManager_RegisterBuiltinDAs() != 0) {
        return DESK_ERR_SYSTEM_ERROR;
    }

    /* Initialize system menu */
    SystemMenu_Update();

    g_deskMgrInitialized = true;
    return DESK_ERR_NONE;
}

/*
 * Shutdown the Desk Manager
 */
void DeskManager_Shutdown(void)
{
    if (!g_deskMgrInitialized) {
        return;
    }

    /* Close all open desk accessories */
    DeskAccessory *da = g_deskMgr.firstDA;
    while (da != NULL) {
        DeskAccessory *next = da->next;
        CloseDeskAcc(da->refNum);
        da = next;
    }

    /* Clean up system menu */
    g_deskMgr.systemMenuHandle = NULL;

    g_deskMgrInitialized = false;
}

/*
 * Open a desk accessory by name
 */
SInt16 OpenDeskAcc(const char *name)
{
    if (!g_deskMgrInitialized || !name) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Check if DA is already open */
    DeskAccessory *existing = DA_GetByName(name);
    if (existing) {
        /* Activate existing DA */
        DA_SetActive(existing);
        return existing->refNum;
    }

    /* Allocate new DA instance */
    DeskAccessory *da = DA_AllocateInstance();
    if (!da) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Set basic properties */
    strncpy(da->name, name, DA_NAME_LENGTH);
    da->name[DA_NAME_LENGTH] = '\0';
    da->refNum = DA_AllocateRefNum();
    da->state = DA_STATE_CLOSED;

    /* Load DA from registry */
    int result = DA_LoadFromRegistry(da, name);
    if (result != 0) {
        DA_FreeInstance(da);
        return result;
    }

    /* Initialize the DA */
    if (da->open) {
        result = da->open(da);
        if (result != 0) {
            DA_FreeInstance(da);
            return result;
        }
    }

    /* Add to DA list */
    DA_AddToList(da);
    da->state = DA_STATE_OPEN;
    g_deskMgr.numDAs++;

    /* Set as active DA */
    DA_SetActive(da);

    /* Update system menu */
    SystemMenu_AddDA(da);

    return da->refNum;
}

/*
 * Close a desk accessory
 */
void CloseDeskAcc(SInt16 refNum)
{
    if (!g_deskMgrInitialized) {
        return;
    }

    DeskAccessory *da = DA_GetByRefNum(refNum);
    if (!da) {
        return;
    }

    /* Send goodbye message */
    DA_SendMessage(da, DA_MSG_GOODBYE, NULL, NULL);

    /* Close the DA */
    if (da->close) {
        da->close(da);
    }

    /* Remove from system menu */
    SystemMenu_RemoveDA(da);

    /* Remove from DA list */
    DA_RemoveFromList(da);
    g_deskMgr.numDAs--;

    /* Update active DA */
    if (g_deskMgr.activeDA == da) {
        g_deskMgr.activeDA = g_deskMgr.firstDA;
    }

    /* Free the DA */
    DA_FreeRefNum(refNum);
    DA_FreeInstance(da);
}

/*
 * Handle system-level events for desk accessories
 */
Boolean SystemEvent(const EventRecord *event)
{
    if (!g_deskMgrInitialized || !event) {
        return false;
    }

    /* Route event to active DA */
    if (g_deskMgr.activeDA && g_deskMgr.activeDA->event) {
        int result = g_deskMgr.activeDA->event(g_deskMgr.activeDA, event);
        return (result == 0);
    }

    return false;
}

/*
 * Handle mouse clicks in system areas
 */
void SystemClick(const EventRecord *event, WindowRecord *window)
{
    if (!g_deskMgrInitialized || !event) {
        return;
    }

    /* Find DA that owns this window */
    DeskAccessory *da = g_deskMgr.firstDA;
    while (da) {
        if (da->window == window) {
            /* Set as active DA */
            DA_SetActive(da);

            /* Send event to DA */
            if (da->event) {
                da->event(da, event);
            }
            return;
        }
        da = da->next;
    }
}

/*
 * Perform periodic processing for all DAs
 */
void SystemTask(void)
{
    /* FIXED: Removed direct PollPS2Input call - this bypasses event system! */
    /* PS/2 input polling should ONLY happen in main event loop via ProcessModernInput */

    if (!g_deskMgrInitialized) {
        return;
    }

    /* Call idle routine for all open DAs */
    DeskAccessory *da = g_deskMgr.firstDA;
    while (da) {
        if (da->idle) {
            da->idle(da);
        }
        da = da->next;
    }
}

/*
 * Handle system menu selections
 */
void SystemMenu(SInt32 menuResult)
{
    if (!g_deskMgrInitialized) {
        return;
    }

    SInt16 menuID = (menuResult >> 16) & 0xFFFF;
    SInt16 itemID = menuResult & 0xFFFF;

    /* Check if this is a DA menu selection */
    if (menuID == 1) { /* Apple menu */
        /* Try to open the selected DA */
        /* Note: In real implementation, would need to map item to DA name */
        /* For now, just route to active DA */
        if (g_deskMgr.activeDA && g_deskMgr.activeDA->menu) {
            g_deskMgr.activeDA->menu(g_deskMgr.activeDA, menuID, itemID);
        }
    }
}

/*
 * Handle system edit operations
 */
Boolean SystemEdit(SInt16 editCmd)
{
    if (!g_deskMgrInitialized) {
        return false;
    }

    /* Route edit command to active DA */
    if (g_deskMgr.activeDA) {
        DAMessage message;
        switch (editCmd) {
            case 1: message = DA_MSG_UNDO; break;
            case 3: message = DA_MSG_CUT; break;
            case 4: message = DA_MSG_COPY; break;
            case 5: message = DA_MSG_PASTE; break;
            case 6: message = DA_MSG_CLEAR; break;
            default: return false;
        }

        int result = DA_SendMessage(g_deskMgr.activeDA, message, NULL, NULL);
        return (result == 0);
    }

    return false;
}

/*
 * Get desk accessory by reference number
 */
DeskAccessory *DA_GetByRefNum(SInt16 refNum)
{
    if (!g_deskMgrInitialized) {
        return NULL;
    }

    DeskAccessory *da = g_deskMgr.firstDA;
    while (da) {
        if (da->refNum == refNum) {
            return da;
        }
        da = da->next;
    }
    return NULL;
}

/*
 * Get desk accessory by name
 */
DeskAccessory *DA_GetByName(const char *name)
{
    if (!g_deskMgrInitialized || !name) {
        return NULL;
    }

    DeskAccessory *da = g_deskMgr.firstDA;
    while (da) {
        if (strcmp(da->name, name) == 0) {
            return da;
        }
        da = da->next;
    }
    return NULL;
}

/*
 * Get currently active desk accessory
 */
DeskAccessory *DA_GetActive(void)
{
    return g_deskMgrInitialized ? g_deskMgr.activeDA : NULL;
}

/*
 * Set active desk accessory
 */
int DA_SetActive(DeskAccessory *da)
{
    if (!g_deskMgrInitialized) {
        return DESK_ERR_SYSTEM_ERROR;
    }

    /* Deactivate current DA */
    if (g_deskMgr.activeDA && g_deskMgr.activeDA != da) {
        if (g_deskMgr.activeDA->activate) {
            g_deskMgr.activeDA->activate(g_deskMgr.activeDA, false);
        }
    }

    /* Activate new DA */
    g_deskMgr.activeDA = da;
    if (da && da->activate) {
        da->activate(da, true);
    }

    return DESK_ERR_NONE;
}

/*
 * Send message to desk accessory
 */
int DA_SendMessage(DeskAccessory *da, DAMessage message,
                   void *param1, void *param2)
{
    if (!da) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Route message based on type */
    switch (message) {
        case DA_MSG_EVENT:
            if (da->event) {
                return da->event(da, (const EventRecord *)param1);
            }
            break;

        case DA_MSG_RUN:
            if (da->idle) {
                da->idle(da);
                return DESK_ERR_NONE;
            }
            break;

        case DA_MSG_MENU:
            if (da->menu) {
                SInt16 menuID = (SInt16)(intptr_t)param1;
                SInt16 itemID = (SInt16)(intptr_t)param2;
                return da->menu(da, menuID, itemID);
            }
            break;

        case DA_MSG_UNDO:
        case DA_MSG_CUT:
        case DA_MSG_COPY:
        case DA_MSG_PASTE:
        case DA_MSG_CLEAR:
            /* These would need DA-specific implementations */
            break;

        case DA_MSG_GOODBYE:
            /* DA is being closed */
            break;

        default:
            return DESK_ERR_INVALID_PARAM;
    }

    return DESK_ERR_NONE;
}

/*
 * Get Desk Manager version
 */
UInt16 DeskManager_GetVersion(void)
{
    return DESK_MGR_VERSION;
}

/*
 * Get number of open desk accessories
 */
SInt16 DeskManager_GetDACount(void)
{
    return g_deskMgrInitialized ? g_deskMgr.numDAs : 0;
}

/*
 * Check if a DA is installed
 */
Boolean DeskManager_IsDAAvailable(const char *name)
{
    if (!name) {
        return false;
    }

    /* Check if DA is registered */
    DARegistryEntry *entry = DA_FindRegistryEntry(name);
    return (entry != NULL);
}

/* Internal Functions */

/*
 * Allocate a new reference number
 */
static int DA_AllocateRefNum(void)
{
    return g_deskMgr.nextRefNum++;
}

/*
 * Free a reference number
 */
static void DA_FreeRefNum(SInt16 refNum)
{
    /* In a real implementation, might want to track and reuse ref nums */
    (void)refNum;
}

/*
 * Allocate a new DA instance
 */
static DeskAccessory *DA_AllocateInstance(void)
{
    DeskAccessory *da = calloc(1, sizeof(DeskAccessory));
    if (da) {
        da->state = DA_STATE_CLOSED;
    }
    return da;
}

/*
 * Free a DA instance
 */
static void DA_FreeInstance(DeskAccessory *da)
{
    if (da) {
        free(da);
    }
}

/*
 * Load DA from registry
 */
static int DA_LoadFromRegistry(DeskAccessory *da, const char *name)
{
    DARegistryEntry *entry = DA_FindRegistryEntry(name);
    if (!entry) {
        return DESK_ERR_NOT_FOUND;
    }

    /* Set DA properties from registry */
    da->type = entry->type;

    /* Set function pointers from interface */
    if (entry->interface) {
        /* Map interface functions to DA functions */
        /* This would need to be implemented based on the specific
           interface design */
    }

    return DESK_ERR_NONE;
}

/*
 * Add DA to the list
 */
static void DA_AddToList(DeskAccessory *da)
{
    if (!da) return;

    da->next = g_deskMgr.firstDA;
    da->prev = NULL;

    if (g_deskMgr.firstDA) {
        g_deskMgr.firstDA->prev = da;
    }

    g_deskMgr.firstDA = da;
}

/*
 * Remove DA from the list
 */
static void DA_RemoveFromList(DeskAccessory *da)
{
    if (!da) return;

    if (da->next) {
        da->next->prev = da->prev;
    }

    if (da->prev) {
        da->prev->next = da->next;
    } else {
        g_deskMgr.firstDA = da->next;
    }

    da->next = da->prev = NULL;
}
