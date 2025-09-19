/*
 * ComponentManager.h
 *
 * Main Component Manager API - System 7.1 Portable Implementation
 * Provides dynamic component loading and management for multimedia applications
 *
 * This is a portable implementation of the Apple Macintosh Component Manager
 * API from System 7.1, enabling modular multimedia functionality.
 */

#ifndef COMPONENTMANAGER_H
#define COMPONENTMANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic types */
typedef uint32_t OSType;
typedef int16_t OSErr;
typedef void* Handle;
typedef void* Ptr;

/* Error codes */
#define noErr                   0
#define badComponentInstance   -3000
#define badComponentSelector   -3001
#define componentNotFound      -3002
#define componentDllLoadErr    -3003
#define componentDllEntryErr   -3004
#define componentVersionErr    -3005
#define componentSecurityErr   -3006

/* Component Manager constants */
#define kAppleManufacturer          'appl'
#define kComponentResourceType      'thng'

#define kAnyComponentType           0
#define kAnyComponentSubType        0
#define kAnyComponentManufacturer   0
#define kAnyComponentFlagsMask      0

#define cmpWantsRegisterMessage     (1L<<31)

/* Standard component selectors */
#define kComponentOpenSelect        -1
#define kComponentCloseSelect       -2
#define kComponentCanDoSelect       -3
#define kComponentVersionSelect     -4
#define kComponentRegisterSelect    -5
#define kComponentTargetSelect      -6
#define kComponentUnregisterSelect  -7

/* Component resource extension flags */
#define componentDoAutoVersion              (1<<0)
#define componentWantsUnregister            (1<<1)
#define componentAutoVersionIncludeFlags    (1<<2)
#define componentHasMultiplePlatforms       (1<<3)

/* Set default component flags */
#define defaultComponentIdentical                       0
#define defaultComponentAnyFlags                        (1<<0)
#define defaultComponentAnyManufacturer                 (1<<1)
#define defaultComponentAnySubType                      (1<<2)
#define defaultComponentAnyFlagsAnyManufacturer         (defaultComponentAnyFlags+defaultComponentAnyManufacturer)
#define defaultComponentAnyFlagsAnyManufacturerAnySubType (defaultComponentAnyFlags+defaultComponentAnyManufacturer+defaultComponentAnySubType)

/* Platform types */
#define gestaltPowerPC              2
#define gestalt68k                  1

/* Component description structure */
typedef struct ComponentDescription {
    OSType componentType;           /* A unique 4-byte code identifying the command set */
    OSType componentSubType;        /* Particular flavor of this instance */
    OSType componentManufacturer;   /* Vendor identification */
    uint32_t componentFlags;        /* 8 each for Component,Type,SubType,Manuf/revision */
    uint32_t componentFlagsMask;    /* Mask for specifying which flags to consider in search */
} ComponentDescription;

/* Resource specification */
typedef struct ResourceSpec {
    OSType resType;                 /* 4-byte code */
    int16_t resId;
} ResourceSpec;

/* Component resource structure */
typedef struct ComponentResource {
    ComponentDescription cd;        /* Registration parameters */
    ResourceSpec component;         /* resource where Component code is found */
    ResourceSpec componentName;     /* name string resource */
    ResourceSpec componentInfo;     /* info string resource */
    ResourceSpec componentIcon;     /* icon resource */
} ComponentResource;

/* Platform information for multi-platform components */
typedef struct ComponentPlatformInfo {
    int32_t componentFlags;         /* flags of Component */
    ResourceSpec component;         /* resource where Component code is found */
    int16_t platformType;           /* gestaltSysArchitecture result */
} ComponentPlatformInfo;

/* Component resource extension */
typedef struct ComponentResourceExtension {
    int32_t componentVersion;       /* version of Component */
    int32_t componentRegisterFlags; /* flags for registration */
    int16_t componentIconFamily;    /* resource id of Icon Family */
} ComponentResourceExtension;

/* Extended component resource with multiple platforms */
typedef struct ExtComponentResource {
    ComponentDescription cd;        /* Registration parameters */
    ResourceSpec component;         /* resource where Component code is found */
    ResourceSpec componentName;     /* name string resource */
    ResourceSpec componentInfo;     /* info string resource */
    ResourceSpec componentIcon;     /* icon resource */
    int32_t componentVersion;       /* version of Component */
    int32_t componentRegisterFlags; /* flags for registration */
    int16_t componentIconFamily;    /* resource id of Icon Family */
    int32_t count;                  /* elements in platformArray */
    ComponentPlatformInfo platformArray[1];
} ExtComponentResource;

/* Component call parameters */
typedef struct ComponentParameters {
    uint8_t flags;                  /* call modifiers: sync/async, deferred, immed, etc */
    uint8_t paramSize;              /* size in bytes of actual parameters passed to this call */
    int16_t what;                   /* routine selector, negative for Component management calls */
    int32_t params[1];              /* actual parameters for the indicated routine */
} ComponentParameters;

/* Opaque component and instance handles */
typedef struct ComponentRecord {
    int32_t data[1];
} ComponentRecord;
typedef ComponentRecord* Component;

typedef struct ComponentInstanceRecord {
    int32_t data[1];
} ComponentInstanceRecord;
typedef ComponentInstanceRecord* ComponentInstance;

typedef int32_t ComponentResult;

/* Component routine function pointer */
typedef ComponentResult (*ComponentRoutine)(ComponentParameters* cp, Handle componentStorage);
typedef ComponentResult (*ComponentFunction)(void);

/* Component Manager API Functions */

/* Component Database Add, Delete, and Query Routines */
Component RegisterComponent(ComponentDescription* cd, ComponentRoutine componentEntryPoint,
                          int16_t global, Handle componentName, Handle componentInfo, Handle componentIcon);
Component RegisterComponentResource(Handle tr, int16_t global);
OSErr UnregisterComponent(Component aComponent);

Component FindNextComponent(Component aComponent, ComponentDescription* looking);
int32_t CountComponents(ComponentDescription* looking);

OSErr GetComponentInfo(Component aComponent, ComponentDescription* cd,
                      Handle componentName, Handle componentInfo, Handle componentIcon);
int32_t GetComponentListModSeed(void);

/* Component Instance Allocation and Dispatch Routines */
ComponentInstance OpenComponent(Component aComponent);
OSErr CloseComponent(ComponentInstance aComponentInstance);

OSErr GetComponentInstanceError(ComponentInstance aComponentInstance);
void SetComponentInstanceError(ComponentInstance aComponentInstance, OSErr theError);

/* Direct calls to components */
int32_t ComponentFunctionImplemented(ComponentInstance ci, int16_t ftnNumber);
int32_t GetComponentVersion(ComponentInstance ci);
int32_t ComponentSetTarget(ComponentInstance ci, ComponentInstance target);

/* Component Management Routines */
int32_t GetComponentRefcon(Component aComponent);
void SetComponentRefcon(Component aComponent, int32_t theRefcon);

int16_t OpenComponentResFile(Component aComponent);
OSErr CloseComponentResFile(int16_t refnum);

/* Component Instance Management Routines */
Handle GetComponentInstanceStorage(ComponentInstance aComponentInstance);
void SetComponentInstanceStorage(ComponentInstance aComponentInstance, Handle theStorage);

int32_t GetComponentInstanceA5(ComponentInstance aComponentInstance);
void SetComponentInstanceA5(ComponentInstance aComponentInstance, int32_t theA5);

int32_t CountComponentInstances(Component aComponent);

/* Utility functions for component dispatching */
int32_t CallComponentFunction(ComponentParameters* params, ComponentFunction func);
int32_t CallComponentFunctionWithStorage(Handle storage, ComponentParameters* params, ComponentFunction func);
int32_t DelegateComponentCall(ComponentParameters* originalParams, ComponentInstance ci);

/* Default component management */
OSErr SetDefaultComponent(Component aComponent, int16_t flags);
ComponentInstance OpenDefaultComponent(OSType componentType, OSType componentSubType);

/* Component capture/delegation */
Component CaptureComponent(Component capturedComponent, Component capturingComponent);
OSErr UncaptureComponent(Component aComponent);

/* Resource file management */
OSErr RegisterComponentResourceFile(int16_t resRefNum, int16_t global);
OSErr GetComponentIconSuite(Component aComponent, Handle* iconSuite);

/* Internal Component Manager initialization */
OSErr InitComponentManager(void);
void CleanupComponentManager(void);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTMANAGER_H */