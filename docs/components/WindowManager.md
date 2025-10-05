# Window Manager

## Overview
Provides classic System 7 window services: creation, drawing, drag/resize interactions, update propagation, activation layering, and region management. Responsible for maintaining the global window list and ensuring redraws respect QuickDraw conventions.

## Source Layout
- `src/WindowManager/WindowManagerCore.c` – primary APIs (`InitWindows`, `NewWindow`, `DisposeWindow`, `ShowWindow`, `HideWindow`, `MoveWindow`, `SelectWindow`, `FrontWindow`)
- `WindowDisplay.c` – chrome drawing (title bars, grow box, drag outline, close box), update handling, window frame rendering
- `WindowLayering.c` – z-order management, `BringToFront`, `SendBehind`, active window tracking
- `WindowEvents.c` – hit testing, grow/drag event dispatch, tracking rectangles
- `WindowDragging.c` – XOR outline drawing, drag limits, ghost window implementation
- `WindowResizing.c` – grow region, size constraints, update propagation after resize
- `WindowManagerHelpers.c` – utility routines for region math, title measurement, cross-module helpers
- `WindowParts.c` – legacy/stubbed window definition parts retained for future reference

## Responsibilities
- Maintain linked list of windows and associated QuickDraw ports (including the desktop window/`qd.wMgrPort`)
- Draw window frames using QuickDraw primitives, integrating Font Manager for title rendering and Control Manager for embedded controls
- Implement drag and resize loops with XOR outlines and live updates while preserving obscured region integrity
- Provide activation semantics (active/inactive title shading), close box handling, and go-away tracking
- Coordinate invalidation/updates, calling each window's update proc and iterating control lists as needed

## Integration Points
- **Event Manager** routes mouse and keyboard events; Window Manager hit testing determines whether events go to controls, content, or chrome
- **Font Manager** supplies title glyph rendering via `DrawString`
- **Control Manager** draws and tracks attached controls during window updates
- **Menu Manager** uses `FrontWindow` and activation changes to keep menu state coherent
- **Finder / Apps** rely on `WindowRecord` layout captured in `docs/layouts/layouts.json`

## Testing & Debugging
- Finder desktop and SimpleText windows exercise drag, grow, and update flows under `make run`
- Serial logging tagged `[WM]` can be enabled for drag/resize diagnostics via `System71StdLib.c`
- Use the window smoke scripts in `tests/wm/` (`update_pipeline.md`, `wdef_hit_matrix.md`) to reproduce historical bugs
- Visual verification: ensure XOR drag outline erases cleanly, active title shading toggles when switching windows, and updates occur after resize

## Future Work
- Implement zoom box semantics and window collapse once Resource Manager assets are available
- Support custom window definition procs beyond the standard document window
- Integrate Scroll Manager once ready to drive scroll bar invalidation regions automatically
