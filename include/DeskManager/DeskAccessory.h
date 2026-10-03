#ifndef DESKACCESSORY_H
#define DESKACCESSORY_H

#include "SystemTypes.h"
#include "DeskManagerTypes.h"

/* Forward declarations */


/*
 * DeskAccessory.h - Desk Accessory Structures and Management
 *
 * Defines the structures and interfaces for implementing desk accessories.
 * DAs are small utility programs that provide specific functionality and
 * integrate with the Mac OS System menu and event handling.
 *
 * Derived from ROM desk accessory model
 */

#include "DeskManager.h"

/* DA Resource Types */
#define DA_RESOURCE_TYPE_DRVR   FOURCC('D', 'R', 'V', 'R') /* Driver resource */
#define DA_RESOURCE_TYPE_WIND   FOURCC('W', 'I', 'N', 'D') /* Window template */
#define DA_RESOURCE_TYPE_DITL   FOURCC('D', 'I', 'T', 'L') /* Dialog item list */
#define DA_RESOURCE_TYPE_DLOG   FOURCC('D', 'L', 'O', 'G') /* Dialog template */
#define DA_RESOURCE_TYPE_MENU   FOURCC('M', 'E', 'N', 'U') /* Menu resource */
#define DA_RESOURCE_TYPE_STR    FOURCC('S', 'T', 'R', ' ') /* String resource */
#define DA_RESOURCE_TYPE_ICN    FOURCC('I', 'C', 'N', '#') /* Icon resource */

/* Standard DA Resource IDs */
#define DA_RESID_CALCULATOR     4           /* Calculator DA */
#define DA_RESID_KEYCAPS        11          /* Key Caps DA */
#define DA_RESID_ALARM          15          /* Alarm Clock DA */
#define DA_RESID_CHOOSER        7           /* Chooser DA */

/* DA Driver Flags */
#define DA_FLAG_NEEDS_EVENTS    0x0001      /* DA processes events */
#define DA_FLAG_NEEDS_TIME      0x0002      /* DA needs periodic calls */
#define DA_FLAG_NEEDS_CURSOR    0x0004      /* DA controls cursor */
#define DA_FLAG_NEEDS_MENU      0x0008      /* DA has menus */
#define DA_FLAG_NEEDS_EDIT      0x0010      /* DA supports edit ops */
#define DA_FLAG_MODAL           0x0020      /* DA is modal */
#define DA_FLAG_SYSTEM_HEAP     0x0040      /* DA uses system heap */


/* DA Window Attributes */

/* DA Driver Header (matches Mac OS DRVR format) */

/* DA Control Parameter Block */

/* DA Event Handling */

/* DA Menu Information */

/* DA Implementation Interface */

/* DA Registry Entry */

/* DA Resource Management */

/**
 * Load DA driver header from resources
 * @param resourceID Resource ID of the DRVR
 * @param header Pointer to header structure to fill
 * @return 0 on success, negative on error
 */
int DA_LoadDriverHeader(SInt16 resourceID, DADriverHeader *header);

/**
 * Load DA window template
 * @param resourceID Resource ID of the WIND
 * @param attr Pointer to window attributes to fill
 * @return 0 on success, negative on error
 */
int DA_LoadWindowTemplate(SInt16 resourceID, DAWindowAttr *attr);

/**
 * Create DA window
 * @param da Pointer to desk accessory
 * @param attr Window attributes
 * @return 0 on success, negative on error
 */
int DA_CreateWindow(DeskAccessory *da, const DAWindowAttr *attr);

/**
 * Destroy DA window
 * @param da Pointer to desk accessory
 */
void DA_DestroyWindow(DeskAccessory *da);

/* DA Registry Functions */

/**
 * Register a desk accessory type
 * @param entry Registry entry for the DA
 * @return 0 on success, negative on error
 */
int DA_Register(const DARegistryEntry *entry);

/**
 * Unregister a desk accessory type
 * @param name Name of the DA to unregister
 */
void DA_Unregister(const char *name);

/**
 * Find DA registry entry by name
 * @param name Name of the DA
 * @return Pointer to registry entry or NULL if not found
 */
DARegistryEntry *DA_FindRegistryEntry(const char *name);

/**
 * Get list of all registered DAs
 * @param entries Array to fill with entry pointers
 * @param maxEntries Maximum number of entries
 * @return Number of entries returned
 */
int DA_GetRegisteredDAs(DARegistryEntry **entries, int maxEntries);

/* DA Instance Management */

/**
 * Create a new DA instance
 * @param name Name of the DA to create
 * @return Pointer to new DA instance or NULL on error
 */
DeskAccessory *DA_CreateInstance(const char *name);

/**
 * Destroy a DA instance
 * @param da Pointer to DA instance
 */
void DA_DestroyInstance(DeskAccessory *da);

/**
 * Initialize a DA instance
 * @param da Pointer to DA instance
 * @return 0 on success, negative on error
 */
int DA_InitializeInstance(DeskAccessory *da);

/**
 * Terminate a DA instance
 * @param da Pointer to DA instance
 * @return 0 on success, negative on error
 */
int DA_TerminateInstance(DeskAccessory *da);

/* DA Communication */

/**
 * Send control message to DA
 * @param da Pointer to DA
 * @param controlCode Control code
 * @param params Control parameters
 * @return Result from DA
 */
int DA_Control(DeskAccessory *da, SInt16 controlCode, DAControlPB *params);

/**
 * Get status from DA
 * @param da Pointer to DA
 * @param statusCode Status code
 * @param params Status parameters
 * @return Result from DA
 */
int DA_Status(DeskAccessory *da, SInt16 statusCode, DAControlPB *params);

/* DA Utility Functions */

/**
 * Convert Pascal string to C string
 * @param pascalStr Pascal string (length byte + string)
 * @param cStr Buffer for C string
 * @param maxLen Maximum length of C string buffer
 */
void DA_PascalToCString(const UInt8 *pascalStr, char *cStr, int maxLen);

/**
 * Convert C string to Pascal string
 * @param cStr C string
 * @param pascalStr Buffer for Pascal string
 * @param maxLen Maximum length of Pascal string buffer
 */
void DA_CStringToPascal(const char *cStr, UInt8 *pascalStr, int maxLen);

/**
 * Check if point is in rectangle
 * @param point Point to test
 * @param rect Rectangle to test against
 * @return true if point is in rectangle
 */
Boolean DA_PointInRect(Point point, const Rect *rect);

/**
 * Calculate rectangle intersection
 * @param rect1 First rectangle
 * @param rect2 Second rectangle
 * @param result Intersection rectangle
 * @return true if rectangles intersect
 */
Boolean DA_SectRect(const Rect *rect1, const Rect *rect2, Rect *result);

/* Standard Control Codes */
#define DA_CONTROL_INITIALIZE   1       /* Initialize DA */
#define DA_CONTROL_TERMINATE    2       /* Terminate DA */
#define DA_CONTROL_ACTIVATE     3       /* Activate/deactivate */
#define DA_CONTROL_UPDATE       4       /* Update display */
#define DA_CONTROL_SUSPEND      5       /* Suspend DA */
#define DA_CONTROL_RESUME       6       /* Resume DA */

/* Standard Status Codes */
#define DA_STATUS_STATE         1       /* Get DA state */
#define DA_STATUS_VERSION       2       /* Get DA version */
#define DA_STATUS_INFO          3       /* Get DA info */

#endif /* DESKACCESSORY_H */
