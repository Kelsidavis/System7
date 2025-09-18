/*
 * File: VoiceManager.c
 *
 * Contains: Voice selection and management implementation
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This file implements voice enumeration, selection,
 *              and management functionality for the Speech Manager.
 */

#include "../include/SpeechManager/VoiceManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

/* ===== Voice Management Globals ===== */

/* Maximum number of voices supported */
#define MAX_VOICES 256
#define MAX_VOICE_NAME 64
#define MAX_PATH_LEN 1024

/* Voice database structure */
typedef struct VoiceEntry {
    VoiceSpec spec;
    VoiceDescription description;
    VoiceInfoExtended extended;
    VoiceFlags flags;
    char filePath[MAX_PATH_LEN];
    bool loaded;
    void *resourceData;
    time_t lastModified;
} VoiceEntry;

/* Voice manager state */
typedef struct VoiceManagerState {
    bool initialized;
    VoiceEntry voices[MAX_VOICES];
    short voiceCount;
    VoiceSpec defaultVoice;
    VoiceNotificationProc notificationCallback;
    void *notificationUserData;
    time_t lastScan;
} VoiceManagerState;

static VoiceManagerState gVoiceManager = {
    .initialized = false,
    .voiceCount = 0,
    .notificationCallback = NULL,
    .notificationUserData = NULL,
    .lastScan = 0
};

/* Built-in voices for basic functionality */
static const VoiceEntry kBuiltinVoices[] = {
    {
        .spec = { .creator = 0x61706C65, .id = 0x64656676 }, /* 'appl' 'defv' */
        .description = {
            .length = sizeof(VoiceDescription),
            .voice = { .creator = 0x61706C65, .id = 0x64656676 },
            .version = 0x01000000,
            .name = "\pDefault Voice",
            .comment = "\pBuilt-in system voice for basic speech synthesis",
            .gender = kNeuter,
            .age = 30,
            .script = 0,
            .language = 0,
            .region = 0,
            .reserved = {0, 0, 0, 0}
        },
        .extended = {
            .spec = { .creator = 0x61706C65, .id = 0x64656676 },
            .flags = kVoiceFlag_Available | kVoiceFlag_System | kVoiceFlag_Synthesis,
            .quality = kVoiceQuality_Medium,
            .sampleRate = 22050,
            .channels = 1,
            .bitDepth = 16,
            .dataSize = 0,
            .manufacturer = "System 7.1 Portable",
            .copyright = "Copyright Apple Computer, Inc.",
            .description = "Built-in system voice for speech synthesis",
            .reserved = {NULL, NULL, NULL, NULL}
        },
        .flags = kVoiceFlag_Available | kVoiceFlag_System,
        .filePath = "",
        .loaded = true,
        .resourceData = NULL,
        .lastModified = 0
    }
};

#define BUILTIN_VOICE_COUNT (sizeof(kBuiltinVoices) / sizeof(VoiceEntry))

/* ===== Internal Functions ===== */

/*
 * NotifyVoiceChange
 * Sends notification of voice changes
 */
static void NotifyVoiceChange(VoiceNotificationType type, const VoiceSpec *voice) {
    if (gVoiceManager.notificationCallback) {
        gVoiceManager.notificationCallback(type, voice, gVoiceManager.notificationUserData);
    }
}

/*
 * FindVoiceEntry
 * Finds a voice entry by voice spec
 */
static VoiceEntry *FindVoiceEntry(const VoiceSpec *voice) {
    if (!voice) return NULL;

    for (short i = 0; i < gVoiceManager.voiceCount; i++) {
        VoiceEntry *entry = &gVoiceManager.voices[i];
        if (entry->spec.creator == voice->creator && entry->spec.id == voice->id) {
            return entry;
        }
    }
    return NULL;
}

/*
 * AddVoiceEntry
 * Adds a new voice entry to the database
 */
static OSErr AddVoiceEntry(const VoiceEntry *newEntry) {
    if (gVoiceManager.voiceCount >= MAX_VOICES) {
        return memFullErr;
    }

    /* Check for duplicates */
    if (FindVoiceEntry(&newEntry->spec)) {
        return noErr; /* Already exists */
    }

    /* Add the new entry */
    gVoiceManager.voices[gVoiceManager.voiceCount] = *newEntry;
    gVoiceManager.voiceCount++;

    NotifyVoiceChange(kVoiceNotification_Added, &newEntry->spec);
    return noErr;
}

/*
 * RemoveVoiceEntry
 * Removes a voice entry from the database
 */
static OSErr RemoveVoiceEntry(const VoiceSpec *voice) {
    for (short i = 0; i < gVoiceManager.voiceCount; i++) {
        VoiceEntry *entry = &gVoiceManager.voices[i];
        if (entry->spec.creator == voice->creator && entry->spec.id == voice->id) {
            /* Free any allocated resources */
            if (entry->resourceData) {
                free(entry->resourceData);
            }

            /* Shift remaining entries */
            memmove(&gVoiceManager.voices[i], &gVoiceManager.voices[i + 1],
                    (gVoiceManager.voiceCount - i - 1) * sizeof(VoiceEntry));
            gVoiceManager.voiceCount--;

            NotifyVoiceChange(kVoiceNotification_Removed, voice);
            return noErr;
        }
    }
    return voiceNotFound;
}

/*
 * ScanSystemVoices
 * Scans system directories for voices
 */
static OSErr ScanSystemVoices(void) {
    /* This would scan platform-specific voice directories */
    /* For now, we'll use built-in voices */
    for (int i = 0; i < BUILTIN_VOICE_COUNT; i++) {
        AddVoiceEntry(&kBuiltinVoices[i]);
    }
    return noErr;
}

/*
 * LoadVoiceFromFile
 * Loads voice data from a file
 */
static OSErr LoadVoiceFromFile(const char *filePath, VoiceEntry *entry) {
    FILE *file = fopen(filePath, "rb");
    if (!file) {
        return resNotFound;
    }

    /* Get file size */
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* For now, just mark as loaded */
    /* In a real implementation, this would parse voice file format */
    strncpy(entry->filePath, filePath, MAX_PATH_LEN - 1);
    entry->filePath[MAX_PATH_LEN - 1] = '\0';
    entry->loaded = true;
    entry->extended.dataSize = fileSize;

    fclose(file);
    return noErr;
}

/* ===== Public API Implementation ===== */

/*
 * InitializeVoiceManager
 * Initializes the voice management system
 */
OSErr InitializeVoiceManager(void) {
    if (gVoiceManager.initialized) {
        return noErr;
    }

    /* Initialize state */
    memset(&gVoiceManager, 0, sizeof(VoiceManagerState));
    gVoiceManager.voiceCount = 0;

    /* Scan for available voices */
    OSErr err = ScanSystemVoices();
    if (err != noErr) {
        return err;
    }

    /* Set default voice */
    if (gVoiceManager.voiceCount > 0) {
        gVoiceManager.defaultVoice = gVoiceManager.voices[0].spec;
    }

    gVoiceManager.initialized = true;
    gVoiceManager.lastScan = time(NULL);

    return noErr;
}

/*
 * CleanupVoiceManager
 * Cleans up voice management resources
 */
void CleanupVoiceManager(void) {
    if (!gVoiceManager.initialized) {
        return;
    }

    /* Free any allocated voice resources */
    for (short i = 0; i < gVoiceManager.voiceCount; i++) {
        VoiceEntry *entry = &gVoiceManager.voices[i];
        if (entry->resourceData) {
            free(entry->resourceData);
            entry->resourceData = NULL;
        }
    }

    gVoiceManager.initialized = false;
    gVoiceManager.voiceCount = 0;
}

/*
 * MakeVoiceSpec
 * Creates a voice specification from creator and ID
 */
OSErr MakeVoiceSpec(OSType creator, OSType id, VoiceSpec *voice) {
    if (!voice) {
        return paramErr;
    }

    voice->creator = creator;
    voice->id = id;
    return noErr;
}

/*
 * CountVoices
 * Returns the number of available voices
 */
OSErr CountVoices(short *numVoices) {
    if (!numVoices) {
        return paramErr;
    }

    if (!gVoiceManager.initialized) {
        OSErr err = InitializeVoiceManager();
        if (err != noErr) {
            return err;
        }
    }

    *numVoices = gVoiceManager.voiceCount;
    return noErr;
}

/*
 * GetIndVoice
 * Returns the voice specification for a given index
 */
OSErr GetIndVoice(short index, VoiceSpec *voice) {
    if (!voice || index < 1) {
        return paramErr;
    }

    if (!gVoiceManager.initialized) {
        OSErr err = InitializeVoiceManager();
        if (err != noErr) {
            return err;
        }
    }

    if (index > gVoiceManager.voiceCount) {
        return voiceNotFound;
    }

    *voice = gVoiceManager.voices[index - 1].spec;
    return noErr;
}

/*
 * GetVoiceDescription
 * Returns detailed information about a voice
 */
OSErr GetVoiceDescription(VoiceSpec *voice, VoiceDescription *info, long infoLength) {
    if (!voice || !info) {
        return paramErr;
    }

    if (!gVoiceManager.initialized) {
        OSErr err = InitializeVoiceManager();
        if (err != noErr) {
            return err;
        }
    }

    VoiceEntry *entry = FindVoiceEntry(voice);
    if (!entry) {
        return voiceNotFound;
    }

    /* Copy description, respecting the provided length */
    long copyLength = (infoLength < sizeof(VoiceDescription)) ? infoLength : sizeof(VoiceDescription);
    memcpy(info, &entry->description, copyLength);

    return noErr;
}

/*
 * GetVoiceInfo
 * Returns specific voice information based on selector
 */
OSErr GetVoiceInfo(VoiceSpec *voice, OSType selector, void *voiceInfo) {
    if (!voice || !voiceInfo) {
        return paramErr;
    }

    if (!gVoiceManager.initialized) {
        OSErr err = InitializeVoiceManager();
        if (err != noErr) {
            return err;
        }
    }

    VoiceEntry *entry = FindVoiceEntry(voice);
    if (!entry) {
        return voiceNotFound;
    }

    switch (selector) {
        case soVoiceDescription:
            *(VoiceDescription **)voiceInfo = &entry->description;
            break;

        case soVoiceFile:
            if (strlen(entry->filePath) > 0) {
                VoiceFileInfo *fileInfo = (VoiceFileInfo *)voiceInfo;
                fileInfo->fileSpec = (void *)entry->filePath;
                fileInfo->resID = 1; /* Default resource ID */
            } else {
                return resNotFound;
            }
            break;

        default:
            return paramErr;
    }

    return noErr;
}

/*
 * RefreshVoiceList
 * Rescans for available voices
 */
OSErr RefreshVoiceList(VoiceSearchFlags searchFlags) {
    if (!gVoiceManager.initialized) {
        OSErr err = InitializeVoiceManager();
        if (err != noErr) {
            return err;
        }
    }

    /* Clear existing non-builtin voices */
    short i = 0;
    while (i < gVoiceManager.voiceCount) {
        VoiceEntry *entry = &gVoiceManager.voices[i];
        if (!(entry->flags & kVoiceFlag_System)) {
            RemoveVoiceEntry(&entry->spec);
        } else {
            i++;
        }
    }

    /* Rescan based on search flags */
    if (searchFlags & kSearchSystemVoices) {
        ScanSystemVoices();
    }

    gVoiceManager.lastScan = time(NULL);
    return noErr;
}

/*
 * FindVoiceByName
 * Finds a voice by its name
 */
OSErr FindVoiceByName(const char *voiceName, VoiceSpec *voice) {
    if (!voiceName || !voice) {
        return paramErr;
    }

    if (!gVoiceManager.initialized) {
        OSErr err = InitializeVoiceManager();
        if (err != noErr) {
            return err;
        }
    }

    for (short i = 0; i < gVoiceManager.voiceCount; i++) {
        VoiceEntry *entry = &gVoiceManager.voices[i];

        /* Convert Pascal string to C string for comparison */
        char entryName[256];
        int nameLen = entry->description.name[0];
        memcpy(entryName, &entry->description.name[1], nameLen);
        entryName[nameLen] = '\0';

        if (strcmp(entryName, voiceName) == 0) {
            *voice = entry->spec;
            return noErr;
        }
    }

    return voiceNotFound;
}

/*
 * ValidateVoice
 * Validates that a voice specification is valid and available
 */
OSErr ValidateVoice(VoiceSpec *voice) {
    if (!voice) {
        return paramErr;
    }

    if (!gVoiceManager.initialized) {
        OSErr err = InitializeVoiceManager();
        if (err != noErr) {
            return err;
        }
    }

    VoiceEntry *entry = FindVoiceEntry(voice);
    return entry ? noErr : voiceNotFound;
}

/*
 * IsVoiceAvailable
 * Checks if a voice is currently available
 */
bool IsVoiceAvailable(VoiceSpec *voice) {
    return ValidateVoice(voice) == noErr;
}

/*
 * SetDefaultVoice
 * Sets the system default voice
 */
OSErr SetDefaultVoice(VoiceSpec *voice) {
    if (!voice) {
        return paramErr;
    }

    if (!gVoiceManager.initialized) {
        OSErr err = InitializeVoiceManager();
        if (err != noErr) {
            return err;
        }
    }

    OSErr err = ValidateVoice(voice);
    if (err != noErr) {
        return err;
    }

    gVoiceManager.defaultVoice = *voice;
    NotifyVoiceChange(kVoiceNotification_Default, voice);

    return noErr;
}

/*
 * GetDefaultVoice
 * Gets the system default voice
 */
OSErr GetDefaultVoice(VoiceSpec *voice) {
    if (!voice) {
        return paramErr;
    }

    if (!gVoiceManager.initialized) {
        OSErr err = InitializeVoiceManager();
        if (err != noErr) {
            return err;
        }
    }

    *voice = gVoiceManager.defaultVoice;
    return noErr;
}

/*
 * VoiceSpecsEqual
 * Compares two voice specifications for equality
 */
bool VoiceSpecsEqual(const VoiceSpec *voice1, const VoiceSpec *voice2) {
    if (!voice1 || !voice2) {
        return false;
    }

    return (voice1->creator == voice2->creator) && (voice1->id == voice2->id);
}

/*
 * RegisterVoiceNotification
 * Registers a callback for voice change notifications
 */
OSErr RegisterVoiceNotification(VoiceNotificationProc callback, void *userData) {
    if (!callback) {
        return paramErr;
    }

    gVoiceManager.notificationCallback = callback;
    gVoiceManager.notificationUserData = userData;

    return noErr;
}

/*
 * UnregisterVoiceNotification
 * Unregisters voice change notifications
 */
OSErr UnregisterVoiceNotification(VoiceNotificationProc callback) {
    if (gVoiceManager.notificationCallback == callback) {
        gVoiceManager.notificationCallback = NULL;
        gVoiceManager.notificationUserData = NULL;
    }

    return noErr;
}