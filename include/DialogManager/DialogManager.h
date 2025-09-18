/*
 * DialogManager.h - Macintosh System 7.1 Dialog Manager API
 *
 * This header provides the complete Dialog Manager interface for Mac System 7.1,
 * maintaining exact behavioral compatibility while providing modern platform
 * integration capabilities.
 *
 * The Dialog Manager is essential for:
 * - Modal and modeless dialog handling
 * - Alert dialogs and system notifications
 * - File dialogs (Standard File Package)
 * - Dialog item management and interaction
 * - Resource-based dialog templates (DLOG/DITL)
 * - Keyboard navigation and accessibility
 */

#ifndef DIALOG_MANAGER_H
#define DIALOG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations for Mac types */
typedef struct Rect {
    int16_t top, left, bottom, right;
} Rect;

typedef struct Point {
    int16_t v, h;
} Point;

typedef unsigned char Str255[256];
typedef void* Handle;
typedef void** RgnHandle;
typedef int16_t OSErr;

/* Window and Event Manager dependencies */
typedef struct WindowRecord* WindowPtr;
typedef struct EventRecord {
    int16_t what;
    int32_t message;
    int32_t when;
    Point where;
    int16_t modifiers;
} EventRecord;

/* TextEdit dependencies */
typedef struct TERecord** TEHandle;

/* Dialog Manager core types */
typedef WindowPtr DialogPtr;
typedef struct DialogRecord* DialogPeek;

/* Dialog item types and constants */
enum {
    /* Dialog item types */
    ctrlItem    = 4,     /* Control item */
    btnCtrl     = 0,     /* Button control */
    chkCtrl     = 1,     /* Checkbox control */
    radCtrl     = 2,     /* Radio button control */
    resCtrl     = 3,     /* Resource control */
    statText    = 8,     /* Static text */
    editText    = 16,    /* Editable text */
    iconItem    = 32,    /* Icon item */
    picItem     = 64,    /* Picture item */
    userItem    = 0,     /* User-defined item */
    itemDisable = 128    /* Item disabled flag */
};

/* Standard dialog button IDs */
enum {
    ok          = 1,     /* OK button */
    cancel      = 2      /* Cancel button */
};

/* Alert icon types */
enum {
    stopIcon    = 0,     /* Stop icon for alerts */
    noteIcon    = 1,     /* Note icon for alerts */
    cautionIcon = 2      /* Caution icon for alerts */
};

/* Dialog item list manipulation methods */
typedef int16_t DITLMethod;
enum {
    overlayDITL        = 0,  /* Overlay items */
    appendDITLRight    = 1,  /* Append to right */
    appendDITLBottom   = 2   /* Append to bottom */
};

/* Stage list type for alerts */
typedef int16_t StageList;

/* Callback procedure types */
typedef void (*ResumeProcPtr)(void);
typedef void (*SoundProcPtr)(int16_t soundNumber);
typedef bool (*ModalFilterProcPtr)(DialogPtr theDialog, EventRecord* theEvent, int16_t* itemHit);
typedef void (*UserItemProcPtr)(DialogPtr theDialog, int16_t itemNo);

/* Dialog record - internal structure exactly matching Mac OS */
typedef struct DialogRecord {
    struct WindowRecord window;         /* Window record */
    Handle          items;              /* Handle to item list */
    TEHandle        textH;              /* TextEdit handle for editable text */
    int16_t         editField;          /* Currently active edit field (-1 if none) */
    int16_t         editOpen;           /* Non-zero if edit field is open */
    int16_t         aDefItem;           /* Default button item number */
} DialogRecord;

/* Dialog template structure for DLOG resources */
typedef struct DialogTemplate {
    Rect            boundsRect;         /* Dialog bounds */
    int16_t         procID;             /* Window definition proc ID */
    uint8_t         visible;            /* Visibility flag */
    uint8_t         filler1;            /* Padding */
    uint8_t         goAwayFlag;         /* Has close box */
    uint8_t         filler2;            /* Padding */
    int32_t         refCon;             /* Reference constant */
    int16_t         itemsID;            /* Resource ID of item list */
    Str255          title;              /* Dialog title */
} DialogTemplate;

typedef DialogTemplate* DialogTPtr;
typedef DialogTemplate** DialogTHndl;

/* Alert template structure for ALRT resources */
typedef struct AlertTemplate {
    Rect            boundsRect;         /* Alert bounds */
    int16_t         itemsID;            /* Resource ID of item list */
    StageList       stages;             /* Alert stages */
} AlertTemplate;

typedef AlertTemplate* AlertTPtr;
typedef AlertTemplate** AlertTHndl;

/* Dialog item structure for DITL resources */
typedef struct DialogItem {
    Handle          handle;             /* Item handle or procedure pointer */
    Rect            bounds;             /* Item bounds */
    uint8_t         type;               /* Item type */
    uint8_t         length;             /* Length of item data (for text) */
    /* Item data follows for certain types */
} DialogItem;

/* Dialog item list (DITL) structure */
typedef struct DialogItemList {
    int16_t         count;              /* Number of items minus 1 */
    DialogItem      items[1];           /* Variable length array of items */
} DialogItemList;

typedef DialogItemList** DITLHandle;

/* Modal dialog window classes */
enum {
    dBoxProc            = 1,            /* Modal dialog */
    plainDBox           = 2,            /* Plain modal dialog */
    altDBoxProc         = 3,            /* Alternate modal dialog */
    noGrowDocProc       = 4,            /* Document window without grow box */
    movableDBoxProc     = 5,            /* Movable modal dialog */
    zoomDocProc         = 8,            /* Zoomable document window */
    zoomNoGrow          = 12,           /* Zoomable without grow box */
    rDocProc            = 16            /* Rounded corner window */
};

/* Extended dialog features flags */
enum {
    cannotTwitchOutOfDialogBit     = 7,
    systemHandlesMenusBit          = 6,
    systemHandlesDefaultButtonBit  = 5,
    systemHandlesCancelButtonBit   = 4,
    systemTracksCursorBit          = 3,
    emulateOrigFilterBit           = 2
};

/*
 * CORE DIALOG MANAGER API
 * These functions provide exact Mac System 7.1 Dialog Manager compatibility
 */

/* Dialog Manager initialization and cleanup */
void InitDialogs(ResumeProcPtr resumeProc);
void ErrorSound(SoundProcPtr soundProc);

/* Dialog creation and disposal */
DialogPtr NewDialog(void* wStorage, const Rect* boundsRect, const unsigned char* title,
                    bool visible, int16_t procID, WindowPtr behind, bool goAwayFlag,
                    int32_t refCon, Handle itmLstHndl);
DialogPtr GetNewDialog(int16_t dialogID, void* dStorage, WindowPtr behind);
DialogPtr NewColorDialog(void* dStorage, const Rect* boundsRect, const unsigned char* title,
                        bool visible, int16_t procID, WindowPtr behind, bool goAwayFlag,
                        int32_t refCon, Handle items);
void CloseDialog(DialogPtr theDialog);
void DisposDialog(DialogPtr theDialog);
void DisposeDialog(DialogPtr theDialog);

/* Dialog drawing and updating */
void DrawDialog(DialogPtr theDialog);
void UpdateDialog(DialogPtr theDialog, RgnHandle updateRgn);
void UpdtDialog(DialogPtr theDialog, RgnHandle updateRgn);

/* Modal dialog processing */
void ModalDialog(ModalFilterProcPtr filterProc, int16_t* itemHit);
bool IsDialogEvent(const EventRecord* theEvent);
bool DialogSelect(const EventRecord* theEvent, DialogPtr* theDialog, int16_t* itemHit);

/* Alert dialogs */
int16_t Alert(int16_t alertID, ModalFilterProcPtr filterProc);
int16_t StopAlert(int16_t alertID, ModalFilterProcPtr filterProc);
int16_t NoteAlert(int16_t alertID, ModalFilterProcPtr filterProc);
int16_t CautionAlert(int16_t alertID, ModalFilterProcPtr filterProc);

/* Alert stage management */
int16_t GetAlertStage(void);
void ResetAlertStage(void);
void ResetAlrtStage(void);

/* Dialog item management */
void GetDialogItem(DialogPtr theDialog, int16_t itemNo, int16_t* itemType,
                   Handle* item, Rect* box);
void SetDialogItem(DialogPtr theDialog, int16_t itemNo, int16_t itemType,
                   Handle item, const Rect* box);
void HideDialogItem(DialogPtr theDialog, int16_t itemNo);
void ShowDialogItem(DialogPtr theDialog, int16_t itemNo);
int16_t FindDialogItem(DialogPtr theDialog, Point thePt);

/* Dialog text management */
void GetDialogItemText(Handle item, unsigned char* text);
void SetDialogItemText(Handle item, const unsigned char* text);
void SelectDialogItemText(DialogPtr theDialog, int16_t itemNo, int16_t strtSel, int16_t endSel);
void ParamText(const unsigned char* param0, const unsigned char* param1,
               const unsigned char* param2, const unsigned char* param3);

/* Dialog edit operations */
void DialogCut(DialogPtr theDialog);
void DialogCopy(DialogPtr theDialog);
void DialogPaste(DialogPtr theDialog);
void DialogDelete(DialogPtr theDialog);

/* Dialog item list manipulation */
void AppendDITL(DialogPtr theDialog, Handle theHandle, DITLMethod method);
int16_t CountDITL(DialogPtr theDialog);
void ShortenDITL(DialogPtr theDialog, int16_t numberItems);

/* Dialog settings */
void SetDialogFont(int16_t fontNum);
void SetDAFont(int16_t fontNum);

/* Standard filter procedure and extended features */
bool StdFilterProc(DialogPtr theDialog, EventRecord* event, int16_t* itemHit);
OSErr GetStdFilterProc(ModalFilterProcPtr* theProc);
OSErr SetDialogDefaultItem(DialogPtr theDialog, int16_t newItem);
OSErr SetDialogCancelItem(DialogPtr theDialog, int16_t newItem);
OSErr SetDialogTracksCursor(DialogPtr theDialog, bool tracks);

/* Window modal class support */
OSErr GetFrontWindowModalClass(int16_t* modalClass);
OSErr GetWindowModalClass(WindowPtr theWindow, int16_t* modalClass);

/* User item procedures */
void SetUserItem(DialogPtr theDialog, int16_t itemNo, UserItemProcPtr procPtr);
UserItemProcPtr GetUserItem(DialogPtr theDialog, int16_t itemNo);

/*
 * BACKWARDS COMPATIBILITY ALIASES
 * These maintain compatibility with existing Mac code
 */
#define GetDItem        GetDialogItem
#define SetDItem        SetDialogItem
#define HideDItem       HideDialogItem
#define ShowDItem       ShowDialogItem
#define SelIText        SelectDialogItemText
#define GetIText        GetDialogItemText
#define SetIText        SetDialogItemText
#define FindDItem       FindDialogItem
#define GetAlrtStage    GetAlertStage
#define DlgCut          DialogCut
#define DlgCopy         DialogCopy
#define DlgPaste        DialogPaste
#define DlgDelete       DialogDelete
#define NewCDialog      NewColorDialog

/*
 * EXTENDED API FOR MODERN PLATFORMS
 * These functions provide additional capabilities for modern integration
 */

/* Platform integration */
void DialogManager_SetNativeDialogEnabled(bool enabled);
bool DialogManager_GetNativeDialogEnabled(void);
void DialogManager_SetAccessibilityEnabled(bool enabled);
bool DialogManager_GetAccessibilityEnabled(void);

/* High-DPI support */
void DialogManager_SetScaleFactor(float scale);
float DialogManager_GetScaleFactor(void);

/* File dialog integration */
OSErr DialogManager_ShowOpenFileDialog(const char* title, const char* defaultPath,
                                      const char* fileTypes, char* selectedPath,
                                      size_t pathSize);
OSErr DialogManager_ShowSaveFileDialog(const char* title, const char* defaultPath,
                                      const char* defaultName, char* selectedPath,
                                      size_t pathSize);

/* Color and theme support */
typedef struct DialogTheme {
    uint32_t backgroundColor;
    uint32_t textColor;
    uint32_t buttonColor;
    uint32_t borderColor;
    bool isDarkMode;
} DialogTheme;

void DialogManager_SetTheme(const DialogTheme* theme);
void DialogManager_GetTheme(DialogTheme* theme);

/*
 * INTERNAL UTILITY FUNCTIONS
 * These are used internally but may be useful for advanced applications
 */
Handle GetDialogItemList(DialogPtr theDialog);
void SetDialogItemList(DialogPtr theDialog, Handle itemList);
int16_t GetDialogDefaultItem(DialogPtr theDialog);
int16_t GetDialogCancelItem(DialogPtr theDialog);
bool GetDialogTracksCursor(DialogPtr theDialog);

/* Dialog state queries */
bool IsModalDialog(DialogPtr theDialog);
bool IsDialogVisible(DialogPtr theDialog);
WindowPtr GetDialogWindow(DialogPtr theDialog);
DialogPtr GetWindowDialog(WindowPtr theWindow);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_MANAGER_H */