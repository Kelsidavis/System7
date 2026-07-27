/*
 * AboutThisMac.h - System 7.1-style "About This Macintosh" Window
 *
 * Public API for the About window.
 */

#ifndef ABOUT_THIS_MAC_H
#define ABOUT_THIS_MAC_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AboutWindow_ShowOrToggle - Request About window to be shown
 *
 * Called from Apple menu handler. DEFERRED: Sets a flag for the main event loop
 * to create the window after event dispatch completes. This prevents event re-entry
 * deadlock from NewWindow posting events during event dispatch.
 */
void AboutWindow_ShowOrToggle(void);

/*
 * AboutWindow_ProcessPendingCreation - Actually create deferred About window
 *
 * Called from main event loop after event dispatch is complete.
 * Processes any pending window creation request from AboutWindow_ShowOrToggle.
 * Safe to call frequently (checks internal flag).
 */
void AboutWindow_ProcessPendingCreation(void);

/*
 * AboutWindow_CloseIf - Close About window if it matches
 *
 * Parameters:
 *   w - Window to check and potentially close
 */
Boolean AboutWindow_CloseIf(WindowPtr w);

/*
 * AboutWindow_HandleUpdate - Handle update event for About window
 *
 * Parameters:
 *   w - Window receiving update event
 *
 * Returns:
 *   true if this was the About window and update was handled
 */
Boolean AboutWindow_HandleUpdate(WindowPtr w);

/*
 * AboutWindow_HandleMouseDown - Handle mouse down in About window
 *
 * Parameters:
 *   w - Window receiving mouse down
 *   part - Window part code (inDrag, inGoAway, etc.)
 *   localPt - Mouse location in local coordinates
 *
 * Returns:
 *   true if this was the About window and event was handled
 */
Boolean AboutWindow_HandleMouseDown(WindowPtr w, short part, Point localPt);

/*
 * AboutWindow_IsOurs - Check if window is the About window
 *
 * Parameters:
 *   w - Window to check
 *
 * Returns:
 *   true if window is the About This Macintosh window
 */
Boolean AboutWindow_IsOurs(WindowPtr w);

#ifdef __cplusplus
}
#endif

#endif /* ABOUT_THIS_MAC_H */
