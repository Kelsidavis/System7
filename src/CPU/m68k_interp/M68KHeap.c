/*
 * M68KHeap.c - the application heap, in the application's own memory
 *
 * A 68K program's handles and pointers have to be addresses it can follow,
 * so its heap is a range of its address space, managed here. A handle is the
 * address of a master pointer; the master pointer holds the block's address
 * in its low 24 bits and the handle's state in its top byte - locked $80,
 * purgeable $40, resource $20 - as on a Macintosh running in 24-bit mode,
 * which is what a program of the period may assume.
 *
 * Every block starts with an 8-byte header: its logical size, then for a
 * relocatable block the address of its master pointer (RecoverHandle needs
 * it). Blocks never move once placed. A block that has to grow and cannot
 * grow where it is gets a new place, as a relocatable one may; a locked one
 * or a pointer cannot, which is the Memory Manager's own rule.
 *
 * Which ranges are in use is kept here, on the host side, sorted by address.
 */

#include <string.h>

#include "CPU/M68KHeap.h"
#include "CPU/M68KInterp.h"

extern UInt8 M68K_Read8(M68KAddressSpace* as, UInt32 addr);
extern UInt32 M68K_Read32(M68KAddressSpace* as, UInt32 addr);
extern void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value);
extern void M68K_Write32(M68KAddressSpace* as, UInt32 addr, UInt32 value);

enum {
    kHeader = 8,
    kMaxBlocks = 16384,
    kMastersPerBlock = 64,
    kMaxFreeMasters = 4096,
    kAddrMask = 0x00FFFFFF
};

typedef struct {
    UInt32 start;           /* of the header */
    UInt32 size;            /* header included, rounded to 4 */
} Block;

static struct {
    M68KAddressSpace* as;
    UInt32 start, end;      /* the zone's range */
    Block blocks[kMaxBlocks];
    int count;
    UInt32 freeMasters[kMaxFreeMasters];
    int freeMasterCount;
    OSErr memErr;
} gHeap;

static UInt32 RoundSize(UInt32 logical) {
    return (logical + kHeader + 3) & ~3u;
}

OSErr M68KHeap_LastError(void) {
    return gHeap.memErr;
}

/* The block whose data begins at p, or -1 */
static int FindBlock(UInt32 p) {
    UInt32 header = (p & kAddrMask) - kHeader;
    int lo = 0, hi = gHeap.count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (gHeap.blocks[mid].start == header) return mid;
        if (gHeap.blocks[mid].start < header) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

/* The first gap that fits size bytes; its address, or 0 */
static UInt32 Place(UInt32 size, int* index) {
    UInt32 at = gHeap.start;
    for (int i = 0; i <= gHeap.count; i++) {
        UInt32 next = i < gHeap.count ? gHeap.blocks[i].start : gHeap.end;
        if (next >= at && next - at >= size) {
            *index = i;
            return at;
        }
        if (i < gHeap.count) at = gHeap.blocks[i].start + gHeap.blocks[i].size;
    }
    return 0;
}

static UInt32 Allocate(UInt32 logical, Boolean clear, UInt32 master) {
    UInt32 size = RoundSize(logical);
    int index;
    if (gHeap.count >= kMaxBlocks) {
        gHeap.memErr = memFullErr;
        return 0;
    }
    UInt32 at = Place(size, &index);
    if (!at) {
        gHeap.memErr = memFullErr;
        return 0;
    }
    memmove(&gHeap.blocks[index + 1], &gHeap.blocks[index],
            (size_t)(gHeap.count - index) * sizeof(Block));
    gHeap.blocks[index].start = at;
    gHeap.blocks[index].size = size;
    gHeap.count++;

    M68K_Write32(gHeap.as, at, logical);
    M68K_Write32(gHeap.as, at + 4, master);
    if (clear) {
        for (UInt32 i = 0; i < logical; i++) M68K_Write8(gHeap.as, at + kHeader + i, 0);
    }
    gHeap.memErr = noErr;
    return at + kHeader;
}

static void Release(int index) {
    memmove(&gHeap.blocks[index], &gHeap.blocks[index + 1],
            (size_t)(gHeap.count - index - 1) * sizeof(Block));
    gHeap.count--;
}

/* Grow or shrink the block at p where it is; false if it cannot grow there */
static Boolean ResizeInPlace(UInt32 p, UInt32 logical) {
    int i = FindBlock(p);
    if (i < 0) return false;
    UInt32 size = RoundSize(logical);
    UInt32 limit = i + 1 < gHeap.count ? gHeap.blocks[i + 1].start : gHeap.end;
    if (gHeap.blocks[i].start + size > limit) return false;
    gHeap.blocks[i].size = size;
    M68K_Write32(gHeap.as, gHeap.blocks[i].start, logical);
    return true;
}

static UInt32 TakeMaster(void) {
    if (gHeap.freeMasterCount == 0) {
        UInt32 block = Allocate(kMastersPerBlock * 4, true, 0);
        if (!block) return 0;
        for (int i = kMastersPerBlock - 1; i >= 0; i--) {
            if (gHeap.freeMasterCount < kMaxFreeMasters)
                gHeap.freeMasters[gHeap.freeMasterCount++] = block + 4 * (UInt32)i;
        }
    }
    return gHeap.freeMasters[--gHeap.freeMasterCount];
}

static void GiveMaster(UInt32 h) {
    M68K_Write32(gHeap.as, h, 0);
    if (gHeap.freeMasterCount < kMaxFreeMasters) gHeap.freeMasters[gHeap.freeMasterCount++] = h;
}

/* ------------------------------------------------------------------------ */

void M68KHeap_Init(M68KAddressSpace* as, UInt32 start, UInt32 size) {
    memset(&gHeap, 0, sizeof(gHeap));
    gHeap.as = as;
    gHeap.start = (start + 3) & ~3u;
    gHeap.end = start + size;
}

void M68KHeap_Bounds(UInt32* start, UInt32* end) {
    *start = gHeap.start;
    *end = gHeap.end;
}

UInt32 M68KHeap_NewPtr(UInt32 logical, Boolean clear) {
    return Allocate(logical, clear, 0);
}

OSErr M68KHeap_DisposePtr(UInt32 p) {
    int i = FindBlock(p);
    if (i < 0) return gHeap.memErr = memWZErr;
    Release(i);
    return gHeap.memErr = noErr;
}

UInt32 M68KHeap_GetPtrSize(UInt32 p) {
    if (FindBlock(p) < 0) {
        gHeap.memErr = memWZErr;
        return 0;
    }
    gHeap.memErr = noErr;
    return M68K_Read32(gHeap.as, (p & kAddrMask) - kHeader);
}

OSErr M68KHeap_SetPtrSize(UInt32 p, UInt32 logical) {
    if (FindBlock(p) < 0) return gHeap.memErr = memWZErr;
    return gHeap.memErr = ResizeInPlace(p, logical) ? noErr : memFullErr;
}

UInt32 M68KHeap_NewHandle(UInt32 logical, Boolean clear) {
    UInt32 h = TakeMaster();
    if (!h) return 0;
    UInt32 p = Allocate(logical, clear, h);
    if (!p) {
        GiveMaster(h);
        return 0;
    }
    M68K_Write32(gHeap.as, h, p);
    return h;
}

UInt32 M68KHeap_NewEmptyHandle(void) {
    UInt32 h = TakeMaster();
    if (h) M68K_Write32(gHeap.as, h, 0);
    gHeap.memErr = h ? noErr : memFullErr;
    return h;
}

UInt32 M68KHeap_Deref(UInt32 h) {
    return M68K_Read32(gHeap.as, h & kAddrMask) & kAddrMask;
}

OSErr M68KHeap_DisposeHandle(UInt32 h) {
    if (!h) return gHeap.memErr = nilHandleErr;
    UInt32 p = M68KHeap_Deref(h);
    if (p) {
        int i = FindBlock(p);
        if (i >= 0) Release(i);
    }
    GiveMaster(h & kAddrMask);
    return gHeap.memErr = noErr;
}

UInt32 M68KHeap_GetHandleSize(UInt32 h) {
    UInt32 p = h ? M68KHeap_Deref(h) : 0;
    if (!p) {
        gHeap.memErr = h ? noErr : nilHandleErr;
        return 0;
    }
    gHeap.memErr = noErr;
    return M68K_Read32(gHeap.as, p - kHeader);
}

OSErr M68KHeap_SetHandleSize(UInt32 h, UInt32 logical) {
    if (!h) return gHeap.memErr = nilHandleErr;
    h &= kAddrMask;
    UInt32 master = M68K_Read32(gHeap.as, h);
    UInt32 p = master & kAddrMask;
    if (!p) return M68KHeap_ReallocHandle(h, logical);
    if (ResizeInPlace(p, logical)) return gHeap.memErr = noErr;
    if (master & 0x80000000u) return gHeap.memErr = memFullErr;     /* locked */

    UInt32 old = M68K_Read32(gHeap.as, p - kHeader);
    UInt32 q = Allocate(logical, false, h);
    if (!q) return gHeap.memErr = memFullErr;
    UInt32 n = old < logical ? old : logical;
    for (UInt32 i = 0; i < n; i++) M68K_Write8(gHeap.as, q + i, M68K_Read8(gHeap.as, p + i));
    int i = FindBlock(p);
    if (i >= 0) Release(i);
    M68K_Write32(gHeap.as, h, (master & 0xFF000000u) | q);
    return gHeap.memErr = noErr;
}

OSErr M68KHeap_ReallocHandle(UInt32 h, UInt32 logical) {
    if (!h) return gHeap.memErr = nilHandleErr;
    h &= kAddrMask;
    UInt32 master = M68K_Read32(gHeap.as, h);
    UInt32 p = master & kAddrMask;
    if (p) {
        int i = FindBlock(p);
        if (i >= 0) Release(i);
    }
    UInt32 q = Allocate(logical, false, h);
    M68K_Write32(gHeap.as, h, q ? ((master & 0xFF000000u) | q) : 0);
    return gHeap.memErr = q ? noErr : memFullErr;
}

OSErr M68KHeap_EmptyHandle(UInt32 h) {
    if (!h) return gHeap.memErr = nilHandleErr;
    h &= kAddrMask;
    UInt32 master = M68K_Read32(gHeap.as, h);
    UInt32 p = master & kAddrMask;
    if (master & 0x80000000u) return gHeap.memErr = memLockedErr;
    if (p) {
        int i = FindBlock(p);
        if (i >= 0) Release(i);
    }
    M68K_Write32(gHeap.as, h, master & 0xFF000000u);
    return gHeap.memErr = noErr;
}

UInt32 M68KHeap_RecoverHandle(UInt32 p) {
    if (FindBlock(p) < 0) {
        gHeap.memErr = memWZErr;
        return 0;
    }
    gHeap.memErr = noErr;
    return M68K_Read32(gHeap.as, (p & kAddrMask) - kHeader + 4);
}

UInt8 M68KHeap_GetState(UInt32 h) {
    return (UInt8)(M68K_Read32(gHeap.as, h & kAddrMask) >> 24);
}

void M68KHeap_SetState(UInt32 h, UInt8 state) {
    h &= kAddrMask;
    UInt32 master = M68K_Read32(gHeap.as, h);
    M68K_Write32(gHeap.as, h, ((UInt32)state << 24) | (master & kAddrMask));
}

Boolean M68KHeap_IsHandle(UInt32 h) {
    h &= kAddrMask;
    if (h < gHeap.start || h >= gHeap.end || (h & 3)) return false;
    UInt32 p = M68KHeap_Deref(h);
    if (!p) return true;                    /* empty */
    int i = FindBlock(p);
    return i >= 0 && M68K_Read32(gHeap.as, gHeap.blocks[i].start + 4) == h;
}

UInt32 M68KHeap_FreeBytes(void) {
    UInt32 used = 0;
    for (int i = 0; i < gHeap.count; i++) used += gHeap.blocks[i].size;
    return (gHeap.end - gHeap.start) - used;
}

UInt32 M68KHeap_LargestFree(void) {
    UInt32 best = 0, at = gHeap.start;
    for (int i = 0; i <= gHeap.count; i++) {
        UInt32 next = i < gHeap.count ? gHeap.blocks[i].start : gHeap.end;
        if (next > at && next - at > best) best = next - at;
        if (i < gHeap.count) at = gHeap.blocks[i].start + gHeap.blocks[i].size;
    }
    return best > kHeader ? best - kHeader : 0;
}
