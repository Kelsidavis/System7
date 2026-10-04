/* Control Manager Internal Functions */
#ifndef CONTROL_INTERNAL_H
#define CONTROL_INTERNAL_H

#include "SystemTypes.h"

/* Control tracking */
ControlHandle GetTrackingControl(void);
SInt16 GetTrackingPart(void);
Boolean IsControlTracking(ControlHandle control);

/* Shared drawing primitive used by the Control and Dialog Managers. */
void CTL_DrawPushButton(const Rect *bounds, const unsigned char *title,
                       Boolean isDefault, Boolean isEnabled, Boolean isPressed);

/* Control list helpers (WM interop) */
ControlHandle _GetFirstControl(WindowPtr window);
void _SetFirstControl(WindowPtr window, ControlHandle control);

/* Basic window bounds helper used during control drag */

/* Attach control to window list head */
void _AttachControlToWindow(ControlHandle ctrl, WindowPtr window);

/* Resource-based control loader (from ControlResources.c) */
ControlHandle LoadControlFromResource(Handle cntlResource, WindowPtr owner);

#endif /* CONTROL_INTERNAL_H */
