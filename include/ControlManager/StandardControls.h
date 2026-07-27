/**
 * @file StandardControls.h
 * @brief Standard control definitions (buttons, checkboxes, radio buttons)
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#ifndef STANDARDCONTROLS_H
#define STANDARDCONTROLS_H

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

#include "ControlManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extended control functions */
void SetCheckboxMixed(ControlHandle checkbox, Boolean mixed);
Boolean GetCheckboxMixed(ControlHandle checkbox);
void SetRadioGroup(ControlHandle radio, SInt16 groupID);
SInt16 GetRadioGroup(ControlHandle radio);

#ifdef __cplusplus
}
#endif

#endif /* STANDARDCONTROLS_H */

/* Draw a push button the way System 7 does: a rounded outline with the title
 * centred, filled when pressed, with a three-pixel ring when it is the
 * default. The Control Manager's button definition and the Dialog Manager
 * both draw through this, so the two cannot disagree. */
void CTL_DrawPushButton(const Rect* bounds, const unsigned char* title,
                        Boolean isDefault, Boolean isEnabled, Boolean isPressed);
