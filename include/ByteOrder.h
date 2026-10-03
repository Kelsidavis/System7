#ifndef BYTE_ORDER_H
#define BYTE_ORDER_H

#include <stdint.h>

static inline uint16_t be16_read(const void* p)
{
    const uint8_t* bytes = (const uint8_t*)p;
    return ((uint16_t)bytes[0] << 8) | bytes[1];
}

static inline uint32_t be32_read(const void* p)
{
    const uint8_t* bytes = (const uint8_t*)p;
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | bytes[3];
}

#endif /* BYTE_ORDER_H */
