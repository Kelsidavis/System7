# System 7.1 API Compatibility Gaps

This checklist records significant differences between the current toolbox reimplementation and the classic System 7.1 APIs. Unstruck entries describe open gaps; struck-through entries are retained as a record of work already fixed or verified. File paths and line numbers in historical entries may have moved.

## QuickDraw & Graphics Pipeline
- ~~`src/QuickDraw/Bitmaps.c` – `CopyBits` still needs full mask handling, colour depth conversion, and transfer-mode coverage to match the System 7 trap.~~ **FIXED** (2025-10-07): CopyBits now clips and aligns rectangles, supports scaling, region masking, and pattern modes, and performs depth conversion for 1/8/16/32-bit sources and destinations.
- `include/QuickDraw/QuickDraw.h` declares `QD_DrawChar`, but no implementation is present in `src/`. The 68K `Trap_DrawChar` currently routes through `DoText`; add and test the native QuickDraw entry point before claiming full trap coverage.
- ~~`src/QuickDraw/QuickDrawCore.c` – patterned fills (`FillRect`, `FillOval`, `FillRgn`, etc.) did not rasterize geometry.~~ **VERIFIED** (2025-10-06): `QuickDrawCore.c` implements patterned fills through `DrawPrimitive()`; there is no separate `PatternManager.c` in the current tree.
- `src/QuickDraw/quickdraw_pictures.c` (`DrawPicture`) – `DrawPicture` still frames the destination rect instead of executing PICT opcodes; full QuickDraw opcode parsing, scaling, and region copying remain to be implemented.
- ~~`src/QuickDraw/quickdraw_pictures.c` – `SetClip`/`GetClip` do not copy regions, breaking callers that expect independent clip regions.~~ **VERIFIED** (2025-10-06): SetClip/GetClip in QuickDrawCore.c properly use CopyRgn() for independent region copies
- ~~`src/QuickDraw/CursorManager.c` – Cursor show/hide/obscure/spin still defer to TODOs; Mac OS required hardware cursor toggles and watch-cursor animation tied to `SpinCursor`.~~ **OBSOLETE** (2026-10-01): `CursorManager_SetCursorInternal()` / `cursor_set_bit()` / `CursorManager_ShouldBeVisible()` implement the state machine now; the old TODOs are gone from the file.

## Window, Dialog, and Control Managers
- ~~`src/WindowManager/WindowEvents.c` – Grow and drag tracking branches in `WM_TrackWindowPart` returned immediately; Window Manager must honour `inDrag`/`inGrow` parts with live XOR outlines and constraint callbacks like the classic implementation.~~ **FIXED** (2025-10-06): WM_TrackWindowPart now calls DragWindow() and GrowWindow() which provide full XOR feedback and mouse tracking
- ~~`src/DialogManager/DialogDrawing.c` – Edit-text items ignore focus rings; System 7 drew a focus frame and moved the caret when the control is active.~~ **FIXED** (2025-10-06): Edit-text focus rings and caret blinking implemented in DialogEditText.c
- ~~`src/DialogManager/dialog_manager_private.c` – `GetNextUserCancelEvent` is a stub; modal dialogs should scan the event queue for cancel gestures (Command-.) as the Classic API allowed.~~ **FIXED** (2025-10-06): IsUserCancelEvent/GetNextUserCancelEvent implemented, modal dialogs support Cmd-. and Escape
- ~~`src/ControlManager/StandardControls.c` – Control metrics are hard-coded to Chicago 12; real `GetFontInfo` must come from the Font Manager so controls respect the active font.~~ **FIXED** (2026-10-01): `GetFontInfo()` queries Font Manager's `GetFontMetrics()` and only falls back to proportional scaling when the strike is missing.
- Mixed-state checkbox paths remain unvalidated; native System 7 controls supported tri-state checkboxes.

## Event & Input Handling
- ~~`src/EventManager/event_manager.c` – Posted events always reported `modifiers = 0`; modifier bits need to be sampled so Command shortcuts and shift-clicking behave correctly.~~ **FIXED** (2025-10-06): `PostEvent` now calls `GetPS2Modifiers()` to populate modifier fields from hardware.
- ~~`src/EventManager/event_manager.c` – `WaitNextEvent` ignored the caller-supplied `mouseRgn`; classic Mac OS clipped null events and mouse-moved events to that region.~~ **FIXED** (2025-10-06): `WaitNextEvent` now monitors `mouseRgn` and generates null events when the mouse exits the region.
- ~~`src/EventManager/EventDispatcher.c` – Command-key menu shortcuts are unimplemented; menu command routing should call `MenuKey`/`MenuChoice` analogues when `cmdKey` is set.~~ **FIXED** (2025-10-06): Event Dispatcher now calls MenuKey() for all command-key events and routes through DoMenuCommand()
- ~~`src/EventManager/MouseEvents.c` – `StillDown` relied on placeholder control routines, making hit testing during tracking unreliable.~~ **VERIFIED** (2025-10-06): `StillDown` and `Button` are implemented in `MouseEvents.c`.
- ~~`src/EventManager/SystemEvents.c` – Update regions are never merged or reduced after validation, causing duplicate `updateEvt`s; the classic manager subtracts validated areas from pending invalidations.~~ **FIXED** (2025-10-06): RequestWindowUpdate now merges update regions using UnionRgn; ValidateWindowRegion subtracts validated areas using DiffRgn

## Text Input & Editing
- `src/TextEdit/TextEditScroll.c` – Horizontal and vertical scroll limits are shared by `TEScroll` and `TEPinScroll`; integration coverage checks both against a long, unwrapped line and multiple hard-returned lines. Mixed-font widths and scroll-bar integration remain unvalidated.
- `src/TextEdit/TextEditClipboard.c` – TEXT and style scrap are copied to and from the Scrap Manager, but `TEStylePaste` currently parses/logs style runs without applying them to the pasted range.

## Memory & Process Infrastructure
- ~~`src/MemoryMgr/MemoryManager.c` – `SetHandleSize` faked success without reallocating; handle-based memory semantics must be honoured for legacy callers.~~ **FIXED** (2025-10-06): `SetHandleSize` now reallocates handles with data copying, respects locked handles, and maintains master pointer integrity.
- ~~`src/System71StdLib.c` – `sprintf`/`snprintf` are placeholder implementations; Toolbox routines expecting formatted output (e.g., `NumToString`) will misbehave.~~ **FIXED** (2025-10-06): Implemented vsnprintf() with format specifiers (%s, %d, %u, %x, %c, %p); sprintf() and snprintf() now fully functional
- `src/ProcessMgr/ProcessManager.c` – Process Manager maintains a cooperative scheduler and process table, but end-to-end multi-process scheduling/context switching remains experimental.

## Fonts & Typography
- `docs/components/FontManager/README.md` & `src/FontManager/FontManagerCore.c` – Only the Chicago 12 strike ships in-tree; Geneva/Monaco fall back to Chicago unless matching strikes are available as resources.
- `src/FontManager/FontResourceLoader.c` – NFNT/FOND parsing and strike construction are implemented, and `FontManagerCore.c` looks up FOND/NFNT through `GetResource`. Validate loading against real resource forks and non-Chicago strikes before claiming broad font coverage.

## Peripheral Toolbox Managers
- ~~`src/ListManager/ListManager.c` – Column APIs (`LAddColumn`, `LDelColumn`) return stub responses; System 7 supported dynamic column manipulation.~~ **FIXED** (2026-10-01): `LAddColumn`/`LDelColumn` insert and remove columns and resize the cell matrix.
- ~~`src/SoundManager/SoundManagerBareMetal.c` – Core Sound Manager channels and playback APIs return `unimpErr`; only `SysBeep` exists, whereas System 7 provided channel-based audio playback.~~ **FIXED** (2026-10-01): the file was rewritten with channel-based routing, a shared `SndMidiNoteToFreq()` lookup table, and `SndPlaySoundHeader()`; `unimpErr` is only a fallback label now.
- ~~`src/PatternMgr/pattern_manager.c` – Desktop pattern installation was unimplemented, leaving the Finder without classic patterned backgrounds.~~ **FIXED** (2026-10-01): the pattern manager in `src/PatternMgr/` serves 17 colour `ppat` and 32 black-and-white `PAT` resources defined by `patterns.json` and embedded from the generated `Patterns.rsrc`; these back the Set Desktop Pattern control panel.
- `src/QuickDraw/quickdraw_pictures.c` – Region allocation/free still rely on `NewHandle` without proper zone management; classic QuickDraw used Region Manager semantics.

## Next Steps
Prioritize the unstruck entries above for implementation or validation; the struck-through entries are historical and are not remaining work. Closing the open gaps will improve compatibility with classic System 7 applications that rely on the documented Toolbox contract.
