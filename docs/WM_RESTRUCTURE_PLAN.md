# Window Manager Restructuring Plan

## Current Status

The Window Manager has architectural issues inherited from incomplete prior implementation:

### Problem Files (24 remaining errors)
- `WindowResizing.c` - Function signature conflicts with WindowManagerHelpers.c
- `WindowDragging.c` - Not yet audited
- `WindowLayering.c` - Not yet audited
- `WindowParts.c` - Not yet audited

### Root Cause

**[WM-017] Header/Implementation Layering Violation**

**Claim**: Window Manager helper functions are duplicated across multiple .c files with conflicting signatures.

**Evidence**:
- IM:Windows Vol I does not specify cross-file helper function sharing
- System 7 ROM shows each WM subsystem (resize, drag, layer) as self-contained
- MPW sample code shows file-local static helpers, not cross-file dependencies

**Problem**:
- `WindowManagerHelpers.c` declares WM_CreateWindowStateData() returning void*
- `WindowResizing.c` redeclares same function returning WindowStateData*
- This violates C linkage rules and causes 24 compilation errors

**Correct Architecture** (per IM:Windows + ROM analysis):
1. Public API functions (GrowWindow, DragWindow, etc.) in their own files
2. File-local helpers are `static` and not in headers
3. Shared utilities (rect math, validation) in Helpers.c with proper types
4. No forward declarations in headers for static functions

**Resolution Strategy**:
1. Remove all stub implementations from WindowManagerHelpers.c that conflict
2. Make WindowResizing.c functions properly static (no header declarations)
3. Define WindowStateData and other internal structs in WindowManagerInternal.h
4. Repeat for WindowDragging.c, WindowLayering.c, WindowParts.c

## Immediate Action (Per User Directive)

User explicitly stated: **"No 'pre-existing issues' excuses. Finish the WM faithfully to System 7."**

Therefore, I must:
1. ✓ Fix all include order violations (done)
2. ✓ Fix all struct access violations (window->port.portRect) (done)
3. ⚠️ Resolve all 24 function signature conflicts (in progress)
4. Add proper archaeological provenance to every fix
5. Ensure behavioral parity with System 7

## Next Steps

1. Define all internal structures properly in WindowManagerInternal.h
2. Remove all conflicting stubs from WindowManagerHelpers.c
3. Make file-local functions properly static
4. Add [WM-###] provenance tags to all changes
5. Build test harness to verify behavior

## Evidence Requirements

Every change must cite:
- Inside Macintosh chapter and page
- MPW header file and line
- ROM offset (where applicable)
- Observed System 7 behavior

No guessing. No "seems reasonable". Only documented facts.