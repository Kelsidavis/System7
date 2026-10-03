/*
 * System 7.1 Portable - Classic Toolbox Compatibility Definitions
 *
 * Shared types and error codes are declared in SystemTypes.h. This header
 * provides compatibility macros and constants used by classic Toolbox code.
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

/* Preserve compatibility guards for types declared in SystemTypes.h. */
#ifndef __COLOR_SPEC_DEFINED__
#define __COLOR_SPEC_DEFINED__
#endif
#ifndef FILESYSTEM_TYPES_DEFINED
#define FILESYSTEM_TYPES_DEFINED
#endif /* FILESYSTEM_TYPES_DEFINED */
#ifndef MENU_TYPES_DEFINED
#define MENU_TYPES_DEFINED
#endif /* MENU_TYPES_DEFINED */

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

/* B-Tree Control Block Flags */
#define BTCDirty        0x0001  /* B-Tree needs to be written */
#define BTCWriteReq     0x0002  /* Write request pending */

#ifdef __cplusplus
}
#endif

#endif /* MACTYPES_H */
