/*
 * SegmentCache.c - Segment Caching and Memory Optimization
 *
 * This file implements segment caching, memory optimization, and intelligent
 * segment loading/unloading strategies for Mac OS 7.1 applications.
 */

#include "../../include/SegmentLoader/SegmentLoader.h"
#include "../../include/MemoryManager/MemoryManager.h"
#include "../../include/Debugging/Debugging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ============================================================================
 * Cache Constants
 * ============================================================================ */

#define MAX_CACHED_SEGMENTS     64      /* Maximum segments in cache */
#define CACHE_SEGMENT_SIZE      0x8000  /* 32KB cache segments */
#define MIN_FREE_MEMORY         0x10000 /* 64KB minimum free */
#define CACHE_CLEANUP_THRESHOLD 0x8000  /* 32KB cleanup threshold */

/* Cache entry states */
#define CACHE_ENTRY_FREE        0x00    /* Entry is available */
#define CACHE_ENTRY_USED        0x01    /* Entry contains segment */
#define CACHE_ENTRY_LOCKED      0x02    /* Entry is locked in memory */
#define CACHE_ENTRY_DIRTY       0x04    /* Entry has been modified */
#define CACHE_ENTRY_RECENT      0x08    /* Recently accessed */

/* Cache replacement policies */
#define CACHE_POLICY_LRU        0       /* Least Recently Used */
#define CACHE_POLICY_LFU        1       /* Least Frequently Used */
#define CACHE_POLICY_SIZE       2       /* Prefer smaller segments */

/* ============================================================================
 * Cache Data Structures
 * ============================================================================ */

typedef struct CacheEntry {
    uint16_t        segmentID;      /* Segment ID (0 = free) */
    uint32_t        codeAddr;       /* Address of cached code */
    uint32_t        codeSize;       /* Size of cached code */
    Handle          codeHandle;     /* Handle to code resource */
    uint32_t        accessCount;    /* Number of accesses */
    uint32_t        lastAccess;     /* Last access time */
    uint8_t         state;          /* Cache entry state */
    uint8_t         priority;       /* Cache priority (0-255) */
    ApplicationControlBlock* owner; /* Owning application */
} CacheEntry;

typedef struct SegmentCache {
    CacheEntry      entries[MAX_CACHED_SEGMENTS];
    uint32_t        totalSize;      /* Total cache size */
    uint32_t        usedSize;       /* Used cache size */
    uint32_t        hitCount;       /* Cache hits */
    uint32_t        missCount;      /* Cache misses */
    uint32_t        evictionCount;  /* Cache evictions */
    uint8_t         policy;         /* Replacement policy */
    bool            enabled;        /* Cache enabled flag */
} SegmentCache;

/* ============================================================================
 * Global Variables
 * ============================================================================ */

static SegmentCache gSegmentCache;
static bool gCacheInitialized = false;
static uint32_t gCacheTime = 0;  /* Simple time counter */

/* ============================================================================
 * Internal Function Prototypes
 * ============================================================================ */

static OSErr AllocateCacheEntry(uint16_t segmentID, ApplicationControlBlock* owner,
                                CacheEntry** outEntry);
static CacheEntry* FindCacheEntry(uint16_t segmentID, ApplicationControlBlock* owner);
static OSErr EvictCacheEntry(CacheEntry* entry);
static CacheEntry* SelectVictimEntry(void);
static OSErr CompactCache(void);
static void UpdateCacheStatistics(CacheEntry* entry, bool hit);
static uint32_t CalculateCachePriority(CacheEntry* entry);

/* ============================================================================
 * Cache Initialization
 * ============================================================================ */

OSErr InitSegmentCache(void)
{
    if (gCacheInitialized) {
        return segNoErr;
    }

    DEBUG_LOG("InitSegmentCache: Initializing segment cache\n");

    /* Clear cache structure */
    memset(&gSegmentCache, 0, sizeof(SegmentCache));

    /* Initialize cache entries */
    for (int i = 0; i < MAX_CACHED_SEGMENTS; i++) {
        gSegmentCache.entries[i].segmentID = 0;  /* Free */
        gSegmentCache.entries[i].state = CACHE_ENTRY_FREE;
    }

    /* Set default cache policy */
    gSegmentCache.policy = CACHE_POLICY_LRU;
    gSegmentCache.enabled = true;

    /* Initialize cache time */
    gCacheTime = 1;

    gCacheInitialized = true;

    DEBUG_LOG("InitSegmentCache: Segment cache initialized\n");
    return segNoErr;
}

void TerminateSegmentCache(void)
{
    if (!gCacheInitialized) {
        return;
    }

    DEBUG_LOG("TerminateSegmentCache: Terminating segment cache\n");

    /* Evict all cached segments */
    for (int i = 0; i < MAX_CACHED_SEGMENTS; i++) {
        if (gSegmentCache.entries[i].state != CACHE_ENTRY_FREE) {
            EvictCacheEntry(&gSegmentCache.entries[i]);
        }
    }

    /* Log cache statistics */
    DEBUG_LOG("Cache Statistics:\n");
    DEBUG_LOG("  Hits: %d\n", gSegmentCache.hitCount);
    DEBUG_LOG("  Misses: %d\n", gSegmentCache.missCount);
    DEBUG_LOG("  Evictions: %d\n", gSegmentCache.evictionCount);
    DEBUG_LOG("  Hit Rate: %.2f%%\n",
             (float)gSegmentCache.hitCount * 100.0f /
             (gSegmentCache.hitCount + gSegmentCache.missCount));

    gCacheInitialized = false;
}

/* ============================================================================
 * Cache Operations
 * ============================================================================ */

OSErr CacheSegment(uint16_t segmentID, ApplicationControlBlock* owner, Handle codeHandle)
{
    if (!gCacheInitialized) {
        OSErr err = InitSegmentCache();
        if (err != segNoErr) return err;
    }

    if (!gSegmentCache.enabled || !codeHandle) {
        return segNoErr;  /* Cache disabled or invalid handle */
    }

    DEBUG_LOG("CacheSegment: Caching segment %d\n", segmentID);

    /* Check if segment is already cached */
    CacheEntry* existing = FindCacheEntry(segmentID, owner);
    if (existing) {
        /* Update existing entry */
        existing->accessCount++;
        existing->lastAccess = gCacheTime++;
        existing->state |= CACHE_ENTRY_RECENT;
        UpdateCacheStatistics(existing, true);
        DEBUG_LOG("CacheSegment: Segment already cached, updated\n");
        return segNoErr;
    }

    /* Allocate new cache entry */
    CacheEntry* entry;
    OSErr err = AllocateCacheEntry(segmentID, owner, &entry);
    if (err != segNoErr) {
        DEBUG_LOG("CacheSegment: Failed to allocate cache entry: %d\n", err);
        return err;
    }

    /* Store segment info */
    entry->segmentID = segmentID;
    entry->owner = owner;
    entry->codeHandle = codeHandle;
    entry->codeSize = GetHandleSize(codeHandle);
    entry->codeAddr = (uint32_t)*codeHandle;
    entry->accessCount = 1;
    entry->lastAccess = gCacheTime++;
    entry->state = CACHE_ENTRY_USED | CACHE_ENTRY_RECENT;
    entry->priority = CalculateCachePriority(entry);

    /* Update cache statistics */
    gSegmentCache.usedSize += entry->codeSize;
    UpdateCacheStatistics(entry, false);

    DEBUG_LOG("CacheSegment: Segment cached successfully (size: %d)\n", entry->codeSize);
    return segNoErr;
}

OSErr UncacheSegment(uint16_t segmentID, ApplicationControlBlock* owner)
{
    if (!gCacheInitialized) {
        return segNoErr;
    }

    DEBUG_LOG("UncacheSegment: Uncaching segment %d\n", segmentID);

    CacheEntry* entry = FindCacheEntry(segmentID, owner);
    if (!entry) {
        DEBUG_LOG("UncacheSegment: Segment not found in cache\n");
        return segSegmentNotFound;
    }

    /* Evict the entry */
    OSErr err = EvictCacheEntry(entry);
    if (err != segNoErr) {
        DEBUG_LOG("UncacheSegment: Failed to evict entry: %d\n", err);
        return err;
    }

    DEBUG_LOG("UncacheSegment: Segment uncached successfully\n");
    return segNoErr;
}

Handle GetCachedSegment(uint16_t segmentID, ApplicationControlBlock* owner)
{
    if (!gCacheInitialized || !gSegmentCache.enabled) {
        return NULL;
    }

    CacheEntry* entry = FindCacheEntry(segmentID, owner);
    if (!entry) {
        gSegmentCache.missCount++;
        return NULL;
    }

    /* Update access statistics */
    entry->accessCount++;
    entry->lastAccess = gCacheTime++;
    entry->state |= CACHE_ENTRY_RECENT;
    UpdateCacheStatistics(entry, true);

    DEBUG_LOG("GetCachedSegment: Cache hit for segment %d\n", segmentID);
    return entry->codeHandle;
}

/* ============================================================================
 * Cache Entry Management
 * ============================================================================ */

static OSErr AllocateCacheEntry(uint16_t segmentID, ApplicationControlBlock* owner,
                                CacheEntry** outEntry)
{
    /* Look for free entry */
    for (int i = 0; i < MAX_CACHED_SEGMENTS; i++) {
        if (gSegmentCache.entries[i].state == CACHE_ENTRY_FREE) {
            *outEntry = &gSegmentCache.entries[i];
            DEBUG_LOG("AllocateCacheEntry: Using free entry %d\n", i);
            return segNoErr;
        }
    }

    /* No free entries, need to evict */
    CacheEntry* victim = SelectVictimEntry();
    if (!victim) {
        DEBUG_LOG("AllocateCacheEntry: No evictable entries found\n");
        return segMemFullErr;
    }

    /* Evict the victim */
    OSErr err = EvictCacheEntry(victim);
    if (err != segNoErr) {
        return err;
    }

    *outEntry = victim;
    DEBUG_LOG("AllocateCacheEntry: Evicted entry for reuse\n");
    return segNoErr;
}

static CacheEntry* FindCacheEntry(uint16_t segmentID, ApplicationControlBlock* owner)
{
    for (int i = 0; i < MAX_CACHED_SEGMENTS; i++) {
        CacheEntry* entry = &gSegmentCache.entries[i];
        if (entry->state != CACHE_ENTRY_FREE &&
            entry->segmentID == segmentID &&
            entry->owner == owner) {
            return entry;
        }
    }
    return NULL;
}

static OSErr EvictCacheEntry(CacheEntry* entry)
{
    if (!entry || entry->state == CACHE_ENTRY_FREE) {
        return segNoErr;
    }

    DEBUG_LOG("EvictCacheEntry: Evicting segment %d (size: %d)\n",
             entry->segmentID, entry->codeSize);

    /* Don't evict locked entries */
    if (entry->state & CACHE_ENTRY_LOCKED) {
        DEBUG_LOG("EvictCacheEntry: Entry is locked, cannot evict\n");
        return segMemFullErr;
    }

    /* Update cache statistics */
    gSegmentCache.usedSize -= entry->codeSize;
    gSegmentCache.evictionCount++;

    /* Clear the entry */
    memset(entry, 0, sizeof(CacheEntry));
    entry->state = CACHE_ENTRY_FREE;

    return segNoErr;
}

static CacheEntry* SelectVictimEntry(void)
{
    CacheEntry* bestVictim = NULL;
    uint32_t bestScore = 0xFFFFFFFF;

    for (int i = 0; i < MAX_CACHED_SEGMENTS; i++) {
        CacheEntry* entry = &gSegmentCache.entries[i];

        /* Skip free or locked entries */
        if (entry->state == CACHE_ENTRY_FREE ||
            (entry->state & CACHE_ENTRY_LOCKED)) {
            continue;
        }

        uint32_t score = 0;

        switch (gSegmentCache.policy) {
            case CACHE_POLICY_LRU:
                /* Prefer least recently used */
                score = gCacheTime - entry->lastAccess;
                break;

            case CACHE_POLICY_LFU:
                /* Prefer least frequently used */
                score = 0xFFFFFFFF - entry->accessCount;
                break;

            case CACHE_POLICY_SIZE:
                /* Prefer larger segments */
                score = entry->codeSize;
                break;
        }

        /* Adjust score based on priority */
        score = score * (256 - entry->priority) / 256;

        if (score < bestScore) {
            bestScore = score;
            bestVictim = entry;
        }
    }

    if (bestVictim) {
        DEBUG_LOG("SelectVictimEntry: Selected segment %d for eviction (score: %d)\n",
                 bestVictim->segmentID, bestScore);
    }

    return bestVictim;
}

/* ============================================================================
 * Cache Optimization
 * ============================================================================ */

OSErr OptimizeCache(void)
{
    if (!gCacheInitialized) {
        return segNoErr;
    }

    DEBUG_LOG("OptimizeCache: Optimizing segment cache\n");

    /* Compact cache if fragmented */
    OSErr err = CompactCache();
    if (err != segNoErr) {
        return err;
    }

    /* Age recent flags */
    for (int i = 0; i < MAX_CACHED_SEGMENTS; i++) {
        if (gSegmentCache.entries[i].state & CACHE_ENTRY_RECENT) {
            if (gCacheTime - gSegmentCache.entries[i].lastAccess > 100) {
                gSegmentCache.entries[i].state &= ~CACHE_ENTRY_RECENT;
            }
        }
    }

    /* Check for memory pressure */
    Size freeMemory = FreeMem();
    if (freeMemory < MIN_FREE_MEMORY) {
        DEBUG_LOG("OptimizeCache: Memory pressure detected, cleaning cache\n");

        /* Evict non-recent entries */
        for (int i = 0; i < MAX_CACHED_SEGMENTS; i++) {
            CacheEntry* entry = &gSegmentCache.entries[i];
            if (entry->state != CACHE_ENTRY_FREE &&
                !(entry->state & (CACHE_ENTRY_LOCKED | CACHE_ENTRY_RECENT))) {
                EvictCacheEntry(entry);
            }
        }
    }

    DEBUG_LOG("OptimizeCache: Cache optimization completed\n");
    return segNoErr;
}

static OSErr CompactCache(void)
{
    DEBUG_LOG("CompactCache: Compacting cache\n");

    /* Move active entries to the beginning of the cache */
    int writeIndex = 0;

    for (int readIndex = 0; readIndex < MAX_CACHED_SEGMENTS; readIndex++) {
        if (gSegmentCache.entries[readIndex].state != CACHE_ENTRY_FREE) {
            if (writeIndex != readIndex) {
                gSegmentCache.entries[writeIndex] = gSegmentCache.entries[readIndex];
                memset(&gSegmentCache.entries[readIndex], 0, sizeof(CacheEntry));
                gSegmentCache.entries[readIndex].state = CACHE_ENTRY_FREE;
            }
            writeIndex++;
        }
    }

    DEBUG_LOG("CompactCache: Compacted %d active entries\n", writeIndex);
    return segNoErr;
}

/* ============================================================================
 * Cache Statistics and Management
 * ============================================================================ */

static void UpdateCacheStatistics(CacheEntry* entry, bool hit)
{
    if (hit) {
        gSegmentCache.hitCount++;
    } else {
        gSegmentCache.missCount++;
    }
}

static uint32_t CalculateCachePriority(CacheEntry* entry)
{
    if (!entry) {
        return 0;
    }

    uint32_t priority = 128;  /* Base priority */

    /* Increase priority for frequently accessed segments */
    if (entry->accessCount > 10) {
        priority += 32;
    } else if (entry->accessCount > 5) {
        priority += 16;
    }

    /* Increase priority for recently accessed segments */
    if (entry->state & CACHE_ENTRY_RECENT) {
        priority += 16;
    }

    /* Decrease priority for large segments */
    if (entry->codeSize > 16384) {
        priority -= 16;
    }

    /* Ensure priority is in valid range */
    if (priority > 255) priority = 255;

    return priority;
}

OSErr GetCacheStatistics(uint32_t* hits, uint32_t* misses, uint32_t* evictions,
                        uint32_t* usedSize, uint32_t* totalSize)
{
    if (!gCacheInitialized) {
        return segNoErr;
    }

    if (hits) *hits = gSegmentCache.hitCount;
    if (misses) *misses = gSegmentCache.missCount;
    if (evictions) *evictions = gSegmentCache.evictionCount;
    if (usedSize) *usedSize = gSegmentCache.usedSize;
    if (totalSize) *totalSize = gSegmentCache.totalSize;

    return segNoErr;
}

/* ============================================================================
 * Cache Configuration
 * ============================================================================ */

OSErr SetCachePolicy(uint8_t policy)
{
    if (!gCacheInitialized) {
        OSErr err = InitSegmentCache();
        if (err != segNoErr) return err;
    }

    if (policy > CACHE_POLICY_SIZE) {
        return segBadFormat;
    }

    gSegmentCache.policy = policy;

    DEBUG_LOG("SetCachePolicy: Cache policy set to %d\n", policy);
    return segNoErr;
}

OSErr SetCacheEnabled(bool enabled)
{
    if (!gCacheInitialized) {
        OSErr err = InitSegmentCache();
        if (err != segNoErr) return err;
    }

    gSegmentCache.enabled = enabled;

    DEBUG_LOG("SetCacheEnabled: Cache %s\n", enabled ? "enabled" : "disabled");

    if (!enabled) {
        /* Flush cache when disabled */
        FlushSegmentCache();
    }

    return segNoErr;
}

OSErr FlushSegmentCache(void)
{
    if (!gCacheInitialized) {
        return segNoErr;
    }

    DEBUG_LOG("FlushSegmentCache: Flushing all cached segments\n");

    for (int i = 0; i < MAX_CACHED_SEGMENTS; i++) {
        if (gSegmentCache.entries[i].state != CACHE_ENTRY_FREE &&
            !(gSegmentCache.entries[i].state & CACHE_ENTRY_LOCKED)) {
            EvictCacheEntry(&gSegmentCache.entries[i]);
        }
    }

    DEBUG_LOG("FlushSegmentCache: Cache flushed\n");
    return segNoErr;
}

/* ============================================================================
 * Application-Specific Cache Management
 * ============================================================================ */

OSErr FlushApplicationCache(ApplicationControlBlock* acb)
{
    if (!gCacheInitialized || !acb) {
        return segNoErr;
    }

    DEBUG_LOG("FlushApplicationCache: Flushing cache for application\n");

    for (int i = 0; i < MAX_CACHED_SEGMENTS; i++) {
        CacheEntry* entry = &gSegmentCache.entries[i];
        if (entry->state != CACHE_ENTRY_FREE && entry->owner == acb) {
            EvictCacheEntry(entry);
        }
    }

    DEBUG_LOG("FlushApplicationCache: Application cache flushed\n");
    return segNoErr;
}

OSErr LockCachedSegment(uint16_t segmentID, ApplicationControlBlock* owner)
{
    if (!gCacheInitialized) {
        return segSegmentNotFound;
    }

    CacheEntry* entry = FindCacheEntry(segmentID, owner);
    if (!entry) {
        return segSegmentNotFound;
    }

    entry->state |= CACHE_ENTRY_LOCKED;

    DEBUG_LOG("LockCachedSegment: Segment %d locked in cache\n", segmentID);
    return segNoErr;
}

OSErr UnlockCachedSegment(uint16_t segmentID, ApplicationControlBlock* owner)
{
    if (!gCacheInitialized) {
        return segSegmentNotFound;
    }

    CacheEntry* entry = FindCacheEntry(segmentID, owner);
    if (!entry) {
        return segSegmentNotFound;
    }

    entry->state &= ~CACHE_ENTRY_LOCKED;

    DEBUG_LOG("UnlockCachedSegment: Segment %d unlocked in cache\n", segmentID);
    return segNoErr;
}

/* ============================================================================
 * Cache Debugging Support
 * ============================================================================ */

void DumpCacheInfo(void)
{
    if (!gCacheInitialized) {
        DEBUG_LOG("DumpCacheInfo: Cache not initialized\n");
        return;
    }

    DEBUG_LOG("Segment Cache Information:\n");
    DEBUG_LOG("  Status: %s\n", gSegmentCache.enabled ? "Enabled" : "Disabled");
    DEBUG_LOG("  Policy: %d\n", gSegmentCache.policy);
    DEBUG_LOG("  Used Size: %d bytes\n", gSegmentCache.usedSize);
    DEBUG_LOG("  Hits: %d\n", gSegmentCache.hitCount);
    DEBUG_LOG("  Misses: %d\n", gSegmentCache.missCount);
    DEBUG_LOG("  Evictions: %d\n", gSegmentCache.evictionCount);

    if (gSegmentCache.hitCount + gSegmentCache.missCount > 0) {
        float hitRate = (float)gSegmentCache.hitCount * 100.0f /
                       (gSegmentCache.hitCount + gSegmentCache.missCount);
        DEBUG_LOG("  Hit Rate: %.2f%%\n", hitRate);
    }

    DEBUG_LOG("  Active Entries:\n");
    for (int i = 0; i < MAX_CACHED_SEGMENTS; i++) {
        CacheEntry* entry = &gSegmentCache.entries[i];
        if (entry->state != CACHE_ENTRY_FREE) {
            DEBUG_LOG("    [%2d] Segment %2d, Size %6d, Access %3d, Priority %3d%s%s\n",
                     i, entry->segmentID, entry->codeSize, entry->accessCount,
                     entry->priority,
                     (entry->state & CACHE_ENTRY_LOCKED) ? " [LOCKED]" : "",
                     (entry->state & CACHE_ENTRY_RECENT) ? " [RECENT]" : "");
        }
    }
}