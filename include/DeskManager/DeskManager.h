#ifndef DESKMANAGER_H
#define DESKMANAGER_H

/*
 * DeskManager.h - Main Desk Manager API
 *
 * Provides the core API for managing desk accessories (DAs) in System 7.1.
 * Desk accessories are small utility programs that provide essential daily
 * computing functions like Calculator, Key Caps, Alarm Clock, and Chooser.
 *
 * Based on Apple's Macintosh Toolbox Desk Manager from System 7.1
 */

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
typedef struct WindowRecord WindowRecord;
typedef struct EventRecord EventRecord;
typedef struct Rect Rect;
typedef struct Point Point;

/* Desk Manager Constants */
#define DESK_MGR_VERSION        0x0701      /* System 7.1 */
#define MAX_DESK_ACCESSORIES    64          /* Maximum concurrent DAs */
#define DA_NAME_LENGTH          255         /* Maximum DA name length */

/* Desk Accessory Messages */
typedef enum {
    DA_MSG_EVENT        = 64,       /* Process event */
    DA_MSG_RUN          = 65,       /* Periodic processing */
    DA_MSG_CURSOR       = 66,       /* Update cursor */
    DA_MSG_MENU         = 67,       /* Handle menu selection */
    DA_MSG_UNDO         = 68,       /* Undo operation */
    DA_MSG_CUT          = 70,       /* Cut operation */
    DA_MSG_COPY         = 71,       /* Copy operation */
    DA_MSG_PASTE        = 72,       /* Paste operation */
    DA_MSG_CLEAR        = 73,       /* Clear operation */
    DA_MSG_GOODBYE      = -1        /* DA closing */
} DAMessage;

/* Desk Accessory Types */
typedef enum {
    DA_TYPE_CALCULATOR  = 1,        /* Calculator utility */
    DA_TYPE_KEYCAPS     = 2,        /* Key Caps character map */
    DA_TYPE_ALARM       = 3,        /* Alarm Clock */
    DA_TYPE_CHOOSER     = 4,        /* Device chooser */
    DA_TYPE_CUSTOM      = 100       /* Custom DA base */
} DAType;

/* Desk Accessory States */
typedef enum {
    DA_STATE_CLOSED     = 0,        /* DA not loaded */
    DA_STATE_OPEN       = 1,        /* DA loaded and active */
    DA_STATE_SUSPENDED  = 2,        /* DA loaded but inactive */
    DA_STATE_ERROR      = -1        /* DA in error state */
} DAState;

/* Desk Accessory Control Block */
typedef struct DeskAccessory {
    int16_t         refNum;         /* Reference number */
    DAType          type;           /* DA type */
    DAState         state;          /* Current state */
    char            name[DA_NAME_LENGTH + 1];  /* DA name */
    WindowRecord    *window;        /* DA window */
    void            *driverData;    /* Driver-specific data */
    void            *userData;      /* User data */

    /* Function pointers for DA operations */
    int (*open)(struct DeskAccessory *da);
    int (*close)(struct DeskAccessory *da);
    int (*activate)(struct DeskAccessory *da, bool active);
    int (*event)(struct DeskAccessory *da, const EventRecord *event);
    int (*menu)(struct DeskAccessory *da, int menuID, int itemID);
    int (*idle)(struct DeskAccessory *da);
    int (*update)(struct DeskAccessory *da);

    /* Linked list management */
    struct DeskAccessory *next;
    struct DeskAccessory *prev;
} DeskAccessory;

/* Desk Manager State */
typedef struct DeskManagerState {
    DeskAccessory   *firstDA;       /* First DA in list */
    DeskAccessory   *activeDA;      /* Currently active DA */
    int16_t         nextRefNum;     /* Next available ref num */
    int16_t         numDAs;         /* Number of open DAs */
    bool            systemMenuEnabled;  /* System menu enabled */
    void            *systemMenuHandle;  /* System menu handle */
} DeskManagerState;

/* Core Desk Manager Functions */

/**
 * Initialize the Desk Manager
 * @return 0 on success, negative on error
 */
int DeskManager_Initialize(void);

/**
 * Shutdown the Desk Manager
 */
void DeskManager_Shutdown(void);

/**
 * Open a desk accessory by name
 * @param name Name of the desk accessory
 * @return Reference number on success, negative on error
 */
int16_t OpenDeskAcc(const char *name);

/**
 * Close a desk accessory
 * @param refNum Reference number of the DA to close
 */
void CloseDeskAcc(int16_t refNum);

/**
 * Handle system-level events for desk accessories
 * @param event Pointer to event record
 * @return true if event was handled by a DA
 */
bool SystemEvent(const EventRecord *event);

/**
 * Handle mouse clicks in system areas
 * @param event Pointer to event record
 * @param window Window where click occurred
 */
void SystemClick(const EventRecord *event, WindowRecord *window);

/**
 * Perform periodic processing for all DAs
 */
void SystemTask(void);

/**
 * Handle system menu selections
 * @param menuResult Menu selection result
 */
void SystemMenu(int32_t menuResult);

/**
 * Handle system edit operations
 * @param editCmd Edit command (cut, copy, paste, etc.)
 * @return true if command was handled
 */
bool SystemEdit(int16_t editCmd);

/* Desk Accessory Management Functions */

/**
 * Get desk accessory by reference number
 * @param refNum Reference number
 * @return Pointer to DA or NULL if not found
 */
DeskAccessory *DA_GetByRefNum(int16_t refNum);

/**
 * Get desk accessory by name
 * @param name DA name
 * @return Pointer to DA or NULL if not found
 */
DeskAccessory *DA_GetByName(const char *name);

/**
 * Get currently active desk accessory
 * @return Pointer to active DA or NULL if none
 */
DeskAccessory *DA_GetActive(void);

/**
 * Set active desk accessory
 * @param da Pointer to DA to activate
 * @return 0 on success, negative on error
 */
int DA_SetActive(DeskAccessory *da);

/**
 * Send message to desk accessory
 * @param da Pointer to DA
 * @param message Message type
 * @param param1 First parameter
 * @param param2 Second parameter
 * @return Result from DA
 */
int DA_SendMessage(DeskAccessory *da, DAMessage message,
                   void *param1, void *param2);

/* System Integration Functions */

/**
 * Add DA to system menu (Apple menu)
 * @param da Pointer to DA
 * @return 0 on success, negative on error
 */
int SystemMenu_AddDA(DeskAccessory *da);

/**
 * Remove DA from system menu
 * @param da Pointer to DA
 */
void SystemMenu_RemoveDA(DeskAccessory *da);

/**
 * Update system menu
 */
void SystemMenu_Update(void);

/* Utility Functions */

/**
 * Get Desk Manager version
 * @return Version number (0x0701 for System 7.1)
 */
uint16_t DeskManager_GetVersion(void);

/**
 * Get number of open desk accessories
 * @return Number of open DAs
 */
int16_t DeskManager_GetDACount(void);

/**
 * Check if a DA is installed
 * @param name DA name
 * @return true if DA is available
 */
bool DeskManager_IsDAAvailable(const char *name);

/* Built-in Desk Accessories */

/**
 * Register built-in desk accessories
 * @return 0 on success, negative on error
 */
int DeskManager_RegisterBuiltinDAs(void);

/* Error Codes */
#define DESK_ERR_NONE           0       /* No error */
#define DESK_ERR_NO_MEMORY      -1      /* Out of memory */
#define DESK_ERR_NOT_FOUND      -2      /* DA not found */
#define DESK_ERR_ALREADY_OPEN   -3      /* DA already open */
#define DESK_ERR_INVALID_PARAM  -4      /* Invalid parameter */
#define DESK_ERR_SYSTEM_ERROR   -5      /* System error */
#define DESK_ERR_DA_ERROR       -6      /* DA-specific error */

#endif /* DESKMANAGER_H */