/*
 * EventHandlers.h
 *
 * Apple Event handler registration and dispatch system
 * Provides event routing, handler management, and dispatch table support
 *
 * Based on Mac OS 7.1 Apple Event handler system
 */

#ifndef EVENT_HANDLERS_H
#define EVENT_HANDLERS_H

#include "AppleEventManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Handler Registration and Management
 * ======================================================================== */

/* Handler table management */
typedef struct AEHandlerTableEntry {
    AEEventClass eventClass;
    AEEventID eventID;
    EventHandlerProcPtr handler;
    int32_t handlerRefcon;
    bool isSystemHandler;
    struct AEHandlerTableEntry* next;
} AEHandlerTableEntry;

/* Coercion handler entry */
typedef struct AECoercionHandlerEntry {
    DescType fromType;
    DescType toType;
    CoercionHandlerProcPtr handler;
    int32_t handlerRefcon;
    bool fromTypeIsDesc;
    bool isSystemHandler;
    struct AECoercionHandlerEntry* next;
} AECoercionHandlerEntry;

/* Special handler entry */
typedef struct AESpecialHandlerEntry {
    AEKeyword functionClass;
    void* handler;
    bool isSystemHandler;
    struct AESpecialHandlerEntry* next;
} AESpecialHandlerEntry;

/* ========================================================================
 * Handler Dispatch System
 * ======================================================================== */

/* Event dispatch context */
typedef struct AEDispatchContext {
    const AppleEvent* currentEvent;
    AppleEvent* currentReply;
    EventHandlerProcPtr currentHandler;
    int32_t currentRefcon;
    bool eventSuspended;
    bool replyRequired;
    AEInteractAllowed interactionLevel;
    int32_t timeoutTicks;
} AEDispatchContext;

/* Handler execution results */
typedef enum {
    kAEHandlerExecuted = 0,
    kAEHandlerNotFound = 1,
    kAEHandlerFailed = 2,
    kAEHandlerSuspended = 3
} AEHandlerResult;

/* ========================================================================
 * Event Handler Functions
 * ======================================================================== */

/* Event handler registration */
OSErr AEInstallEventHandler_Extended(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr handler, int32_t handlerRefcon, bool isSysHandler, int32_t priority);
OSErr AEInstallWildcardHandler(AEEventClass theAEEventClass, EventHandlerProcPtr handler, int32_t handlerRefcon, bool isSysHandler);
OSErr AEInstallDefaultHandler(EventHandlerProcPtr handler, int32_t handlerRefcon);

/* Handler enumeration */
typedef bool (*AEHandlerEnumProc)(AEEventClass eventClass, AEEventID eventID, EventHandlerProcPtr handler, int32_t refcon, bool isSystemHandler, void* userData);
OSErr AEEnumerateEventHandlers(AEHandlerEnumProc enumProc, void* userData);

/* Handler validation */
OSErr AEValidateEventHandler(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr handler);
bool AEIsHandlerInstalled(AEEventClass theAEEventClass, AEEventID theAEEventID, EventHandlerProcPtr handler, bool isSysHandler);

/* ========================================================================
 * Event Dispatch Functions
 * ======================================================================== */

/* Main dispatch function */
OSErr AEDispatchAppleEvent(const AppleEvent* theAppleEvent, AppleEvent* reply, AEHandlerResult* result);

/* Custom dispatch */
OSErr AEDispatchToHandler(const AppleEvent* theAppleEvent, AppleEvent* reply, EventHandlerProcPtr handler, int32_t handlerRefcon);

/* Dispatch context management */
OSErr AEGetDispatchContext(AEDispatchContext** context);
OSErr AESetDispatchContext(const AEDispatchContext* context);

/* Pre- and post-dispatch hooks */
typedef OSErr (*AEPreDispatchProc)(const AppleEvent* theAppleEvent, AppleEvent* reply, void* userData);
typedef OSErr (*AEPostDispatchProc)(const AppleEvent* theAppleEvent, AppleEvent* reply, OSErr handlerResult, void* userData);

OSErr AEInstallPreDispatchHook(AEPreDispatchProc proc, void* userData);
OSErr AEInstallPostDispatchHook(AEPostDispatchProc proc, void* userData);
OSErr AERemovePreDispatchHook(AEPreDispatchProc proc);
OSErr AERemovePostDispatchHook(AEPostDispatchProc proc);

/* ========================================================================
 * Coercion Handler Functions
 * ======================================================================== */

/* Extended coercion handler management */
OSErr AEInstallCoercionHandler_Extended(DescType fromType, DescType toType, CoercionHandlerProcPtr handler, int32_t handlerRefcon, bool fromTypeIsDesc, bool isSysHandler, int32_t priority);
OSErr AEInstallWildcardCoercionHandler(DescType fromType, CoercionHandlerProcPtr handler, int32_t handlerRefcon, bool isSysHandler);

/* Coercion handler enumeration */
typedef bool (*AECoercionEnumProc)(DescType fromType, DescType toType, CoercionHandlerProcPtr handler, int32_t refcon, bool fromTypeIsDesc, bool isSystemHandler, void* userData);
OSErr AEEnumerateCoercionHandlers(AECoercionEnumProc enumProc, void* userData);

/* Coercion execution */
OSErr AEPerformCoercion(const AEDesc* fromDesc, DescType toType, AEDesc* result);
OSErr AECanCoerce(DescType fromType, DescType toType, bool* canCoerce);

/* ========================================================================
 * Special Handler Functions
 * ======================================================================== */

/* Special handler types */
#define kAEPreDispatchHandler 'prdp'
#define kAEPostDispatchHandler 'podp'
#define kAEErrorHandler 'errh'
#define kAETimeoutHandler 'tout'
#define kAEInteractionHandler 'intr'

/* Special handler management */
OSErr AEInstallSpecialHandler_Extended(AEKeyword functionClass, void* handler, bool isSysHandler, int32_t priority);

/* Special handler enumeration */
typedef bool (*AESpecialHandlerEnumProc)(AEKeyword functionClass, void* handler, bool isSystemHandler, void* userData);
OSErr AEEnumerateSpecialHandlers(AESpecialHandlerEnumProc enumProc, void* userData);

/* ========================================================================
 * Handler Table Management
 * ======================================================================== */

/* Handler table operations */
OSErr AECreateHandlerTable(void** handlerTable);
OSErr AEDisposeHandlerTable(void* handlerTable);
OSErr AECopyHandlerTable(void* sourceTable, void** destTable);
OSErr AEMergeHandlerTables(void* table1, void* table2, void** mergedTable);

/* Handler table persistence */
OSErr AESaveHandlerTable(void* handlerTable, const char* filename);
OSErr AELoadHandlerTable(const char* filename, void** handlerTable);

/* ========================================================================
 * Event Filtering and Routing
 * ======================================================================== */

/* Event filter callback */
typedef bool (*AEEventFilterProc)(const AppleEvent* theAppleEvent, void* userData);

/* Event routing */
OSErr AEInstallEventFilter(AEEventFilterProc filterProc, void* userData);
OSErr AERemoveEventFilter(AEEventFilterProc filterProc);

/* Event routing by process */
OSErr AERouteEventToProcess(const AppleEvent* theAppleEvent, const ProcessSerialNumber* targetPSN);

/* Event broadcasting */
OSErr AEBroadcastEvent(const AppleEvent* theAppleEvent, bool systemHandlersOnly);

/* ========================================================================
 * Handler Statistics and Monitoring
 * ======================================================================== */

typedef struct AEHandlerStats {
    int32_t totalHandlers;
    int32_t systemHandlers;
    int32_t userHandlers;
    int32_t eventsDispatched;
    int32_t eventsHandled;
    int32_t eventsFailed;
    int32_t eventsSuspended;
    int32_t coercionsPerformed;
    int32_t coercionsFailed;
} AEHandlerStats;

OSErr AEGetHandlerStats(AEHandlerStats* stats);
void AEResetHandlerStats(void);

/* Handler performance monitoring */
typedef struct AEHandlerPerfInfo {
    AEEventClass eventClass;
    AEEventID eventID;
    EventHandlerProcPtr handler;
    int32_t callCount;
    int32_t totalTimeMilliseconds;
    int32_t averageTimeMilliseconds;
    int32_t maxTimeMilliseconds;
} AEHandlerPerfInfo;

OSErr AEGetHandlerPerformanceInfo(AEHandlerPerfInfo** perfInfo, int32_t* count);
void AEResetHandlerPerformanceInfo(void);

/* ========================================================================
 * Error Handling and Recovery
 * ======================================================================== */

/* Error handling callback */
typedef OSErr (*AEErrorHandlerProc)(OSErr errorCode, const AppleEvent* theAppleEvent, AppleEvent* reply, void* userData);

OSErr AEInstallErrorHandler(AEErrorHandlerProc errorHandler, void* userData);
OSErr AERemoveErrorHandler(AEErrorHandlerProc errorHandler);

/* Handler recovery */
OSErr AERecoverFromHandlerError(const AppleEvent* theAppleEvent, AppleEvent* reply, OSErr originalError);

/* ========================================================================
 * Thread Safety and Concurrency
 * ======================================================================== */

/* Thread-safe handler operations */
OSErr AELockHandlerTable(void);
OSErr AEUnlockHandlerTable(void);

/* Concurrent event processing */
OSErr AEEnableConcurrentProcessing(bool enable);
bool AEIsConcurrentProcessingEnabled(void);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_HANDLERS_H */