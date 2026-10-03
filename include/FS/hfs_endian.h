/* HFS Endian Conversion Utilities */
#pragma once
#include <stdint.h>
#include "ByteOrder.h"

/* Write big-endian values to memory */
static inline void be16_write(void* p, uint16_t v) {
    uint8_t* d = (uint8_t*)p;
    d[0] = v >> 8;
    d[1] = v & 0xFF;
}

static inline void be32_write(void* p, uint32_t v) {
    uint8_t* d = (uint8_t*)p;
    d[0] = v >> 24;
    d[1] = (v >> 16) & 0xFF;
    d[2] = (v >> 8) & 0xFF;
    d[3] = v & 0xFF;
}

/* Convert OSType (4-char code) to/from big-endian */
static inline uint32_t ostype_from_be(uint32_t be) {
    return be32_read(&be);
}

static inline uint32_t ostype_to_be(uint32_t native) {
    uint32_t be;
    be32_write(&be, native);
    return be;
}

/* Make OSType from 4 characters */
static inline uint32_t make_ostype(char a, char b, char c, char d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) |
           ((uint32_t)c << 8)  | (uint32_t)d;
}

/* Swap bytes in 16-bit value to convert to big-endian */
static inline uint16_t be16_swap(uint16_t v) {
    return (v >> 8) | (v << 8);
}
