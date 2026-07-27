#include "MemoryMgr/MemoryManager.h"
#include "SuperCompat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * AppleEventExample.c
 *
 * Comprehensive example demonstrating Apple Event Manager capabilities
 * Shows how to use the Apple Event Manager for inter-application communication,
 * scripting, automation, and modern system integration
 */

#include "CompatibilityFix.h"
#include "AppleEvents/AppleEventTypes.h"
#include "SystemTypes.h"
#include "System71StdLib.h"
#include "AppleEventManager/AppleEventManager.h"
#include "AppleEventManager/EventDescriptors.h"
#include "AppleEventManager/EventHandlers.h"
#include "AppleEventManager/AppleScript.h"
#include "AppleEventManager/ObjectModel.h"
#include "AppleEventManager/ProcessTargeting.h"
#include "AppleEventManager/EventRecording.h"


/* ========================================================================
 * Example Application Event Handlers
 * ======================================================================== */

/* Handler for Open Documents event */
static OSErr HandleOpenDocuments(const AppleEvent* theAppleEvent, AppleEvent* reply, SInt32 handlerRefcon) {
    printf("HandleOpenDocuments: Received open documents event\n");

    /* Extract the direct object (list of files) */
    AEDescList documentList;
    OSErr err = AEGetParamDesc(theAppleEvent, keyDirectObject, typeAEList, &documentList);
    if (err != noErr) {
        printf("Error: Could not get document list (%d)\n", err);
        return err;
    }

    /* Get the number of documents */
    SInt32 documentCount;
    err = AECountItems(&documentList, &documentCount);
    if (err != noErr) {
        AEDisposeDesc(&documentList);
        return err;
    }

    printf("Opening %d document(s):\n", documentCount);

    /* Process each document */
    for (SInt32 i = 1; i <= documentCount; i++) {
        AEDesc fileDesc;
        AEKeyword keyword;

        err = AEGetNthDesc(&documentList, i, typeWildCard, &keyword, &fileDesc);
        if (err == noErr) {
            /* Convert to text for display */
            char* textData;
            Size textSize;
            if (AECoerceToText(&fileDesc, &textData, &textSize) == noErr) {
                printf("  Document %d: %.*s\n", i, (int)textSize, textData);
                DisposePtr((Ptr)textData);
            }
            AEDisposeDesc(&fileDesc);
        }
    }

    AEDisposeDesc(&documentList);

    /* Create a successful reply */
    if (reply) {
        OSErr replyErr = noErr;
        AEPutParamPtr(reply, keyErrorNumber, typeLongInteger, &replyErr, sizeof(replyErr));
    }

    return noErr;
}

/* Handler for Quit Application event */
static OSErr HandleQuitApplication(const AppleEvent* theAppleEvent, AppleEvent* reply, SInt32 handlerRefcon) {
    printf("HandleQuitApplication: Received quit event\n");

    /* Check if we should save documents */
    Boolean shouldSave = true;
    AEDesc saveDesc;
    if (AEGetParamDesc(theAppleEvent, 'save', typeBoolean, &saveDesc) == noErr) {
        AECoerceToBoolean(&saveDesc, &shouldSave);
        AEDisposeDesc(&saveDesc);
    }

    printf("Quitting application (save documents: %s)\n", shouldSave ? "yes" : "no");

    /* In a real application, perform cleanup here */

    return noErr;
}

/* Handler for custom Get Info event */
static OSErr HandleGetInfo(const AppleEvent* theAppleEvent, AppleEvent* reply, SInt32 handlerRefcon) {
    printf("HandleGetInfo: Received get info request\n");

    if (reply) {
        /* Return application information */
        const char* appName = "Apple Event Example";
        const char* appVersion = "1.0";

        AEPutParamPtr(reply, 'name', typeChar, appName, strlen(appName));
        AEPutParamPtr(reply, 'vers', typeChar, appVersion, strlen(appVersion));

        /* Return current time as additional info */
        time_t currentTime = time(NULL);
        AEPutParamPtr(reply, 'time', typeLongInteger, &currentTime, sizeof(currentTime));
    }

    return noErr;
}

/* ========================================================================
 * AppleScript Integration Example
 * ======================================================================== */

static void DemonstrateAppleScript(void) {
    printf("\n=== AppleScript Integration Example ===\n");

    /* Initialize AppleScript */
    OSErr err = OSAInit();
    if (err != noErr) {
        printf("Error: Could not initialize AppleScript (%d)\n", err);
        return;
    }

    /* Open the default AppleScript component */
    OSAComponentInstance scriptingComponent;
    err = OSAOpenDefaultComponent(&scriptingComponent);
    if (err != noErr) {
        printf("Error: Could not open AppleScript component (%d)\n", err);
        OSACleanup();
        return;
    }

    /* Compile and execute a simple script */
    const char* scriptSource = "tell application \"Finder\" to get name";
    OSAScript script;

    err = OSACompile(scriptingComponent, scriptSource, kOSAModeNull, &script);
    if (err == noErr) {
        printf("Script compiled successfully\n");

        /* Execute the script */
        AEDesc result;
        err = OSAExecute(scriptingComponent, script, NULL, kOSAModeNull, &result);
        if (err == noErr) {
            /* Convert result to text */
            char* resultText;
            Size resultSize;
            if (AECoerceToText(&result, &resultText, &resultSize) == noErr) {
                printf("Script result: %.*s\n", (int)resultSize, resultText);
                DisposePtr((Ptr)resultText);
            }
            AEDisposeDesc(&result);
        } else {
            printf("Error executing script (%d)\n", err);
        }

        OSADisposeScript(script);
    } else {
        printf("Error compiling script (%d)\n", err);
    }

    /* Clean up */
    OSACloseComponent(scriptingComponent);
    OSACleanup();
}

/* ========================================================================
 * Event Recording and Playback Example
 * ======================================================================== */

static void DemonstrateEventRecording(void) {
    printf("\n=== Event Recording Example ===\n");

    /* Create a recording session */
    AERecordingSession session;
    OSErr err = AECreateRecordingSession(&session);
    if (err != noErr) {
        printf("Error: Could not create recording session (%d)\n", err);
        return;
    }

    /* Set recording options */
    AERecordingOptions options = {0};
    options.recordingMode = kAERecordApplicationEvents | kAERecordUserActions;
    options.includeTimestamps = true;
    options.includeUserContext = true;
    options.maxRecordingSize = 1024 * 1024; /* 1 MB */
    options.maxRecordingTime = 60; /* 60 seconds */
    options.outputFormat = kAERecordingFormatAppleScript;

    err = AESetRecordingOptions(session, &options);
    if (err != noErr) {
        printf("Error: Could not set recording options (%d)\n", err);
        AEDisposeRecordingSession(session);
        return;
    }

    /* Start recording */
    printf("Starting event recording...\n");
    err = AEStartRecording(session, options.recordingMode);
    if (err != noErr) {
        printf("Error: Could not start recording (%d)\n", err);
        AEDisposeRecordingSession(session);
        return;
    }

    /* Simulate some events */
    printf("Recording some example events...\n");

    /* Record a custom user action */
    const char* actionDesc = "User clicked main window";
    char actionData[] = {100, 150, 1}; /* x, y, button */
    AERecordUserAction(session, actionDesc, actionData, sizeof(actionData));

    /* Record a simulated Apple Event */
    AppleEvent testEvent;
    ProcessSerialNumber currentPSN;
    AEGetCurrentProcess(&currentPSN);

    AEAddressDesc targetDesc;
    AECreateProcessDesc(&currentPSN, &targetDesc);

    AECreateAppleEvent(kCoreEventClass, kAEOpenApplication, &targetDesc, kAutoGenerateReturnID, kAnyTransactionID, &testEvent);
    AERecordEvent(session, &testEvent, kAERecordedAppleEvent);

    AEDisposeDesc(&testEvent);
    AEDisposeDesc(&targetDesc);

    /* Stop recording */
    printf("Stopping recording...\n");
    AEStopRecording(session);

    /* Get recording statistics */
    SInt32 eventCount;
    err = AEGetRecordingEventCount(session, &eventCount);
    if (err == noErr) {
        printf("Recorded %d events\n", eventCount);

        /* Display recorded events */
        for (SInt32 i = 0; i < eventCount; i++) {
            AERecordedEvent recordedEvent;
            if (AEGetRecordedEvent(session, i, &recordedEvent) == noErr) {
                printf("Event %d: Type=%d, Description=%s\n",
                       i + 1, recordedEvent.eventType,
                       recordedEvent.description ? recordedEvent.description : "No description");
            }
        }
    }

    /* Generate AppleScript from recording */
    char* scriptText;
    Size scriptSize;
    AEScriptGenerationOptions scriptOptions = {0};
    scriptOptions.scriptLanguage = "AppleScript";
    scriptOptions.includeComments = true;
    scriptOptions.optimizeForReadability = true;
    scriptOptions.includeErrorHandling = true;

    err = AEGenerateScriptFromRecording(session, &scriptOptions, &scriptText, &scriptSize);
    if (err == noErr) {
        printf("\nGenerated AppleScript:\n%.*s\n", (int)scriptSize, scriptText);
        DisposePtr((Ptr)scriptText);
    }

    /* Save recording to file */
    err = AESaveRecording(session, "example_recording.aer", kAERecordingFormatBinary);
    if (err == noErr) {
        printf("Recording saved to example_recording.aer\n");
    }

    /* Clean up */
    AEDisposeRecordingSession(session);
}

/* ========================================================================
 * Process Targeting Example
 * ======================================================================== */

static void DemonstrateProcessTargeting(void) {
    printf("\n=== Process Targeting Example ===\n");

    /* Get list of running processes */
    ProcessInfo* processList;
    SInt32 processCount;

    OSErr err = AEGetProcessList(&processList, &processCount);
    if (err != noErr) {
        printf("Error: Could not get process list (%d)\n", err);
        return;
    }

    printf("Found %d running processes:\n", processCount);

    /* Display first few processes */
    SInt32 displayCount = (processCount < 5) ? processCount : 5;
    for (SInt32 i = 0; i < displayCount; i++) {
        printf("  %d. %s (PSN: %u.%u)\n",
               i + 1,
               processList[i].processName ? processList[i].processName : "Unknown",
               processList[i].processSerialNumber.highLongOfPSN,
               processList[i].processSerialNumber.lowLongOfPSN);
    }

    if (processCount > displayCount) {
        printf("  ... and %d more processes\n", processCount - displayCount);
    }

    /* Try to find current process */
    ProcessSerialNumber currentPSN;
    err = AEGetCurrentProcess(&currentPSN);
    if (err == noErr) {
        printf("\nCurrent process PSN: %u.%u\n",
               currentPSN.highLongOfPSN, currentPSN.lowLongOfPSN);

        /* Get detailed info about current process */
        ProcessInfo currentInfo;
        err = AEGetProcessInfo(&currentPSN, &currentInfo);
        if (err == noErr) {
            printf("Current process info:\n");
            printf("  Name: %s\n", currentInfo.processName ? currentInfo.processName : "Unknown");
            printf("  Signature: %s\n", currentInfo.processSignature ? currentInfo.processSignature : "Unknown");
            printf("  Accepts Apple Events: %s\n", currentInfo.acceptsHighLevelEvents ? "Yes" : "No");
            printf("  Has Scripting Terminology: %s\n", currentInfo.hasScriptingTerminology ? "Yes" : "No");
        }
    }

    /* Clean up */
    AEDisposeProcessList(processList, processCount);
}

/* ========================================================================
 * Object Model Example
 * ======================================================================== */

static void DemonstrateObjectModel(void) {
    printf("\n=== Object Model Example ===\n");

    /* Initialize Object Support Library */
    OSErr err = OSLInit();
    if (err != noErr) {
        printf("Error: Could not initialize Object Support Library (%d)\n", err);
        return;
    }

    /* Create a simple object specifier */
    AEDesc containerDesc;
    AECreateDesc(typeNull, NULL, 0, &containerDesc); /* null container = application */

    AEDesc keyData;
    const char* documentName = "Example Document";
    AECreateDesc(typeChar, documentName, strlen(documentName), &keyData);

    AEDesc objSpecifier;
    err = CreateObjSpecifier(cDocument, &containerDesc, formName, &keyData, false, &objSpecifier);

    if (err == noErr) {
        printf("Created object specifier for document named '%s'\n", documentName);

        /* Parse the object specifier */
        ObjectSpecifier parsedSpec;
        err = ParseObjSpecifier(&objSpecifier, &parsedSpec);
        if (err == noErr) {
            printf("Object specifier details:\n");
            printf("  Object class: '%.4s'\n", (char*)&parsedSpec.objectClass);
            printf("  Key form: '%.4s'\n", (char*)&parsedSpec.keyForm);
        }

        AEDisposeDesc(&objSpecifier);
    }

    AEDisposeDesc(&containerDesc);
    AEDisposeDesc(&keyData);

    /* Clean up */
    OSLCleanup();
}

/* ========================================================================
 * Main Example Application
 * ======================================================================== */

int main(int argc, char* argv[]) {
    printf("Apple Event Manager Example Application\n");
    printf("========================================\n");

    /* Initialize Apple Event Manager */
    OSErr err = AEManagerInit();
    if (err != noErr) {
        printf("Error: Could not initialize Apple Event Manager (%d)\n", err);
        return 1;
    }

    printf("Apple Event Manager initialized successfully\n");

    /* Install event handlers */
    printf("\nInstalling Apple Event handlers...\n");

    err = AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, HandleOpenDocuments, 0, false);
    if (err == noErr) {
        printf("Installed handler for Open Documents event\n");
    }

    err = AEInstallEventHandler(kCoreEventClass, kAEQuitApplication, HandleQuitApplication, 0, false);
    if (err == noErr) {
        printf("Installed handler for Quit Application event\n");
    }

    /* Install custom event handler */
    err = AEInstallEventHandler('DEMO', 'info', HandleGetInfo, 0, false);
    if (err == noErr) {
        printf("Installed handler for custom Get Info event\n");
    }

    /* Demonstrate various Apple Event Manager features */

    /* 1. Basic Apple Event creation and processing */
    printf("\n=== Basic Apple Event Example ===\n");

    /* Create a test Apple Event */
    ProcessSerialNumber currentPSN;
    AEGetCurrentProcess(&currentPSN);

    AEAddressDesc targetDesc;
    AECreateProcessDesc(&currentPSN, &targetDesc);

    AppleEvent testEvent;
    AECreateAppleEvent('DEMO', 'info', &targetDesc, kAutoGenerateReturnID, kAnyTransactionID, &testEvent);

    /* Process the event */
    AppleEvent reply;
    AECreateList(NULL, 0, true, &reply);

    AEHandlerResult result;
    err = AEDispatchAppleEvent(&testEvent, &reply, &result);

    if (err == noErr && result == kAEHandlerExecuted) {
        printf("Successfully processed custom event\n");

        /* Extract reply information */
        char* appName;
        Size nameSize;
        if (AEGetParamPtr(&reply, 'name', typeChar, NULL, NULL, 0, &nameSize) == noErr) {
            appName = NewPtr(nameSize + 1);
            if (AEGetParamPtr(&reply, 'name', typeChar, NULL, appName, nameSize, &nameSize) == noErr) {
                appName[nameSize] = '\0';
                printf("Application name from reply: %s\n", appName);
            }
            DisposePtr((Ptr)appName);
        }
    }

    AEDisposeDesc(&testEvent);
    AEDisposeDesc(&reply);
    AEDisposeDesc(&targetDesc);

    /* 2. Demonstrate descriptor manipulation */
    printf("\n=== Descriptor Manipulation Example ===\n");

    /* Create a list of strings */
    const char* stringArray[] = {"Apple", "Banana", "Cherry", "Date"};
    AEDescList stringList;
    AECreateStringArray(stringArray, 4, &stringList);

    SInt32 itemCount;
    AECountItems(&stringList, &itemCount);
    printf("Created list with %d items:\n", itemCount);

    for (SInt32 i = 1; i <= itemCount; i++) {
        AEDesc item;
        AEKeyword keyword;
        if (AEGetNthDesc(&stringList, i, typeChar, &keyword, &item) == noErr) {
            char* text;
            Size textSize;
            if (AECoerceToText(&item, &text, &textSize) == noErr) {
                printf("  Item %d: %.*s\n", i, (int)textSize, text);
                DisposePtr((Ptr)text);
            }
            AEDisposeDesc(&item);
        }
    }

    AEDisposeDesc(&stringList);

    /* 3. Demonstrate other features */
    DemonstrateProcessTargeting();
    DemonstrateObjectModel();
    DemonstrateAppleScript();
    DemonstrateEventRecording();

    /* 4. Display statistics */
    printf("\n=== Apple Event Manager Statistics ===\n");

    AEHandlerStats stats;
    if (AEGetHandlerStats(&stats) == noErr) {
        printf("Handler Statistics:\n");
        printf("  Total handlers: %d\n", stats.totalHandlers);
        printf("  Events dispatched: %d\n", stats.eventsDispatched);
        printf("  Events handled: %d\n", stats.eventsHandled);
        printf("  Events failed: %d\n", stats.eventsFailed);
    }

    /* Clean up */
    printf("\nCleaning up...\n");
    AEManagerCleanup();

    printf("Apple Event Manager Example completed successfully\n");
    return 0;
}
