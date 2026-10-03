#ifndef MENU_BITS_PRIVATE_H
#define MENU_BITS_PRIVATE_H

#include "SystemTypes.h"

/* Shared handle layout used by the menu-bit allocator and save/restore code. */
typedef struct {
    Rect bounds;
    SInt16 mode;
    void *bitsData;
    SInt32 dataSize;
    Boolean valid;
    Boolean fromPool;
} SavedBitsRec, *SavedBitsPtr, **SavedBitsHandle;

#endif /* MENU_BITS_PRIVATE_H */
