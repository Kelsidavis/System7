/*
 * File: SpeechManagerCore.c
 *
 * Contains: Core Speech Manager implementation for System 7.1 Portable
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This file implements the core Speech Manager functionality,
 *              providing the main API entry points and system integration.
 */

#include "../include/SpeechManager/SpeechManager.h"
#include "../include/SpeechManager/VoiceManager.h"
#include "../include/SpeechManager/SpeechChannels.h"
#include "../include/SpeechManager/TextToSpeech.h"
#include "../include/SpeechManager/SpeechSynthesis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ===== Global Variables ===== */

/* Speech Manager version */
#define kSpeechManagerVersion 0x01008000  /* Version 1.0.8 */

/* Global state structure */
typedef struct SpeechManagerGlobals {
    bool initialized;
    SpeechChannel defaultChannel;
    VoiceSpec defaultVoice;
    short totalVoices;
    bool systemWideActivity;
    time_t lastActivity;
} SpeechManagerGlobals;

static SpeechManagerGlobals gSpeechGlobals = {
    .initialized = false,
    .defaultChannel = NULL,
    .totalVoices = 0,
    .systemWideActivity = false,
    .lastActivity = 0
};

/* ===== Internal Functions ===== */

/*
 * InitializeSpeechManager
 * Initializes the Speech Manager subsystem
 */
static OSErr InitializeSpeechManager(void) {
    if (gSpeechGlobals.initialized) {
        return noErr;
    }

    /* Initialize voice manager */
    OSErr err = InitializeVoiceManager();
    if (err != noErr) {
        return err;
    }

    /* Initialize synthesis engine */
    err = InitializeSpeechSynthesis();
    if (err != noErr) {
        return err;
    }

    /* Count available voices */
    err = CountVoices(&gSpeechGlobals.totalVoices);
    if (err != noErr) {
        return err;
    }

    /* Set up default voice */
    if (gSpeechGlobals.totalVoices > 0) {
        err = GetIndVoice(1, &gSpeechGlobals.defaultVoice);
        if (err != noErr) {
            return err;
        }
    }

    gSpeechGlobals.initialized = true;
    return noErr;
}

/*
 * CleanupSpeechManager
 * Cleans up Speech Manager resources
 */
static void CleanupSpeechManager(void) {
    if (!gSpeechGlobals.initialized) {
        return;
    }

    /* Dispose default channel if it exists */
    if (gSpeechGlobals.defaultChannel) {
        DisposeSpeechChannel(gSpeechGlobals.defaultChannel);
        gSpeechGlobals.defaultChannel = NULL;
    }

    /* Clean up synthesis engine */
    CleanupSpeechSynthesis();

    /* Clean up voice manager */
    CleanupVoiceManager();

    gSpeechGlobals.initialized = false;
}

/*
 * EnsureDefaultChannel
 * Ensures the default channel exists for SpeakString
 */
static OSErr EnsureDefaultChannel(void) {
    if (gSpeechGlobals.defaultChannel) {
        return noErr;
    }

    OSErr err = NewSpeechChannel(&gSpeechGlobals.defaultVoice,
                                 &gSpeechGlobals.defaultChannel);
    return err;
}

/*
 * UpdateSystemActivity
 * Updates system-wide speech activity tracking
 */
static void UpdateSystemActivity(bool active) {
    gSpeechGlobals.systemWideActivity = active;
    if (active) {
        gSpeechGlobals.lastActivity = time(NULL);
    }
}

/* ===== Public API Implementation ===== */

/*
 * SpeechManagerVersion
 * Returns the version of the Speech Manager
 */
uint32_t SpeechManagerVersion(void) {
    return kSpeechManagerVersion;
}

/*
 * SpeakString
 * Speaks a Pascal string using the default voice
 */
OSErr SpeakString(StringPtr textString) {
    if (!textString || textString[0] == 0) {
        return paramErr;
    }

    /* Initialize if needed */
    OSErr err = InitializeSpeechManager();
    if (err != noErr) {
        return err;
    }

    /* Ensure default channel exists */
    err = EnsureDefaultChannel();
    if (err != noErr) {
        return err;
    }

    /* Convert Pascal string to C string */
    char cString[256];
    int len = textString[0];
    if (len > 255) len = 255;

    memcpy(cString, &textString[1], len);
    cString[len] = '\0';

    /* Speak the text */
    UpdateSystemActivity(true);
    err = SpeakText(gSpeechGlobals.defaultChannel, cString, len);

    return err;
}

/*
 * SpeechBusy
 * Returns whether any speech channel is currently active
 */
short SpeechBusy(void) {
    if (!gSpeechGlobals.initialized) {
        return 0;
    }

    return IsSpeechChannelBusy(gSpeechGlobals.defaultChannel) ? 1 : 0;
}

/*
 * SpeechBusySystemWide
 * Returns whether any speech is active system-wide
 */
short SpeechBusySystemWide(void) {
    if (!gSpeechGlobals.initialized) {
        return 0;
    }

    /* Check if any channels are busy */
    if (IsSpeechSystemBusy()) {
        return 1;
    }

    /* Check recent activity (within last 2 seconds) */
    time_t now = time(NULL);
    if (gSpeechGlobals.systemWideActivity &&
        (now - gSpeechGlobals.lastActivity) < 2) {
        return 1;
    }

    gSpeechGlobals.systemWideActivity = false;
    return 0;
}

/*
 * SetSpeechRate
 * Sets the speaking rate for a speech channel
 */
OSErr SetSpeechRate(SpeechChannel chan, Fixed rate) {
    if (!chan) {
        return paramErr;
    }

    if (!gSpeechGlobals.initialized) {
        OSErr err = InitializeSpeechManager();
        if (err != noErr) {
            return err;
        }
    }

    return SetSpeechChannelRate(chan, rate);
}

/*
 * GetSpeechRate
 * Gets the speaking rate for a speech channel
 */
OSErr GetSpeechRate(SpeechChannel chan, Fixed *rate) {
    if (!chan || !rate) {
        return paramErr;
    }

    if (!gSpeechGlobals.initialized) {
        OSErr err = InitializeSpeechManager();
        if (err != noErr) {
            return err;
        }
    }

    return GetSpeechChannelRate(chan, rate);
}

/*
 * SetSpeechPitch
 * Sets the speaking pitch for a speech channel
 */
OSErr SetSpeechPitch(SpeechChannel chan, Fixed pitch) {
    if (!chan) {
        return paramErr;
    }

    if (!gSpeechGlobals.initialized) {
        OSErr err = InitializeSpeechManager();
        if (err != noErr) {
            return err;
        }
    }

    return SetSpeechChannelPitch(chan, pitch);
}

/*
 * GetSpeechPitch
 * Gets the speaking pitch for a speech channel
 */
OSErr GetSpeechPitch(SpeechChannel chan, Fixed *pitch) {
    if (!chan || !pitch) {
        return paramErr;
    }

    if (!gSpeechGlobals.initialized) {
        OSErr err = InitializeSpeechManager();
        if (err != noErr) {
            return err;
        }
    }

    return GetSpeechChannelPitch(chan, pitch);
}

/*
 * SetSpeechInfo
 * Sets various speech parameters using selector codes
 */
OSErr SetSpeechInfo(SpeechChannel chan, OSType selector, void *speechInfo) {
    if (!chan || !speechInfo) {
        return paramErr;
    }

    if (!gSpeechGlobals.initialized) {
        OSErr err = InitializeSpeechManager();
        if (err != noErr) {
            return err;
        }
    }

    return SetSpeechChannelInfo(chan, selector, speechInfo);
}

/*
 * GetSpeechInfo
 * Gets various speech parameters using selector codes
 */
OSErr GetSpeechInfo(SpeechChannel chan, OSType selector, void *speechInfo) {
    if (!chan || !speechInfo) {
        return paramErr;
    }

    if (!gSpeechGlobals.initialized) {
        OSErr err = InitializeSpeechManager();
        if (err != noErr) {
            return err;
        }
    }

    return GetSpeechChannelInfo(chan, selector, speechInfo);
}

/*
 * UseDictionary
 * Associates a pronunciation dictionary with a speech channel
 */
OSErr UseDictionary(SpeechChannel chan, void *dictionary) {
    if (!chan) {
        return paramErr;
    }

    if (!gSpeechGlobals.initialized) {
        OSErr err = InitializeSpeechManager();
        if (err != noErr) {
            return err;
        }
    }

    return SetSpeechChannelDictionary(chan, dictionary);
}

/* ===== Module Cleanup ===== */

/*
 * Module cleanup function (called at exit)
 */
static void __attribute__((destructor)) SpeechManagerCleanup(void) {
    CleanupSpeechManager();
}

/*
 * Module initialization function (called at load)
 */
static void __attribute__((constructor)) SpeechManagerInit(void) {
    /* Initialize on first use rather than at load time */
}