/*
 * ResourceManager.c
 *
 * Notification resource loading and management
 * Handles icons, sounds, and other notification resources
 *
 * Converted from original Mac OS System 7.1 source code
 */

#include "NotificationManager/NotificationManager.h"
#include "Memory.h"
#include "Errors.h"
#include "OSUtils.h"
#include "Resources.h"
#include "Sound.h"

/* Resource types */
#define kNotificationIconType   'nICN'      /* Notification icon resource */
#define kNotificationSoundType  'nSND'      /* Notification sound resource */
#define kNotificationStringType 'nSTR'      /* Notification string resource */
#define kNotificationTemplateType 'nTMP'    /* Notification template resource */

/* Resource IDs */
#define kDefaultNoteIcon        128         /* Default note icon */
#define kDefaultCautionIcon     129         /* Default caution icon */
#define kDefaultStopIcon        130         /* Default stop icon */
#define kDefaultSound           128         /* Default notification sound */
#define kSystemBeepSound        129         /* System beep sound */

/* Resource cache entry */
typedef struct ResourceCacheEntry {
    ResType                     type;       /* Resource type */
    short                       id;         /* Resource ID */
    Handle                      resource;   /* Cached resource handle */
    UInt32                      lastAccess; /* Last access time */
    Boolean                     locked;     /* Resource is locked */
    short                       refCount;   /* Reference count */
    struct ResourceCacheEntry  *next;      /* Next in cache */
} ResourceCacheEntry, *ResourceCachePtr;

/* Resource manager state */
typedef struct {
    Boolean             initialized;
    Boolean             cachingEnabled;
    short               cacheSize;
    short               maxCacheSize;
    ResourceCachePtr    cache;
    Handle              defaultIcons[4];    /* Default icons for alert types */
    Handle              defaultSound;       /* Default notification sound */
    short               resourceFile;       /* Notification resource file */
    Boolean             resourceFileOpen;   /* Resource file is open */
    UInt32              cacheHits;         /* Cache statistics */
    UInt32              cacheMisses;
    UInt32              totalAccesses;
} ResourceManagerState;

static ResourceManagerState gResState = {0};

/* Internal prototypes */
static OSErr NMOpenResourceFile(void);
static void NMCloseResourceFile(void);
static ResourceCachePtr NMFindCachedResource(ResType type, short id);
static OSErr NMAddToCache(ResType type, short id, Handle resource);
static void NMRemoveFromCache(ResourceCachePtr entry);
static void NMCompactCache(void);
static void NMPurgeCache(void);
static OSErr NMLoadDefaultResources(void);
static void NMUnloadDefaultResources(void);

/*
 * Resource Manager Initialization
 */

OSErr NMLoadNotificationResources(void)
{
    OSErr err;

    if (gResState.initialized) {
        return noErr;
    }

    /* Initialize resource manager state */
    gResState.cachingEnabled = true;
    gResState.cacheSize = 0;
    gResState.maxCacheSize = 50;
    gResState.cache = NULL;
    gResState.resourceFile = -1;
    gResState.resourceFileOpen = false;
    gResState.cacheHits = 0;
    gResState.cacheMisses = 0;
    gResState.totalAccesses = 0;

    /* Initialize default resource handles */
    for (int i = 0; i < 4; i++) {
        gResState.defaultIcons[i] = NULL;
    }
    gResState.defaultSound = NULL;

    /* Open notification resource file */
    err = NMOpenResourceFile();
    if (err != noErr && err != fnfErr) {
        /* Non-fatal error - we can work without external resources */
    }

    /* Load default resources */
    err = NMLoadDefaultResources();
    if (err != noErr) {
        NMCloseResourceFile();
        return err;
    }

    gResState.initialized = true;
    return noErr;
}

OSErr NMUnloadNotificationResources(void)
{
    if (!gResState.initialized) {
        return noErr;
    }

    /* Purge resource cache */
    NMPurgeCache();

    /* Unload default resources */
    NMUnloadDefaultResources();

    /* Close resource file */
    NMCloseResourceFile();

    gResState.initialized = false;
    return noErr;
}

/*
 * Resource Access Functions
 */

Handle NMGetDefaultIcon(void)
{
    return NMGetDefaultAlertIcon(alertTypeNote);
}

Handle NMGetDefaultAlertIcon(AlertType type)
{
    short iconIndex;

    if (!gResState.initialized) {
        return NULL;
    }

    /* Map alert type to icon index */
    switch (type) {
        case alertTypeNote:
            iconIndex = 0;
            break;
        case alertTypeCaution:
            iconIndex = 1;
            break;
        case alertTypeStop:
            iconIndex = 2;
            break;
        case alertTypeCustom:
            iconIndex = 3;
            break;
        default:
            iconIndex = 0;
            break;
    }

    if (iconIndex >= 0 && iconIndex < 4) {
        return gResState.defaultIcons[iconIndex];
    }

    return NULL;
}

Handle NMGetDefaultSound(void)
{
    if (!gResState.initialized) {
        return NULL;
    }

    return gResState.defaultSound;
}

OSErr NMSetDefaultAlertIcon(AlertType type, Handle icon)
{
    short iconIndex;
    Handle newIcon = NULL;

    if (!gResState.initialized) {
        return nmErrNotInstalled;
    }

    /* Map alert type to icon index */
    switch (type) {
        case alertTypeNote:
            iconIndex = 0;
            break;
        case alertTypeCaution:
            iconIndex = 1;
            break;
        case alertTypeStop:
            iconIndex = 2;
            break;
        case alertTypeCustom:
            iconIndex = 3;
            break;
        default:
            return nmErrInvalidParameter;
    }

    /* Copy the icon if provided */
    if (icon) {
        Size iconSize = GetHandleSize(icon);
        newIcon = NewHandle(iconSize);
        if (!newIcon) {
            return nmErrOutOfMemory;
        }
        BlockMoveData(*icon, *newIcon, iconSize);
    }

    /* Dispose old icon */
    if (gResState.defaultIcons[iconIndex]) {
        DisposeHandle(gResState.defaultIcons[iconIndex]);
    }

    gResState.defaultIcons[iconIndex] = newIcon;
    return noErr;
}

OSErr NMSetDefaultAlertSound(Handle sound)
{
    Handle newSound = NULL;

    if (!gResState.initialized) {
        return nmErrNotInstalled;
    }

    /* Copy the sound if provided */
    if (sound) {
        Size soundSize = GetHandleSize(sound);
        newSound = NewHandle(soundSize);
        if (!newSound) {
            return nmErrOutOfMemory;
        }
        BlockMoveData(*sound, *newSound, soundSize);
    }

    /* Dispose old sound */
    if (gResState.defaultSound) {
        DisposeHandle(gResState.defaultSound);
    }

    gResState.defaultSound = newSound;
    return noErr;
}

/*
 * Resource Loading Functions
 */

Handle NMLoadNotificationIcon(short iconID)
{
    ResourceCachePtr cacheEntry;
    Handle iconHandle;

    if (!gResState.initialized) {
        return NULL;
    }

    gResState.totalAccesses++;

    /* Check cache first */
    if (gResState.cachingEnabled) {
        cacheEntry = NMFindCachedResource(kNotificationIconType, iconID);
        if (cacheEntry) {
            cacheEntry->lastAccess = NMGetTimestamp();
            cacheEntry->refCount++;
            gResState.cacheHits++;
            return cacheEntry->resource;
        }
    }

    gResState.cacheMisses++;

    /* Load from resource file */
    iconHandle = GetResource(kNotificationIconType, iconID);
    if (!iconHandle) {
        /* Try standard icon resource */
        iconHandle = GetResource('ICON', iconID);
    }

    if (!iconHandle) {
        /* Try small icon resource */
        iconHandle = GetResource('SICN', iconID);
    }

    if (iconHandle) {
        /* Add to cache if caching is enabled */
        if (gResState.cachingEnabled) {
            NMAddToCache(kNotificationIconType, iconID, iconHandle);
        }
        DetachResource(iconHandle);
    }

    return iconHandle;
}

Handle NMLoadNotificationSound(short soundID)
{
    ResourceCachePtr cacheEntry;
    Handle soundHandle;

    if (!gResState.initialized) {
        return NULL;
    }

    gResState.totalAccesses++;

    /* Check cache first */
    if (gResState.cachingEnabled) {
        cacheEntry = NMFindCachedResource(kNotificationSoundType, soundID);
        if (cacheEntry) {
            cacheEntry->lastAccess = NMGetTimestamp();
            cacheEntry->refCount++;
            gResState.cacheHits++;
            return cacheEntry->resource;
        }
    }

    gResState.cacheMisses++;

    /* Load from resource file */
    soundHandle = GetResource(kNotificationSoundType, soundID);
    if (!soundHandle) {
        /* Try standard sound resource */
        soundHandle = GetResource('snd ', soundID);
    }

    if (soundHandle) {
        /* Add to cache if caching is enabled */
        if (gResState.cachingEnabled) {
            NMAddToCache(kNotificationSoundType, soundID, soundHandle);
        }
        DetachResource(soundHandle);
    }

    return soundHandle;
}

StringPtr NMLoadNotificationString(short stringID)
{
    Handle stringHandle;
    StringPtr stringPtr = NULL;

    if (!gResState.initialized) {
        return NULL;
    }

    /* Load string resource */
    stringHandle = GetResource(kNotificationStringType, stringID);
    if (!stringHandle) {
        /* Try standard string resource */
        stringHandle = GetResource('STR ', stringID);
    }

    if (stringHandle) {
        Size stringSize = GetHandleSize(stringHandle);
        stringPtr = (StringPtr)NewPtr(stringSize);
        if (stringPtr) {
            BlockMoveData(*stringHandle, stringPtr, stringSize);
        }
        ReleaseResource(stringHandle);
    }

    return stringPtr;
}

/*
 * Resource Cache Management
 */

OSErr NMSetResourceCaching(Boolean enabled)
{
    if (!gResState.initialized) {
        return nmErrNotInstalled;
    }

    gResState.cachingEnabled = enabled;

    /* Purge cache if disabling */
    if (!enabled) {
        NMPurgeCache();
    }

    return noErr;
}

Boolean NMIsResourceCachingEnabled(void)
{
    return gResState.initialized && gResState.cachingEnabled;
}

OSErr NMSetMaxCacheSize(short maxSize)
{
    if (!gResState.initialized) {
        return nmErrNotInstalled;
    }

    if (maxSize < 0 || maxSize > 200) {
        return nmErrInvalidParameter;
    }

    gResState.maxCacheSize = maxSize;

    /* Compact cache if we're now over the limit */
    while (gResState.cacheSize > maxSize) {
        NMCompactCache();
    }

    return noErr;
}

short NMGetCacheSize(void)
{
    return gResState.cacheSize;
}

OSErr NMPurgeResourceCache(void)
{
    if (!gResState.initialized) {
        return nmErrNotInstalled;
    }

    NMPurgeCache();
    return noErr;
}

OSErr NMGetResourceStatistics(UInt32 *hits, UInt32 *misses, UInt32 *total)
{
    if (!gResState.initialized) {
        return nmErrNotInstalled;
    }

    if (hits) *hits = gResState.cacheHits;
    if (misses) *misses = gResState.cacheMisses;
    if (total) *total = gResState.totalAccesses;

    return noErr;
}

/*
 * Internal Implementation
 */

static OSErr NMOpenResourceFile(void)
{
    short refNum;
    OSErr err;

    /* Try to open notification resources file */
    refNum = OpenResFile("\pNotification Resources");
    err = ResError();

    if (err == noErr) {
        gResState.resourceFile = refNum;
        gResState.resourceFileOpen = true;
    }

    return err;
}

static void NMCloseResourceFile(void)
{
    if (gResState.resourceFileOpen && gResState.resourceFile != -1) {
        CloseResFile(gResState.resourceFile);
        gResState.resourceFile = -1;
        gResState.resourceFileOpen = false;
    }
}

static ResourceCachePtr NMFindCachedResource(ResType type, short id)
{
    ResourceCachePtr entry;

    entry = gResState.cache;
    while (entry) {
        if (entry->type == type && entry->id == id) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

static OSErr NMAddToCache(ResType type, short id, Handle resource)
{
    ResourceCachePtr entry;

    if (!resource) {
        return nmErrInvalidParameter;
    }

    /* Check if we need to compact cache */
    if (gResState.cacheSize >= gResState.maxCacheSize) {
        NMCompactCache();
    }

    /* Create cache entry */
    entry = (ResourceCachePtr)NewPtrClear(sizeof(ResourceCacheEntry));
    if (!entry) {
        return nmErrOutOfMemory;
    }

    entry->type = type;
    entry->id = id;
    entry->resource = resource;
    entry->lastAccess = NMGetTimestamp();
    entry->locked = false;
    entry->refCount = 1;

    /* Add to front of cache */
    entry->next = gResState.cache;
    gResState.cache = entry;
    gResState.cacheSize++;

    return noErr;
}

static void NMRemoveFromCache(ResourceCachePtr entry)
{
    ResourceCachePtr current, previous = NULL;

    if (!entry) {
        return;
    }

    /* Find entry in cache */
    current = gResState.cache;
    while (current) {
        if (current == entry) {
            /* Remove from chain */
            if (previous) {
                previous->next = current->next;
            } else {
                gResState.cache = current->next;
            }

            /* Dispose resource and entry */
            if (current->resource) {
                DisposeHandle(current->resource);
            }
            DisposePtr((Ptr)current);
            gResState.cacheSize--;

            break;
        }
        previous = current;
        current = current->next;
    }
}

static void NMCompactCache(void)
{
    ResourceCachePtr entry, nextEntry, oldestEntry = NULL;
    UInt32 oldestTime = 0xFFFFFFFF;

    /* Find oldest unused entry */
    entry = gResState.cache;
    while (entry) {
        if (entry->refCount == 0 && !entry->locked) {
            if (entry->lastAccess < oldestTime) {
                oldestTime = entry->lastAccess;
                oldestEntry = entry;
            }
        }
        entry = entry->next;
    }

    /* Remove oldest entry */
    if (oldestEntry) {
        NMRemoveFromCache(oldestEntry);
    }
}

static void NMPurgeCache(void)
{
    ResourceCachePtr entry, nextEntry;

    entry = gResState.cache;
    while (entry) {
        nextEntry = entry->next;
        NMRemoveFromCache(entry);
        entry = nextEntry;
    }

    gResState.cache = NULL;
    gResState.cacheSize = 0;
}

static OSErr NMLoadDefaultResources(void)
{
    Handle iconHandle, soundHandle;

    /* Load default note icon */
    iconHandle = GetResource('ICON', kDefaultNoteIcon);
    if (iconHandle) {
        DetachResource(iconHandle);
        gResState.defaultIcons[0] = iconHandle;
    }

    /* Load default caution icon */
    iconHandle = GetResource('ICON', kDefaultCautionIcon);
    if (iconHandle) {
        DetachResource(iconHandle);
        gResState.defaultIcons[1] = iconHandle;
    }

    /* Load default stop icon */
    iconHandle = GetResource('ICON', kDefaultStopIcon);
    if (iconHandle) {
        DetachResource(iconHandle);
        gResState.defaultIcons[2] = iconHandle;
    }

    /* Load default sound */
    soundHandle = GetResource('snd ', kDefaultSound);
    if (soundHandle) {
        DetachResource(soundHandle);
        gResState.defaultSound = soundHandle;
    }

    return noErr;
}

static void NMUnloadDefaultResources(void)
{
    int i;

    /* Dispose default icons */
    for (i = 0; i < 4; i++) {
        if (gResState.defaultIcons[i]) {
            DisposeHandle(gResState.defaultIcons[i]);
            gResState.defaultIcons[i] = NULL;
        }
    }

    /* Dispose default sound */
    if (gResState.defaultSound) {
        DisposeHandle(gResState.defaultSound);
        gResState.defaultSound = NULL;
    }
}

/*
 * Resource Validation
 */

OSErr NMValidateIconResource(Handle iconHandle)
{
    if (!iconHandle) {
        return nmErrInvalidParameter;
    }

    /* Basic validation - check size and structure */
    Size iconSize = GetHandleSize(iconHandle);
    if (iconSize < 128) { /* Minimum icon size */
        return nmErrInvalidResource;
    }

    return noErr;
}

OSErr NMValidateSoundResource(Handle soundHandle)
{
    if (!soundHandle) {
        return nmErrInvalidParameter;
    }

    /* Basic validation - check size and format */
    Size soundSize = GetHandleSize(soundHandle);
    if (soundSize < 20) { /* Minimum sound header size */
        return nmErrInvalidResource;
    }

    return noErr;
}

/*
 * Resource Utilities
 */

Handle NMDuplicateResource(Handle original)
{
    Handle duplicate;
    Size resourceSize;

    if (!original) {
        return NULL;
    }

    resourceSize = GetHandleSize(original);
    duplicate = NewHandle(resourceSize);

    if (duplicate) {
        BlockMoveData(*original, *duplicate, resourceSize);
    }

    return duplicate;
}

OSErr NMReleaseResource(Handle resourceHandle)
{
    ResourceCachePtr entry;

    if (!resourceHandle) {
        return nmErrInvalidParameter;
    }

    /* Find in cache and decrement reference count */
    entry = gResState.cache;
    while (entry) {
        if (entry->resource == resourceHandle) {
            if (entry->refCount > 0) {
                entry->refCount--;
            }
            return noErr;
        }
        entry = entry->next;
    }

    /* Not in cache - just dispose it */
    DisposeHandle(resourceHandle);
    return noErr;
}