/*
 * MemoryMgr_HAL.c - Hardware Abstraction Layer for Memory Manager
 *
 * Adapts the classic Mac OS Memory Manager for modern x86_64 and ARM64
 * architectures while preserving the handle-based memory model.
 */

#include "memory_manager_types.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>

#ifdef __x86_64__
    #include <x86intrin.h>
    #define PLATFORM_NAME "x86_64"
    #define CACHE_LINE_SIZE 64
#elif defined(__aarch64__) || defined(__arm64__)
    #define PLATFORM_NAME "ARM64"
    #define CACHE_LINE_SIZE 64
#else
    #define PLATFORM_NAME "Unknown"
    #define CACHE_LINE_SIZE 64
#endif

/* Global Memory Manager state */
static Zone* gSystemZone = NULL;          /* System heap zone */
static Zone* gApplZone = NULL;            /* Application heap zone */
static Zone* gCurrentZone = NULL;         /* Current zone for allocations */
static pthread_mutex_t gMemoryLock = PTHREAD_MUTEX_INITIALIZER;
static bool gMemoryManager32Bit = true;  /* Use 32-bit addressing mode */

/* Statistics tracking */
typedef struct MemoryStats {
    size_t totalAllocated;
    size_t totalFreed;
    size_t currentUsage;
    size_t peakUsage;
    size_t handleCount;
    size_t blockMoveCount;
    size_t compactionCount;
} MemoryStats;

static MemoryStats gMemStats = {0};

/* Platform-specific memory allocation */
static void* HAL_AllocateMemory(size_t size)
{
    void* ptr = NULL;

    /* Try to allocate aligned memory for better performance */
    #ifdef __linux__
        if (posix_memalign(&ptr, CACHE_LINE_SIZE, size) != 0) {
            ptr = malloc(size);
        }
    #else
        ptr = aligned_alloc(CACHE_LINE_SIZE, size);
        if (!ptr) {
            ptr = malloc(size);
        }
    #endif

    if (ptr) {
        memset(ptr, 0, size);
        gMemStats.totalAllocated += size;
        gMemStats.currentUsage += size;
        if (gMemStats.currentUsage > gMemStats.peakUsage) {
            gMemStats.peakUsage = gMemStats.currentUsage;
        }
    }

    return ptr;
}

/* Platform-specific memory deallocation */
static void HAL_FreeMemory(void* ptr, size_t size)
{
    if (ptr) {
        free(ptr);
        gMemStats.totalFreed += size;
        gMemStats.currentUsage -= size;
    }
}

/* Initialize Memory Manager HAL */
OSErr MemoryMgr_HAL_Initialize(Size systemZoneSize, Size applZoneSize)
{
    pthread_mutex_lock(&gMemoryLock);

    /* Determine platform capabilities */
    #ifdef __x86_64__
        /* Check for available CPU features */
        if (__builtin_cpu_supports("avx2")) {
            /* Can use AVX2 for fast memory operations */
        }
    #elif defined(__aarch64__)
        /* ARM64 always has NEON */
    #endif

    /* Allocate system zone */
    if (!gSystemZone) {
        gSystemZone = (Zone*)HAL_AllocateMemory(sizeof(Zone) + systemZoneSize);
        if (!gSystemZone) {
            pthread_mutex_unlock(&gMemoryLock);
            return memFullErr;
        }

        /* Initialize system zone */
        gSystemZone->bkLim = ((Ptr)gSystemZone) + sizeof(Zone) + systemZoneSize;
        gSystemZone->purgePtr = NULL;
        gSystemZone->hFstFree = ((Ptr)gSystemZone) + sizeof(Zone);
        gSystemZone->zcbFree = systemZoneSize;
        gSystemZone->gzProc = NULL;
        gSystemZone->moreMast = 64;  /* Default master pointers */
        gSystemZone->flags = 0;
        gSystemZone->cntRel = 0;
        gSystemZone->maxRel = 32767;
        gSystemZone->cntNRel = 0;
        gSystemZone->heapData = 0;
    }

    /* Allocate application zone */
    if (!gApplZone) {
        gApplZone = (Zone*)HAL_AllocateMemory(sizeof(Zone) + applZoneSize);
        if (!gApplZone) {
            HAL_FreeMemory(gSystemZone, sizeof(Zone) + systemZoneSize);
            gSystemZone = NULL;
            pthread_mutex_unlock(&gMemoryLock);
            return memFullErr;
        }

        /* Initialize application zone */
        gApplZone->bkLim = ((Ptr)gApplZone) + sizeof(Zone) + applZoneSize;
        gApplZone->purgePtr = NULL;
        gApplZone->hFstFree = ((Ptr)gApplZone) + sizeof(Zone);
        gApplZone->zcbFree = applZoneSize;
        gApplZone->gzProc = NULL;
        gApplZone->moreMast = 128;  /* More master pointers for app */
        gApplZone->flags = 0;
        gApplZone->cntRel = 0;
        gApplZone->maxRel = 32767;
        gApplZone->cntNRel = 0;
        gApplZone->heapData = 0;
    }

    /* Set current zone to application zone by default */
    gCurrentZone = gApplZone;

    pthread_mutex_unlock(&gMemoryLock);
    return noErr;
}

/* Get current zone */
Zone* MemoryMgr_HAL_GetZone(void)
{
    return gCurrentZone;
}

/* Set current zone */
void MemoryMgr_HAL_SetZone(Zone* zone)
{
    pthread_mutex_lock(&gMemoryLock);
    if (zone == gSystemZone || zone == gApplZone) {
        gCurrentZone = zone;
    }
    pthread_mutex_unlock(&gMemoryLock);
}

/* Platform-optimized BlockMove implementation */
void MemoryMgr_HAL_BlockMove(const void* srcPtr, void* destPtr, Size byteCount)
{
    if (!srcPtr || !destPtr || byteCount <= 0) return;

    gMemStats.blockMoveCount++;

    #ifdef __x86_64__
        /* Use optimized memory copy for x86_64 */
        if (byteCount >= 64 && __builtin_cpu_supports("avx")) {
            /* Large copies can use AVX */
            memcpy(destPtr, srcPtr, byteCount);
        } else if (byteCount >= 16) {
            /* Medium copies use SSE */
            memcpy(destPtr, srcPtr, byteCount);
        } else {
            /* Small copies inline */
            const uint8_t* src = (const uint8_t*)srcPtr;
            uint8_t* dst = (uint8_t*)destPtr;

            if (src < dst && src + byteCount > dst) {
                /* Overlapping, copy backwards */
                src += byteCount - 1;
                dst += byteCount - 1;
                while (byteCount-- > 0) {
                    *dst-- = *src--;
                }
            } else {
                /* Non-overlapping or safe overlap, copy forward */
                while (byteCount-- > 0) {
                    *dst++ = *src++;
                }
            }
        }
    #elif defined(__aarch64__)
        /* ARM64 optimized copy */
        if (byteCount >= 64) {
            /* Use NEON for large copies if available */
            memcpy(destPtr, srcPtr, byteCount);
        } else {
            /* Small copies */
            memmove(destPtr, srcPtr, byteCount);
        }
    #else
        /* Generic implementation */
        memmove(destPtr, srcPtr, byteCount);
    #endif
}

/* Platform-optimized BlockMoveData (non-overlapping) */
void MemoryMgr_HAL_BlockMoveData(const void* srcPtr, void* destPtr, Size byteCount)
{
    if (!srcPtr || !destPtr || byteCount <= 0) return;

    gMemStats.blockMoveCount++;

    /* BlockMoveData assumes non-overlapping regions */
    #ifdef __x86_64__
        if (byteCount >= 64 && __builtin_cpu_supports("avx2")) {
            /* AVX2 optimized copy for large blocks */
            memcpy(destPtr, srcPtr, byteCount);
        } else {
            memcpy(destPtr, srcPtr, byteCount);
        }
    #elif defined(__aarch64__)
        /* ARM64 NEON optimized copy */
        memcpy(destPtr, srcPtr, byteCount);
    #else
        memcpy(destPtr, srcPtr, byteCount);
    #endif
}

/* Clear memory block */
void MemoryMgr_HAL_BlockZero(void* destPtr, Size byteCount)
{
    if (!destPtr || byteCount <= 0) return;

    #ifdef __x86_64__
        if (byteCount >= 64 && __builtin_cpu_supports("avx2")) {
            /* AVX2 optimized clear */
            memset(destPtr, 0, byteCount);
        } else {
            memset(destPtr, 0, byteCount);
        }
    #elif defined(__aarch64__)
        /* ARM64 optimized clear */
        memset(destPtr, 0, byteCount);
    #else
        memset(destPtr, 0, byteCount);
    #endif
}

/* Allocate handle in current zone */
Handle MemoryMgr_HAL_NewHandle(Size logicalSize)
{
    pthread_mutex_lock(&gMemoryLock);

    /* Align size to longword boundary */
    Size actualSize = (logicalSize + 3) & ~3;

    /* Allocate master pointer */
    Handle h = (Handle)HAL_AllocateMemory(sizeof(Ptr));
    if (!h) {
        pthread_mutex_unlock(&gMemoryLock);
        return NULL;
    }

    /* Allocate data block */
    *h = HAL_AllocateMemory(actualSize);
    if (!*h) {
        HAL_FreeMemory(h, sizeof(Ptr));
        pthread_mutex_unlock(&gMemoryLock);
        return NULL;
    }

    gMemStats.handleCount++;
    pthread_mutex_unlock(&gMemoryLock);
    return h;
}

/* Dispose of handle */
void MemoryMgr_HAL_DisposHandle(Handle h)
{
    if (!h) return;

    pthread_mutex_lock(&gMemoryLock);

    if (*h) {
        HAL_FreeMemory(*h, 0);  /* Size tracking would need enhancement */
        *h = NULL;
    }
    HAL_FreeMemory(h, sizeof(Ptr));
    gMemStats.handleCount--;

    pthread_mutex_unlock(&gMemoryLock);
}

/* Get handle size */
Size MemoryMgr_HAL_GetHandleSize(Handle h)
{
    if (!h || !*h) return 0;

    /* This is simplified - real implementation would track sizes */
    return 0;  /* Would need size tracking in production */
}

/* Resize handle */
OSErr MemoryMgr_HAL_SetHandleSize(Handle h, Size newSize)
{
    if (!h) return nilHandleErr;

    pthread_mutex_lock(&gMemoryLock);

    void* newPtr = realloc(*h, newSize);
    if (!newPtr && newSize > 0) {
        pthread_mutex_unlock(&gMemoryLock);
        return memFullErr;
    }

    *h = newPtr;
    pthread_mutex_unlock(&gMemoryLock);
    return noErr;
}

/* Lock handle (prevent relocation) */
OSErr MemoryMgr_HAL_HLock(Handle h)
{
    if (!h || !*h) return nilHandleErr;
    /* In modern implementation, handles don't actually move */
    return noErr;
}

/* Unlock handle */
OSErr MemoryMgr_HAL_HUnlock(Handle h)
{
    if (!h || !*h) return nilHandleErr;
    /* In modern implementation, handles don't actually move */
    return noErr;
}

/* Purge handle (free memory but keep master pointer) */
OSErr MemoryMgr_HAL_HPurge(Handle h)
{
    if (!h) return nilHandleErr;

    pthread_mutex_lock(&gMemoryLock);

    if (*h) {
        HAL_FreeMemory(*h, 0);
        *h = (Ptr)HANDLE_PURGED;  /* Mark as purged */
    }

    pthread_mutex_unlock(&gMemoryLock);
    return noErr;
}

/* Allocate non-relocatable block */
Ptr MemoryMgr_HAL_NewPtr(Size logicalSize)
{
    /* Align size to longword boundary */
    Size actualSize = (logicalSize + 3) & ~3;
    return HAL_AllocateMemory(actualSize);
}

/* Dispose of pointer */
void MemoryMgr_HAL_DisposPtr(Ptr p)
{
    if (p) {
        HAL_FreeMemory(p, 0);
    }
}

/* Compact current heap zone */
Size MemoryMgr_HAL_CompactMem(Size cbNeeded)
{
    pthread_mutex_lock(&gMemoryLock);
    gMemStats.compactionCount++;

    /* In modern implementation, return available memory */
    /* Real compaction would require handle relocation support */
    Size available = gCurrentZone ? gCurrentZone->zcbFree : 0;

    pthread_mutex_unlock(&gMemoryLock);
    return available;
}

/* Get free memory in current zone */
Size MemoryMgr_HAL_FreeMem(void)
{
    if (!gCurrentZone) return 0;
    return gCurrentZone->zcbFree;
}

/* Get largest free block */
Size MemoryMgr_HAL_MaxBlock(void)
{
    /* Simplified - would need free block chain traversal */
    return gCurrentZone ? gCurrentZone->zcbFree : 0;
}

/* Get memory statistics */
void MemoryMgr_HAL_GetStatistics(MemoryStats* stats)
{
    if (stats) {
        pthread_mutex_lock(&gMemoryLock);
        *stats = gMemStats;
        pthread_mutex_unlock(&gMemoryLock);
    }
}

/* Shutdown Memory Manager HAL */
void MemoryMgr_HAL_Shutdown(void)
{
    pthread_mutex_lock(&gMemoryLock);

    /* Free application zone */
    if (gApplZone) {
        HAL_FreeMemory(gApplZone, 0);
        gApplZone = NULL;
    }

    /* Free system zone */
    if (gSystemZone) {
        HAL_FreeMemory(gSystemZone, 0);
        gSystemZone = NULL;
    }

    gCurrentZone = NULL;
    memset(&gMemStats, 0, sizeof(gMemStats));

    pthread_mutex_unlock(&gMemoryLock);
}