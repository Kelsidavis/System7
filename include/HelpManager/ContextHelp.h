/*
 * ContextHelp.h - Context-Sensitive Help Detection
 *
 * This file defines structures and functions for detecting when context-sensitive
 * help should be displayed, tracking mouse position, and determining appropriate
 * help content based on interface elements.
 */

#ifndef CONTEXTHELP_H
#define CONTEXTHELP_H

#include "MacTypes.h"
#include "Events.h"
#include "Controls.h"
#include "Dialogs.h"
#include "Menus.h"
#include "Windows.h"
#include "HelpManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Context types for help detection */
typedef enum {
    kHMContextNone = 0,              /* No context */
    kHMContextWindow = 1,            /* Window context */
    kHMContextDialog = 2,            /* Dialog context */
    kHMContextControl = 3,           /* Control context */
    kHMContextMenu = 4,              /* Menu context */
    kHMContextMenuItem = 5,          /* Menu item context */
    kHMContextRect = 6,              /* Rectangle region context */
    kHMContextCustom = 7             /* Custom application context */
} HMContextType;

/* Context detection modes */
typedef enum {
    kHMDetectionOff = 0,             /* Context detection disabled */
    kHMDetectionPassive = 1,         /* Passive detection (hover) */
    kHMDetectionActive = 2,          /* Active detection (click) */
    kHMDetectionModal = 3            /* Modal context help mode */
} HMDetectionMode;

/* Help timing parameters */
typedef struct HMHelpTiming {
    long hoverDelay;                 /* Delay before showing help (ticks) */
    long displayDuration;            /* How long to show help (ticks) */
    long fadeInTime;                 /* Fade in animation time (ticks) */
    long fadeOutTime;                /* Fade out animation time (ticks) */
    long retriggerDelay;             /* Delay before retriggering (ticks) */
    Boolean autoHide;                /* Auto-hide after duration */
    Boolean persistOnClick;          /* Keep visible when clicked */
} HMHelpTiming;

/* Context information */
typedef struct HMContextInfo {
    HMContextType contextType;       /* Type of context */
    WindowPtr window;                /* Window containing context */
    union {
        struct {
            DialogPtr dialog;        /* Dialog reference */
            short itemNumber;        /* Dialog item number */
            ControlHandle control;   /* Control handle if applicable */
        } dialog;
        struct {
            MenuHandle menu;         /* Menu handle */
            short menuID;            /* Menu ID */
            short itemNumber;        /* Menu item number */
            long itemFlags;          /* Menu item flags */
        } menu;
        struct {
            Rect hotRect;            /* Hot rectangle */
            short rectID;            /* Rectangle ID */
            ResType resourceType;    /* Associated resource type */
            short resourceID;        /* Associated resource ID */
        } rect;
        struct {
            void *userData;          /* Custom user data */
            long userID;             /* Custom user ID */
            char identifier[64];     /* Custom identifier string */
        } custom;
    } context;
    Point mouseLocation;             /* Mouse location when detected */
    long detectionTime;              /* Time context was detected */
    Boolean isValid;                 /* Context is still valid */
} HMContextInfo;

/* Context detection state */
typedef struct HMDetectionState {
    HMDetectionMode mode;            /* Current detection mode */
    HMContextInfo currentContext;    /* Current context */
    HMContextInfo lastContext;       /* Previous context */
    Point lastMousePoint;            /* Last mouse position */
    long lastMouseTime;              /* Last mouse movement time */
    Boolean mouseMoving;             /* Mouse is currently moving */
    Boolean helpVisible;             /* Help is currently visible */
    HMHelpTiming timing;             /* Help timing parameters */
    Boolean trackingEnabled;         /* Context tracking enabled */
} HMDetectionState;

/* Context tracking callback */
typedef OSErr (*HMContextCallback)(const HMContextInfo *context, Boolean entering);

/* Custom context provider callback */
typedef OSErr (*HMContextProvider)(Point mousePoint, WindowPtr window,
                                  HMContextInfo *context);

/* Context detection functions */
OSErr HMContextDetectionInit(const HMHelpTiming *timing);
void HMContextDetectionShutdown(void);

OSErr HMContextSetDetectionMode(HMDetectionMode mode);
HMDetectionMode HMContextGetDetectionMode(void);

OSErr HMContextSetTiming(const HMHelpTiming *timing);
OSErr HMContextGetTiming(HMHelpTiming *timing);

/* Context tracking functions */
OSErr HMContextStartTracking(void);
OSErr HMContextStopTracking(void);
Boolean HMContextIsTracking(void);

OSErr HMContextUpdate(Point mousePoint, WindowPtr frontWindow);
OSErr HMContextCheck(Point mousePoint, WindowPtr window, HMContextInfo *context);

/* Context detection for specific UI elements */
OSErr HMContextDetectWindow(Point mousePoint, WindowPtr window, HMContextInfo *context);
OSErr HMContextDetectDialog(Point mousePoint, DialogPtr dialog, HMContextInfo *context);
OSErr HMContextDetectControl(Point mousePoint, ControlHandle control, HMContextInfo *context);
OSErr HMContextDetectMenu(MenuHandle menu, short itemNumber, HMContextInfo *context);
OSErr HMContextDetectRect(Point mousePoint, WindowPtr window, HMContextInfo *context);

/* Context validation functions */
Boolean HMContextIsValid(const HMContextInfo *context);
Boolean HMContextIsSame(const HMContextInfo *context1, const HMContextInfo *context2);
OSErr HMContextValidateDialog(DialogPtr dialog, short itemNumber);
OSErr HMContextValidateControl(ControlHandle control);
OSErr HMContextValidateMenu(MenuHandle menu, short itemNumber);

/* Mouse tracking utilities */
OSErr HMMouseTrackingStart(void);
OSErr HMMouseTrackingStop(void);
Boolean HMMouseIsMoving(Point currentPoint, Point lastPoint, long currentTime, long lastTime);
long HMMouseGetIdleTime(void);

/* Help triggering functions */
OSErr HMContextTriggerHelp(const HMContextInfo *context);
OSErr HMContextHideHelp(const HMContextInfo *context);
Boolean HMContextShouldShowHelp(const HMContextInfo *context);
Boolean HMContextShouldHideHelp(const HMContextInfo *context);

/* Context callback management */
OSErr HMContextRegisterCallback(HMContextType contextType, HMContextCallback callback);
OSErr HMContextUnregisterCallback(HMContextType contextType);
OSErr HMContextRegisterProvider(HMContextProvider provider);
OSErr HMContextUnregisterProvider(HMContextProvider provider);

/* Dialog item context functions */
OSErr HMContextSetDialogItem(DialogPtr dialog, short itemNumber,
                           ResType resourceType, short resourceID);
OSErr HMContextGetDialogItem(DialogPtr dialog, short itemNumber,
                           ResType *resourceType, short *resourceID);
OSErr HMContextClearDialogItem(DialogPtr dialog, short itemNumber);

/* Control context functions */
OSErr HMContextSetControl(ControlHandle control, ResType resourceType, short resourceID);
OSErr HMContextGetControl(ControlHandle control, ResType *resourceType, short *resourceID);
OSErr HMContextClearControl(ControlHandle control);

/* Menu context functions */
OSErr HMContextSetMenuItem(MenuHandle menu, short itemNumber,
                         ResType resourceType, short resourceID);
OSErr HMContextGetMenuItem(MenuHandle menu, short itemNumber,
                         ResType *resourceType, short *resourceID);
OSErr HMContextClearMenuItem(MenuHandle menu, short itemNumber);

/* Rectangle context functions */
OSErr HMContextAddRect(WindowPtr window, const Rect *hotRect, short rectID,
                     ResType resourceType, short resourceID);
OSErr HMContextRemoveRect(WindowPtr window, short rectID);
OSErr HMContextFindRect(WindowPtr window, Point mousePoint, HMContextInfo *context);
OSErr HMContextClearAllRects(WindowPtr window);

/* Window context functions */
OSErr HMContextSetWindow(WindowPtr window, ResType resourceType, short resourceID);
OSErr HMContextGetWindow(WindowPtr window, ResType *resourceType, short *resourceID);
OSErr HMContextClearWindow(WindowPtr window);

/* Context state management */
OSErr HMContextSaveState(HMDetectionState *savedState);
OSErr HMContextRestoreState(const HMDetectionState *savedState);
OSErr HMContextGetCurrentState(HMDetectionState *currentState);

/* Custom context support */
OSErr HMContextAddCustom(const char *identifier, void *userData, long userID,
                       ResType resourceType, short resourceID);
OSErr HMContextRemoveCustom(const char *identifier);
OSErr HMContextFindCustom(const char *identifier, HMContextInfo *context);

/* Event integration */
OSErr HMContextProcessEvent(const EventRecord *event, Boolean *eventHandled);
OSErr HMContextIdleProcessing(void);
Boolean HMContextWantsEvent(const EventRecord *event);

/* Modal help mode */
OSErr HMContextEnterModalMode(void);
OSErr HMContextExitModalMode(void);
Boolean HMContextIsModalMode(void);

/* Accessibility support */
OSErr HMContextSetAccessibilityInfo(const HMContextInfo *context,
                                  const char *accessibilityDescription);
OSErr HMContextGetAccessibilityInfo(const HMContextInfo *context,
                                  char **accessibilityDescription);
Boolean HMContextIsAccessibilityEnabled(void);

/* Debugging and diagnostics */
OSErr HMContextGetDebugInfo(char *debugInfo, long maxLength);
OSErr HMContextDumpState(void);
Boolean HMContextValidateDatabase(void);

/* Performance optimization */
OSErr HMContextSetCacheSize(short maxCachedContexts);
OSErr HMContextFlushCache(void);
OSErr HMContextPreloadWindowContexts(WindowPtr window);

/* Context persistence */
OSErr HMContextSaveToPrefs(void);
OSErr HMContextLoadFromPrefs(void);
OSErr HMContextResetToDefaults(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTEXTHELP_H */