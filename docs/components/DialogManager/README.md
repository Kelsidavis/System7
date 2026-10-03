# Dialog Manager

## Overview
Provides modal and modeless dialog services, resource loading, draw/update cycles, event routing, and keyboard navigation that mirror classic System 7 behaviour. Dialog Manager glues together Control Manager controls, QuickDraw ports, and the Event Manager loop.

## Source Layout
- `src/DialogManager/DialogManagerCore.c` – top-level APIs (`InitDialogs`, `NewDialog`, `ModalDialog`, `IsDialogEvent`, `DialogSelect`)
- `src/DialogManager/DialogResourceParser.c` / `DialogResources.c` – DITL/DLOG resource parsing and instantiation
- `src/DialogManager/DialogItems.c` – item table management, item hit dispatch, and `Get/SetDItem`
- `src/DialogManager/DialogDrawing.c` – item rendering and region invalidation
- `src/DialogManager/DialogEvents.c` – event loop integration and `DialogSelect`
- `src/DialogManager/DialogHelpers.c` / `dialog_manager_private.c` – internal helpers and private state
- `src/DialogManager/DialogKeyboard.c` – Return/Esc/Space/Tab handling and focus tracking (see [KeyboardIntegration](KeyboardIntegration.md))
- `src/DialogManager/ModalDialogs.c` / `AlertDialogs.c` – modal dialog and alert APIs
- `src/DialogManager/DITLBuilder.c` – helpers for constructing dialog item lists

## Responsibilities
- Construct dialog windows from DLOG/DITL resources and attach cloned controls
- Maintain the dialog item list, including text item state, control handles, and icon references
- Provide modal loop entry points (`ModalDialog`, `StandardAlert`) that block until an item is activated
- Integrate keyboard focus and control activation semantics (default, cancel, Tab order)
- Coordinate dialog invalidation/redraw by deferring to Control Manager for control items and TextEdit for editable fields

## Integration Points
- **Control Manager** supplies the concrete control handles for button/checkbox/radio items; Dialog Manager stores them in the item list and reuses `Draw1Control`, `HiliteControl`, etc.
- **Event Manager** feeds `DialogSelect` from `WaitNextEvent`; modal loops use `GetNextEvent` fallbacks to stay responsive
- **Window Manager** provides window creation, focus, and update notifications; dialogs use window kinds specific to System 7
- **TextEdit** populates edit fields; Dialog Manager installs hooks so keyboard focus can hand off to TE when appropriate

## Testing & Debugging
- Run `make check` for the automated project checks, then use `make run` to exercise dialog interaction in QEMU
- Use `DialogManager` serial logs (`[DM]` / `[CTRL]`) for tracing; whitelist entries live in `System71StdLib.c`
- Alerts can be exercised through `StandardAlert` from an application or integration test

## Future Work
- Hook modal dialogs into StandardFile file selection once List Manager and File Manager APIs stabilise
- Expand support for user item procs and custom item types
- Add automated tab-order verification powered by scripted key sequences
