/**
 * @file PlatformControls.h
 * @brief Platform abstraction for native controls
 *
 * Copyright (c) 2024 - System 7.1 Portable Toolbox Project
 * Licensed under MIT License
 */

#ifndef PLATFORMCONTROLS_H
#define PLATFORMCONTROLS_H

#include "ControlManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform types */
typedef enum {
    kPlatformGeneric = 0,
    kPlatformCocoa = 1,
    kPlatformWin32 = 2,
    kPlatformGTK = 3
} PlatformType;

/* Platform control types */
typedef enum {
    kPlatformControlButton = 0,
    kPlatformControlCheckbox = 1,
    kPlatformControlRadio = 2,
    kPlatformControlScrollbar = 3,
    kPlatformControlEditText = 4,
    kPlatformControlStaticText = 5,
    kPlatformControlPopup = 6
} PlatformControlType;

/* Platform features */
typedef enum {
    kFeatureHighDPI = 0,
    kFeatureAccessibility = 1,
    kFeatureTouch = 2,
    kFeatureAnimation = 3,
    kFeatureNative = 4
} PlatformFeature;

/* Platform callback procedures */
typedef OSErr (*PlatformDrawProcPtr)(ControlHandle control, void *nativeControl);
typedef OSErr (*PlatformEventProcPtr)(ControlHandle control, void *nativeControl, void *event);
typedef OSErr (*PlatformUpdateProcPtr)(ControlHandle control, void *nativeControl);

/* Platform control management */
OSErr InitializePlatformControls(void);
OSErr CreatePlatformControl(ControlHandle control, PlatformControlType type);
void DestroyPlatformControl(ControlHandle control);

/* Platform settings */
void SetNativeControlsEnabled(bool enabled);
bool GetNativeControlsEnabled(void);
void SetHighDPIEnabled(bool enabled);
bool GetHighDPIEnabled(void);
void SetAccessibilityEnabled(bool enabled);
bool GetAccessibilityEnabled(void);

/* Platform information */
PlatformType GetCurrentPlatform(void);
bool ControlSupportsFeature(ControlHandle control, PlatformFeature feature);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORMCONTROLS_H */