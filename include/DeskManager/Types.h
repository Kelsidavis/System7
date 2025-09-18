#ifndef DESKMANAGER_TYPES_H
#define DESKMANAGER_TYPES_H

/*
 * Types.h - Common Type Definitions for Desk Manager
 *
 * Provides common type definitions used throughout the Desk Manager
 * implementation, including Mac OS compatibility types and structures.
 */

#include <stdint.h>
#include <stdbool.h>

/* Basic Mac OS Types */
typedef struct Point {
    int16_t v;          /* Vertical coordinate */
    int16_t h;          /* Horizontal coordinate */
} Point;

typedef struct Rect {
    int16_t top;        /* Top coordinate */
    int16_t left;       /* Left coordinate */
    int16_t bottom;     /* Bottom coordinate */
    int16_t right;      /* Right coordinate */
} Rect;

/* Forward declarations for Mac OS structures */
typedef struct WindowRecord WindowRecord;

/* Event Record Structure */
typedef struct EventRecord {
    int16_t     what;           /* Event type */
    int32_t     message;        /* Event message */
    int32_t     when;           /* Event time */
    Point       where;          /* Event location */
    int16_t     modifiers;      /* Modifier keys */
} EventRecord;

/* Event Types */
#define NULL_EVENT          0
#define MOUSE_DOWN          1
#define MOUSE_UP            2
#define KEY_DOWN            3
#define KEY_UP              4
#define AUTO_KEY            5
#define UPDATE_EVENT        6
#define DISK_EVENT          7
#define ACTIVATE_EVENT      8
#define OS_EVENT            15

/* Modifier Key Masks */
#define ACTIVE_FLAG         0x0001
#define BTN_STATE           0x0080
#define CMD_KEY             0x0100
#define SHIFT_KEY           0x0200
#define ALPHA_LOCK          0x0400
#define OPTION_KEY          0x0800
#define CONTROL_KEY         0x1000

/* String Types */
typedef unsigned char Str255[256];
typedef const unsigned char *ConstStr255Param;

/* Resource Types */
typedef int32_t OSType;
typedef int16_t ResID;
typedef void** Handle;

/* Error Codes */
typedef int16_t OSErr;
#define noErr           0
#define memFullErr      -108
#define resNotFound     -192
#define paramErr        -50

/* Boolean type for compatibility */
typedef unsigned char Boolean;
#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

/* Utility Macros */
#define HiWord(x)       ((int16_t)(((int32_t)(x) >> 16) & 0xFFFF))
#define LoWord(x)       ((int16_t)((int32_t)(x) & 0xFFFF))
#define MAKE_LONG(hi, lo) (((int32_t)(hi) << 16) | ((int32_t)(lo) & 0xFFFF))

/* Rectangle Utilities */
#define RECT_WIDTH(r)   ((r)->right - (r)->left)
#define RECT_HEIGHT(r)  ((r)->bottom - (r)->top)
#define POINT_IN_RECT(pt, r) \
    ((pt).h >= (r)->left && (pt).h < (r)->right && \
     (pt).v >= (r)->top && (pt).v < (r)->bottom)

#endif /* DESKMANAGER_TYPES_H */