#include "MemoryMgr/MemoryManager.h"
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
 * Derived from ROM analysis (System 7)
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
    if (!g_deskMgrInitialized || !event || !window) {
        return;
    }

    /* Find DA that owns this window */
    DeskAccessory *da = g_deskMgr.firstDA;
    while (da && da->window != window) {
        da = da->next;
    }
    if (!da) {
        return;
    }

    /*
     * FindWindow reports a system window as inSysWindow whole, without saying
     * which part was hit, so the part decision belongs here. Without it a
     * click anywhere on an accessory - title bar included - would go straight
     * through to the accessory as content, and its window could never be
     * moved or closed.
     */
    extern short Platform_WindowHitTest(WindowPtr window, Point pt);
    extern void DragWindow(WindowPtr theWindow, Point startPt, const Rect* boundsRect);
    extern Boolean TrackGoAway(WindowPtr theWindow, Point thePt);
    extern void SelectWindow(WindowPtr theWindow);
    extern QDGlobals qd;

    short part = Platform_WindowHitTest(window, event->where);

    switch (part) {
        case wInGoAway:
            if (TrackGoAway(window, event->where)) {
                CloseDeskAcc(da->refNum);
            }
            return;

        case wInDrag:
            SelectWindow(window);
            DA_SetActive(da);
            DragWindow(window, event->where, &qd.screenBits.bounds);
            return;

        case wInContent:
        default:
            /* Clicking an inactive accessory activates it; the click that does
             * so is not passed on, which is what the Finder does for its own
             * windows and what Inside Macintosh describes. */
            if (g_deskMgr.activeDA != da) {
                SelectWindow(window);
                DA_SetActive(da);
                return;
            }
            if (da->event) {
                da->event(da, event);
            }
            return;
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

    /* Update menu bar clock (lightweight - only redraws if minute changed) */
    extern void MenuBar_UpdateClock(void);
    MenuBar_UpdateClock();

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
                return da->event(da, (EventRecord *)param1);
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
    DeskAccessory *da = NewPtrClear(sizeof(DeskAccessory));
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
        DisposePtr((Ptr)da);
    }
}

/*
 * Load DA from registry
 */
/*
 * Adapters from DAInterface to the per-instance handlers.
 *
 * A desk accessory supplies either the individual procs on its registry entry
 * or a DAInterface, and every built-in supplies the interface. The two do not
 * line up exactly - initialize takes a driver header the instance path has not
 * got, and four of them return void here and int there - which is why the
 * mapping below used to be an empty "this would need to be implemented" and
 * every DA came out with a NULL open handler.
 *
 * processEvent and handleMenu take DAEventInfo and DAMenuInfo where the
 * instance procs take an EventRecord and a menu/item pair, so those two get a
 * conversion rather than a cast. See DA_EventViaInterface for the part of it
 * that is not a field copy.
 */
static int DA_OpenViaInterface(DeskAccessory *da)
{
    if (!da || !da->interface || !da->interface->initialize) return DESK_ERR_NONE;
    return da->interface->initialize(da, NULL);   /* built-ins ignore the header */
}

static void DA_CloseViaInterface(DeskAccessory *da)
{
    if (da && da->interface && da->interface->terminate) {
        (void)da->interface->terminate(da);
    }
}

static void DA_IdleViaInterface(DeskAccessory *da)
{
    if (da && da->interface && da->interface->idle) {
        (void)da->interface->idle(da);
    }
}

static void DA_ActivateViaInterface(DeskAccessory *da, Boolean active)
{
    if (da && da->interface && da->interface->activate) {
        (void)da->interface->activate(da, active);
    }
}

static void DA_UpdateViaInterface(DeskAccessory *da)
{
    if (da && da->interface && da->interface->update) {
        (void)da->interface->update(da);
    }
}

/*
 * DAEventInfo carries the same five fields an EventRecord does, plus a separate
 * v/h pair. The pair is not a copy of `where`: every built-in hit-tests with it
 * against its own content - CalcDA_HitTest takes button coordinates, KeyCaps
 * and Chooser build a Point from it for their click handlers - so it holds the
 * point in the DA window's local space while `where` stays global. Copying
 * `where` into it would put every click in the wrong place, off by the window
 * origin, which is why this needed more than a cast.
 *
 * GlobalToLocalWindow works off the window's contRgn rather than the current
 * port, so no SetPort dance is needed here.
 */
static int DA_EventViaInterface(DeskAccessory *da, const EventRecord *event)
{
    if (!da || !event) {
        return DESK_ERR_INVALID_PARAM;
    }
    if (!da->interface || !da->interface->processEvent) {
        return DESK_ERR_NONE;
    }

    Point local = event->where;
    if (da->window) {
        extern void GlobalToLocalWindow(WindowPtr window, Point *pt);
        GlobalToLocalWindow(da->window, &local);
    }

    DAEventInfo info;
    info.what      = event->what;
    info.message   = event->message;
    info.when      = event->when;
    info.where     = event->where;   /* global, as the caller supplied it */
    info.modifiers = event->modifiers;
    info.v         = local.v;
    info.h         = local.h;

    return da->interface->processEvent(da, &info);
}

static int DA_MenuViaInterface(DeskAccessory *da, SInt16 menuID, SInt16 itemID)
{
    if (!da) {
        return DESK_ERR_INVALID_PARAM;
    }
    if (!da->interface || !da->interface->handleMenu) {
        return DESK_ERR_NONE;
    }

    DAMenuInfo info;
    info.menuID = menuID;
    info.itemID = itemID;

    return da->interface->handleMenu(da, &info);
}

static int DA_LoadFromRegistry(DeskAccessory *da, const char *name)
{
    DARegistryEntry *entry = DA_FindRegistryEntry(name);
    if (!entry) {
        return DESK_ERR_NOT_FOUND;
    }

    /* Set DA properties from registry */
    da->type = entry->type;

    /* Whatever the entry names directly wins; it is the more specific. */
    da->open     = entry->open;
    da->close    = entry->close;
    da->event    = entry->event;
    da->idle     = entry->idle;
    da->activate = entry->activate;
    da->update   = entry->update;
    da->edit     = entry->edit;
    da->menu     = entry->menu;

    /* Otherwise go through the interface, if it supplied one. */
    da->interface = entry->interface;
    if (entry->interface) {
        if (!da->open)     da->open     = DA_OpenViaInterface;
        if (!da->close)    da->close    = DA_CloseViaInterface;
        if (!da->idle)     da->idle     = DA_IdleViaInterface;
        if (!da->activate) da->activate = DA_ActivateViaInterface;
        if (!da->update)   da->update   = DA_UpdateViaInterface;
        if (!da->edit)     da->edit     = entry->interface->doEdit;
        if (!da->event)    da->event    = DA_EventViaInterface;
        if (!da->menu)     da->menu     = DA_MenuViaInterface;
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

/*
 * Suspend a desk accessory
 */
int DeskManager_SuspendDA(SInt16 refNum)
{
    if (!g_deskMgrInitialized) {
        return DESK_ERR_SYSTEM_ERROR;
    }

    DeskAccessory *da = DA_GetByRefNum(refNum);
    if (!da) {
        return DESK_ERR_NOT_FOUND;
    }

    if (da->state == DA_STATE_CLOSED || da->state == DA_STATE_SUSPENDED) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Call DA's suspend callback if available */
    DARegistryEntry *entry = DA_FindRegistryEntry(da->name);
    if (entry && entry->interface && entry->interface->suspend) {
        entry->interface->suspend(da);
    }

    da->state = DA_STATE_SUSPENDED;
    return DESK_ERR_NONE;
}

/*
 * Resume a desk accessory
 */
int DeskManager_ResumeDA(SInt16 refNum)
{
    if (!g_deskMgrInitialized) {
        return DESK_ERR_SYSTEM_ERROR;
    }

    DeskAccessory *da = DA_GetByRefNum(refNum);
    if (!da) {
        return DESK_ERR_NOT_FOUND;
    }

    if (da->state != DA_STATE_SUSPENDED) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Call DA's resume callback if available */
    DARegistryEntry *entry = DA_FindRegistryEntry(da->name);
    if (entry && entry->interface && entry->interface->resume) {
        entry->interface->resume(da);
    }

    da->state = DA_STATE_OPEN;
    return DESK_ERR_NONE;
}

/*
 * Suspend all desk accessories
 */
int DeskManager_SuspendAll(void)
{
    if (!g_deskMgrInitialized) {
        return 0;
    }

    int count = 0;
    DeskAccessory *da = g_deskMgr.firstDA;
    while (da) {
        if (da->state == DA_STATE_OPEN || da->state == DA_STATE_ACTIVE) {
            if (DeskManager_SuspendDA(da->refNum) == DESK_ERR_NONE) {
                count++;
            }
        }
        da = da->next;
    }

    return count;
}

/*
 * Resume all desk accessories
 */
int DeskManager_ResumeAll(void)
{
    if (!g_deskMgrInitialized) {
        return 0;
    }

    int count = 0;
    DeskAccessory *da = g_deskMgr.firstDA;
    while (da) {
        if (da->state == DA_STATE_SUSPENDED) {
            if (DeskManager_ResumeDA(da->refNum) == DESK_ERR_NONE) {
                count++;
            }
        }
        da = da->next;
    }

    return count;
}
