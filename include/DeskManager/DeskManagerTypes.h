#ifndef DESKMANAGER_TYPES_H_DEFS
#define DESKMANAGER_TYPES_H_DEFS

#include "../MacTypes.h"
#include "../WindowManager/WindowManager.h"
#include "../EventManager/EventManager.h"

/* Desk Accessory Constants - Note: Additional constants are in DeskManager.h */
#define DA_FLAG_NEEDS_EVENTS    0x0001
#define DA_FLAG_NEEDS_TIME      0x0002
#define DA_FLAG_NEEDS_CURSOR    0x0004
#define DA_FLAG_NEEDS_MENU      0x0008
#define DA_FLAG_NEEDS_EDIT      0x0010
#define DA_FLAG_MODAL           0x0020
#define DA_FLAG_SYSTEM_HEAP     0x0040

/* Forward declarations */
typedef struct DeskAccessory DeskAccessory;
typedef struct DADriverHeader DADriverHeader;
typedef struct DAWindowAttr DAWindowAttr;
typedef struct DARegistryEntry DARegistryEntry;
typedef struct DeskManagerState DeskManagerState;
typedef struct DAControlPB DAControlPB;
typedef struct DAEventInfo DAEventInfo;
typedef struct DAMenuInfo DAMenuInfo;
typedef struct DAInterface DAInterface;

/* DA State */
typedef enum {
    DA_STATE_CLOSED = 0,
    DA_STATE_OPEN = 1,
    DA_STATE_ACTIVE = 2,
    DA_STATE_SUSPENDED = 3
} DAState;

/* DA Message Types */
typedef enum {
    DA_MSG_OPEN = 0,
    DA_MSG_CLOSE = 1,
    DA_MSG_EVENT = 2,
    DA_MSG_IDLE = 3,
    DA_MSG_ACTIVATE = 4,
    DA_MSG_DEACTIVATE = 5,
    DA_MSG_UPDATE = 6,
    DA_MSG_CUT = 7,
    DA_MSG_COPY = 8,
    DA_MSG_PASTE = 9,
    DA_MSG_CLEAR = 10,
    DA_MSG_UNDO = 11,
    DA_MSG_RUN = 12,
    DA_MSG_MENU = 13,
    DA_MSG_GOODBYE = 14
} DAMessage;

/* DA Window Attributes */
struct DAWindowAttr {
    Rect bounds;
    char title[256];
    Boolean visible;
    Boolean hasGoAway;
    SInt16 procID;
    SInt32 refCon;
};

/* DA Control Parameter Block */
struct DAControlPB {
    void *ioCompletion;
    SInt16 ioResult;
    char *ioNamePtr;
    SInt16 ioVRefNum;
    SInt16 ioCRefNum;
    SInt16 csCode;
    SInt32 csParam[11];
};

/* DA Driver Header */
struct DADriverHeader {
    UInt16 flags;
    UInt16 delay;
    UInt16 eventMask;
    UInt16 menuID;
    UInt16 openOffset;
    UInt16 primeOffset;
    UInt16 controlOffset;
    UInt16 statusOffset;
    UInt16 closeOffset;
    char name[32];
};

/* DA Event Information */
typedef struct DAEventInfo {
    SInt16 what;
    SInt32 message;
    unsigned long when;
    Point where;
    SInt16 modifiers;
    SInt16 v;
    SInt16 h;
} DAEventInfo;

/* DA Menu Information */
typedef struct DAMenuInfo {
    SInt16 menuID;
    SInt16 itemID;
} DAMenuInfo;

/* DA Interface Structure */
typedef struct DAInterface {
    int (*initialize)(DeskAccessory *da, const DADriverHeader *header);
    int (*terminate)(DeskAccessory *da);
    int (*processEvent)(DeskAccessory *da, const DAEventInfo *event);
    int (*handleMenu)(DeskAccessory *da, const DAMenuInfo *menu);
    int (*doEdit)(DeskAccessory *da, DAMessage editOp);
    int (*idle)(DeskAccessory *da);
    int (*updateCursor)(DeskAccessory *da);
    int (*activate)(DeskAccessory *da, Boolean active);
    int (*update)(DeskAccessory *da);
    int (*resize)(DeskAccessory *da, Rect newBounds);
    int (*suspend)(DeskAccessory *da);
    int (*resume)(DeskAccessory *da);
    int (*sleep)(DeskAccessory *da);
    int (*wakeup)(DeskAccessory *da);
} DAInterface;

/* DA Function Pointers */
typedef int (*DAOpenProc)(DeskAccessory *da);
typedef void (*DACloseProc)(DeskAccessory *da);
typedef int (*DAEventProc)(DeskAccessory *da, const EventRecord *event);
typedef void (*DAIdleProc)(DeskAccessory *da);
typedef void (*DAActivateProc)(DeskAccessory *da, Boolean active);
typedef void (*DAUpdateProc)(DeskAccessory *da);
typedef int (*DAEditProc)(DeskAccessory *da, DAMessage editOp);
typedef int (*DAMenuProc)(DeskAccessory *da, SInt16 menuID, SInt16 itemID);

/* DA Registry Entry */
struct DARegistryEntry {
    char name[32];
    DAOpenProc open;
    DACloseProc close;
    DAEventProc event;
    DAIdleProc idle;
    DAActivateProc activate;
    DAUpdateProc update;
    DAEditProc edit;
    DAMenuProc menu;
    UInt32 flags;
    SInt16 menuID;
    SInt16 type;
    SInt16 resourceID;
    DAInterface *interface;
    struct DARegistryEntry *next;
};

/* Desk Accessory Structure */
struct DeskAccessory {
    SInt16 refNum;
    char name[32];
    WindowPtr window;
    void *window_obj;
    Boolean active;
    UInt32 flags;
    SInt16 menuID;
    DAState state;
    SInt16 type;

    /* Function pointers */
    DAOpenProc open;
    DACloseProc close;
    DAEventProc event;
    DAIdleProc idle;
    DAActivateProc activate;
    DAUpdateProc update;
    DAEditProc edit;
    DAMenuProc menu;

    /* The interface this DA registered with, if it supplied one. */
    DAInterface *interface;

    /* Private data */
    void *privateData;
    void *driverData;
    void *userData;

    /* List management */
    DeskAccessory *next;
    DeskAccessory *prev;
};

/* Desk Manager State */
struct DeskManagerState {
    DeskAccessory *firstDA;
    DeskAccessory *lastDA;
    DeskAccessory *activeDA;
    SInt16 nextRefNum;
    Handle systemMenuHandle;
    Boolean systemMenuEnabled;
    SInt16 openDACount;
    SInt16 numDAs;

    /* These should be on activeDA, not here */
    DAEventProc event;
    DAMenuProc menu;
    DAActivateProc activate;
    DeskAccessory *prev;
};

/* Error codes */
#define DESK_ERR_NONE           0
#define DESK_ERR_NO_MEMORY     -1
#define DESK_ERR_NOT_FOUND     -2
#define DESK_ERR_ALREADY_OPEN  -3
#define DESK_ERR_SYSTEM_ERROR  -4
#define DESK_ERR_PARAM_ERROR   -5

#endif /* DESKMANAGER_TYPES_H_DEFS */