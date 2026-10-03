# System 7.1 API Compatibility Gaps

This checklist captures the most significant differences between the current toolbox reimplementation and the classic System 7.1 APIs. Each bullet calls out the incomplete behaviour, the file/line where the gap is documented in code, and the work expected to regain parity.

## QuickDraw & Graphics Pipeline
- ~~`src/QuickDraw/Bitmaps.c:134` – `CopyBits` still needs full mask handling, colour depth conversion, and transfer-mode coverage to match the System 7 trap.~~ **FIXED** (2025-10-07): CopyBits now clips and aligns rectangles, supports scaling, region masking, and pattern modes, and performs depth conversion for 1/8/16/32-bit sources and destinations.
- ~~`src/QuickDraw/Text.c:84` – `DrawChar` was a placeholder that only advanced the pen without rendering glyphs; `DrawString` and `DrawText` depend on it for actual text rendering.~~ **FIXED** (2025-10-06): DrawChar now extracts glyph bitmaps from Chicago font strike and renders them via QDPlatform_DrawGlyphBitmap()
- ~~`src/QuickDraw/PatternManager.c:177`–`257` – All patterned fill variants (`FillRect`, `FillOval`, `FillRgn`, etc.) are stubs that set patterns but never rasterize geometry.~~ **VERIFIED** (2025-10-06): Patterned fills are fully implemented in QuickDrawCore.c via DrawPrimitive(); PatternManager.c is not in build
- `src/QuickDraw/quickdraw_pictures.c` (`DrawPicture`, ~line 261) – `DrawPicture` still frames the destination rect instead of executing PICT opcodes; full QuickDraw opcode parsing, scaling, and region copying remain to be implemented. (Line numbers here have drifted since October 2025 — grep for the routine, don't trust the offset.)
- ~~`src/QuickDraw/quickdraw_pictures.c:94`–`115` – `SetClip`/`GetClip` do not copy regions, breaking callers that expect independent clip regions.~~ **VERIFIED** (2025-10-06): SetClip/GetClip in QuickDrawCore.c properly use CopyRgn() for independent region copies
- ~~`src/QuickDraw/CursorManager.c:151`–`177` – Cursor show/hide/obscure/spin still defer to TODOs; Mac OS required hardware cursor toggles and watch-cursor animation tied to `SpinCursor`.~~ **OBSOLETE** (2026-10-01): `CursorManager_SetCursorInternal()` / `cursor_set_bit()` / `CursorManager_ShouldBeVisible()` implement the state machine now; the old TODOs are gone from the file.

## Window, Dialog, and Control Managers
- ~~`src/WindowManager/WindowEvents.c:738`–`742` – Grow and drag tracking branches in `WM_TrackWindowPart` returned immediately; Window Manager must honour `inDrag`/`inGrow` parts with live XOR outlines and constraint callbacks like the classic implementation.~~ **FIXED** (2025-10-06): WM_TrackWindowPart now calls DragWindow() and GrowWindow() which provide full XOR feedback and mouse tracking
- ~~`src/DialogManager/DialogDrawing.c:428` – Edit-text items ignore focus rings; System 7 drew a focus frame and moved the caret when the control is active.~~ **FIXED** (2025-10-06): Edit-text focus rings and caret blinking implemented in DialogEditText.c
- ~~`src/DialogManager/dialog_manager_private.c:104` – `GetNextUserCancelEvent` is a stub; modal dialogs should scan the event queue for cancel gestures (Command-.) as the Classic API allowed.~~ **FIXED** (2025-10-06): IsUserCancelEvent/GetNextUserCancelEvent implemented, modal dialogs support Cmd-. and Escape
- ~~`src/ControlManager/StandardControls.c:84` – Control metrics are hard-coded to Chicago 12; real `GetFontInfo` must come from the Font Manager so controls respect the active font.~~ **FIXED** (2026-10-01): `GetFontInfo()` (line ~87) queries Font Manager's `GetFontMetrics()` and only falls back to proportional scaling when the strike is missing.
- Mixed-state checkbox paths remain unvalidated; native System 7 controls supported tri-state checkboxes.

## Event & Input Handling
- ~~`src/EventManager/event_manager.c:251` – Posted events always report `modifiers = 0`; modifier bits (shift, option, command) need to be sampled from the PS/2 layer so Command shortcuts and shift-clicking behave correctly.~~ **FIXED** (2025-10-06): PostEvent now calls GetPS2Modifiers() to populate modifier fields from hardware
- ~~`src/EventManager/event_manager.c:285` – `WaitNextEvent` ignores the caller-supplied `mouseRgn`; classic Mac OS clipped null events and mouse moved events to that region.~~ **FIXED** (2025-10-06): WaitNextEvent now monitors mouseRgn and generates null events when mouse exits region
- ~~`src/EventManager/EventDispatcher.c:413`–`416` – Command-key menu shortcuts are unimplemented; menu command routing should call `MenuKey`/`MenuChoice` analogues when `cmdKey` is set.~~ **FIXED** (2025-10-06): Event Dispatcher now calls MenuKey() for all command-key events and routes through DoMenuCommand()
- ~~`src/EventManager/MouseEvents.c:454` & `src/EventManager/EventManagerCore.c:655` – `StillDown` references stubs in `control_stubs.c`; until the real implementation arrives, hit testing during tracking is unreliable.~~ **VERIFIED** (2025-10-06): StillDown and Button are properly implemented in MouseEvents.c
- ~~`src/EventManager/SystemEvents.c:331` & `:390` – Update regions are never merged or reduced after validation, causing duplicate `updateEvt`s; the classic manager subtracts validated areas from pending invalidations.~~ **FIXED** (2025-10-06): RequestWindowUpdate now merges update regions using UnionRgn; ValidateWindowRegion subtracts validated areas using DiffRgn

## Text Input & Editing
- `src/TextEdit/TextEditScroll.c:91` & `:180` – Horizontal scroll limits are uncomputed, so TE windows cannot properly constrain scroll bars.
- `src/TextEdit/TextEditClipboard.c:164`–`267` – Styled scrap handling is stubbed; classic TE mirrored styled text into the clipboard flavours.

## Resource & File Systems

## Memory & Process Infrastructure
- ~~`src/MemoryMgr/memory_manager_core.c:446`–`458` & `src/MemoryMgr/MemoryManager.c:399`–`465` – `SetHandleSize` fakes success without reallocating; handle-based memory semantics (moveable/relocatable blocks, zone compaction) must be honoured for legacy callers.~~ **FIXED** (2025-10-06): SetHandleSize now properly reallocates handles with data copying, respects locked handles, and maintains master pointer integrity
- ~~`src/System71StdLib.c:576`–`583` – `sprintf`/`snprintf` are placeholder implementations; Toolbox routines expecting formatted output (e.g., `NumToString`) will misbehave.~~ **FIXED** (2025-10-06): Implemented vsnprintf() with format specifiers (%s, %d, %u, %x, %c, %p); sprintf() and snprintf() now fully functional
- `src/ProcessMgr/ProcessManager.c` (~line 386) – `WaitNextEvent` still comes from `sys71_stubs.c`; multi-process scheduling remains experimental.

## Fonts & Typography
- `docs/components/FontManager/README.md` & `src/FontManager/FontManagerCore.c` – Only the Chicago 12 strike is available; Geneva/Monaco map to Chicago metrics, and true resource-driven strike loading is pending, unlike System 7’s font ecosystem.
- `src/FontManager/FontResourceLoader.c` – `NewPtr`/`DisposePtr`/`GetHandleSize`/`HLock`/`HUnlock` are declared as externs and remain stubs, blocking runtime NFNT/FOND ingestion.

## Peripheral Toolbox Managers
- ~~`src/ListManager/ListManager.c:428`–`438` – Column APIs (`LAddColumn`, `LDelColumn`) return stub responses; System 7 supported dynamic column manipulation.~~ **FIXED** (2026-10-01): `LAddColumn`/`LDelColumn` (line ~433 onward) insert and remove columns and resize the cell matrix.
- ~~`src/SoundManager/SoundManagerBareMetal.c:150`–`205` – Core Sound Manager channels and playback APIs return `unimpErr`; only `SysBeep` exists, whereas System 7 provided channel-based audio playback.~~ **FIXED** (2026-10-01): the file was rewritten with channel-based routing, a shared `SndMidiNoteToFreq()` lookup table, and `SndPlaySoundHeader()`; `unimpErr` is only a fallback label now.
- ~~`src/QuickDraw/PatternManager.c:286` – Desktop pattern installation is unimplemented, leaving the Finder without classic patterned backgrounds.~~ **MOVED** (2026-10-01): the pattern manager lives in `src/PatternMgr/` (`pattern_manager.c`, `pattern_resources.c`); the 34 `ppat` patterns in `Patterns.rsrc` back the Set Desktop Pattern control panel.
- `src/QuickDraw/quickdraw_pictures.c` – Region allocation/free still rely on `NewHandle` without proper zone management; classic QuickDraw used Region Manager semantics.

## Next Steps
The items above should be prioritised for implementation or alignment work. Restoring these behaviours will unblock compatibility with classic System 7 applications that rely on the documented Toolbox contract.
