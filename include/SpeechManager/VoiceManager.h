/*
 * File: VoiceManager.h
 *
 * Contains: Voice selection and management for Speech Manager
 *
 *
 * Description: This header provides voice management functionality
 *              including voice enumeration, selection, and properties.
 */

#ifndef _VOICEMANAGER_H_
#define _VOICEMANAGER_H_

#include "SystemTypes.h"

/* Forward declarations */


#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Voice Management Constants ===== */

/* Voice search locations */
#define kVoiceSearchLocal   0x0001
#define kVoiceSearchRemote  0x0002
#define kVoiceSearchAll     (kVoiceSearchLocal | kVoiceSearchRemote)

/* Voice flags */
#define kVoiceFlagAvailable 0x0001
#define kVoiceFlagActive    0x0002
#define kVoiceFlagDefault   0x0004

/* Voice quality levels */
#define kVoiceQualityLow    0
#define kVoiceQualityMedium 1
#define kVoiceQualityHigh   2

/* ===== Voice Type Definitions ===== */

/* Voice search flags */
typedef unsigned short VoiceSearchFlags;

/* Voice gender enumeration */
typedef enum {
    kVoiceGenderNeuter = 0,
    kVoiceGenderMale = 1,
    kVoiceGenderFemale = 2,
    kVoiceGenderUnknown = 3
} VoiceGender;

/* Extended voice information structure */
typedef struct {
    VoiceSpec voiceSpec;
    Str31 voiceName;
    Str31 languageName;
    VoiceGender gender;
    short quality;
    short priority;
    short language;
    short region;
    long capabilities;
    long intonation;
    long modulation;
    long duration;
} VoiceInfoExtended;

/* Voice enumeration callback - returns true to continue, false to stop */
typedef Boolean (*VoiceEnumerationProc)(VoiceSpec *voice, void *userData);

/* Voice filter callback - returns true if voice matches filter */
typedef Boolean (*VoiceFilterProc)(VoiceSpec *voice, void *filterData);

/* Voice notification callback */
typedef void (*VoiceNotificationProc)(VoiceSpec *voice, long notificationType, void *userData);

/* ===== Extended Voice Structures ===== */

/* Extended voice information already defined above */

/* Voice enumeration callback already defined above */

/* ===== Voice Management Functions ===== */

/* Initialization and cleanup */
OSErr InitializeVoiceManager(void);
void CleanupVoiceManager(void);


/* Voice filtering */


/* ===== Voice Manager Notifications ===== */

/* Voice change notification types */

/* Voice notification callback */


/* ===== Utility Functions ===== */


#ifdef __cplusplus
}
#endif

#endif /* _VOICEMANAGER_H_ */
