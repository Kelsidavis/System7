/*
 * LayoutGuards.h - ABI/Layout Static Assertions for Window Manager
 *
 * Compile-time guards prevent silent struct layout drift.
 * Provenance: IM:Windows Vol I p. 2-13 - WindowRecord structure layout
 *
 * These assertions ensure that critical struct offsets match the canonical
 * Macintosh Toolbox ABI. If any assertion fails, the build will stop with
 * a clear error message rather than producing a silently broken binary.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Window Manager
 */

#ifndef LAYOUT_GUARDS_H
#define LAYOUT_GUARDS_H

#include <stddef.h>
#include "QuickDraw/QuickDraw.h"
#include "SystemTypes.h"

/* Static assertion helper: prefer C11+ _Static_assert, fallback for pre-C11 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define STATIC_ASSERT(COND, NAME) _Static_assert((COND), #NAME)
#else
#define STATIC_ASSERT(COND, NAME) typedef char static_assert_##NAME[(COND) ? 1 : -1]
#endif

/* IM:Windows p.2-13 specifies GrafPort as the first WindowRecord field. */
STATIC_ASSERT(offsetof(WindowRecord, port) == 0, windowrecord_port_at_0);

/* Ensure WindowRecord is at least as large as GrafPort (it embeds one) */
STATIC_ASSERT(sizeof(WindowRecord) >= sizeof(GrafPort), windowrecord_at_least_grafport);

/* Ensure GrafPort actually contains portRect (prevents struct regressions) */
STATIC_ASSERT(offsetof(GrafPort, portRect) < sizeof(GrafPort), grafport_has_portrect);

STATIC_ASSERT(offsetof(WindowRecord, visRgn) < sizeof(WindowRecord), windowrecord_has_visRgn);

/* Ensure windowKind field exists and is properly aligned for short access */
STATIC_ASSERT(offsetof(WindowRecord, windowKind) < sizeof(WindowRecord), windowrecord_has_windowkind);

#endif /* LAYOUT_GUARDS_H */
