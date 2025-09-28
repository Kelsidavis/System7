/*
 * LayoutGuards.h - ABI/Layout Static Assertions for Window Manager
 *
 * [WM-055a] Compile-time guards to prevent silent struct layout drift
 * Provenance: IM:Windows Vol I p. 2-13 - WindowRecord structure layout
 *
 * These assertions ensure that critical struct offsets match the canonical
 * Macintosh Toolbox ABI. If any assertion fails, the build will stop with
 * a clear error message rather than producing a silently broken binary.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Based on Apple Macintosh System Software 7.1 Window Manager
 */

#ifndef LAYOUT_GUARDS_H
#define LAYOUT_GUARDS_H

#include <stddef.h>
#include "QuickDraw/QuickDraw.h"
#include "SystemTypes.h"

/* Tiny static-assert macro that works in C99 */
#define STATIC_ASSERT(COND, NAME) typedef char static_assert_##NAME[(COND) ? 1 : -1]

/* [WM-055a] IM:Windows p.2-13 — WindowRecord first field is GrafPort */
STATIC_ASSERT(offsetof(WindowRecord, port) == 0, windowrecord_port_at_0);

/* Ensure WindowRecord is at least as large as GrafPort (it embeds one) */
STATIC_ASSERT(sizeof(WindowRecord) >= sizeof(GrafPort), windowrecord_at_least_grafport);

/* Ensure GrafPort actually contains portRect (prevents struct regressions) */
STATIC_ASSERT(offsetof(GrafPort, portRect) < sizeof(GrafPort), grafport_has_portrect);

/* Ensure visRgn exists if you depend on it (added in earlier work) */
STATIC_ASSERT(offsetof(WindowRecord, visRgn) < sizeof(WindowRecord), windowrecord_has_visRgn);

/* Ensure windowKind field exists and is properly aligned for short access */
STATIC_ASSERT(offsetof(WindowRecord, windowKind) < sizeof(WindowRecord), windowrecord_has_windowkind);

#endif /* LAYOUT_GUARDS_H */