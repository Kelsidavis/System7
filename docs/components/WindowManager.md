# Window Manager

## Overview
Provides classic System 7 window services: creation, drawing, drag/resize interactions, update propagation, activation layering, and region management. Responsible for maintaining the global window list and ensuring redraws respect QuickDraw conventions.

## Source Layout
- `src/WindowManager/WindowManagerCore.c` – primary APIs (`InitWindows`, `NewWindow`, `DisposeWindow`, `ShowWindow`, `HideWindow`, `MoveWindow`, `SelectWindow`, `FrontWindow`)
- `src/WindowManager/WindowDisplay.c` – chrome drawing (title bars, grow box, drag outline, close box), update handling, window frame rendering
- `src/WindowManager/WindowLayering.c` – z-order management, `BringToFront`, `SendBehind`, active window tracking
- `src/WindowManager/WindowEvents.c` – hit testing, grow/drag event dispatch, tracking rectangles
- `src/WindowManager/WindowDragging.c` – XOR outline drawing, drag limits, ghost window implementation
- `src/WindowManager/WindowResizing.c` – grow region, size constraints, zoom, and update propagation after resize
- `src/WindowManager/WindowManagerHelpers.c` – utility routines for region math, title measurement, cross-module helpers
- `src/WindowManager/WindowParts.c` – active standard and dialog window definition procedures, drawing, and hit testing

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
- Implement window collapse and support custom window definition procedures beyond the standard document and dialog definitions
- Integrate Scroll Manager once ready to drive scroll bar invalidation regions automatically
