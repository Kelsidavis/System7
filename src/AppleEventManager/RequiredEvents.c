/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * RequiredEvents.c
 *
 * Implementation of the four required Apple Events
 * that all applications must support
 *
 * Based on Mac OS 7.1 Apple Event Manager
 */

#include "AppleEventManager/AppleEventManager.h"
#include "AppleEventManager/EventHandlers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* External access to app state (would normally be in ProcessMgr) */
typedef struct AppState {
    Boolean hasOpenDocuments;
    Boolean hasUnsavedChanges;
    char** openDocumentPaths;
    int openDocumentCount;
    void (*openApplicationCallback)(void);
    void (*openDocumentCallback)(const char* path);
    void (*printDocumentCallback)(const char* path);
    Boolean (*quitApplicationCallback)(void);
} AppState;

static AppState g_appState = {0};

/* ========================================================================
 * Open Application Event (oapp)
 * ======================================================================== */

static OSErr HandleOpenApplicationEvent(const AppleEvent* theAppleEvent, AppleEvent* reply, long handlerRefcon) {
    (void)theAppleEvent;
    (void)reply;
    (void)handlerRefcon;

    /* Call application's open handler if registered */
    if (g_appState.openApplicationCallback) {
        g_appState.openApplicationCallback();
    }

    /* If no documents are open, create a new untitled document */
    if (!g_appState.hasOpenDocuments) {
        /* Application-specific behavior */
        /* For example, create new untitled window */
    }

    return noErr;
}

/* ========================================================================
 * Open Documents Event (odoc)
 * ======================================================================== */

static OSErr HandleOpenDocumentsEvent(const AppleEvent* theAppleEvent, AppleEvent* reply, long handlerRefcon) {
    (void)reply;
    (void)handlerRefcon;

    OSErr err;
    AEDescList docList;
    long itemsInList;

    /* Get the direct object (list of documents) */
    err = AEGetParamDesc(theAppleEvent, keyDirectObject, typeAEList, &docList);
    if (err != noErr) return err;

    /* Count the documents */
    err = AECountItems(&docList, &itemsInList);
    if (err != noErr) {
        AEDisposeDesc(&docList);
        return err;
    }

    /* Process each document */
    for (long index = 1; index <= itemsInList; index++) {
        AEDesc fileSpec;
        err = AEGetNthDesc(&docList, index, typeFSS, NULL, &fileSpec);

        if (err == noErr) {
            /* Extract file path from FSSpec */
            /* In a real implementation, this would convert FSSpec to path */
            if (g_appState.openDocumentCallback) {
                /* For now, pass a placeholder path */
                char pathBuffer[256];
                snprintf(pathBuffer, sizeof(pathBuffer), "Document_%ld", index);
                g_appState.openDocumentCallback(pathBuffer);
            }

            g_appState.hasOpenDocuments = true;
            AEDisposeDesc(&fileSpec);
        }
    }

    AEDisposeDesc(&docList);
    return noErr;
}

/* ========================================================================
 * Print Documents Event (pdoc)
 * ======================================================================== */

static OSErr HandlePrintDocumentsEvent(const AppleEvent* theAppleEvent, AppleEvent* reply, long handlerRefcon) {
    (void)reply;
    (void)handlerRefcon;

    OSErr err;
    AEDescList docList;
    long itemsInList;

    /* Get the direct object (list of documents) */
    err = AEGetParamDesc(theAppleEvent, keyDirectObject, typeAEList, &docList);
    if (err != noErr) return err;

    /* Count the documents */
    err = AECountItems(&docList, &itemsInList);
    if (err != noErr) {
        AEDisposeDesc(&docList);
        return err;
    }

    /* Process each document */
    for (long index = 1; index <= itemsInList; index++) {
        AEDesc fileSpec;
        err = AEGetNthDesc(&docList, index, typeFSS, NULL, &fileSpec);

        if (err == noErr) {
            /* Extract file path from FSSpec */
            /* In a real implementation, this would convert FSSpec to path */
            if (g_appState.printDocumentCallback) {
                /* For now, pass a placeholder path */
                char pathBuffer[256];
                snprintf(pathBuffer, sizeof(pathBuffer), "Document_%ld", index);
                g_appState.printDocumentCallback(pathBuffer);
            }

            AEDisposeDesc(&fileSpec);
        }
    }

    AEDisposeDesc(&docList);
    return noErr;
}

/* ========================================================================
 * Quit Application Event (quit)
 * ======================================================================== */

static OSErr HandleQuitApplicationEvent(const AppleEvent* theAppleEvent, AppleEvent* reply, long handlerRefcon) {
    (void)theAppleEvent;
    (void)reply;
    (void)handlerRefcon;

    Boolean shouldQuit = true;

    /* Check if application has unsaved changes */
    if (g_appState.hasUnsavedChanges) {
        /* Ask user if they want to save */
        /* This would normally show a dialog */
        /* For now, we'll use a callback */
        if (g_appState.quitApplicationCallback) {
            shouldQuit = g_appState.quitApplicationCallback();
        }
    }

    if (shouldQuit) {
        /* Clean up and quit */
        /* In a real implementation, this would call ExitToShell() */
        /* For now, we just mark that we're quitting */
        g_appState.hasOpenDocuments = false;
        g_appState.hasUnsavedChanges = false;
    }

    return shouldQuit ? noErr : userCanceledErr;
}

/* ========================================================================
 * Required Event Installation
 * ======================================================================== */

OSErr AEInstallRequiredEventHandlers(void) {
    OSErr err;

    /* Install Open Application handler */
    err = AEInstallEventHandler(kCoreEventClass, kAEOpenApplication,
                               HandleOpenApplicationEvent, 0, false);
    if (err != noErr) return err;

    /* Install Open Documents handler */
    err = AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
                               HandleOpenDocumentsEvent, 0, false);
    if (err != noErr) return err;

    /* Install Print Documents handler */
    err = AEInstallEventHandler(kCoreEventClass, kAEPrintDocuments,
                               HandlePrintDocumentsEvent, 0, false);
    if (err != noErr) return err;

    /* Install Quit Application handler */
    err = AEInstallEventHandler(kCoreEventClass, kAEQuitApplication,
                               HandleQuitApplicationEvent, 0, false);
    if (err != noErr) return err;

    return noErr;
}

OSErr AERemoveRequiredEventHandlers(void) {
    OSErr err, finalErr = noErr;

    /* Remove Open Application handler */
    err = AERemoveEventHandler(kCoreEventClass, kAEOpenApplication,
                              HandleOpenApplicationEvent, false);
    if (err != noErr) finalErr = err;

    /* Remove Open Documents handler */
    err = AERemoveEventHandler(kCoreEventClass, kAEOpenDocuments,
                              HandleOpenDocumentsEvent, false);
    if (err != noErr) finalErr = err;

    /* Remove Print Documents handler */
    err = AERemoveEventHandler(kCoreEventClass, kAEPrintDocuments,
                              HandlePrintDocumentsEvent, false);
    if (err != noErr) finalErr = err;

    /* Remove Quit Application handler */
    err = AERemoveEventHandler(kCoreEventClass, kAEQuitApplication,
                              HandleQuitApplicationEvent, false);
    if (err != noErr) finalErr = err;

    return finalErr;
}

/* ========================================================================
 * Application Callback Registration
 * ======================================================================== */

void AERegisterOpenApplicationCallback(void (*callback)(void)) {
    g_appState.openApplicationCallback = callback;
}

void AERegisterOpenDocumentCallback(void (*callback)(const char* path)) {
    g_appState.openDocumentCallback = callback;
}

void AERegisterPrintDocumentCallback(void (*callback)(const char* path)) {
    g_appState.printDocumentCallback = callback;
}

void AERegisterQuitApplicationCallback(Boolean (*callback)(void)) {
    g_appState.quitApplicationCallback = callback;
}

void AESetHasUnsavedChanges(Boolean hasChanges) {
    g_appState.hasUnsavedChanges = hasChanges;
}

void AESetHasOpenDocuments(Boolean hasDocuments) {
    g_appState.hasOpenDocuments = hasDocuments;
}