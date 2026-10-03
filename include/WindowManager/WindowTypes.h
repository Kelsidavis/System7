/*
 * WindowTypes.h - Window Manager Type Definitions
 *
 * This header defines all data structures, constants, and type definitions
 * used by the portable Window Manager. Binary and behavioral compatibility
 * is not guaranteed for every original Apple Macintosh System 7.1 interface.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef __WINDOW_TYPES_H__
#define __WINDOW_TYPES_H__

#include "SystemTypes.h"


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Ptr is defined in MacTypes.h */
/* WindowPeek is defined in MacTypes.h */
/* Ptr is defined in MacTypes.h */
/* WindowPeek is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Additional Window Manager types */
typedef void (*DragGrayRgnProcPtr)(void);
typedef struct WCTab** WCTabHandle;
typedef struct WindowManagerState WindowManagerState;

/* ============================================================================
 * Basic Mac OS Types
 * ============================================================================ */

/* Basic types are defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

#ifndef true
#define true 1
#define false 0
#endif

/* Window Part Codes are defined in SystemTypes.h */

/* ============================================================================
 * Event Modifier Flags - Missing from SystemTypes.h
 * ============================================================================ */
/* These are now defined in EventTypes.h as an enum */
/* Skip old-style macro definitions since EventTypes.h provides enums */

/* ============================================================================
 * Geometry Types
 * ============================================================================ */

/* Point type defined in MacTypes.h */

/* Rect type defined in MacTypes.h */

/* Handle is defined in MacTypes.h */

/* ============================================================================
 * Graphics Types
 * ============================================================================ */

/* Pattern, BitMap, GrafPort, GrafPtr, CGrafPort and CGrafPtr are defined in MacTypes.h */

/* Handle is defined in MacTypes.h */

/* ColorSpec is defined in SystemTypes.h. */

/* ============================================================================
 * Control and Event Types
 * ============================================================================ */

/* Handle is defined in MacTypes.h */

/* EventRecord is defined in MacTypes.h */

/* ============================================================================
 * Window Definition Constants
 * ============================================================================ */

/* Window definition procedure IDs */

/* Window kinds */

/* FindWindow result codes */

/* Window messages for WDEF */

/* Window part codes for WDEF hit testing */

/* Window color table part identifiers */

/* Desktop pattern ID */

/* Floating window kinds (System 7.1 extension) */

/* ============================================================================
 * Window Data Structures
 * ============================================================================ */

/* Window state data for zooming */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* WindowRecord is defined in MacTypes.h */

/* Color window record - extends WindowRecord for color */

/* Auxiliary window record for color information */

/* Window color table structure */
typedef struct WinCTab {
    SInt32 ctSeed;                      /* Color table seed */
    short wCReserved;                   /* Reserved field */
    short ctSize;                       /* Number of entries (usually 4) */
    ColorSpec ctTable[5];               /* Color specifications */
} WinCTab, WCTab;

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* ============================================================================
 * Function Pointer Types
 * ============================================================================ */

/* Window definition procedure */

/* Drag gray region callback */

/* ============================================================================
 * Window Manager State Types
 * ============================================================================ */

/* Window Manager port structure */

/* Window list entry for internal management */

/* Complete Window Manager state */

/* ============================================================================
 * Extended Window Attributes (System 7.1)
 * ============================================================================ */

/* Window attributes for modern features */

/* Window classes for layering */

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

/* Rectangle and Point utilities - implemented as functions in QuickDraw */
/*
#define EmptyRect(r) ((r)->left >= (r)->right || (r)->top >= (r)->bottom)
#define EqualRect(r1, r2) ((r1)->left == (r2)->left && (r1)->top == (r2)->top && \
                          (r1)->right == (r2)->right && (r1)->bottom == (r2)->bottom)
#define EqualPt(p1, p2) ((p1).h == (p2).h && (p1).v == (p2).v)
*/

/* Window utilities */
/* Temporarily disabled - conflicts with function declarations
#define GetWindowPort(w) ((GrafPtr)(w))
#define GetWindowFromPort(p) ((WindowPtr)(p))
#define IsWindowVisible(w) ((w) && (w)->visible)
#define IsWindowHilited(w) ((w) && (w)->hilited)
*/

/* Type checking macros */
#define IsWindowPtr(w) ((w) != NULL)
#define IsColorWindow(w) (sizeof(*(w)) == sizeof(CWindowRecord))

#ifdef __cplusplus
}
#endif

#endif /* __WINDOW_TYPES_H__ */
