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

#include <Types.h>
#include <QuickDraw.h>

/* Startup screen phases */
typedef enum {
    kStartupPhaseInit,
    kStartupPhaseWelcome,
    kStartupPhaseExtensions,
    kStartupPhaseDrivers,
    kStartupPhaseFinder,
    kStartupPhaseComplete
} StartupPhase;

/* Extension info for display */
typedef struct ExtensionInfo {
    Str255          name;
    UInt16          iconID;
    Boolean         loaded;
    OSErr           error;
} ExtensionInfo;

/* Startup progress info */
typedef struct StartupProgress {
    StartupPhase    phase;
    UInt16          currentItem;
    UInt16          totalItems;
    UInt8           percentComplete;
} StartupProgress;

/* Startup screen configuration */
typedef struct StartupScreenConfig {
    Boolean         showWelcome;
    Boolean         showExtensions;
    Boolean         showProgress;
    UInt32          welcomeDuration;    /* Ticks to show welcome */
    Boolean         enableSound;
    RGBColor        backgroundColor;
    RGBColor        textColor;
} StartupScreenConfig;

/* Startup screen state */
typedef struct StartupScreenState {
    Boolean         initialized;
    Boolean         visible;
    WindowPtr       window;
    GrafPtr         offscreenPort;
    BitMap          offscreenBitmap;
    StartupPhase    currentPhase;
    StartupProgress progress;
    Rect            screenBounds;
    Rect            logoRect;
    Rect            textRect;
    Rect            extensionRect;
    Rect            progressRect;
} StartupScreenState;

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

/* Custom startup screen */
OSErr SetCustomStartupScreen(PicHandle picture);

/* Extension loading */
OSErr BeginExtensionLoading(UInt16 extensionCount);
OSErr ShowExtensionIcon(ConstStr255Param name, Handle iconSuite);
OSErr EndExtensionLoading(void);

/* Error display */
OSErr ShowStartupError(ConstStr255Param errorMessage, OSErr errorCode);

/* Cleanup */
void CleanupStartupScreen(void);

/* Startup screen customization */
OSErr SetStartupBackgroundPattern(const Pattern* pattern);
OSErr SetStartupColors(const RGBColor* background, const RGBColor* text);
OSErr SetStartupLogo(PicHandle logo);

/* Progress callbacks */
typedef void (*StartupProgressProc)(StartupPhase phase, UInt8 percent, void* userData);
OSErr RegisterStartupProgressCallback(StartupProgressProc proc, void* userData);

/* Animation support */
OSErr StartStartupAnimation(void);
OSErr StopStartupAnimation(void);
Boolean IsStartupAnimating(void);

/* Debug mode */
void EnableStartupDebugMode(Boolean enable);
void DumpStartupScreenState(void);

#endif /* STARTUP_SCREEN_H */