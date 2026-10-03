/*
 * System 7.1 Portable - Common Mac OS Types and Error Codes
 *
 * This header defines common types and error codes used throughout
 * the Mac OS 7.1 portable implementation to avoid duplication and
 * ensure consistency.
 *
 * Copyright (c) 2024 - Portable Mac OS Project
 */

#ifndef MACTYPES_H
#define MACTYPES_H

#include "SystemTypes.h"

/* Pascal calling convention (ignored on modern systems) */
#ifndef pascal
#define pascal
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Basic Mac OS Types
 * ============================================================================ */

/* Classic Mac OS types */

/* Bits16 is defined in SystemTypes.h as an array */

/* Pascal string type (length byte followed by characters) */

/* Fixed-point types */
         /* 16.16 fixed point */
         /* 2.14 fixed point */

/* Rectangle and Point */
/* Type moved to SystemTypes.h */

/* Type moved to SystemTypes.h */

/* Pattern - 8x8 bit pattern */

/* BitMap - Basic bitmap structure */

/* RGBColor - RGB color specification */

/* ColorSpec - Color specification entry */
#ifndef __COLOR_SPEC_DEFINED__
#define __COLOR_SPEC_DEFINED__

#endif

/* Forward declarations for QuickDraw types */

/* GrafPort - Basic drawing environment */
/* Type moved to SystemTypes.h */

/* CGrafPort - Color graphics port */
/* Type moved to SystemTypes.h */

/* Memory sizes */

/* Reference numbers */

       /* Volume reference number */
     /* Driver reference number */
          /* Directory ID */

/* File System Specification */
#ifndef FILESYSTEM_TYPES_DEFINED
#define FILESYSTEM_TYPES_DEFINED

#endif /* FILESYSTEM_TYPES_DEFINED */

/* Menu handle */
#ifndef MENU_TYPES_DEFINED
#define MENU_TYPES_DEFINED

#endif /* MENU_TYPES_DEFINED */

/* Dialog pointer */

/* Procedure pointer */

/* Control handle */

/* Modal filter procedure */

/* Event record */

/* Event mask */

/* Text encoding */

/* String handle */

/* Additional missing types */

/* Additional type aliases */

/* Color Table Handle */

/* Auxiliary Window structures */

/* Additional missing handle types from error analysis */

/* Text Manager types */

/* Icon Manager types */

/* Palette Manager types */

/* Window Manager color table */

/* Control Manager types */

/* Sound Manager types */

/* Driver types */

/* Window record */
/* Type moved to SystemTypes.h */

/* ============================================================================
 * Common Mac OS Error Codes
 * ============================================================================ */

/* ============================================================================
 * Common Constants
 * ============================================================================ */

/* Boolean values */
#ifndef true
#define true    1
#endif

#ifndef false
#define false   0
#endif

/* NULL definitions */
#ifndef NULL
#define NULL    ((void*)0)
#endif

#ifndef nil
#define nil     NULL
#endif

/* File permissions */
#define fsRdPerm    1       /* Read permission */
#define fsWrPerm    2       /* Write permission */
#define fsRdWrPerm  3       /* Read/write permission */

/* File positioning modes */
#define fsAtMark    0       /* At current mark */
#define fsFromStart 1       /* From beginning of file */
#define fsFromLEOF  2       /* From logical end of file */
#define fsFromMark  3       /* From current mark */

/* Resource attributes - now defined in ResourceMgr/resource_types.h */

/* B-Tree Control Block Flags */
#define BTCDirty        0x0001  /* B-Tree needs to be written */
#define BTCWriteReq     0x0002  /* Write request pending */

/* Additional forward declarations for undefined structs */

/* Control color table handle */

/* Missing cursor handle type */

/* Additional missing types */

/* Component Manager types */

#ifdef __cplusplus
}
#endif

#endif /* MACTYPES_H */
