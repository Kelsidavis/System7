# Keyboard Integration for Standard Controls & Dialog Manager

The Dialog Manager implements keyboard navigation and activation for standard
dialog controls. `DM_HandleDialogKey()` handles `keyDown` and `autoKey` events
for Return, numeric-keypad Enter, Escape, Tab, and Space. Tab moves between
editable text items when one has focus; otherwise the project's control-focus
extension cycles through focusable controls. This is a partial implementation,
not complete System 7 keyboard behavior.

## Feature Summary

### Default / Cancel Buttons
- **Return / Enter (`\r` or `0x03`)** locates the default push button. When
  it finds a control, it flashes it with XOR inversion, calls its action proc,
  and returns its dialog item number to the modal loop. If lookup only finds a
  configured item number, that item is returned without control activation.
- **Escape (`0x1B`)** locates the cancel button and follows the same activation
  path, including the item-number fallback.
- `DM_FindDefaultButton()` / `DM_FindCancelButton()` first scan controls for
  their `ButtonData` default/cancel flags, then fall back to the corresponding
  item number in the dialog record. This lookup does not use the focus
  traversal filter.

### Focus Tracking & Tab Navigation
- Lightweight focus table (up to 16 dialogs) stores the focused control per window.
- `DM_FocusNextControl()` / `DM_SetKeyboardFocus()` handle forward and reverse control traversal, wrapping when you hit the ends. Classic Tab traversal between editable text items is handled by `AdvanceDialogEditTextFocus()`.
- Controls must be visible, non-zero sized, and active to receive focus; the focus ring XORs so the outline erases cleanly.
- With an editable text field focused, `Tab` selects the next editable field and `Shift+Tab` walks backwards, wrapping at either end. With no editable field focused, the control-focus extension traverses standard controls.

### Space Key Activation
- Space toggles the currently focused checkbox or radio, or activates a focused push button.
- Checkbox toggles call `contrlAction` so hooks still fire; radio buttons leverage `HandleRadioGroup()` to maintain exclusivity.

### Debounce Guard
- `DM_DebounceAction()` suppresses a mouse action immediately following a keyboard action, or vice versa, when they occur within six ticks (about 100 ms). Repeated actions of the same kind are allowed.

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
- `IsDefaultButton()` / `IsCancelButton()` read flags from the control's `ButtonData`.
- `TestControl()` rejects invisible or inactive controls before invoking the control definition's `testCntl` method.

## Logging
- All debug output uses `[CTRL]` / `[DM]` prefixes, already whitelisted in `System71StdLib.c`
- Example traces:
```
[DM] DM_HandleDialogKey: ch=0x0D (?)
[CTRL] DM_HandleReturnKey: Activating default button
[CTRL] DM_ActivatePushButton: Flashing button (refCon=1)
```

## Test Checklist
1. `make check` builds the integration kernel; the guest regression sends Tab and Shift+Tab through `DM_HandleDialogKey()` and checks focus movement across edit fields.
2. For visual verification, launch with `make run` and check that the XOR ring moves through visible, active controls when no edit field has focus.
3. Shift+Tab walks backwards and wraps around to the bottom/top.
4. Space toggles checkbox/radio state and prints updated refCons in the serial log.
5. Return activates the default (OK) button, Esc activates Cancel, and the dialog closes when you honour those `itemHit` values.
6. Drag the mouse across buttons while pressing Return to confirm debounce prevents double actions.

## Future Enhancements
- Validate focus traversal and visual focus rings against a booted System 7.1 guest.
- Add optional visual chrome (e.g., dotted outline vs. XOR) once we have pattern resources for classic focus rings.
- Surface a `DM_SetInitialFocus()` helper for callers who want something other than the first control to receive focus on dialog open.
