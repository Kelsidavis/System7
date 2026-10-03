/* Private Dialog Manager selectors, state types, and implementation APIs. */

#ifndef DIALOG_MANAGER_PRIVATE_H
#define DIALOG_MANAGER_PRIVATE_H

#include "SystemTypes.h"

#include "dialog_manager_core.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Private calls (negative selectors) */
#define selectDMgrCite4               -5
#define paramWordsDMgrCite4           10
#define selectDMgrCitationsSH         -4
#define paramWordsDMgrCitationsSH     7
#define selectDMgrCitationsCH         -3
#define paramWordsDMgrCitationsCH     5
#define selectDMgrPopMenuState        -2
#define paramWordsDMgrPopMenuState    0
#define selectDMgrPushMenuState       -1
#define paramWordsDMgrPushMenuState   0

/* Public dispatch calls (positive selectors) */
#define selectGetFrontWindowModalClass      1
#define paramWordsGetFrontWindowModalClass  2
#define selectGetWindowModalClass           2
#define paramWordsGetWindowModalClass       4
#define selectIsUserCancelEvent             7
#define paramWordsIsUserCancelEvent         2
#define selectGetNextUserCancelEvent        8
#define paramWordsGetNextUserCancelEvent    2

/* Modal window class types */
typedef enum {
    kModalClassNone = 0,
    kModalClassModal = 1,
    kModalClassMovable = 2,
    kModalClassAlert = 3
} ModalWindowClass;

/* Dialog Manager state shared by private implementation modules. */
typedef struct DialogMgrGlobals {
    WindowPtr AnalyzedWindow;
    SInt16 AnalyzedWindowState;
    SInt16 IsDialogState;
    void* SavedMenuState;
} DialogMgrGlobals;

/* Dialog Manager dispatch entry */
typedef struct DialogDispatchEntry {
    SInt16 selector;
    void* function;
} DialogDispatchEntry;


/*
 * IsUserCancelEvent - Check if event is user cancel
 * Determines if event represents user cancel action (Cmd-. etc.)
 */
Boolean IsUserCancelEvent(const EventRecord* theEvent);

/*
 * GetNextUserCancelEvent - Get next cancel event from queue
 * Retrieves next user cancel event if available
 */
Boolean GetNextUserCancelEvent(EventRecord* theEvent);

/* Menu state management functions - evidence from negative selectors */

/*
 * PushMenuState - Save current menu state
 * Saves menu bar state before modal dialog operations
 */
void DMgrPushMenuState(void);

/*
 * PopMenuState - Restore saved menu state
 * Restores menu bar state after modal dialog operations
 */
void DMgrPopMenuState(void);

/* Text Services citation handlers. */

/*
 * CitationsCH - Citations character handling
 * System 7 text services citation character processing
 */
void DMgrCitationsCH(SInt16 param1, SInt16 param2, SInt32 param3);

/*
 * CitationsSH - Citations string handling
 * System 7 text services citation string processing
 */
void DMgrCitationsSH(SInt16 param1, SInt32 param2, SInt32 param3, SInt16 param4);

/*
 * Cite4 - Four-parameter citation function
 * Advanced System 7 text services citation processing
 */
void DMgrCite4(SInt16 param1, SInt32 param2, SInt32 param3, SInt32 param4, SInt16 param5);

/* Dialog Manager dispatch mechanism */

/*
 * DoDialogMgrDispatch - Main dispatch entry point
 * Dispatches to specific Dialog Manager functions by selector
 */

/* Internal utility functions */
const void* GetDialogManagerDispatchTable(void);
Boolean IsValidDialogManagerSelector(SInt16 selector);

/* Accessors for Dialog Manager state. */
void SetAnalyzedWindowState(SInt16 state);
SInt16 GetAnalyzedWindowState(void);
void SetIsDialogState(SInt16 state);
SInt16 GetIsDialogState(void);
void SetAnalyzedWindow(WindowPtr window);
WindowPtr GetAnalyzedWindow(void);
void SetSavedMenuState(void* menuState);
void* GetSavedMenuState(void);

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_MANAGER_PRIVATE_H */
