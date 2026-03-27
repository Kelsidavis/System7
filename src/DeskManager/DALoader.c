#include "MemoryMgr/MemoryManager.h"
// #include "CompatibilityFix.h" // Removed
#include <stdlib.h>
#include <string.h>
/*
 * DALoader.c - Desk Accessory Resource Loading and Management
 *
 * Handles loading and management of desk accessory resources including
 * driver resources (DRVR), window templates, and other DA-specific resources.
 * Provides abstraction for resource loading across different platforms.
 *
 * Derived from ROM desk accessory patterns
 */

#include "SystemTypes.h"
#include "System71StdLib.h"

#include "DeskManager/DeskAccessory.h"
#include "DeskManager/DeskManager.h"


/* Global Registry */
static DARegistryEntry *g_daRegistry = NULL;
static int g_registryCount = 0;

/* Internal Function Prototypes */
static DARegistryEntry *DA_AllocateRegistryEntry(void);
static void DA_FreeRegistryEntry(DARegistryEntry *entry);
static int DA_LoadResourceData(SInt16 resourceID, UInt32 resourceType,
                               void **data, size_t *size);
static void DA_FreeResourceData(void *data);

/*
 * Load DA driver header from resources
 */
int DA_LoadDriverHeader(SInt16 resourceID, DADriverHeader *header)
{
    if (!header) {
        return DESK_ERR_INVALID_PARAM;
    }

    void *data;
    size_t size;
    int result = DA_LoadResourceData(resourceID, DA_RESOURCE_TYPE_DRVR,
                                     &data, &size);
    if (result != 0) {
        return result;
    }

    /* Parse DRVR resource format */
    if (size < sizeof(DADriverHeader)) {
        DA_FreeResourceData(data);
        return DESK_ERR_SYSTEM_ERROR;
    }

    memcpy(header, data, sizeof(DADriverHeader));
    DA_FreeResourceData(data);

    return DESK_ERR_NONE;
}

/*
 * Load DA window template
 */
int DA_LoadWindowTemplate(SInt16 resourceID, DAWindowAttr *attr)
{
    if (!attr) {
        return DESK_ERR_INVALID_PARAM;
    }

    void *data;
    size_t size;
    int result = DA_LoadResourceData(resourceID, DA_RESOURCE_TYPE_WIND,
                                     &data, &size);
    if (result != 0) {
        return result;
    }

    /* Parse WIND resource format (simplified) */
    memset(attr, 0, sizeof(DAWindowAttr));

    /* Set default values */
    attr->bounds.left = 100;
    attr->bounds.top = 100;
    attr->bounds.right = 400;
    attr->bounds.bottom = 300;
    attr->procID = 0;  /* Standard window */
    attr->visible = true;
    attr->hasGoAway = true;
    strncpy(attr->title, "Desk Accessory", sizeof(attr->title) - 1);
    attr->title[sizeof(attr->title) - 1] = '\0';

    DA_FreeResourceData(data);
    return DESK_ERR_NONE;
}

/*
 * Create DA window
 */
int DA_CreateWindow(DeskAccessory *da, const DAWindowAttr *attr)
{
    if (!da || !attr) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* In a real implementation, this would create a platform-specific window */
    /* For now, just store the attributes */
    da->window = NewPtr(sizeof(DAWindowAttr));
    if (!da->window) {
        return DESK_ERR_NO_MEMORY;
    }

    memcpy(da->window, attr, sizeof(DAWindowAttr));
    return DESK_ERR_NONE;
}

/*
 * Destroy DA window
 */
void DA_DestroyWindow(DeskAccessory *da)
{
    if (da && da->window) {
        /* In a real implementation, would destroy platform window */
        DisposePtr((Ptr)da->window);
        da->window = NULL;
    }
}

/*
 * Register a desk accessory type
 */
int DA_Register(const DARegistryEntry *entry)
{
    if (!entry || !entry->name[0]) {
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
    g_registryCount++;

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
            g_registryCount--;
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

/*
 * Create a new DA instance
 */
DeskAccessory *DA_CreateInstance(const char *name)
{
    if (!name) {
        return NULL;
    }

    DARegistryEntry *entry = DA_FindRegistryEntry(name);
    if (!entry) {
        return NULL;
    }

    DeskAccessory *da = NewPtrClear(sizeof(DeskAccessory));
    if (!da) {
        return NULL;
    }

    /* Initialize basic properties */
    strncpy(da->name, name, DA_NAME_LENGTH);
    da->name[DA_NAME_LENGTH] = '\0';
    da->type = entry->type;
    da->state = DA_STATE_CLOSED;

    return da;
}

/*
 * Destroy a DA instance
 */
void DA_DestroyInstance(DeskAccessory *da)
{
    if (da) {
        DA_DestroyWindow(da);
        DisposePtr((Ptr)da->userData);
        DisposePtr((Ptr)da->driverData);
        DisposePtr((Ptr)da);
    }
}

/*
 * Initialize a DA instance
 */
int DA_InitializeInstance(DeskAccessory *da)
{
    if (!da) {
        return DESK_ERR_INVALID_PARAM;
    }

    DARegistryEntry *entry = DA_FindRegistryEntry(da->name);
    if (!entry || !entry->interface) {
        return DESK_ERR_NOT_FOUND;
    }

    /* Initialize using the interface */
    if (entry->interface->initialize) {
        DADriverHeader header;
        int result = DA_LoadDriverHeader(entry->resourceID, &header);
        if (result != 0) {
            return result;
        }

        result = entry->interface->initialize(da, &header);
        if (result != 0) {
            return result;
        }
    }

    da->state = DA_STATE_OPEN;
    return DESK_ERR_NONE;
}

/*
 * Terminate a DA instance
 */
int DA_TerminateInstance(DeskAccessory *da)
{
    if (!da) {
        return DESK_ERR_INVALID_PARAM;
    }

    DARegistryEntry *entry = DA_FindRegistryEntry(da->name);
    if (entry && entry->interface && entry->interface->terminate) {
        entry->interface->terminate(da);
    }

    da->state = DA_STATE_CLOSED;
    return DESK_ERR_NONE;
}

/*
 * Send control message to DA
 */
int DA_Control(DeskAccessory *da, SInt16 controlCode, DAControlPB *params)
{
    if (!da || !params) {
        return DESK_ERR_INVALID_PARAM;
    }

    /* Handle standard control codes */
    switch (controlCode) {
        case DA_CONTROL_INITIALIZE:
            return DA_InitializeInstance(da);

        case DA_CONTROL_TERMINATE:
            return DA_TerminateInstance(da);

        case DA_CONTROL_ACTIVATE:
            if (da->activate) {
                Boolean active = (params->csParam[0] != 0);
                da->activate(da, active);
            }
            break;

        case DA_CONTROL_UPDATE:
            if (da->update) {
                da->update(da);
            }
            break;

        case DA_CONTROL_SUSPEND:
            da->state = DA_STATE_SUSPENDED;
            break;

        case DA_CONTROL_RESUME:
            da->state = DA_STATE_OPEN;
            break;

        default:
            return DESK_ERR_INVALID_PARAM;
    }

    return DESK_ERR_NONE;
}

/*
 * Get status from DA
 */
int DA_Status(DeskAccessory *da, SInt16 statusCode, DAControlPB *params)
{
    if (!da || !params) {
        return DESK_ERR_INVALID_PARAM;
    }

    switch (statusCode) {
        case DA_STATUS_STATE:
            *(DAState *)params->csParam = da->state;
            break;

        case DA_STATUS_VERSION:
            *(UInt16 *)params->csParam = DESK_MGR_VERSION;
            break;

        case DA_STATUS_INFO:
            /* Return DA information */
            break;

        default:
            return DESK_ERR_INVALID_PARAM;
    }

    return DESK_ERR_NONE;
}

/*
 * Convert Pascal string to C string
 */
void DA_PascalToCString(const UInt8 *pascalStr, char *cStr, int maxLen)
{
    if (!pascalStr || !cStr || maxLen <= 0) {
        return;
    }

    int len = pascalStr[0];
    if (len >= maxLen) {
        len = maxLen - 1;
    }

    memcpy(cStr, &pascalStr[1], len);
    cStr[len] = '\0';
}

/*
 * Convert C string to Pascal string
 */
void DA_CStringToPascal(const char *cStr, UInt8 *pascalStr, int maxLen)
{
    if (!cStr || !pascalStr || maxLen <= 1) {
        return;
    }

    int len = strlen(cStr);
    if (len >= maxLen) {
        len = maxLen - 1;
    }

    pascalStr[0] = len;
    memcpy(&pascalStr[1], cStr, len);
}

/*
 * Check if point is in rectangle
 */
Boolean DA_PointInRect(Point point, const Rect *rect)
{
    if (!rect) {
        return false;
    }

    return (point.h >= rect->left && point.h < rect->right &&
            point.v >= rect->top && point.v < rect->bottom);
}

/*
 * Calculate rectangle intersection
 */
Boolean DA_SectRect(const Rect *rect1, const Rect *rect2, Rect *result)
{
    if (!rect1 || !rect2 || !result) {
        return false;
    }

    result->left = (rect1->left > rect2->left) ? rect1->left : rect2->left;
    result->top = (rect1->top > rect2->top) ? rect1->top : rect2->top;
    result->right = (rect1->right < rect2->right) ? rect1->right : rect2->right;
    result->bottom = (rect1->bottom < rect2->bottom) ? rect1->bottom : rect2->bottom;

    return (result->left < result->right && result->top < result->bottom);
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

/*
 * Load resource data (platform-specific)
 */
static int DA_LoadResourceData(SInt16 resourceID, UInt32 resourceType,
                               void **data, size_t *size)
{
    /* For now, return default templates based on resource type */

    switch (resourceType) {
        case DA_RESOURCE_TYPE_WIND:
            /* Default window template for DA windows */
            *size = 64;
            break;

        case DA_RESOURCE_TYPE_DLOG:
            /* Default dialog template */
            *size = 128;
            break;

        case DA_RESOURCE_TYPE_DITL:
            /* Default dialog item list */
            *size = 256;
            break;

        case DA_RESOURCE_TYPE_MENU:
            /* Default menu template */
            *size = 512;
            break;

        case DA_RESOURCE_TYPE_DRVR:
            /* Driver resource */
            *size = 32;
            break;

        default:
            *size = 256;
            break;
    }

    *data = NewPtr(*size);
    if (!*data) {
        return DESK_ERR_NO_MEMORY;
    }

    memset(*data, 0, *size);

    /* Initialize with default values based on resource type */
    if (resourceType == DA_RESOURCE_TYPE_WIND) {
        /* Default WIND template values */
        UInt16 *wind_data = (UInt16 *)*data;
        if (*size >= 8) {
            wind_data[0] = 100;  /* top */
            wind_data[1] = 100;  /* left */
            wind_data[2] = 400;  /* bottom */
            wind_data[3] = 300;  /* right */
        }
    }

    return DESK_ERR_NONE;
}

/*
 * Free resource data
 */
static void DA_FreeResourceData(void *data)
{
    DisposePtr((Ptr)data);
}
