/*
 * File: VoiceManager.h
 *
 * Contains: Voice selection and management for Speech Manager
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager
 *
 * Description: This header provides voice management functionality
 *              including voice enumeration, selection, and properties.
 */

#ifndef _VOICEMANAGER_H_
#define _VOICEMANAGER_H_

#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Voice Management Constants ===== */

/* Voice search locations */
typedef enum {
    kSearchSystemVoices      = (1 << 0),  /* Search System folder */
    kSearchExtensionVoices   = (1 << 1),  /* Search Extensions folder */
    kSearchApplicationVoices = (1 << 2),  /* Search application bundle */
    kSearchUserVoices        = (1 << 3),  /* Search user preferences */
    kSearchAllVoices         = 0x0F       /* Search all locations */
} VoiceSearchFlags;

/* Voice flags */
typedef enum {
    kVoiceFlag_Available     = (1 << 0),  /* Voice is available */
    kVoiceFlag_System        = (1 << 1),  /* System-provided voice */
    kVoiceFlag_Extension     = (1 << 2),  /* Extension voice */
    kVoiceFlag_Application   = (1 << 3),  /* Application voice */
    kVoiceFlag_User          = (1 << 4),  /* User-installed voice */
    kVoiceFlag_Embedded      = (1 << 5),  /* Embedded voice data */
    kVoiceFlag_Network       = (1 << 6),  /* Network-based voice */
    kVoiceFlag_Synthesis     = (1 << 7)   /* Synthesis voice */
} VoiceFlags;

/* Voice quality levels */
typedef enum {
    kVoiceQuality_Low        = 1,
    kVoiceQuality_Medium     = 2,
    kVoiceQuality_High       = 3,
    kVoiceQuality_Premium    = 4
} VoiceQuality;

/* ===== Extended Voice Structures ===== */

/* Extended voice information */
typedef struct VoiceInfoExtended {
    VoiceSpec spec;             /* Basic voice specification */
    VoiceFlags flags;           /* Voice capability flags */
    VoiceQuality quality;       /* Voice quality level */
    long sampleRate;            /* Preferred sample rate */
    short channels;             /* Audio channels (1=mono, 2=stereo) */
    short bitDepth;             /* Audio bit depth */
    long dataSize;              /* Voice data size in bytes */
    char *manufacturer;         /* Voice manufacturer name */
    char *copyright;            /* Copyright information */
    char *description;          /* Detailed description */
    void *reserved[4];          /* Reserved for future use */
} VoiceInfoExtended;

/* Voice enumeration callback */
typedef bool (*VoiceEnumerationProc)(const VoiceSpec *voice,
                                      const VoiceDescription *desc,
                                      void *userData);

/* ===== Voice Management Functions ===== */

/* Initialization and cleanup */
OSErr InitializeVoiceManager(void);
void CleanupVoiceManager(void);

/* Voice enumeration and search */
OSErr RefreshVoiceList(VoiceSearchFlags searchFlags);
OSErr EnumerateVoices(VoiceEnumerationProc callback, void *userData);
OSErr FindVoiceByName(const char *voiceName, VoiceSpec *voice);
OSErr FindVoiceByLanguage(short language, short region, VoiceSpec *voice);
OSErr FindVoiceByGender(VoiceGender gender, VoiceSpec *voice);
OSErr FindBestVoice(const VoiceDescription *criteria, VoiceSpec *voice);

/* Voice information */
OSErr GetVoiceInfoExtended(VoiceSpec *voice, VoiceInfoExtended *info);
OSErr GetVoiceCapabilities(VoiceSpec *voice, long *capabilities);
OSErr GetVoiceLanguages(VoiceSpec *voice, short *languages, short *count);

/* Voice validation */
OSErr ValidateVoice(VoiceSpec *voice);
bool IsVoiceAvailable(VoiceSpec *voice);
OSErr GetVoiceAvailability(VoiceSpec *voice, bool *available, char **reason);

/* Default voice management */
OSErr SetDefaultVoice(VoiceSpec *voice);
OSErr GetDefaultVoice(VoiceSpec *voice);
OSErr ResetToSystemDefaultVoice(void);

/* Voice installation and removal */
OSErr InstallVoice(const char *voicePath, VoiceSpec *installedVoice);
OSErr RemoveVoice(VoiceSpec *voice);
OSErr GetVoiceInstallPath(VoiceSpec *voice, char *path, long pathSize);

/* Voice preferences */
OSErr SaveVoicePreferences(VoiceSpec *voice, const char *prefName);
OSErr LoadVoicePreferences(const char *prefName, VoiceSpec *voice);
OSErr DeleteVoicePreferences(const char *prefName);

/* Voice comparison and sorting */
int CompareVoices(const VoiceSpec *voice1, const VoiceSpec *voice2);
OSErr SortVoiceList(VoiceSpec *voices, short count, int (*compareFunc)(const VoiceSpec *, const VoiceSpec *));

/* Voice resource management */
OSErr LoadVoiceResources(VoiceSpec *voice);
OSErr UnloadVoiceResources(VoiceSpec *voice);
OSErr GetVoiceResourceInfo(VoiceSpec *voice, OSType resourceType, void **resourceData, long *resourceSize);

/* Voice testing */
OSErr TestVoice(VoiceSpec *voice, const char *testText);
OSErr GetVoiceTestResults(VoiceSpec *voice, bool *success, char **errorMessage);

/* Voice filtering */
typedef bool (*VoiceFilterProc)(const VoiceSpec *voice, const VoiceDescription *desc, void *filterData);
OSErr FilterVoices(VoiceFilterProc filter, void *filterData, VoiceSpec **filteredVoices, short *count);

/* Voice groups and categories */
OSErr GetVoiceCategories(char ***categories, short *categoryCount);
OSErr GetVoicesInCategory(const char *category, VoiceSpec **voices, short *count);
OSErr SetVoiceCategory(VoiceSpec *voice, const char *category);

/* Voice compatibility */
OSErr CheckVoiceCompatibility(VoiceSpec *voice, long systemVersion, bool *compatible);
OSErr GetMinimumSystemVersion(VoiceSpec *voice, long *minVersion);

/* Voice licensing */
OSErr GetVoiceLicense(VoiceSpec *voice, char **licenseText);
OSErr AcceptVoiceLicense(VoiceSpec *voice, bool accept);
bool IsVoiceLicenseAccepted(VoiceSpec *voice);

/* ===== Voice Manager Notifications ===== */

/* Voice change notification types */
typedef enum {
    kVoiceNotification_Added    = 1,
    kVoiceNotification_Removed  = 2,
    kVoiceNotification_Modified = 3,
    kVoiceNotification_Default  = 4
} VoiceNotificationType;

/* Voice notification callback */
typedef void (*VoiceNotificationProc)(VoiceNotificationType type,
                                       const VoiceSpec *voice,
                                       void *userData);

/* Notification registration */
OSErr RegisterVoiceNotification(VoiceNotificationProc callback, void *userData);
OSErr UnregisterVoiceNotification(VoiceNotificationProc callback);

/* ===== Utility Functions ===== */

/* Voice spec utilities */
bool VoiceSpecsEqual(const VoiceSpec *voice1, const VoiceSpec *voice2);
OSErr CopyVoiceSpec(const VoiceSpec *source, VoiceSpec *dest);
OSErr VoiceSpecToString(const VoiceSpec *voice, char *string, long stringSize);
OSErr StringToVoiceSpec(const char *string, VoiceSpec *voice);

/* Voice description utilities */
OSErr CopyVoiceDescription(const VoiceDescription *source, VoiceDescription *dest);
void FreeVoiceDescription(VoiceDescription *desc);

/* Voice list utilities */
OSErr AllocateVoiceList(short count, VoiceSpec **voices);
void FreeVoiceList(VoiceSpec *voices);
OSErr DuplicateVoiceList(const VoiceSpec *source, short count, VoiceSpec **dest);

#ifdef __cplusplus
}
#endif

#endif /* _VOICEMANAGER_H_ */