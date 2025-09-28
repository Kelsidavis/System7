/*
 * WindowKinds.h - Window Kind Constants
 *
 * [WM-055] Single source of truth for window kind constants
 * Provenance: IM:Windows Vol I p. 2-15 "Window Kinds"
 *
 * This header defines the canonical window kind values used to classify
 * windows by their behavior and ownership. The window kind is stored in
 * the windowKind field of the WindowRecord structure.
 *
 * Window Kind Values (IM:Windows Vol I p. 2-15):
 * - dialogKind (2): Dialog windows created by the Dialog Manager
 * - userKind (8): Standard application document windows
 * - deskKind (8): Same as userKind in System 7 (desktop windows)
 * - systemKind (-16): System-owned windows (desk accessories, alerts)
 *
 * Negative values are reserved for system windows; positive values for
 * application windows. Zero is reserved and should not be used.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

#ifndef WINDOW_KINDS_H
#define WINDOW_KINDS_H

/* [WM-055] Window Kind Constants from IM:Windows Vol I p. 2-15 */
#ifndef dialogKind
#define dialogKind   2  /* Dialog window */
#endif

#ifndef userKind
#define userKind     8  /* User/application window */
#endif

#ifndef deskKind
#define deskKind     8  /* Same as userKind per System 7 */
#endif

#ifndef systemKind
#define systemKind  (-16)  /* System window (desk accessory, etc.) */
#endif

#endif /* WINDOW_KINDS_H */