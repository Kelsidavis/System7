/*
 * ComponentInstances.h
 *
 * Component Instance Management API - System 7.1 Portable Implementation
 * Manages component instance lifecycle, storage, and properties
 */

#ifndef COMPONENTINSTANCES_H
#define COMPONENTINSTANCES_H

#include "ComponentManager.h"
#include "ComponentRegistry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Component instance structure */
typedef struct ComponentInstanceData {
    struct ComponentInstanceData* next;
    ComponentRegistryEntry* component;  /* Associated component */
    Handle storage;                      /* Instance storage */
    ComponentInstance target;            /* Target for delegation */
    int32_t a5World;                    /* A5 world (68k compatibility) */
    int32_t refcon;                     /* Instance reference constant */
    OSErr lastError;                    /* Last error for this instance */
    uint32_t flags;                     /* Instance flags */
    uint32_t openCount;                 /* Reference count */
    void* userData;                     /* User data pointer */
    uint32_t threadId;                  /* Owning thread ID */
    ComponentMutex* mutex;              /* Instance mutex for thread safety */
    uint64_t creationTime;              /* Instance creation timestamp */
    uint64_t lastAccessTime;            /* Last access timestamp */
} ComponentInstanceData;

/* Instance flags */
#define kComponentInstanceFlagValid         (1<<0)  /* Instance is valid */
#define kComponentInstanceFlagClosing       (1<<1)  /* Instance is being closed */
#define kComponentInstanceFlagThreadSafe    (1<<2)  /* Instance is thread-safe */
#define kComponentInstanceFlagExclusive     (1<<3)  /* Exclusive access required */
#define kComponentInstanceFlagDelegate      (1<<4)  /* Instance delegates calls */
#define kComponentInstanceFlagSecure        (1<<5)  /* Secure instance */

/* Instance management initialization */
OSErr InitComponentInstances(void);
void CleanupComponentInstances(void);

/* Component instance creation and destruction */
ComponentInstance CreateComponentInstance(ComponentRegistryEntry* component);
OSErr DestroyComponentInstance(ComponentInstance instance);
OSErr ValidateComponentInstance(ComponentInstance instance);

/* Component instance operations */
ComponentInstance OpenComponentInstance(Component component);
OSErr CloseComponentInstance(ComponentInstance instance);
OSErr ReferenceComponentInstance(ComponentInstance instance);
OSErr DereferenceComponentInstance(ComponentInstance instance);

/* Component instance data access */
ComponentInstanceData* GetComponentInstanceData(ComponentInstance instance);
ComponentRegistryEntry* GetInstanceComponent(ComponentInstance instance);
Handle GetInstanceStorage(ComponentInstance instance);
OSErr SetInstanceStorage(ComponentInstance instance, Handle storage);

/* Component instance properties */
OSErr GetInstanceError(ComponentInstance instance);
void SetInstanceError(ComponentInstance instance, OSErr error);

int32_t GetInstanceA5(ComponentInstance instance);
void SetInstanceA5(ComponentInstance instance, int32_t a5);

int32_t GetInstanceRefcon(ComponentInstance instance);
void SetInstanceRefcon(ComponentInstance instance, int32_t refcon);

ComponentInstance GetInstanceTarget(ComponentInstance instance);
OSErr SetInstanceTarget(ComponentInstance instance, ComponentInstance target);

/* Component instance enumeration */
int32_t CountInstances(Component component);
ComponentInstance GetFirstInstance(Component component);
ComponentInstance GetNextInstance(ComponentInstance instance);

/* Instance iteration */
typedef bool (*InstanceIteratorFunc)(ComponentInstance instance, void* userData);
void IterateInstances(Component component, InstanceIteratorFunc iterator, void* userData);
void IterateAllInstances(InstanceIteratorFunc iterator, void* userData);

/* Component instance storage management */
OSErr AllocateInstanceStorage(ComponentInstance instance, uint32_t size);
OSErr ReallocateInstanceStorage(ComponentInstance instance, uint32_t newSize);
OSErr FreeInstanceStorage(ComponentInstance instance);
uint32_t GetInstanceStorageSize(ComponentInstance instance);

/* Instance storage utilities */
OSErr CopyInstanceStorage(ComponentInstance source, ComponentInstance destination);
OSErr SaveInstanceStorage(ComponentInstance instance, void** data, uint32_t* size);
OSErr RestoreInstanceStorage(ComponentInstance instance, void* data, uint32_t size);

/* Component instance thread safety */
OSErr LockInstance(ComponentInstance instance);
OSErr UnlockInstance(ComponentInstance instance);
OSErr TryLockInstance(ComponentInstance instance);
bool IsInstanceLocked(ComponentInstance instance);

/* Component instance delegation */
OSErr DelegateInstance(ComponentInstance instance, ComponentInstance target);
OSErr UndelegateInstance(ComponentInstance instance);
ComponentInstance GetDelegationTarget(ComponentInstance instance);
ComponentInstance GetDelegationSource(ComponentInstance instance);

/* Component instance lifecycle callbacks */
typedef enum {
    kInstanceEventCreated,
    kInstanceEventOpened,
    kInstanceEventClosed,
    kInstanceEventDestroyed,
    kInstanceEventDelegated,
    kInstanceEventUndelegated,
    kInstanceEventError
} ComponentInstanceEvent;

typedef void (*InstanceEventCallback)(ComponentInstance instance, ComponentInstanceEvent event, void* userData);
OSErr RegisterInstanceEventCallback(InstanceEventCallback callback, void* userData);
OSErr UnregisterInstanceEventCallback(InstanceEventCallback callback);

/* Component instance persistence */
typedef struct InstancePersistenceHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    ComponentDescription componentDesc;
    uint32_t storageSize;
    uint32_t flags;
    uint64_t timestamp;
} InstancePersistenceHeader;

OSErr SaveInstanceToFile(ComponentInstance instance, const char* filePath);
OSErr LoadInstanceFromFile(const char* filePath, ComponentInstance* instance);
OSErr SaveInstanceToStream(ComponentInstance instance, void* stream);
OSErr LoadInstanceFromStream(void* stream, ComponentInstance* instance);

/* Component instance debugging */
typedef struct InstanceDebugInfo {
    ComponentInstance instance;
    ComponentDescription componentDesc;
    char* componentName;
    uint32_t storageSize;
    uint32_t openCount;
    uint32_t flags;
    uint64_t creationTime;
    uint64_t lastAccessTime;
    uint32_t threadId;
} InstanceDebugInfo;

OSErr GetInstanceDebugInfo(ComponentInstance instance, InstanceDebugInfo* info);
OSErr DumpInstanceInfo(ComponentInstance instance, char** infoString);
OSErr ValidateAllInstances(void);

/* Component instance performance monitoring */
typedef struct InstancePerformanceStats {
    ComponentInstance instance;
    uint64_t callCount;
    uint64_t totalCallTime;
    uint64_t averageCallTime;
    uint64_t lastCallTime;
    uint32_t errorCount;
    uint32_t delegationCount;
} InstancePerformanceStats;

OSErr GetInstancePerformanceStats(ComponentInstance instance, InstancePerformanceStats* stats);
OSErr ResetInstancePerformanceStats(ComponentInstance instance);
OSErr EnableInstanceProfiling(ComponentInstance instance, bool enable);

/* Component instance resource management */
typedef struct InstanceResourceQuota {
    uint32_t maxMemory;         /* Maximum memory usage */
    uint32_t maxHandles;        /* Maximum handle count */
    uint32_t maxCallDepth;      /* Maximum call stack depth */
    uint32_t maxCallTime;       /* Maximum call time (ms) */
    uint32_t maxInstances;      /* Maximum instances per component */
} InstanceResourceQuota;

OSErr SetInstanceResourceQuota(ComponentInstance instance, InstanceResourceQuota* quota);
OSErr GetInstanceResourceQuota(ComponentInstance instance, InstanceResourceQuota* quota);
OSErr GetInstanceResourceUsage(ComponentInstance instance, InstanceResourceQuota* usage);

/* Component instance security */
typedef struct InstanceSecurityContext {
    uint32_t securityLevel;     /* Security level (0-255) */
    uint32_t permissions;       /* Permission flags */
    void* platformContext;      /* Platform-specific security context */
    char* trustedPath;          /* Trusted component path */
} InstanceSecurityContext;

OSErr CreateInstanceSecurityContext(ComponentInstance instance, InstanceSecurityContext* context);
OSErr DestroyInstanceSecurityContext(InstanceSecurityContext* context);
OSErr ValidateInstanceSecurity(ComponentInstance instance);

/* Component instance cleanup and leak detection */
OSErr CleanupOrphanedInstances(void);
OSErr DetectInstanceLeaks(void);
OSErr ForceCloseAllInstances(Component component);
OSErr GetInstanceLeakReport(char** report);

/* Component instance caching */
typedef struct InstanceCache {
    ComponentInstance* instances;
    uint32_t size;
    uint32_t count;
    uint32_t maxAge;            /* Maximum age in seconds */
    bool enabled;
} InstanceCache;

OSErr InitInstanceCache(InstanceCache* cache, uint32_t size, uint32_t maxAge);
OSErr CleanupInstanceCache(InstanceCache* cache);
OSErr CacheInstance(InstanceCache* cache, ComponentInstance instance);
ComponentInstance GetCachedInstance(InstanceCache* cache, Component component);
OSErr InvalidateInstanceCache(InstanceCache* cache);

/* Component instance migration (for component updates) */
OSErr MigrateInstance(ComponentInstance oldInstance, Component newComponent, ComponentInstance* newInstance);
OSErr BeginInstanceMigration(ComponentInstance instance);
OSErr CompleteInstanceMigration(ComponentInstance oldInstance, ComponentInstance newInstance);
OSErr AbortInstanceMigration(ComponentInstance instance);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTINSTANCES_H */