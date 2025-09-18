/*
 * MemoryMgr_HAL.h - Hardware Abstraction Layer for Memory Manager
 *
 * Modern platform support for the classic Mac OS Memory Manager
 */

#ifndef MEMORYMGR_HAL_H
#define MEMORYMGR_HAL_H

#include "memory_manager_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct MemoryStats MemoryStats;

/* Memory Manager HAL initialization and shutdown */
OSErr MemoryMgr_HAL_Initialize(Size systemZoneSize, Size applZoneSize);
void MemoryMgr_HAL_Shutdown(void);

/* Zone management */
Zone* MemoryMgr_HAL_GetZone(void);
void MemoryMgr_HAL_SetZone(Zone* zone);

/* Handle-based memory allocation */
Handle MemoryMgr_HAL_NewHandle(Size logicalSize);
void MemoryMgr_HAL_DisposHandle(Handle h);
Size MemoryMgr_HAL_GetHandleSize(Handle h);
OSErr MemoryMgr_HAL_SetHandleSize(Handle h, Size newSize);
OSErr MemoryMgr_HAL_HLock(Handle h);
OSErr MemoryMgr_HAL_HUnlock(Handle h);
OSErr MemoryMgr_HAL_HPurge(Handle h);

/* Pointer-based memory allocation */
Ptr MemoryMgr_HAL_NewPtr(Size logicalSize);
void MemoryMgr_HAL_DisposPtr(Ptr p);

/* Memory operations */
void MemoryMgr_HAL_BlockMove(const void* srcPtr, void* destPtr, Size byteCount);
void MemoryMgr_HAL_BlockMoveData(const void* srcPtr, void* destPtr, Size byteCount);
void MemoryMgr_HAL_BlockZero(void* destPtr, Size byteCount);

/* Memory management */
Size MemoryMgr_HAL_CompactMem(Size cbNeeded);
Size MemoryMgr_HAL_FreeMem(void);
Size MemoryMgr_HAL_MaxBlock(void);

/* Statistics */
struct MemoryStats {
    size_t totalAllocated;
    size_t totalFreed;
    size_t currentUsage;
    size_t peakUsage;
    size_t handleCount;
    size_t blockMoveCount;
    size_t compactionCount;
};

void MemoryMgr_HAL_GetStatistics(MemoryStats* stats);

/* Classic Mac OS Memory Manager API compatibility */
#define NewHandle(size)         MemoryMgr_HAL_NewHandle(size)
#define DisposeHandle(h)        MemoryMgr_HAL_DisposHandle(h)
#define GetHandleSize(h)        MemoryMgr_HAL_GetHandleSize(h)
#define SetHandleSize(h, size)  MemoryMgr_HAL_SetHandleSize(h, size)
#define HLock(h)                MemoryMgr_HAL_HLock(h)
#define HUnlock(h)              MemoryMgr_HAL_HUnlock(h)
#define HPurge(h)               MemoryMgr_HAL_HPurge(h)

#define NewPtr(size)            MemoryMgr_HAL_NewPtr(size)
#define DisposePtr(p)           MemoryMgr_HAL_DisposPtr(p)

#define BlockMove(src, dst, n)      MemoryMgr_HAL_BlockMove(src, dst, n)
#define BlockMoveData(src, dst, n)  MemoryMgr_HAL_BlockMoveData(src, dst, n)

#define CompactMem(size)        MemoryMgr_HAL_CompactMem(size)
#define FreeMem()               MemoryMgr_HAL_FreeMem()
#define MaxBlock()              MemoryMgr_HAL_MaxBlock()

#define GetZone()               MemoryMgr_HAL_GetZone()
#define SetZone(zone)           MemoryMgr_HAL_SetZone(zone)

#ifdef __cplusplus
}
#endif

#endif /* MEMORYMGR_HAL_H */