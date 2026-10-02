/*
 * M68KHeap.h - the application heap, in the application's own memory
 *
 * Addresses here are 68K addresses. Functions that can fail set the error
 * M68KHeap_LastError answers, as MemError would.
 */

#ifndef M68K_HEAP_H
#define M68K_HEAP_H

#include "SystemTypes.h"

struct M68KAddressSpace;

void   M68KHeap_Init(struct M68KAddressSpace* as, UInt32 start, UInt32 size);
void   M68KHeap_Bounds(UInt32* start, UInt32* end);
OSErr  M68KHeap_LastError(void);

UInt32 M68KHeap_NewPtr(UInt32 size, Boolean clear);
OSErr  M68KHeap_DisposePtr(UInt32 p);
UInt32 M68KHeap_GetPtrSize(UInt32 p);
OSErr  M68KHeap_SetPtrSize(UInt32 p, UInt32 size);

UInt32 M68KHeap_NewHandle(UInt32 size, Boolean clear);
UInt32 M68KHeap_NewEmptyHandle(void);
OSErr  M68KHeap_DisposeHandle(UInt32 h);
UInt32 M68KHeap_Deref(UInt32 h);
UInt32 M68KHeap_GetHandleSize(UInt32 h);
OSErr  M68KHeap_SetHandleSize(UInt32 h, UInt32 size);
OSErr  M68KHeap_ReallocHandle(UInt32 h, UInt32 size);
OSErr  M68KHeap_EmptyHandle(UInt32 h);
UInt32 M68KHeap_RecoverHandle(UInt32 p);
UInt8  M68KHeap_GetState(UInt32 h);
void   M68KHeap_SetState(UInt32 h, UInt8 state);
Boolean M68KHeap_IsHandle(UInt32 h);

UInt32 M68KHeap_FreeBytes(void);
UInt32 M68KHeap_LargestFree(void);

#endif /* M68K_HEAP_H */
