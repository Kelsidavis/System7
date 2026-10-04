#include "MemoryMgr/MemoryManager.h"
#include <string.h>
/*
 * DeskAccessory.c - Native accessory registry and Window Manager ownership
 *
 * The registry stores callback tables for native accessories. Windows are
 * created and disposed through the Window Manager, not DRVR resources.
 *
 * Derived from ROM desk accessory patterns
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeskManager/DeskAccessory.h"
#include "DeskManager/DeskManager.h"
#include "QuickDraw/QuickDraw.h"
#include "WindowManager/WindowManager.h"


/* Global Registry */
static DARegistryEntry *g_daRegistry = NULL;

/* Internal Function Prototypes */
static DARegistryEntry *DA_AllocateRegistryEntry(void);
static void DA_FreeRegistryEntry(DARegistryEntry *entry);

/* Create a Window Manager record; procID passes through unchanged. */
int DA_CreateWindow(DeskAccessory *da, const DAWindowAttr *attr)
{
    if (!da || !attr) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Reopening without closing first would leak the old window. */
    if (da->window) {
        DA_DestroyWindow(da);
    }

    Rect bounds = attr->bounds;
    Str255 title;
    c2pstrcpy(title, attr->title);

    da->window = NewWindow(NULL, &bounds, (ConstStr255Param)title,
                           attr->visible, attr->procID, (WindowPtr)-1L,
                           attr->hasGoAway, attr->refCon);
    if (!da->window) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Negative windowKind routes clicks through SystemClick. */
    da->window->windowKind = (SInt16)(-(da->refNum));

    /* NewWindow's visible flag draws the frame; the DA still needs the port set
     * so whatever its initialiser draws next lands in the right place. */
    if (attr->visible) {
        ShowWindow(da->window);
        SelectWindow(da->window);
    }
    SetPort((GrafPtr)da->window);

    return DESK_ERR_NONE;
}

/*
 * Destroy DA window
 */
void DA_DestroyWindow(DeskAccessory *da)
{
    if (da && da->window) {
        /* Dispose the port, regions, and window-list membership together. */
        DisposeWindow(da->window);
        da->window = NULL;
    }
}

/*
 * Register a desk accessory type
 */
int DA_Register(const DARegistryEntry *entry)
{
    if (!entry || !entry->name[0] || !memchr(entry->name, '\0', sizeof(entry->name))) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Check if already registered */
    if (DA_FindRegistryEntry(entry->name)) {
        return DESK_ERR_ALREADY_OPEN;
    }

    /* Allocate new registry entry */
    DARegistryEntry *newEntry = DA_AllocateRegistryEntry();
    if (!newEntry) {
        return DESK_ERR_NO_MEMORY;
    }

    /* Copy entry data */
    *newEntry = *entry;
    newEntry->next = NULL;

    /* Add to registry */
    newEntry->next = g_daRegistry;
    g_daRegistry = newEntry;

    return DESK_ERR_NONE;
}

/*
 * Unregister a desk accessory type
 */
void DA_Unregister(const char *name)
{
    if (!name) {
        return;
    }

    DARegistryEntry *prev = NULL;
    DARegistryEntry *curr = g_daRegistry;

    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            /* Remove from list */
            if (prev) {
                prev->next = curr->next;
            } else {
                g_daRegistry = curr->next;
            }

            DA_FreeRegistryEntry(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

/*
 * Find DA registry entry by name
 */
DARegistryEntry *DA_FindRegistryEntry(const char *name)
{
    if (!name) {
        return NULL;
    }

    DARegistryEntry *entry = g_daRegistry;
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

/*
 * Get list of all registered DAs
 */
int DA_GetRegisteredDAs(DARegistryEntry **entries, int maxEntries)
{
    if (!entries || maxEntries <= 0) {
        return 0;
    }

    int count = 0;
    DARegistryEntry *entry = g_daRegistry;

    while (entry && count < maxEntries) {
        entries[count++] = entry;
        entry = entry->next;
    }

    return count;
}

/* Internal Functions */

/*
 * Allocate a new registry entry
 */
static DARegistryEntry *DA_AllocateRegistryEntry(void)
{
    return NewPtrClear(sizeof(DARegistryEntry));
}

/*
 * Free a registry entry
 */
static void DA_FreeRegistryEntry(DARegistryEntry *entry)
{
    if (entry) {
        DisposePtr((Ptr)entry);
    }
}
