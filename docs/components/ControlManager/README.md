# Control Manager

## Overview
Coordinate creation, drawing, tracking, and activation of classic System 7 controls (push buttons, checkboxes, radios, scrollbars, popup controls). The manager owns the control list attached to each window and bridges Dialog Manager, Window Manager, and QuickDraw.

## Source Layout
- `src/ControlManager/ControlManagerCore.c` – entry points (`InitControls`, `NewControl`, `DisposeControl`, `Draw1Control`, `FindControl`, `TrackControl`, `HiliteControl`)
- `ControlDrawing.c` / `ControlTracking.c` – shared drawing primitives and tracking state
- `StandardControls.c` – push buttons, checkboxes, radio buttons and variant flag handling
- `ScrollbarControls.c` – vertical/horizontal scrollbar CDEF with proportional thumbs, repeat timers
- `PopupControls.c`, `TextControls.c`, `PlatformControls.c` – specialised control definitions and host platform glue
- `ControlResources.c` – parsing of CNTL resources for future external definitions
- `ControlSmoke.c` – test harness compiled with `CTRL_SMOKE_TEST=1`

## Responsibilities
- Maintain per-window linked list of controls (`contrlNext`) and manage lifetime via `DisposeControl`
- Handle control visibility, highlighting, and activation states (`contrlVis`, `contrlHilite`)
- Dispatch hit testing and mouse tracking to the correct CDEF implementation
- Expose helper predicates (`IsButtonControl`, `IsCheckboxControl`, `IsRadioControl`, etc.) used by Dialog Manager and tests
- Bridge dialog keyboard handlers with `IsDefaultButton` / `IsCancelButton` variant decoding

## Integration Points
- **Dialog Manager** uses `Draw1Control`, `HiliteControl`, `FindControl`, and helper predicates while processing keyboard shortcuts
- **Window Manager** calls `DrawControls` during update regions and defers to the control list when routing events
- **QuickDraw** is assumed to be the active port when drawing; all routines save/restore pen, clip, and pattern state
- **Menu Manager / StandardFile** rely on scrollbars and list controls that originate here

## Testing & Debugging
- Build smoke harness: `make CTRL_SMOKE_TEST=1 run` to exercise default/cancel buttons, checkbox/radio toggles, and focus traversal (keyboard + mouse)
- Serial logging is guarded with `[CTRL]` prefixes and whitelisted in `System71StdLib.c`
- Run `make check-exports` after modifying exported Toolbox traps to keep `docs/symbols_allowlist.txt` in sync

## Future Work
- Replace hard-coded Chicago metrics with `GetFontInfo()` once additional Font Manager strikes land
- Expand CDEF coverage (progress bars, disclosure triangles) as Resource Manager support matures
- Add automated pixel-diff comparisons for control redraws once CI screenshots are available
