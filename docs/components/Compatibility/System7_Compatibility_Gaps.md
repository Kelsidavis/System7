# System 7.1 API Compatibility Gaps

This checklist records significant differences between the current toolbox reimplementation and the classic System 7.1 APIs. Unstruck entries describe open gaps; struck-through entries are retained as a record of work already fixed or verified. File paths and line numbers in historical entries may have moved.

## QuickDraw & Graphics Pipeline
- ~~`src/QuickDraw/Bitmaps.c` – `CopyBits` still needs full mask handling, colour depth conversion, and transfer-mode coverage to match the System 7 trap.~~ **FIXED** (2025-10-07): CopyBits now clips and aligns rectangles, supports scaling, region masking, and pattern modes, and performs depth conversion for 1/8/16/32-bit sources and destinations.
- ~~`include/QuickDraw/QuickDraw.h` declares `QD_DrawChar`, but no implementation is present in `src/`.~~ **FIXED** (2026-10-04): `QD_DrawChar()` now delegates to the Font Manager's character renderer; the guest integration test verifies that it advances the current QuickDraw pen by the active font's character width. The 68K trap retains its independent guest-memory rendering path.
- ~~`src/QuickDraw/Regions.c` – `FrameRgn()` is a no-op.~~ **FIXED** (2026-10-04): `FrameRgn()` traces only exposed edges of the region's rectangle decomposition through QuickDraw's current-pen line path, avoiding internal seams. Guest coverage frames both the outer contour and a hole, checks an internal seam and unpainted interior, and verifies that the current pen location is preserved.
- ~~`src/QuickDraw/Regions.c` – `NewRgn()` used pointer allocations even though region operations lock the outer object as a handle.~~ **FIXED** (2026-10-04): regions now use Memory Manager handles and grow through `SetHandleSize`; validation checks the rectangle-list size, count, bounds, and overlap, with integration coverage for handle recovery, copying into a grown destination, and rejecting malformed lists.
- ~~`src/QuickDraw/QuickDrawCore.c` – patterned fills (`FillRect`, `FillOval`, `FillRgn`, etc.) did not rasterize geometry.~~ **VERIFIED** (2025-10-06): QuickDraw rasterizes patterned fills through `DrawPrimitive()` in `QuickDrawCore.c`; desktop wallpaper pattern selection is handled separately in `src/PatternMgr/`.
- `src/QuickDraw/quickdraw_pictures.c` (`DrawPicture`) – the player handles core v1/v2
  PICT state, shape, text, clipping, and bitmap opcodes, including scaling and region
  masks. Reserved or unsupported opcodes are skipped where their lengths are known;
  broad opcode coverage and malformed-picture validation remain incomplete.
- ~~`src/QuickDraw/quickdraw_pictures.c` – `SetClip`/`GetClip` do not copy regions, breaking callers that expect independent clip regions.~~ **VERIFIED** (2025-10-06): SetClip/GetClip in QuickDrawCore.c properly use CopyRgn() for independent region copies
- `src/QuickDraw/CursorManager.c` implements software cursor visibility, obscuring until mouse movement, and watch-frame cycling through `SpinCursor()`. `IntegrationTests.c` covers nested hide/show and movement-based reveal; screen-level background restoration and watch-frame stepping remain unverified.

## Window, Dialog, Control, and Menu Managers
- ~~`NewDialog` created its window with `userKind`, and `IsDialogEvent` excluded null and activate events.~~ **FIXED** (2026-10-04): dialogs now use `dialogKind`; `IsDialogEvent` recognizes any event for a front dialog and update/activate events targeted at a dialog. `DialogSelect` handles targeted activation events and updates the focused edit caret.
- ~~`DialogSelect` edited enabled edit-text items but did not return their item number for key or mouse events.~~ **FIXED** (2026-10-04): it now reports the enabled item in `itemHit` while keeping disabled edit fields interactive without reporting them.
- ~~`DialogCut`, `DialogCopy`, `DialogPaste`, and `DialogDelete` route through
  the focused dialog edit-text field.~~ **IMPLEMENTED** (2026-10-04): all four
  operations use the focused field; cut, paste, and delete synchronize the
  updated text back to the dialog item. Integration regression coverage checks
  clipboard transfer and selection deletion.
- ~~Dialog headers advertised non-classic helper APIs without definitions, including platform-native dialog controls, per-dialog event filters, and accessibility text accessors.~~ **FIXED** (2026-10-04): removed declarations for unsupported helpers from `DialogEvents.h`, `DialogItems.h`, `DialogManager.h`, `ModalDialogs.h`, and `ControlManager.h`; retained the implemented classic event, item, and modal APIs.
- ~~Dialog edit-text and control focus state is owned per live dialog.~~
  **IMPLEMENTED** (2026-10-04): state is released on disposal; integration
  regression coverage checks independent state across more than sixteen
  simultaneous dialogs.
- ~~`AdvanceDialogFocus()` returned 0 without moving keyboard focus.~~ **FIXED**
  (2026-10-04): It now uses the edit-text focus traversal for Tab and Shift-Tab,
  returning the new item number; the guest regression checks both directions.
- ~~Window Manager mouse-down handling returned immediately for `inDrag`/`inGrow` parts instead of starting window tracking.~~ **FIXED** (2025-10-06): The current routing is in `src/EventManager/EventDispatcher.c`: `inDrag` calls `DragWindow`, while `inGrow` calls `GrowWindow` and applies its returned dimensions with `SizeWindow`.
- ~~`src/DialogManager/DialogDrawing.c` – Edit-text items ignore focus rings; System 7 drew a focus frame and moved the caret when the control is active.~~ **FIXED** (2025-10-06): Edit-text focus rings and caret blinking implemented in DialogEditText.c
- ~~`src/DialogManager/dialog_manager_private.c` – `GetNextUserCancelEvent` is a stub; modal dialogs should scan the event queue for cancel gestures (Command-.) as the Classic API allowed.~~ **FIXED** (2025-10-06): IsUserCancelEvent/GetNextUserCancelEvent implemented, modal dialogs support Cmd-. and Escape
- ~~`src/ControlManager/StandardControls.c` – Push-button title baselines used hard-coded ascent/descent values instead of the selected system font metrics.~~ **FIXED**: push-button, checkbox, and radio labels now use `GetFontInfo()`, which queries Font Manager metrics and falls back to fixed defaults when unavailable.
- ~~The compatibility checklist claimed standard System 7 checkboxes supported a mixed state.~~ **CORRECTED** (2026-10-04): the classic Control Manager defines checkbox settings as off (`0`) and on (`1`); removed nonclassic state/group helpers and the empty public standard-controls header. Radio-button peer selection is left to the owning application, as in the original Control Manager.
- `PopUpMenuSelect()` uses the existing menu tracker, positions the previous item at the closed pop-up box, and leaves the menu bar untouched. Guest coverage verifies pointer selection and the packed menu ID/item result. Off-screen placement, scrolling, and custom MDEF behavior still need validation against System 7.
- `InsertIntlResMenu` is implemented in `src/MenuManager/MenuResourceNames.c` and filters named resources by script. `InitProcMenu` now initializes the current menu list and stores the requested `mbResID`, but the Menu Manager does not yet load or invoke custom `'MBDF'` resources for menu-bar drawing and tracking.

## Event & Input Handling
- ~~`src/EventManager/event_manager.c` – Posted events always reported `modifiers = 0`; modifier bits need to be sampled so Command shortcuts and shift-clicking behave correctly.~~ **FIXED** (2025-10-06): `PostEvent` now calls `GetPS2Modifiers()` to populate modifier fields from hardware.
- ~~`src/EventManager/event_manager.c` – `WaitNextEvent` ignored the caller-supplied `mouseRgn`; classic Mac OS clipped null events and mouse-moved events to that region.~~ **FIXED** (2025-10-06): `WaitNextEvent` now monitors `mouseRgn` and generates null events when the mouse exits the region.
- ~~`src/EventManager/EventDispatcher.c` – Command-key menu shortcuts are unimplemented; menu command routing should call `MenuKey`/`MenuChoice` analogues when `cmdKey` is set.~~ **FIXED** (2025-10-06): Event Dispatcher now calls MenuKey() for all command-key events and routes through DoMenuCommand()
- ~~`src/EventManager/MouseEvents.c` – `StillDown` relied on placeholder control routines, making hit testing during tracking unreliable.~~ **VERIFIED** (2025-10-06): `StillDown` and `Button` are implemented in `MouseEvents.c`.
- ~~`src/EventManager/SystemEvents.c` – Update regions are never merged or reduced after validation, causing duplicate `updateEvt`s; the classic manager subtracts validated areas from pending invalidations.~~ **FIXED** (2025-10-06): RequestWindowUpdate now merges update regions using UnionRgn; ValidateWindowRegion subtracts validated areas using DiffRgn

## Text Input & Editing
- `src/TextEdit/TextEditScroll.c` – Horizontal and vertical scroll limits are shared by `TEScroll` and `TEPinScroll`; integration coverage checks both against a long, unwrapped line and multiple hard-returned lines. Mixed-font widths and scroll-bar integration remain unvalidated.
- `src/TextEdit/TextEditClipboard.c` and `include/TextEdit/TextEdit.h` – `TEStylePaste` parses style runs without applying them, and the private `'styl'` serializer/parser does not use the classic `StScrpRec`/`ScrpSTElement` layout. `TEGetStyle`, `TESetStyle`, `TEUseStyleScrap`, and `TEStyleInsert` are declared but have no definitions. Styled TextEdit copy/paste and these public APIs need implementation against the classic style-table contract.

## Memory & Process Infrastructure
- ~~`src/MemoryMgr/MemoryManager.c` – `SetHandleSize` faked success without reallocating; handle-based memory semantics must be honoured for legacy callers.~~ **FIXED** (2025-10-06): `SetHandleSize` now reallocates handles with data copying, respects locked handles, and maintains master pointer integrity.
- ~~`src/System71StdLib.c` – `sprintf`/`snprintf` are placeholder implementations; Toolbox routines expecting formatted output (e.g., `NumToString`) will misbehave.~~ **FIXED** (2025-10-06): Implemented vsnprintf() with format specifiers (%s, %d, %u, %x, %c, %p); sprintf() and snprintf() now fully functional
- `src/ProcessMgr/ProcessManager.c` – Process Manager maintains a cooperative scheduler and process table, but end-to-end multi-process scheduling/context switching remains experimental.

## System Information
- ~~`Gestalt('fpu ')` reported a Boolean host-CPU FPU probe as a Motorola coprocessor type.~~ **FIXED** (2026-10-04): the selector now reports `gestaltNoFPU` while the 68K execution path lacks FPU emulation; classic type constants and guest-level regression coverage are defined.

## Fonts & Typography
- `docs/components/FontManager/README.md` & `src/FontManager/FontManagerCore.c` – Only the Chicago 12 strike ships in-tree; Geneva/Monaco fall back to Chicago unless matching strikes are available as resources.
- `src/FontManager/FontResourceLoader.c` – NFNT/FOND parsing and strike construction are implemented, and `FontManagerCore.c` looks up FOND/NFNT through `GetResource`. Validate loading against real resource forks and non-Chicago strikes before claiming broad font coverage.

## Peripheral Toolbox Managers
- ~~`src/ListManager/ListManager.c` – Column APIs (`LAddColumn`, `LDelColumn`) return stub responses; System 7 supported dynamic column manipulation.~~ **FIXED** (2026-10-01): `LAddColumn`/`LDelColumn` insert and remove columns and resize the cell matrix.
- ~~`src/SoundManager/SoundManagerBareMetal.c` – Core Sound Manager channels and playback APIs return `unimpErr`; only `SysBeep` exists, whereas System 7 provided channel-based audio playback.~~ **FIXED** (2026-10-01): channel-based playback is implemented with a shared `SndMidiNoteToFreq()` lookup table and `SndPlaySoundHeader()`.
- `SndStartFilePlay`, `SndPauseFilePlay`, and `SndStopFilePlay` are defined but still return `unimpErr`; file-based playback is not implemented.
- Declared Sound Input Manager calls are linkable, but this build has no recording backend: recording returns `notEnoughHardwareErr` and operations on absent input-device references return `siBadSoundInDevice`. `SetupSndHeader` creates format-1 uncompressed mono 8-bit, mono 16-bit, and stereo 8/16-bit headers; compressed headers and the AIFF setup/parse routines are not implemented, and functional recording remains unsupported.
- ~~`src/PatternMgr/pattern_manager.c` – Desktop pattern installation was unimplemented, leaving the Finder without classic patterned backgrounds.~~ **FIXED** (2026-10-01): the pattern manager in `src/PatternMgr/` serves 17 colour `ppat` and 32 black-and-white `PAT` resources defined by `patterns.json` and embedded from the generated `Patterns.rsrc`; these back the Set Desktop Pattern control panel.

## Next Steps
Prioritize the unstruck entries above for implementation or validation; the struck-through entries are historical and are not remaining work. Closing the open gaps will improve compatibility with classic System 7 applications that rely on the documented Toolbox contract.
