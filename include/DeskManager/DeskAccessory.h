#ifndef DESKACCESSORY_H
#define DESKACCESSORY_H

/*
 * DeskAccessory.h - Desk Accessory Structures and Management
 *
 * Defines the structures and interfaces for implementing desk accessories.
 * DAs are small utility programs that provide specific functionality and
 * integrate with the Mac OS System menu and event handling.
 *
 * Based on Apple's Macintosh Toolbox desk accessory driver model
 */

#include "DeskManager.h"
#include <stdint.h>
#include <stdbool.h>

/* DA Resource Types */
#define DA_RESOURCE_TYPE_DRVR   'DRVR'      /* Driver resource */
#define DA_RESOURCE_TYPE_WIND   'WIND'      /* Window template */
#define DA_RESOURCE_TYPE_DITL   'DITL'      /* Dialog item list */
#define DA_RESOURCE_TYPE_DLOG   'DLOG'      /* Dialog template */
#define DA_RESOURCE_TYPE_MENU   'MENU'      /* Menu resource */
#define DA_RESOURCE_TYPE_STR    'STR '      /* String resource */
#define DA_RESOURCE_TYPE_ICN    'ICN#'      /* Icon resource */

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
typedef struct DAWindowAttr {
    int16_t     procID;         /* Window definition ID */
    bool        visible;        /* Initially visible */
    bool        goAwayFlag;     /* Has close box */
    int32_t     refCon;         /* Reference constant */
    Rect        bounds;         /* Window bounds */
    char        title[256];     /* Window title */
} DAWindowAttr;

/* DA Driver Header (matches Mac OS DRVR format) */
typedef struct DADriverHeader {
    uint16_t    flags;          /* Driver flags */
    uint16_t    delay;          /* Delay for periodic calls */
    uint16_t    mask;           /* Event mask */
    uint16_t    menu;           /* Menu ID */
    uint16_t    open;           /* Offset to open routine */
    uint16_t    prime;          /* Offset to prime routine */
    uint16_t    control;        /* Offset to control routine */
    uint16_t    status;         /* Offset to status routine */
    uint16_t    close;          /* Offset to close routine */
    char        name[256];      /* Driver name (Pascal string) */
} DADriverHeader;

/* DA Control Parameter Block */
typedef struct DAControlPB {
    int16_t     ioResult;       /* Result code */
    int32_t     ioCompletion;   /* Completion routine */
    int16_t     ioVRefNum;      /* Volume reference */
    int16_t     ioCRefNum;      /* Reference number */
    int16_t     csCode;         /* Control/status code */
    void        *csParam;       /* Parameters */
} DAControlPB;

/* DA Event Handling */
typedef struct DAEventInfo {
    int16_t     what;           /* Event type */
    int32_t     message;        /* Event message */
    int32_t     when;           /* Event time */
    Point       where;          /* Event location */
    int16_t     modifiers;      /* Modifier keys */
} DAEventInfo;

/* DA Menu Information */
typedef struct DAMenuInfo {
    int16_t     menuID;         /* Menu ID */
    int16_t     itemID;         /* Item ID */
    char        itemText[256];  /* Item text */
    bool        enabled;        /* Item enabled */
    bool        checked;        /* Item checked */
} DAMenuInfo;

/* DA Implementation Interface */
typedef struct DAInterface {
    /* Required functions */
    int (*initialize)(DeskAccessory *da, const DADriverHeader *header);
    int (*terminate)(DeskAccessory *da);

    /* Event handling */
    int (*processEvent)(DeskAccessory *da, const DAEventInfo *event);
    int (*handleMenu)(DeskAccessory *da, const DAMenuInfo *menu);
    int (*doEdit)(DeskAccessory *da, int16_t editCmd);

    /* Periodic processing */
    int (*idle)(DeskAccessory *da);
    int (*updateCursor)(DeskAccessory *da, Point mousePos);

    /* Window management */
    int (*activate)(DeskAccessory *da, bool active);
    int (*update)(DeskAccessory *da, const Rect *updateRect);
    int (*resize)(DeskAccessory *da, const Rect *newBounds);

    /* Optional functions */
    int (*suspend)(DeskAccessory *da);
    int (*resume)(DeskAccessory *da);
    int (*sleep)(DeskAccessory *da);
    int (*wakeup)(DeskAccessory *da);
} DAInterface;

/* DA Registry Entry */
typedef struct DARegistryEntry {
    char                name[DA_NAME_LENGTH + 1];
    DAType              type;
    int16_t             resourceID;
    uint16_t            flags;
    DAInterface         *interface;
    void                *initData;

    struct DARegistryEntry *next;
} DARegistryEntry;

/* DA Resource Management */

/**
 * Load DA driver header from resources
 * @param resourceID Resource ID of the DRVR
 * @param header Pointer to header structure to fill
 * @return 0 on success, negative on error
 */
int DA_LoadDriverHeader(int16_t resourceID, DADriverHeader *header);

/**
 * Load DA window template
 * @param resourceID Resource ID of the WIND
 * @param attr Pointer to window attributes to fill
 * @return 0 on success, negative on error
 */
int DA_LoadWindowTemplate(int16_t resourceID, DAWindowAttr *attr);

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
int DA_Control(DeskAccessory *da, int16_t controlCode, DAControlPB *params);

/**
 * Get status from DA
 * @param da Pointer to DA
 * @param statusCode Status code
 * @param params Status parameters
 * @return Result from DA
 */
int DA_Status(DeskAccessory *da, int16_t statusCode, DAControlPB *params);

/* DA Utility Functions */

/**
 * Convert Pascal string to C string
 * @param pascalStr Pascal string (length byte + string)
 * @param cStr Buffer for C string
 * @param maxLen Maximum length of C string buffer
 */
void DA_PascalToCString(const uint8_t *pascalStr, char *cStr, int maxLen);

/**
 * Convert C string to Pascal string
 * @param cStr C string
 * @param pascalStr Buffer for Pascal string
 * @param maxLen Maximum length of Pascal string buffer
 */
void DA_CStringToPascal(const char *cStr, uint8_t *pascalStr, int maxLen);

/**
 * Check if point is in rectangle
 * @param point Point to test
 * @param rect Rectangle to test against
 * @return true if point is in rectangle
 */
bool DA_PointInRect(Point point, const Rect *rect);

/**
 * Calculate rectangle intersection
 * @param rect1 First rectangle
 * @param rect2 Second rectangle
 * @param result Intersection rectangle
 * @return true if rectangles intersect
 */
bool DA_SectRect(const Rect *rect1, const Rect *rect2, Rect *result);

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