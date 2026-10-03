# Control Manager

## Overview
Coordinate creation, drawing, tracking, and activation of classic System 7 push buttons, checkboxes, radio buttons, and scrollbars. The manager owns the control list attached to each window and bridges Dialog Manager, Window Manager, and QuickDraw.

## Source Layout
- `src/ControlManager/ControlManagerCore.c` – entry points (`_InitControlManager`, `_CleanupControlManager`, `NewControl`, `DisposeControl`, `Draw1Control`, `FindControl`, `TrackControl`, `HiliteControl`)
- `src/ControlManager/ControlTracking.c` – control hit testing and tracking state
- `src/ControlManager/StandardControls.c` – push buttons, checkboxes, radio buttons and variant flag handling
- `src/ControlManager/ScrollbarControls.c` – vertical/horizontal scrollbar CDEFs
- `src/ControlManager/ControlResources.c` – parsing of CNTL resources

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
- Run `make check` for the automated project checks; use `make run` to exercise controls in QEMU
- Serial logging is guarded with `[CTRL]` prefixes and whitelisted in `System71StdLib.c`
- Run `make check-exports` after modifying exported Toolbox traps to keep `docs/symbols_allowlist.txt` in sync

## Future Work
- Expand CDEF coverage (progress bars, disclosure triangles) as Resource Manager support matures
- Add automated pixel-diff comparisons for control redraws once CI screenshots are available
