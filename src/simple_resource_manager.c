/*
 * Simple Resource Manager
 * Minimal implementation for kernel environment
 */

#ifndef ENABLE_RESOURCES  /* Only compile when not using full ResourceMgr */

#include "SystemTypes.h"
#include "ResourceMgr/resource_manager.h"
#include <string.h>
#include <stdlib.h>

/* Linked resource data from patterns_rsrc.c */
extern const unsigned char patterns_rsrc_data[];
extern const unsigned int patterns_rsrc_size;

/* Memory-based resource fork registration */
static const void* gMemoryResourceFork = NULL;
static size_t gMemoryResourceSize = 0;

/* Register a memory-based resource fork */
void RegisterMemoryResourceFork(const void* ptr, size_t len) {
    gMemoryResourceFork = ptr;
    gMemoryResourceSize = len;
}

/* Parse big-endian 16-bit value */
static uint16_t ReadBE16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

/* Parse big-endian 32-bit value */
static uint32_t ReadBE32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/* Initialize the Resource Manager with built-in resources */
OSErr ResourceManagerInit(void) {
    /* Register built-in resource fork */
    RegisterMemoryResourceFork(patterns_rsrc_data, patterns_rsrc_size);
    return noErr;
}

/* Get a resource by type and ID from memory-based fork */
Handle GetResource(ResType type, short id) {
    extern Handle NewHandle(Size byteCount);
    extern void serial_puts(const char* str);
    extern void uart_flush(void);

    serial_puts("[SRM] GetResource enter\n");
    uart_flush();

    if (!gMemoryResourceFork && patterns_rsrc_size > 0) {
        /* Auto-initialize with built-in resource fork */
        RegisterMemoryResourceFork(patterns_rsrc_data, patterns_rsrc_size);
    }

    if (!gMemoryResourceFork || gMemoryResourceSize < 16) {
        return NULL;
    }

    const uint8_t* rsrc = (const uint8_t*)gMemoryResourceFork;

    /* Parse resource header */
    uint32_t dataOffset = ReadBE32(rsrc + 0);
    uint32_t mapOffset = ReadBE32(rsrc + 4);
    uint32_t dataLength = ReadBE32(rsrc + 8);
    uint32_t mapLength = ReadBE32(rsrc + 12);

    if (dataOffset + dataLength > gMemoryResourceSize ||
        mapOffset + mapLength > gMemoryResourceSize) {
        return NULL;
    }

    /* Parse resource map */
    const uint8_t* map = rsrc + mapOffset;

    /* Skip map header (first 24 bytes) */
    uint16_t typeListOffset = ReadBE16(map + 24);

    const uint8_t* typeList = map + typeListOffset;
    uint16_t numTypes = ReadBE16(typeList) + 1;
    typeList += 2;

    /* Search for the requested type */
    for (int i = 0; i < numTypes; i++) {
        /* Read type as 4 bytes */
        uint32_t resType = ReadBE32(typeList + i * 8);
        uint16_t numRes = ReadBE16(typeList + i * 8 + 4) + 1;
        uint16_t refListOffset = ReadBE16(typeList + i * 8 + 6);

        /* Debug: ARM64 safe debug (no sprintf which can hang due to va_list) */
        if (type == 0x70706174) {  /* 'ppat' */
            serial_puts("[SRM] Searching ppat\n");
            uart_flush();
        }

        if (resType == type) {
            /* Found the type, search for the ID */
            const uint8_t* refList = map + typeListOffset + refListOffset;

            for (int j = 0; j < numRes; j++) {
                int16_t resID = (int16_t)ReadBE16(refList + j * 12);

                if (resID == id) {
                    /* Found it! Get the data offset */
                    uint32_t dataOff = (refList[j * 12 + 5] << 16) |
                                      (refList[j * 12 + 6] << 8) |
                                      refList[j * 12 + 7];

                    /* Read the data */
                    const uint8_t* data = rsrc + dataOffset + dataOff;
                    uint32_t dataSize = ReadBE32(data);
                    data += 4;

                    /* Allocate and return a handle */
                    Handle h = NewHandle(dataSize);
                    if (h) {
                        memcpy(*h, data, dataSize);
                    }
                    return h;
                }
            }
        }
    }

    return NULL;
}
#endif /* !ENABLE_RESOURCES */
