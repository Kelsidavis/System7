/* Address reservations and page storage shared by the interpreter backends. */
#ifndef CPU_BACKEND_MEMORY_H
#define CPU_BACKEND_MEMORY_H

#include "SystemTypes.h"
#include <string.h>

static inline Boolean CPU_AddressRangeValid(UInt32 addr, Size size, UInt32 limit)
{
    return size >= 0 && addr <= limit && (UInt64)size <= limit - addr;
}

static inline UInt32 CPU_ReserveAddress(UInt32* next, Size size, UInt32 limit)
{
    UInt64 aligned = ((UInt64)*next + 15) & ~(UInt64)15;
    if (aligned >= limit || !CPU_AddressRangeValid((UInt32)aligned, size, limit)) {
        return 0;
    }
    UInt32 addr = (UInt32)aligned;
    *next = addr + (UInt32)size;
    return addr;
}

typedef void* (*CPUPageAllocator)(void* context, UInt32 addr);

static inline OSErr CPU_WritePages(void* context, CPUPageAllocator allocate,
                                  UInt32 pageSize, UInt32 addr, const void* data, Size size)
{
    const UInt8* bytes = (const UInt8*)data;
    for (Size done = 0; done < size;) {
        UInt32 at = addr + (UInt32)done;
        UInt32 offset = at & (pageSize - 1);
        UInt32 count = pageSize - offset;
        if ((Size)count > size - done) count = (UInt32)(size - done);
        UInt8* page = (UInt8*)allocate(context, at);
        if (!page) return memFullErr;
        if (bytes) memcpy(page + offset, bytes + done, count);
        else memset(page + offset, 0, count);
        done += count;
    }
    return noErr;
}

#endif
