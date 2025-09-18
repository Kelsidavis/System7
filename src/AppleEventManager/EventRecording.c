/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */
/*
 * EventRecording.c
 *
 * Apple Event recording and playback functionality
 * Used for scripting and macro recording
 *
 * Based on Mac OS 7.1 Apple Event Manager
 */

#include "AppleEventManager/AppleEventManager.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* ========================================================================
 * Recording State Structure
 * ======================================================================== */

#define MAX_RECORDED_EVENTS 1000
#define MAX_SCRIPT_NAME 256

typedef struct RecordedEvent {
    AppleEvent event;
    AppleEvent reply;
    time_t timestamp;
    ProcessSerialNumber targetPSN;
    Boolean hasReply;
} RecordedEvent;

typedef struct RecordingSession {
    RecordedEvent* events;
    int eventCount;
    int maxEvents;
    Boolean isRecording;
    Boolean isPaused;
    char scriptName[MAX_SCRIPT_NAME];
    time_t startTime;
    time_t endTime;
} RecordingSession;

static RecordingSession g_recordingSession = {0};
static pthread_mutex_t g_recordingMutex = PTHREAD_MUTEX_INITIALIZER;

/* ========================================================================
 * Recording Control Functions
 * ======================================================================== */

OSErr AEStartRecording(const char* scriptName) {
    pthread_mutex_lock(&g_recordingMutex);

    if (g_recordingSession.isRecording) {
        pthread_mutex_unlock(&g_recordingMutex);
        return errAERecordingIsAlreadyOn;
    }

    /* Initialize recording session */
    g_recordingSession.events = calloc(MAX_RECORDED_EVENTS, sizeof(RecordedEvent));
    if (!g_recordingSession.events) {
        pthread_mutex_unlock(&g_recordingMutex);
        return memFullErr;
    }

    g_recordingSession.eventCount = 0;
    g_recordingSession.maxEvents = MAX_RECORDED_EVENTS;
    g_recordingSession.isRecording = true;
    g_recordingSession.isPaused = false;
    g_recordingSession.startTime = time(NULL);
    g_recordingSession.endTime = 0;

    if (scriptName) {
        strncpy(g_recordingSession.scriptName, scriptName, MAX_SCRIPT_NAME - 1);
        g_recordingSession.scriptName[MAX_SCRIPT_NAME - 1] = '\0';
    } else {
        strcpy(g_recordingSession.scriptName, "Untitled Script");
    }

    pthread_mutex_unlock(&g_recordingMutex);
    return noErr;
}

OSErr AEStopRecording(void) {
    pthread_mutex_lock(&g_recordingMutex);

    if (!g_recordingSession.isRecording) {
        pthread_mutex_unlock(&g_recordingMutex);
        return errAENotRecording;
    }

    g_recordingSession.isRecording = false;
    g_recordingSession.endTime = time(NULL);

    pthread_mutex_unlock(&g_recordingMutex);
    return noErr;
}

OSErr AEPauseRecording(void) {
    pthread_mutex_lock(&g_recordingMutex);

    if (!g_recordingSession.isRecording) {
        pthread_mutex_unlock(&g_recordingMutex);
        return errAENotRecording;
    }

    g_recordingSession.isPaused = true;

    pthread_mutex_unlock(&g_recordingMutex);
    return noErr;
}

OSErr AEResumeRecording(void) {
    pthread_mutex_lock(&g_recordingMutex);

    if (!g_recordingSession.isRecording) {
        pthread_mutex_unlock(&g_recordingMutex);
        return errAENotRecording;
    }

    g_recordingSession.isPaused = false;

    pthread_mutex_unlock(&g_recordingMutex);
    return noErr;
}

Boolean AEIsRecording(void) {
    Boolean result;

    pthread_mutex_lock(&g_recordingMutex);
    result = g_recordingSession.isRecording && !g_recordingSession.isPaused;
    pthread_mutex_unlock(&g_recordingMutex);

    return result;
}

/* ========================================================================
 * Event Recording Functions
 * ======================================================================== */

OSErr AERecordAppleEvent(const AppleEvent* theEvent, const AppleEvent* reply, const ProcessSerialNumber* targetPSN) {
    if (!theEvent) return errAENotAEDesc;

    pthread_mutex_lock(&g_recordingMutex);

    if (!g_recordingSession.isRecording || g_recordingSession.isPaused) {
        pthread_mutex_unlock(&g_recordingMutex);
        return noErr;  /* Not an error, just not recording */
    }

    if (g_recordingSession.eventCount >= g_recordingSession.maxEvents) {
        pthread_mutex_unlock(&g_recordingMutex);
        return errAERecordingBufferFull;
    }

    /* Record the event */
    RecordedEvent* recorded = &g_recordingSession.events[g_recordingSession.eventCount];

    /* Duplicate the event */
    OSErr err = AEDuplicateDesc(theEvent, &recorded->event);
    if (err != noErr) {
        pthread_mutex_unlock(&g_recordingMutex);
        return err;
    }

    /* Duplicate the reply if present */
    if (reply) {
        err = AEDuplicateDesc(reply, &recorded->reply);
        if (err != noErr) {
            AEDisposeDesc(&recorded->event);
            pthread_mutex_unlock(&g_recordingMutex);
            return err;
        }
        recorded->hasReply = true;
    } else {
        recorded->hasReply = false;
    }

    /* Record metadata */
    recorded->timestamp = time(NULL);
    if (targetPSN) {
        recorded->targetPSN = *targetPSN;
    } else {
        memset(&recorded->targetPSN, 0, sizeof(ProcessSerialNumber));
    }

    g_recordingSession.eventCount++;

    pthread_mutex_unlock(&g_recordingMutex);
    return noErr;
}

/* ========================================================================
 * Script Generation Functions
 * ======================================================================== */

OSErr AEGenerateScriptFromRecording(char** scriptText, Size* scriptSize) {
    if (!scriptText || !scriptSize) return paramErr;

    pthread_mutex_lock(&g_recordingMutex);

    if (g_recordingSession.eventCount == 0) {
        pthread_mutex_unlock(&g_recordingMutex);
        return errAENoUserSelection;
    }

    /* Estimate script size */
    Size estimatedSize = 1024;  /* Header */
    estimatedSize += g_recordingSession.eventCount * 512;  /* Estimate per event */

    *scriptText = malloc(estimatedSize);
    if (!*scriptText) {
        pthread_mutex_unlock(&g_recordingMutex);
        return memFullErr;
    }

    /* Generate script header */
    char* current = *scriptText;
    int written = sprintf(current, "-- AppleScript generated from recording\n");
    written += sprintf(current + written, "-- Script: %s\n", g_recordingSession.scriptName);
    written += sprintf(current + written, "-- Events recorded: %d\n\n", g_recordingSession.eventCount);

    /* Generate script for each event */
    for (int i = 0; i < g_recordingSession.eventCount; i++) {
        RecordedEvent* recorded = &g_recordingSession.events[i];

        /* Get event class and ID */
        AEEventClass eventClass;
        AEEventID eventID;
        Size actualSize;

        OSErr err = AEGetKeyPtr(&recorded->event, keyEventClassAttr, typeType,
                               NULL, &eventClass, sizeof(eventClass), &actualSize);
        if (err != noErr) continue;

        err = AEGetKeyPtr(&recorded->event, keyEventIDAttr, typeType,
                         NULL, &eventID, sizeof(eventID), &actualSize);
        if (err != noErr) continue;

        /* Generate AppleScript command based on event type */
        if (eventClass == kCoreEventClass) {
            if (eventID == kAEOpenApplication) {
                written += sprintf(current + written, "tell application \"Application\"\n");
                written += sprintf(current + written, "    activate\n");
                written += sprintf(current + written, "end tell\n\n");
            } else if (eventID == kAEOpenDocuments) {
                written += sprintf(current + written, "tell application \"Application\"\n");
                written += sprintf(current + written, "    open {file \"Document\"}\n");
                written += sprintf(current + written, "end tell\n\n");
            } else if (eventID == kAEPrintDocuments) {
                written += sprintf(current + written, "tell application \"Application\"\n");
                written += sprintf(current + written, "    print {file \"Document\"}\n");
                written += sprintf(current + written, "end tell\n\n");
            } else if (eventID == kAEQuitApplication) {
                written += sprintf(current + written, "tell application \"Application\"\n");
                written += sprintf(current + written, "    quit\n");
                written += sprintf(current + written, "end tell\n\n");
            }
        } else {
            /* Generic event */
            written += sprintf(current + written, "-- Event Class: '%.4s', Event ID: '%.4s'\n",
                             (char*)&eventClass, (char*)&eventID);
            written += sprintf(current + written, "-- (Custom event - manual translation required)\n\n");
        }
    }

    *scriptSize = written;

    pthread_mutex_unlock(&g_recordingMutex);
    return noErr;
}

/* ========================================================================
 * Playback Functions
 * ======================================================================== */

OSErr AEPlaybackRecording(int startIndex, int endIndex) {
    pthread_mutex_lock(&g_recordingMutex);

    if (g_recordingSession.eventCount == 0) {
        pthread_mutex_unlock(&g_recordingMutex);
        return errAENoUserSelection;
    }

    if (startIndex < 0) startIndex = 0;
    if (endIndex < 0 || endIndex >= g_recordingSession.eventCount) {
        endIndex = g_recordingSession.eventCount - 1;
    }

    if (startIndex > endIndex) {
        pthread_mutex_unlock(&g_recordingMutex);
        return paramErr;
    }

    /* Playback each event */
    for (int i = startIndex; i <= endIndex; i++) {
        RecordedEvent* recorded = &g_recordingSession.events[i];
        AppleEvent reply;

        /* Send the event */
        OSErr err = AESendMessage(&recorded->event, &reply, kAEWaitReply, kAEDefaultTimeout);

        if (err != noErr) {
            pthread_mutex_unlock(&g_recordingMutex);
            return err;
        }

        AEDisposeDesc(&reply);
    }

    pthread_mutex_unlock(&g_recordingMutex);
    return noErr;
}

/* ========================================================================
 * Recording Management Functions
 * ======================================================================== */

OSErr AESaveRecording(const char* filePath) {
    if (!filePath) return paramErr;

    pthread_mutex_lock(&g_recordingMutex);

    FILE* file = fopen(filePath, "wb");
    if (!file) {
        pthread_mutex_unlock(&g_recordingMutex);
        return ioErr;
    }

    /* Write header */
    fwrite(&g_recordingSession.eventCount, sizeof(int), 1, file);
    fwrite(g_recordingSession.scriptName, sizeof(char), MAX_SCRIPT_NAME, file);
    fwrite(&g_recordingSession.startTime, sizeof(time_t), 1, file);
    fwrite(&g_recordingSession.endTime, sizeof(time_t), 1, file);

    /* Write events */
    for (int i = 0; i < g_recordingSession.eventCount; i++) {
        RecordedEvent* recorded = &g_recordingSession.events[i];

        /* Write event metadata */
        fwrite(&recorded->timestamp, sizeof(time_t), 1, file);
        fwrite(&recorded->targetPSN, sizeof(ProcessSerialNumber), 1, file);
        fwrite(&recorded->hasReply, sizeof(Boolean), 1, file);

        /* Write event descriptor */
        Size eventSize = recorded->event.dataHandle ? AEGetHandleSize(recorded->event.dataHandle) : 0;
        fwrite(&recorded->event.descriptorType, sizeof(DescType), 1, file);
        fwrite(&eventSize, sizeof(Size), 1, file);
        if (eventSize > 0) {
            fwrite(AEGetHandleData(recorded->event.dataHandle), 1, eventSize, file);
        }

        /* Write reply descriptor if present */
        if (recorded->hasReply) {
            Size replySize = recorded->reply.dataHandle ? AEGetHandleSize(recorded->reply.dataHandle) : 0;
            fwrite(&recorded->reply.descriptorType, sizeof(DescType), 1, file);
            fwrite(&replySize, sizeof(Size), 1, file);
            if (replySize > 0) {
                fwrite(AEGetHandleData(recorded->reply.dataHandle), 1, replySize, file);
            }
        }
    }

    fclose(file);

    pthread_mutex_unlock(&g_recordingMutex);
    return noErr;
}

OSErr AELoadRecording(const char* filePath) {
    if (!filePath) return paramErr;

    pthread_mutex_lock(&g_recordingMutex);

    /* Clear existing recording */
    AEClearRecording();

    FILE* file = fopen(filePath, "rb");
    if (!file) {
        pthread_mutex_unlock(&g_recordingMutex);
        return fnfErr;
    }

    /* Read header */
    int eventCount;
    fread(&eventCount, sizeof(int), 1, file);
    fread(g_recordingSession.scriptName, sizeof(char), MAX_SCRIPT_NAME, file);
    fread(&g_recordingSession.startTime, sizeof(time_t), 1, file);
    fread(&g_recordingSession.endTime, sizeof(time_t), 1, file);

    /* Allocate events */
    g_recordingSession.events = calloc(eventCount, sizeof(RecordedEvent));
    if (!g_recordingSession.events) {
        fclose(file);
        pthread_mutex_unlock(&g_recordingMutex);
        return memFullErr;
    }

    g_recordingSession.maxEvents = eventCount;

    /* Read events */
    for (int i = 0; i < eventCount; i++) {
        RecordedEvent* recorded = &g_recordingSession.events[i];

        /* Read event metadata */
        fread(&recorded->timestamp, sizeof(time_t), 1, file);
        fread(&recorded->targetPSN, sizeof(ProcessSerialNumber), 1, file);
        fread(&recorded->hasReply, sizeof(Boolean), 1, file);

        /* Read event descriptor */
        DescType eventType;
        Size eventSize;
        fread(&eventType, sizeof(DescType), 1, file);
        fread(&eventSize, sizeof(Size), 1, file);

        if (eventSize > 0) {
            void* eventData = malloc(eventSize);
            if (!eventData) {
                fclose(file);
                pthread_mutex_unlock(&g_recordingMutex);
                return memFullErr;
            }
            fread(eventData, 1, eventSize, file);
            AECreateDesc(eventType, eventData, eventSize, &recorded->event);
            free(eventData);
        }

        /* Read reply descriptor if present */
        if (recorded->hasReply) {
            DescType replyType;
            Size replySize;
            fread(&replyType, sizeof(DescType), 1, file);
            fread(&replySize, sizeof(Size), 1, file);

            if (replySize > 0) {
                void* replyData = malloc(replySize);
                if (!replyData) {
                    fclose(file);
                    pthread_mutex_unlock(&g_recordingMutex);
                    return memFullErr;
                }
                fread(replyData, 1, replySize, file);
                AECreateDesc(replyType, replyData, replySize, &recorded->reply);
                free(replyData);
            }
        }

        g_recordingSession.eventCount++;
    }

    fclose(file);

    pthread_mutex_unlock(&g_recordingMutex);
    return noErr;
}

void AEClearRecording(void) {
    pthread_mutex_lock(&g_recordingMutex);

    if (g_recordingSession.events) {
        /* Dispose all recorded events */
        for (int i = 0; i < g_recordingSession.eventCount; i++) {
            RecordedEvent* recorded = &g_recordingSession.events[i];
            AEDisposeDesc(&recorded->event);
            if (recorded->hasReply) {
                AEDisposeDesc(&recorded->reply);
            }
        }

        free(g_recordingSession.events);
        g_recordingSession.events = NULL;
    }

    g_recordingSession.eventCount = 0;
    g_recordingSession.maxEvents = 0;
    g_recordingSession.isRecording = false;
    g_recordingSession.isPaused = false;

    pthread_mutex_unlock(&g_recordingMutex);
}