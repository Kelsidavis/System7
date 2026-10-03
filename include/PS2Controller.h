/* PS/2 Controller Interface */
#ifndef PS2_CONTROLLER_H
#define PS2_CONTROLLER_H

#include "SystemTypes.h"
#include "Platform/PS2Input.h"

/* Additional state-update hooks exposed by the legacy PS/2 header. */
void UpdateMouseStateDelta(SInt16 dx, SInt16 dy, UInt8 buttons);
void UpdateMouseStateAbsolute(SInt16 x, SInt16 y, UInt8 buttons);
void PS2_SetIRQDriven(Boolean enabled);

#endif /* PS2_CONTROLLER_H */
