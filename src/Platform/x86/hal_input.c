/*
 * hal_input.c - x86 Input HAL Implementation
 *
 * Platform-specific input handling for x86 (PS/2 keyboard and mouse).
 */

#include "Platform/include/input.h"
#include "PS2Controller.h"

/*
 * hal_input_poll - Poll for input events
 *
 * Wrapper for PS/2 input polling.
 */
void hal_input_poll(void) {
    PollPS2Input();
}

/*
 * hal_input_get_mouse - Get current mouse position
 *
 * @param mouse_loc - Pointer to Point structure to receive mouse coordinates
 */
void hal_input_get_mouse(Point* mouse_loc) {
    GetMouse(mouse_loc);
}

/*
 * hal_input_get_keyboard_state - Get current keyboard state
 *
 * @param key_map - KeyMap array to receive keyboard state
 * @return Boolean indicating success
 */
Boolean hal_input_get_keyboard_state(KeyMap key_map) {
    return GetPS2KeyboardState(key_map);
}
