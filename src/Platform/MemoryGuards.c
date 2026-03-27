/*
 * MemoryGuards.c - Memory Safety Utilities Implementation
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#include "Platform/MemoryGuards.h"
#include <stdio.h>

/* External serial logging function */
extern void serial_puts(const char* str);
extern int sprintf(char* buf, const char* fmt, ...);

Boolean MemGuard_ValidateBufferNotInStruct(const void* bufferPtr,
                                           const void* structPtr,
                                           size_t structSize,
                                           const char* errorMsg) {
    if (!bufferPtr || !structPtr) {
        return true;  /* NULL pointers handled elsewhere */
    }

    if (MemGuard_PointerInRange(bufferPtr, structPtr, structSize)) {
        /* Corruption detected - log detailed error */
        serial_puts("[MEMGUARD] ");
        if (errorMsg) {
            serial_puts(errorMsg);
            serial_puts("\n");
        }

        char buf[128];
        snprintf(buf, sizeof(buf), "[MEMGUARD] struct=0x%08x buffer=0x%08x offset=0x%x size=0x%x\n",
                (unsigned int)(uintptr_t)structPtr,
                (unsigned int)(uintptr_t)bufferPtr,
                (unsigned int)((uintptr_t)bufferPtr - (uintptr_t)structPtr),
                (unsigned int)structSize);
        serial_puts(buf);

        return false;  /* Validation failed */
    }

    return true;  /* Validation passed */
}

OSErr MemGuard_ValidateRange(const void* ptr, size_t size) {
    if (!ptr) {
        return nilHandleErr;
    }

    /* Basic sanity checks */
    uintptr_t addr = (uintptr_t)ptr;

    /* Check for obviously invalid addresses */
    if (addr < 0x1000) {
        /* Too close to NULL - likely dereferenced NULL */
        return nilHandleErr;
    }

    if (size > 0x10000000) {  /* > 256MB */
        /* Unreasonably large size - likely corrupted */
        return memFullErr;
    }

    /* Check for wrap-around */
    if (addr + size < addr) {
        return memFullErr;
    }

    return noErr;
}

Boolean MemGuard_LooksValid(const void* ptr, size_t alignment) {
    if (!ptr) {
        return false;
    }

    uintptr_t addr = (uintptr_t)ptr;

    /* Check alignment */
    if (alignment > 0 && (addr % alignment) != 0) {
        return false;
    }

    /* Check for obviously invalid addresses */
    if (addr < 0x1000) {
        return false;  /* Too close to NULL */
    }

    /* Basic reasonableness check - address should be in user space range */
    /* For a 32-bit system, reasonable range is roughly 0x1000 to 0x80000000 */
    if (addr > 0x80000000) {
        return false;  /* Likely kernel space or corrupted */
    }

    return true;
}
