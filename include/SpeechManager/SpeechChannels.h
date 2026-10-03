/*
 * File: SpeechChannels.h
 *
 * Contains: Speech channel management and control for Speech Manager
 *
 * Written by: Claude Code (Portable Implementation)
 *
 *
 * Description: This header provides speech channel functionality
 *              including channel lifecycle, properties, and control.
 */

#ifndef _SPEECHCHANNELS_H_
#define _SPEECHCHANNELS_H_

#include "SystemTypes.h"
#include <stdio.h>

/* Forward declarations */


#include "SpeechManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Speech Channel Constants ===== */

/* Channel states */
#define kChannelStateUnitialized 0
#define kChannelStateClosed      1
#define kChannelStateOpen        2
#define kChannelStateActive      3
#define kChannelStatePaused      4

/* Channel types */
#define kChannelTypeMonophonic   0
#define kChannelTypePolyphonic   1

/* Channel priorities */
#define kChannelPriorityMin      0
#define kChannelPriorityNormal   128
#define kChannelPriorityMax      255

/* Channel flags */
#define kChannelFlagNone         0x0000
#define kChannelFlagAsync        0x0001
#define kChannelFlagDoNotInterrupt 0x0002
#define kChannelFlagPreemptible  0x0004

/* ===== Speech Channel Type Definitions ===== */

/* Channel priority type */
typedef unsigned char SpeechChannelPriority;

/* Channel flags type */
typedef unsigned short SpeechChannelFlags;

/* Channel state structure */
typedef struct {
    short state;
    short type;
    SpeechChannelPriority priority;
    SpeechChannelFlags flags;
    Boolean busy;
    Boolean paused;
    long bytesProcessed;
    long bytesRemaining;
} SpeechChannelState;

/* Channel configuration structure */
typedef struct {
    VoiceSpec voice;
    Fixed rate;
    Fixed pitch;
    Fixed volume;
    SpeechChannelPriority priority;
    SpeechChannelFlags flags;
    short inputMode;
    short outputMode;
} SpeechChannelConfig;

/* Channel information structure */
typedef struct {
    SpeechChannelConfig config;
    SpeechChannelState state;
    long totalBytesProcessed;
    long totalBytesRemaining;
    long creationTime;
    long lastActivityTime;
} SpeechChannelInfo;

/* Channel statistics structure */
typedef struct {
    long bytesProcessed;
    long bytesRemaining;
    long timeSinceCreation;
    long timeActive;
    long timePaused;
    long synthesisCount;
    long errorCount;
} SpeechChannelStats;

/* Channel event callback */
typedef void (*SpeechChannelEventProc)(SpeechChannel chan, long eventType, void *eventData, void *userData);

/* ===== Speech Channel Structures ===== */

/* Channel information already defined above */

/* Channel configuration already defined above */

/* Channel statistics already defined above */

/* ===== Channel Management ===== */


/* ===== Channel Properties ===== */


/* ===== Channel Information and Control ===== */


/* ===== Channel Text Processing ===== */

/* ===== Channel Monitoring ===== */


/* Event monitoring */

/* Event callback */

OSErr SetSpeechChannelEventCallback(SpeechChannel chan, SpeechChannelEventProc callback,
                                    void *userData);

/* ===== System Channel Management ===== */


/* ===== Channel Resource Management ===== */


/* ===== Channel Threading ===== */


/* ===== Channel Configuration ===== */


/* Configuration validation */
OSErr ValidateChannelConfig(const SpeechChannelConfig *config, Boolean *isValid,
                            char **errorMessage);

/* ===== Channel Debugging ===== */


/* ===== Channel Utilities ===== */


#ifdef __cplusplus
}
#endif

#endif /* _SPEECHCHANNELS_H_ */
