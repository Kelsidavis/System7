# Keyboard Integration for Standard Controls & Dialog Manager

Complete System 7-style keyboard navigation and control activation for dialogs. The Dialog Manager now tracks focus, debounces events, and routes keystrokes to controls so Return/Esc/Space/Tab behave just like the classic OS.

## Feature Summary

### Default / Cancel Buttons
- **Return / Enter (`\r` or `0x03`)** locates the default push button (`varCode & 1`), flashes it via `HiliteControl()`, calls its action proc, and returns the item number to the modal loop.
- **Escape (`0x1B`)** finds the cancel button (`varCode & 2`) and performs the same sequence.
- `DM_FindDefaultButton()` / `DM_FindCancelButton()` live in `DialogKeyboard.c` and walk the control list intelligently (skipping hidden/disabled items).

### Focus Tracking & Tab Navigation
- Lightweight focus table (up to 16 dialogs) stores the focused control per window.
- `DM_FocusNextControl()` / `DM_SetKeyboardFocus()` handle forward and reverse traversal, wrapping when you hit the ends.
- Controls must be visible, non-zero sized, and active to receive focus; the focus ring XORs so the outline erases cleanly.
- `Tab` selects the next focusable control, `Shift+Tab` walks backwards.

### Space Key Activation
- Space toggles the currently focused checkbox or radio, or activates a focused push button.
- Checkbox toggles call `contrlAction` so hooks still fire; radio buttons leverage `HandleRadioGroup()` to maintain exclusivity.

### Debounce Guard
- `DM_DebounceAction()` suppresses duplicate triggers when a mouse click and key press arrive within ~100 ms, avoiding double-activation glitches.

### Dialog Integration
Pass `keyDown` / `autoKey` events to `DM_HandleDialogKey()` inside your modal loop:
```c
if (evt.what == keyDown || evt.what == autoKey) {
    if (DM_HandleDialogKey(dialogWindow, &evt, &itemHit)) {
        // itemHit is the 1-based dialog item index
        continue; // key consumed
    }
}
```
Modal dialogs exit automatically when default/cancel buttons activate because `itemHit` mirrors the clicked control.

## Control Manager Hooks
- `IsDefaultButton()` / `IsCancelButton()` evaluate variant codes (`pushButProc | variant`)
- Inactive controls bail out in `testCntl`, so both mouse and keyboard events pass through

## Logging
- All debug output uses `[CTRL]` / `[DM]` prefixes, already whitelisted in `System71StdLib.c`
- Example traces:
```
[DM] DM_HandleDialogKey: ch=0x0D (?)
[CTRL] DM_HandleReturnKey: Activating default button
[CTRL] DM_ActivatePushButton: Flashing button (refCon=1)
```

## Manual Test Checklist
1. Build and launch with `make run`, then open a dialog containing keyboard-focusable controls.
2. Tab through controls; watch the XOR ring move in order, skipping hidden/disabled controls.
3. Shift+Tab walks backwards and wraps around to the bottom/top.
4. Space toggles checkbox/radio state and prints updated refCons in the serial log.
5. Return activates the default (OK) button, Esc activates Cancel, and the dialog closes when you honour those `itemHit` values.
6. Drag the mouse across buttons while pressing Return to confirm debounce prevents double actions.

## Future Enhancements
- Extend focus bookkeeping to StandardFile so embedded List Manager controls advertise focus.
- Add optional visual chrome (e.g., dotted outline vs. XOR) once we have pattern resources for classic focus rings.
- Surface a `DM_SetInitialFocus()` helper for callers who want something other than the first control to receive focus on dialog open.
