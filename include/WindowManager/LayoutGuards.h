/*
 * LayoutGuards.h - Native Window Manager layout invariants
 *
 * Checks the embedded GrafPort and required members of native records.
 * These are not byte-layout checks for the emulated 68k Toolbox records.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 * Derived from System 7 ROM analysis (Ghidra) Window Manager
 */

#ifndef LAYOUT_GUARDS_H
#define LAYOUT_GUARDS_H

#include <stddef.h>
#include "QuickDraw/QuickDraw.h"
#include "SystemTypes.h"
#include "StaticAssert.h"

/* IM:Windows p.2-13 specifies GrafPort as the first WindowRecord field. */
SYSTEM7_STATIC_ASSERT(offsetof(WindowRecord, port) == 0, windowrecord_port_at_0);

/* Ensure WindowRecord is at least as large as GrafPort (it embeds one) */
SYSTEM7_STATIC_ASSERT(sizeof(WindowRecord) >= sizeof(GrafPort), windowrecord_at_least_grafport);

/* Ensure GrafPort actually contains portRect (prevents struct regressions) */
SYSTEM7_STATIC_ASSERT(offsetof(GrafPort, portRect) < sizeof(GrafPort), grafport_has_portrect);

SYSTEM7_STATIC_ASSERT(offsetof(WindowRecord, visRgn) < sizeof(WindowRecord), windowrecord_has_visRgn);

/* Ensure windowKind remains a member of the record. */
SYSTEM7_STATIC_ASSERT(offsetof(WindowRecord, windowKind) < sizeof(WindowRecord), windowrecord_has_windowkind);

#endif /* LAYOUT_GUARDS_H */
