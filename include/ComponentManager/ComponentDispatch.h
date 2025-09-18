/*
 * ComponentDispatch.h
 *
 * Component Dispatch API - System 7.1 Portable Implementation
 * Handles component API dispatch and calling conventions
 */

#ifndef COMPONENTDISPATCH_H
#define COMPONENTDISPATCH_H

#include "ComponentManager.h"
#include "ComponentRegistry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Component call context */
typedef struct ComponentCallContext {
    ComponentInstance instance;
    ComponentRegistryEntry* component;
    ComponentParameters* params;
    Handle storage;
    ComponentInstance target;
    uint32_t callDepth;
    uint32_t flags;
    int32_t result;
    OSErr error;
} ComponentCallContext;

/* Component call flags */
#define kComponentCallFlagAsync         (1<<0)  /* Asynchronous call */
#define kComponentCallFlagDeferred      (1<<1)  /* Deferred call */
#define kComponentCallFlagImmediate     (1<<2)  /* Immediate call */
#define kComponentCallFlagNoResult      (1<<3)  /* Don't wait for result */
#define kComponentCallFlagInterruptible (1<<4)  /* Call can be interrupted */
#define kComponentCallFlagSecure        (1<<5)  /* Secure call context */

/* Dispatch initialization */
OSErr InitComponentDispatch(void);
void CleanupComponentDispatch(void);

/* Main component dispatch function */
ComponentResult ComponentDispatch(ComponentInstance instance, ComponentParameters* params);

/* Component calling conventions */
ComponentResult CallComponent(ComponentInstance instance, int16_t selector, ...);
ComponentResult CallComponentWithParams(ComponentInstance instance, ComponentParameters* params);
ComponentResult CallComponentFunction(ComponentParameters* params, ComponentFunction func);
ComponentResult CallComponentFunctionWithStorage(Handle storage, ComponentParameters* params, ComponentFunction func);

/* Standard component selectors dispatch */
ComponentResult DispatchComponentOpen(ComponentCallContext* context);
ComponentResult DispatchComponentClose(ComponentCallContext* context);
ComponentResult DispatchComponentCanDo(ComponentCallContext* context);
ComponentResult DispatchComponentVersion(ComponentCallContext* context);
ComponentResult DispatchComponentRegister(ComponentCallContext* context);
ComponentResult DispatchComponentTarget(ComponentCallContext* context);
ComponentResult DispatchComponentUnregister(ComponentCallContext* context);

/* Component delegation support */
ComponentResult DelegateComponentCall(ComponentParameters* originalParams, ComponentInstance ci);
ComponentResult RedirectComponentCall(ComponentCallContext* context, ComponentInstance target);

/* Call stack management */
typedef struct ComponentCallStack {
    ComponentCallContext* contexts;
    uint32_t depth;
    uint32_t maxDepth;
    uint32_t flags;
} ComponentCallStack;

OSErr InitComponentCallStack(ComponentCallStack* stack, uint32_t maxDepth);
OSErr PushComponentCall(ComponentCallStack* stack, ComponentCallContext* context);
OSErr PopComponentCall(ComponentCallStack* stack, ComponentCallContext** context);
ComponentCallContext* GetCurrentComponentCall(ComponentCallStack* stack);
void CleanupComponentCallStack(ComponentCallStack* stack);

/* Component call monitoring */
typedef void (*ComponentCallMonitor)(ComponentCallContext* context, bool entering);
OSErr RegisterComponentCallMonitor(ComponentCallMonitor monitor);
OSErr UnregisterComponentCallMonitor(ComponentCallMonitor monitor);

/* Parameter marshaling */
typedef struct ComponentParamDescriptor {
    uint16_t offset;            /* Offset in parameter block */
    uint16_t size;              /* Size of parameter */
    uint8_t type;               /* Parameter type */
    uint8_t flags;              /* Parameter flags */
} ComponentParamDescriptor;

/* Parameter types */
#define kComponentParamTypeVoid         0
#define kComponentParamTypeInt8         1
#define kComponentParamTypeInt16        2
#define kComponentParamTypeInt32        3
#define kComponentParamTypeInt64        4
#define kComponentParamTypeFloat32      5
#define kComponentParamTypeFloat64      6
#define kComponentParamTypePointer      7
#define kComponentParamTypeHandle       8
#define kComponentParamTypeOSType       9
#define kComponentParamTypeString       10
#define kComponentParamTypeRect         11
#define kComponentParamTypeRegion       12

/* Parameter flags */
#define kComponentParamFlagInput        (1<<0)
#define kComponentParamFlagOutput       (1<<1)
#define kComponentParamFlagOptional     (1<<2)
#define kComponentParamFlagArray        (1<<3)

OSErr MarshalParameters(ComponentParameters* params, ComponentParamDescriptor* descriptors, int32_t count);
OSErr UnmarshalParameters(ComponentParameters* params, ComponentParamDescriptor* descriptors, int32_t count);

/* Cross-platform calling convention support */
#ifdef _WIN32
#define COMPONENT_CALLING_CONVENTION __stdcall
#elif defined(__APPLE__)
#define COMPONENT_CALLING_CONVENTION
#elif defined(__linux__)
#define COMPONENT_CALLING_CONVENTION
#else
#define COMPONENT_CALLING_CONVENTION
#endif

/* Component function prototypes for different calling conventions */
typedef ComponentResult (COMPONENT_CALLING_CONVENTION *ComponentStdCallProc)(ComponentParameters* cp, Handle componentStorage);
typedef ComponentResult (*ComponentCCallProc)(ComponentParameters* cp, Handle componentStorage);
typedef ComponentResult (*ComponentPascalProc)(ComponentParameters* cp, Handle componentStorage);

/* Calling convention detection and adaptation */
ComponentResult AdaptComponentCall(ComponentCallContext* context);
ComponentRoutine WrapComponentRoutine(void* routine, int32_t callingConvention);

/* Component exception handling */
typedef struct ComponentException {
    int32_t code;
    char* message;
    ComponentCallContext* context;
    void* platformException;
} ComponentException;

typedef bool (*ComponentExceptionHandler)(ComponentException* exception);
OSErr RegisterComponentExceptionHandler(ComponentExceptionHandler handler);
OSErr UnregisterComponentExceptionHandler(ComponentExceptionHandler handler);
OSErr HandleComponentException(ComponentException* exception);

/* Component call profiling */
typedef struct ComponentCallProfile {
    ComponentInstance instance;
    int16_t selector;
    uint64_t callCount;
    uint64_t totalTime;
    uint64_t minTime;
    uint64_t maxTime;
    uint64_t lastCallTime;
} ComponentCallProfile;

OSErr EnableComponentProfiling(bool enable);
OSErr GetComponentCallProfile(ComponentInstance instance, int16_t selector, ComponentCallProfile* profile);
OSErr ResetComponentProfiling(void);

/* Thread safety support */
typedef struct ComponentMutex {
    void* platformMutex;
    uint32_t lockCount;
    uint32_t ownerThread;
} ComponentMutex;

OSErr CreateComponentMutex(ComponentMutex* mutex);
OSErr DestroyComponentMutex(ComponentMutex* mutex);
OSErr LockComponentMutex(ComponentMutex* mutex);
OSErr UnlockComponentMutex(ComponentMutex* mutex);

/* Component call queuing for async operations */
typedef struct ComponentCallQueue {
    ComponentCallContext* calls;
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint32_t count;
    ComponentMutex mutex;
    void* semaphore;
} ComponentCallQueue;

OSErr CreateComponentCallQueue(ComponentCallQueue* queue, uint32_t size);
OSErr DestroyComponentCallQueue(ComponentCallQueue* queue);
OSErr QueueComponentCall(ComponentCallQueue* queue, ComponentCallContext* context);
OSErr DequeueComponentCall(ComponentCallQueue* queue, ComponentCallContext** context);
OSErr ProcessComponentCallQueue(ComponentCallQueue* queue);

/* Component debugging support */
typedef struct ComponentDebugInfo {
    char* componentName;
    char* modulePath;
    uint32_t loadAddress;
    uint32_t entryPoint;
    ComponentDescription description;
} ComponentDebugInfo;

OSErr GetComponentDebugInfo(ComponentInstance instance, ComponentDebugInfo* info);
OSErr SetComponentBreakpoint(ComponentInstance instance, int16_t selector);
OSErr ClearComponentBreakpoint(ComponentInstance instance, int16_t selector);

/* Component call tracing */
typedef void (*ComponentTraceCallback)(ComponentCallContext* context, const char* message);
OSErr EnableComponentTracing(bool enable);
OSErr RegisterComponentTraceCallback(ComponentTraceCallback callback);
OSErr TraceComponentCall(ComponentCallContext* context, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTDISPATCH_H */