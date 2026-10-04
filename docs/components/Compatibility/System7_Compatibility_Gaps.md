# System 7.1 API Compatibility Gaps

This checklist records significant differences between the current toolbox reimplementation and the classic System 7.1 APIs. Unstruck entries describe open gaps; struck-through entries are retained as a record of work already fixed or verified. File paths and line numbers in historical entries may have moved.

## QuickDraw & Graphics Pipeline
- ~~`src/QuickDraw/Bitmaps.c` – `CopyBits` still needs full mask handling, colour depth conversion, and transfer-mode coverage to match the System 7 trap.~~ **FIXED** (2025-10-07): CopyBits now clips and aligns rectangles, supports scaling, region masking, and pattern modes, and performs depth conversion for 1/8/16/32-bit sources and destinations.
- ~~`include/QuickDraw/QuickDraw.h` declares `QD_DrawChar`, but no implementation is present in `src/`.~~ **FIXED** (2026-10-04): `QD_DrawChar()` now delegates to the Font Manager's character renderer; the guest integration test verifies that it advances the current QuickDraw pen by the active font's character width. The 68K trap retains its independent guest-memory rendering path.
- `src/QuickDraw/Regions.c` – `FrameRgn()` is a no-op; implement region outline drawing and add coverage for complex region boundaries.
- ~~`src/QuickDraw/QuickDrawCore.c` – patterned fills (`FillRect`, `FillOval`, `FillRgn`, etc.) did not rasterize geometry.~~ **VERIFIED** (2025-10-06): QuickDraw rasterizes patterned fills through `DrawPrimitive()` in `QuickDrawCore.c`; desktop wallpaper pattern selection is handled separately in `src/PatternMgr/`.
- `src/QuickDraw/quickdraw_pictures.c` (`DrawPicture`) – the player handles core v1/v2
  PICT state, shape, text, clipping, and bitmap opcodes, including scaling and region
  masks. Reserved or unsupported opcodes are skipped where their lengths are known;
  broad opcode coverage and malformed-picture validation remain incomplete.
- ~~`src/QuickDraw/quickdraw_pictures.c` – `SetClip`/`GetClip` do not copy regions, breaking callers that expect independent clip regions.~~ **VERIFIED** (2025-10-06): SetClip/GetClip in QuickDrawCore.c properly use CopyRgn() for independent region copies
- `src/QuickDraw/CursorManager.c` implements software cursor visibility, obscuring until mouse movement, and watch-frame cycling through `SpinCursor()`. `IntegrationTests.c` covers nested hide/show and movement-based reveal; screen-level background restoration and watch-frame stepping remain unverified.

## Window, Dialog, Control, and Menu Managers
- `include/DialogManager/DialogManager.h` declares `DialogCut`, `DialogCopy`,
  `DialogPaste`, `DialogDelete`, `DialogManager_ShowOpenFileDialog`, and
  `DialogManager_ShowSaveFileDialog`, and `ModalDialogs.h` declares
  `AnnounceModalDialog`; none has a definition in `src/`. Implement the
  intended contracts or remove declarations that are not supported APIs.
- `include/DialogManager/DialogEvents.h` and `DialogItems.h` declare helper APIs
  with no definitions in `src/`:
  `ProcessDialogEvent`, `HandleDialogMouseDown`, `HandleDialogKeyDown`,
  `HandleDialogUpdate`, `SetDialogFocus`, `GetDialogFocus`,
  `IsDialogItemFocusable`, `InstallDialogEventFilter`,
  `RemoveDialogEventFilter`, `CallDialogEventFilter`,
  `SetDialogKeyboardShortcut`, `RemoveDialogKeyboardShortcut`,
  `ProcessDialogKeyboardShortcut`, `HandleDialogTextEdit`,
  `GetDialogTextSelection`, `SetDialogTextSelection`, `SetDialogIdleProc`,
  `GetDialogEventError`, `ConvertPlatformEvent`, `HandlePlatformDialogEvent`,
  `SetDialogEventLogging`, `LogDialogEvent`, `NotifyDialogEventHandlers`,
  `ValidateDialogEvent`, `CreatePlatformDialogItem`,
  `DestroyPlatformDialogItem`, `GetDialogItemAccessibilityText`,
  `GetDialogItemControl`, `GetDialogItemValue`, `SetDialogItemValue`,
  `SetDialogItemControl`, `GetDialogItemRefCon`, `GetDialogItemUserData`,
  `SetDialogItemRefCon`, `SetDialogItemUserData`, and
  `SetDialogItemAccessibilityText`. Implement the supported contract or remove
  declarations that are not part of the intended API.
- ~~Window Manager mouse-down handling returned immediately for `inDrag`/`inGrow` parts instead of starting window tracking.~~ **FIXED** (2025-10-06): The current routing is in `src/EventManager/EventDispatcher.c`: `inDrag` calls `DragWindow`, while `inGrow` calls `GrowWindow` and applies its returned dimensions with `SizeWindow`.
- ~~`src/DialogManager/DialogDrawing.c` – Edit-text items ignore focus rings; System 7 drew a focus frame and moved the caret when the control is active.~~ **FIXED** (2025-10-06): Edit-text focus rings and caret blinking implemented in DialogEditText.c
- ~~`src/DialogManager/dialog_manager_private.c` – `GetNextUserCancelEvent` is a stub; modal dialogs should scan the event queue for cancel gestures (Command-.) as the Classic API allowed.~~ **FIXED** (2025-10-06): IsUserCancelEvent/GetNextUserCancelEvent implemented, modal dialogs support Cmd-. and Escape
- ~~`src/ControlManager/StandardControls.c` – Push-button title baselines used hard-coded ascent/descent values instead of the selected system font metrics.~~ **FIXED**: push-button, checkbox, and radio labels now use `GetFontInfo()`, which queries Font Manager metrics and falls back to fixed defaults when unavailable.
- Mixed-state checkbox paths remain unvalidated; native System 7 controls supported tri-state checkboxes.
- `include/MenuManager/MenuManager.h` declares `PopUpMenuSelect`, but there is no definition in `src/`; implement popup tracking and return encoding, then add integration coverage.
- `include/MenuManager/MenuManager.h` also declares `InsertIntlResMenu` and `InitProcMenu`, with no definitions in `src/`.
- `include/ControlManager/ControlManager.h` declares `NewEditTextControl`, `NewStaticTextControl`, and `NewPopupControl`, but none has a definition in `src/`.

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
- ~~`src/SoundManager/SoundManagerBareMetal.c` – Core Sound Manager channels and playback APIs return `unimpErr`; only `SysBeep` exists, whereas System 7 provided channel-based audio playback.~~ **FIXED** (2026-10-01): channel-based playback is implemented with a shared `SndMidiNoteToFreq()` lookup table and `SndPlaySoundHeader()`.
- `SndStartFilePlay`, `SndPauseFilePlay`, and `SndStopFilePlay` are defined but still return `unimpErr`; file-based playback is not implemented.
- `include/SoundManager/SoundManager.h` declares `SndRecord`, `SndRecordToFile`, and the `SPB*` recording/input APIs, but none have definitions in `src/`; implement these APIs or remove declarations that are intentionally unsupported.
- ~~`src/PatternMgr/pattern_manager.c` – Desktop pattern installation was unimplemented, leaving the Finder without classic patterned backgrounds.~~ **FIXED** (2026-10-01): the pattern manager in `src/PatternMgr/` serves 17 colour `ppat` and 32 black-and-white `PAT` resources defined by `patterns.json` and embedded from the generated `Patterns.rsrc`; these back the Set Desktop Pattern control panel.
- `src/QuickDraw/Regions.c` – `NewRgn()` builds its `RgnHandle` and region data from two `NewPtr()` allocations, while region operations also use `HLock()`/`HUnlock()` as if the outer pointer were a relocatable handle. Reconcile the representation with the Memory Manager's handle contract and validate region movement/disposal under heap compaction.

## Next Steps
Prioritize the unstruck entries above for implementation or validation; the struck-through entries are historical and are not remaining work. Closing the open gaps will improve compatibility with classic System 7 applications that rely on the documented Toolbox contract.
