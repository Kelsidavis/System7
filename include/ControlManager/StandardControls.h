/**
 * @file StandardControls.h
 * @brief Standard control definitions (buttons, checkboxes, radio buttons)
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#ifndef STANDARDCONTROLS_H
#define STANDARDCONTROLS_H

#include "ControlManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Extended control functions */
void SetCheckboxMixed(ControlHandle checkbox, bool mixed);
bool GetCheckboxMixed(ControlHandle checkbox);
void SetRadioGroup(ControlHandle radio, int16_t groupID);
int16_t GetRadioGroup(ControlHandle radio);

#ifdef __cplusplus
}
#endif

#endif /* STANDARDCONTROLS_H */