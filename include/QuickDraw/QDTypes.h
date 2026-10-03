/*
 * QDTypes.h - QuickDraw Core Data Types
 *
 * Comprehensive data type definitions for QuickDraw graphics system.
 * This is the foundation header that defines all structures used by QuickDraw.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) QuickDraw
 */

#ifndef __QDTYPES_H__
#define __QDTYPES_H__

#include "SystemTypes.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Basic types */
/* Ptr, Handle, Fixed, Style are defined in MacTypes.h */
/* Str255 and ConstStr255Param are defined in MacTypes.h */

/* QuickDraw Error Types */
typedef short QDErr;
typedef short RegionError;

/* QuickDraw parameter types */
typedef const Pattern* ConstPatternParam;

/* QuickDraw verb types */
typedef short GrafVerb;

/* Cursor is defined in SystemTypes.h */

/* QDGlobalsPtr is defined in SystemTypes.h */

/* Pixel pattern handle */
typedef struct PixPat** PixPatHandle;

/* QuickDraw Constants */

/* QuickDraw verb types for shape drawing */

/* Pixel type enumeration */

/* Basic geometric types */
/* Point type defined in MacTypes.h */

/* Rect type defined in MacTypes.h */

/* Pattern structure - 8x8 bit pattern */
/* Pattern type defined in MacTypes.h */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Bitmap structures */

/* BitMap is in WindowManager/WindowTypes.h */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Cursor structure */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Pen state */

/* Region, RgnPtr and RgnHandle are defined in MacTypes.h */

/* Picture structure */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Polygon structure */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Color structures */
/* RGBColor and ColorSpec are in MacTypes.h */

/* Ptr is defined in MacTypes.h */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* PixMap - Color bitmap */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* PixPat - Pixel pattern */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Color cursor */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Color icon */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Match record for color matching */

/* Gamma table */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Inverse table */

/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */

/* Font information */

/* Search and complement procedure types */

/* Search procedure record */

/* Ptr is defined in MacTypes.h */

/* Complement procedure record */

/* Ptr is defined in MacTypes.h */

/* Device loop flags */

/* Device loop drawing procedure */

/* OpenCPicture parameters */

/* Request list record */

/* Bit manipulation constants */

#ifdef __cplusplus
}
#endif

#endif /* __QDTYPES_H__ */