/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * HelpManager_HAL.h - Hardware Abstraction Layer for Help Manager
 * Platform-specific help system interfaces
 */

#ifndef HELPMANAGER_HAL_H
#define HELPMANAGER_HAL_H

#include "HelpManager/HelpManager.h"

/* HAL Capabilities */
enum {
    kHMCapBasicBalloons    = (1 << 0),  /* Basic balloon help */
    kHMCapTooltips        = (1 << 1),  /* Native tooltips */
    kHMCapHTMLHelp        = (1 << 2),  /* HTML help viewer */
    kHMCapAccessibility   = (1 << 3),  /* Screen reader support */
    kHMCapSearch          = (1 << 4),  /* Help search */
    kHMCapNavigation      = (1 << 5),  /* Help navigation */
    kHMCapMultiLanguage   = (1 << 6),  /* Multiple languages */
    kHMCapAnimation       = (1 << 7)   /* Animated balloons */
};

/* HAL Initialization */
OSErr HelpManager_HAL_Init(void);
void HelpManager_HAL_Cleanup(void);

/* Balloon Display */
OSErr HelpManager_HAL_ShowBalloon(const char *text, int x, int y,
                                 int width, int height, int variant);
OSErr HelpManager_HAL_HideBalloon(void);
OSErr HelpManager_HAL_DrawBalloonFrame(void *context, const Rect *bounds,
                                       int variant, Boolean hasTip);

/* HTML Help Support */
OSErr HelpManager_HAL_ShowHTMLHelp(const char *topic, const char *anchor);
OSErr HelpManager_HAL_SearchHelp(const char *searchTerm, char *results,
                                int maxResults);

/* Accessibility Support */
OSErr HelpManager_HAL_EnableAccessibility(Boolean enable);

/* Configuration */
OSErr HelpManager_HAL_SetHelpPath(const char *path);
OSErr HelpManager_HAL_SetTooltipMode(Boolean useTooltips);
Boolean HelpManager_HAL_GetTooltipMode(void);

/* Platform Capabilities */
UInt32 HelpManager_HAL_GetCapabilities(void);

#endif /* HELPMANAGER_HAL_H */