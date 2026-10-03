/*
 * StartupScreen.h - System 7 Startup Screen Display
 *
 * Provides the classic "Welcome to Macintosh" startup screen with
 * extension loading display and progress indication.
 *
 * Copyright (c) 2024 System7.1-Portable Project
 * MIT License
 */

#ifndef STARTUP_SCREEN_H
#define STARTUP_SCREEN_H

#include "SystemTypes.h"

/* Forward declarations */

#include "EventManager/EventTypes.h"
#include "WindowManager/WindowTypes.h"
#include <QuickDraw.h>

/* Startup screen phases */
typedef enum {
    kStartupPhaseInit = 0,
    kStartupPhaseWelcome,
    kStartupPhaseExtensions,
    kStartupPhaseDrivers,
    kStartupPhaseFinder,
    kStartupPhaseComplete
} StartupPhase;

/* Extension info for display */
typedef struct {
    Str255 name;
    UInt16 iconID;
    Boolean loaded;
    OSErr error;
} ExtensionInfo;

/* Startup progress info */
typedef struct {
    StartupPhase phase;
    short percentComplete;
} StartupProgress;

/* Startup screen configuration */
typedef struct {
    Boolean showWelcome;
    Boolean showExtensions;
    Boolean showProgress;
    UInt16 welcomeDuration;
    Boolean enableSound;
    RGBColor backgroundColor;
    RGBColor textColor;
} StartupScreenConfig;

/* Startup screen state */
typedef struct {
    Boolean initialized;
    Boolean visible;
    WindowPtr window;
    StartupPhase currentPhase;
    Rect screenBounds;
    Rect logoRect;
    Rect textRect;
    Rect extensionRect;
    Rect progressRect;
    BitMap offscreenBitmap;
    StartupProgress progress;
} StartupScreenState;

/* Progress callbacks */
typedef void (*StartupProgressProc)(StartupPhase phase, short percent, void* userData);

/* Public API */

/* Initialize startup screen system */
OSErr InitStartupScreen(const StartupScreenConfig* config);

/* Show welcome screen */
OSErr ShowWelcomeScreen(void);

/* Hide startup screen */
void HideStartupScreen(void);

/* Update startup progress */
OSErr UpdateStartupProgress(const StartupProgress* progress);

/* Display loading extension */
OSErr ShowLoadingExtension(const ExtensionInfo* extension);

/* Show startup item with icon */
OSErr ShowStartupItem(ConstStr255Param itemName, UInt16 iconID);

/* Set startup phase */
OSErr SetStartupPhase(StartupPhase phase);

/* Draw progress bar */
void DrawProgressBar(short percent);

/* Play startup sound */
OSErr PlayStartupSound(void);


/* Extension loading */
OSErr BeginExtensionLoading(UInt16 extensionCount);

/* Error display */
OSErr ShowStartupError(ConstStr255Param errorMessage, OSErr errorCode);

/* Cleanup */
void CleanupStartupScreen(void);

/* Startup screen customization */
OSErr SetStartupBackgroundPattern(const Pattern* pattern);
OSErr SetStartupColors(const RGBColor* background, const RGBColor* text);

/* Progress callbacks */

OSErr RegisterStartupProgressCallback(StartupProgressProc proc, void* userData);


/* Debug mode */
void EnableStartupDebugMode(Boolean enable);
void DumpStartupScreenState(void);

#endif /* STARTUP_SCREEN_H */
