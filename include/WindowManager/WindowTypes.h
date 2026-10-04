/*
 * WindowTypes.h - Window Manager Type Definitions
 *
 * This header declares Window Manager-specific types that are not provided
 * by SystemTypes.h. Binary and behavioral compatibility is not guaranteed
 * for every original Apple Macintosh System 7.1 interface.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef __WINDOW_TYPES_H__
#define __WINDOW_TYPES_H__

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Window Manager-specific types */
typedef void (*DragGrayRgnProcPtr)(void);
typedef struct WCTab** WCTabHandle;
typedef struct WindowManagerState WindowManagerState;

/* ============================================================================
 * Window Data Structures
 * ============================================================================ */

typedef struct WinCTab {
    SInt32 ctSeed;                      /* Color table seed */
    short wCReserved;                   /* Reserved field */
    short ctSize;                       /* Number of entries (usually 4) */
    ColorSpec ctTable[5];               /* Color specifications */
} WinCTab, WCTab;
/* Type checking macros */
#define IsWindowPtr(w) ((w) != NULL)
#define IsColorWindow(w) (sizeof(*(w)) == sizeof(CWindowRecord))

#ifdef __cplusplus
}
#endif

#endif /* __WINDOW_TYPES_H__ */
