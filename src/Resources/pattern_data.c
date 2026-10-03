/* Built-in classic desktop pattern data. */

#include "SystemTypes.h"
#include "PatternMgr/pattern_resources.h"

/* Classic Mac desktop patterns (8x8 bitmaps, 1 bit per pixel) */
/* Each byte represents one row, bit 7 is leftmost pixel */

/* Pattern ID 16: Desktop default (50% gray) */
const uint8_t kPattern16[] = {
    0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55
};

/* Pattern ID 17: 25% gray */
const uint8_t kPattern17[] = {
    0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22
};

/* Pattern ID 18: 75% gray */
const uint8_t kPattern18[] = {
    0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77
};

/* Pattern ID 19: Horizontal lines */
const uint8_t kPattern19[] = {
    0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00
};

/* Pattern ID 20: Vertical lines */
const uint8_t kPattern20[] = {
    0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA
};

/* Pattern ID 21: Diagonal lines (NE-SW) */
const uint8_t kPattern21[] = {
    0x88, 0x44, 0x22, 0x11, 0x88, 0x44, 0x22, 0x11
};

/* Pattern ID 22: Diagonal lines (NW-SE) */
const uint8_t kPattern22[] = {
    0x11, 0x22, 0x44, 0x88, 0x11, 0x22, 0x44, 0x88
};

/* Pattern ID 23: Grid */
const uint8_t kPattern23[] = {
    0xFF, 0xAA, 0xAA, 0xAA, 0xFF, 0xAA, 0xAA, 0xAA
};

/* Pattern ID 24: Dots */
const uint8_t kPattern24[] = {
    0x44, 0x00, 0x11, 0x00, 0x44, 0x00, 0x11, 0x00
};

/* Pattern ID 25: Checkerboard */
const uint8_t kPattern25[] = {
    0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F
};

/* Pattern ID 26: Bricks */
const uint8_t kPattern26[] = {
    0xFF, 0x80, 0x80, 0x80, 0xFF, 0x08, 0x08, 0x08
};

/* Pattern ID 27: Weave */
const uint8_t kPattern27[] = {
    0x94, 0xA9, 0x52, 0xA5, 0x4A, 0x95, 0x2A, 0x55
};

/* Pattern ID 28: Plaid */
const uint8_t kPattern28[] = {
    0xCC, 0x66, 0x33, 0x99, 0xCC, 0x66, 0x33, 0x99
};

/* Pattern ID 29: Scales */
const uint8_t kPattern29[] = {
    0x04, 0x02, 0x01, 0x80, 0x40, 0x20, 0x10, 0x08
};

/* Pattern ID 30: Waves */
const uint8_t kPattern30[] = {
    0x30, 0x48, 0x84, 0x84, 0x48, 0x30, 0x00, 0x00
};

/* Pattern ID 31: Triangles */
const uint8_t kPattern31[] = {
    0x10, 0x38, 0x7C, 0xFE, 0x00, 0x00, 0x00, 0x00
};

/* Pattern ID 32: Cross hatch */
const uint8_t kPattern32[] = {
    0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA
};

/* Pattern ID 33: Diamond */
const uint8_t kPattern33[] = {
    0x10, 0x28, 0x44, 0x82, 0x44, 0x28, 0x10, 0x00
};

/* Pattern ID 34: Circle */
const uint8_t kPattern34[] = {
    0x38, 0x44, 0x82, 0x82, 0x82, 0x44, 0x38, 0x00
};

/* Pattern ID 35: Stars */
const uint8_t kPattern35[] = {
    0x10, 0x54, 0x38, 0xFE, 0x38, 0x54, 0x10, 0x00
};

/* Pattern ID 36: Hexagons */
const uint8_t kPattern36[] = {
    0x30, 0x48, 0x48, 0x30, 0x0C, 0x12, 0x12, 0x0C
};

/* Pattern ID 37: Stipple light */
const uint8_t kPattern37[] = {
    0x08, 0x00, 0x02, 0x00, 0x08, 0x00, 0x02, 0x00
};

/* Pattern ID 38: Stipple medium */
const uint8_t kPattern38[] = {
    0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11
};

/* Pattern ID 39: Stipple heavy */
const uint8_t kPattern39[] = {
    0xEE, 0xBB, 0xEE, 0xBB, 0xEE, 0xBB, 0xEE, 0xBB
};

/* Pattern ID 40: Horizontal gradient */
const uint8_t kPattern40[] = {
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00
};

/* Pattern ID 41: Vertical gradient */
const uint8_t kPattern41[] = {
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08
};

/* Pattern ID 42: Diagonal gradient */
const uint8_t kPattern42[] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
};

/* Pattern ID 43: Zigzag */
const uint8_t kPattern43[] = {
    0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x02
};

/* Pattern ID 44: Solid black */
const uint8_t kPattern44[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

/* Pattern ID 45: Solid white */
const uint8_t kPattern45[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* Pattern ID 46: Fine mesh */
const uint8_t kPattern46[] = {
    0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA
};

/* Pattern ID 47: Coarse mesh */
const uint8_t kPattern47[] = {
    0xCC, 0x33, 0xCC, 0x33, 0xCC, 0x33, 0xCC, 0x33
};

/* Pattern resource table for easy access */
typedef struct PatternEntry {
    int16_t id;
    const uint8_t* data;
} PatternEntry;

const PatternEntry gPatternTable[] = {
    { 16, kPattern16 },
    { 17, kPattern17 },
    { 18, kPattern18 },
    { 19, kPattern19 },
    { 20, kPattern20 },
    { 21, kPattern21 },
    { 22, kPattern22 },
    { 23, kPattern23 },
    { 24, kPattern24 },
    { 25, kPattern25 },
    { 26, kPattern26 },
    { 27, kPattern27 },
    { 28, kPattern28 },
    { 29, kPattern29 },
    { 30, kPattern30 },
    { 31, kPattern31 },
    { 32, kPattern32 },
    { 33, kPattern33 },
    { 34, kPattern34 },
    { 35, kPattern35 },
    { 36, kPattern36 },
    { 37, kPattern37 },
    { 38, kPattern38 },
    { 39, kPattern39 },
    { 40, kPattern40 },
    { 41, kPattern41 },
    { 42, kPattern42 },
    { 43, kPattern43 },
    { 44, kPattern44 },
    { 45, kPattern45 },
    { 46, kPattern46 },
    { 47, kPattern47 },
    { 0, NULL }  /* Sentinel */
};

/* Helper function to get pattern data by ID */
const uint8_t* GetBuiltInPatternData(int16_t patternID) {
    for (int i = 0; gPatternTable[i].data != NULL; i++) {
        if (gPatternTable[i].id == patternID) {
            return gPatternTable[i].data;
        }
    }
    return NULL;
}
