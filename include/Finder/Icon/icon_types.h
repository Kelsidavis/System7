/* System 7 Icon Types and Data Structures
 * Faithful implementation of classic Mac icon system
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Icon bit depths */
typedef enum {
    kIconDepth1,    /* ICN# (1-bit 32×32 w/ mask) */
    kIconDepth4,    /* icl4 (optional) */
    kIconDepth8,    /* icl8 (optional) */
    kIconColor32    /* cicn (32×32 color icon + mask) */
} IconDepth;

/* Icon bitmap data */
typedef struct {
    uint16_t w, h;          /* always 32×32 in System 7 menus/desktop */
    IconDepth depth;
    /* Classic pairs: mask + image */
    const uint8_t* mask1b;  /* 32×32, 1-bit, 128 bytes (4 bytes/row) */
    const uint8_t* img1b;   /* 32×32, 1-bit, 128 bytes (ICN# image) */
    /* Or color: */
    const uint32_t* argb32; /* 32×32, AARRGGBB (for cicn) */
} IconBitmap;

/* Icon family - different sizes/depths */
typedef struct {
    /* Resolved from resources */
    IconBitmap large;   /* 32×32 (desktop/finder) */
    /* Small 16×16 (optional for lists): add if you have SICN/ics# */
    IconBitmap small;   /* 16×16 (list views) */
    bool hasSmall;
} IconFamily;

/* File/folder metadata for icon resolution */
typedef struct {
    uint32_t type;      /* 'TEXT', 'APPL', etc. */
    uint32_t creator;   /* app signature, e.g. 'ttxt' */
    uint16_t flags;     /* custom icon, folder, volume, etc. */
    bool     isFolder;
    bool     isVolume;
    bool     isTrash;
    bool     isTrashFull;
    bool     hasCustomIcon;
    const char* path;   /* for custom icon lookup */
} FileKind;

/* Resolved icon handle */
typedef struct {
    /* Resolved draw recipe */
    const IconFamily* fam;  /* icon to draw (resource-backed or compiled default) */
    bool selected;          /* for selection state */
    bool italicLabel;       /* an alias's name is italic, as in System 7 */
} IconHandle;

/* Rectangle type for hit testing */
typedef struct {
    int left, top, right, bottom;
} IconRect;

/* Icon slot for hit testing */
typedef struct {
    IconRect iconR;     /* Icon bounds */
    IconRect labelR;    /* Label bounds */
    int objectId;       /* Object identifier */
} IconSlot;

/* Public API */
bool Icon_Init(void);  /* Load system icon set once */
bool Icon_ResolveForNode(const FileKind* fk, IconHandle* out); /* Lookup by custom icon / BNDL/FREF / defaults */
void Icon_Draw32(const IconHandle* h, int x, int y, bool selected);  /* Draw at 32×32, optionally darkened if selected */
void Icon_Draw16(const IconHandle* h, int x, int y);           /* Draw at 16×16 (list views) */

/* Icon with label */
IconRect Icon_DrawWithLabel(const IconHandle* h, const char* name, int centerX, int iconTopY, bool selected);

/* Hit testing */
int Icon_HitTest(const IconSlot* slots, int count, int x, int y);
