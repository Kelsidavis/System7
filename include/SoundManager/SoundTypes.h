/*
 * SoundTypes.h - Sound Manager Data Types and Structures
 *
 * Defines data structures, constants, and type definitions used by the
 * portable Sound Manager. Compatibility coverage varies by type and API.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef _SOUNDTYPES_H_
#define _SOUNDTYPES_H_

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Four Character Code macro */
#define FOUR_CHAR_CODE(x) ((UInt32)(x))

/* UnsignedFixed type */
typedef UInt32 UnsignedFixed;

/* Sound channel handle types */
typedef struct SndListResource** SndListHandle;

/* Sound Channel Status */
typedef struct SCStatus {
    UnsignedFixed sampleRate;
    SInt16 sampleSize;
    SInt16 numChannels;
    UnsignedFixed synthType;
    UnsignedFixed init;
} SCStatus;

/* Sound Manager Status */
typedef struct SMStatus {
    SInt16 smMaxCPULoad;
    SInt16 smNumChannels;
    SInt16 smCurCPULoad;
} SMStatus;

/* Audio Selection Structure */
typedef struct AudioSelection {
    SInt32 unitType;
    UnsignedFixed selStart;
    UnsignedFixed selEnd;
} AudioSelection;
typedef AudioSelection* AudioSelectionPtr;

/* File Play Completion UPP */
typedef void (*FilePlayCompletionUPP)(SndChannelPtr chan);

/* Sound Completion UPP */
typedef void (*SoundCompletionUPP)(void);

/* Sound Callback Proc */
typedef void (*SndCallBackProcPtr)(SndChannelPtr chan, SndCommand* cmd);

/* Sound Parameter Block for Sound Input */
typedef struct SPB {
    SInt32 inRefNum;
    UInt32 count;
    UInt32 milliseconds;
    UInt32 bufferLength;
    Ptr bufferPtr;
    void* completionRoutine;
    void* interruptRoutine;
    SInt32 userLong;
    OSErr error;
    SInt32 unused1;
} SPB;
typedef SPB* SPBPtr;

/* Compression Information */
typedef struct CompressionInfo {
    SInt32 recordSize;
    OSType format;
    SInt16 compressionID;
    UInt16 samplesPerPacket;
    UInt16 bytesPerPacket;
    UInt16 bytesPerFrame;
    UInt16 bytesPerSample;
    UInt16 numChannels;
} CompressionInfo;
typedef CompressionInfo* CompressionInfoPtr;

/* MIDI Types */
typedef UInt32 MIDIPortDirectionFlags;
typedef struct MIDIPortParams {
    UInt32 flags;
    Ptr refCon;
} MIDIPortParams;

typedef struct MIDIPacketList* MIDIPacketListPtr;

/* Version information structure */
typedef struct NumVersion {
    UInt8 majorRev;
    UInt8 minorAndBugRev;
    UInt8 stage;
    UInt8 nonRelRev;
} NumVersion;

/* Fixed Point Utilities */
#define Fixed2Long(f)       ((SInt32)((f) >> 16))
#define Long2Fixed(l)       ((Fixed)((l) << 16))
#define Fixed2Frac(f)       ((SInt16)(f))
#define Frac2Fixed(fr)      ((Fixed)(fr))
#define FixedRound(f)       ((SInt16)(((f) + 0x00008000) >> 16))
#define Fixed2X(f)          ((f) >> 16)
#define X2Fixed(x)          ((x) << 16)

/* Rate conversion macros */
#define Rate22khz           0x56220000UL    /* 22.254 kHz */
#define Rate11khz           0x2B110000UL    /* 11.127 kHz */
#define Rate44khz           0xAC440000UL    /* 44.100 kHz */
#define Rate48khz           0xBB800000UL    /* 48.000 kHz */

#ifdef __cplusplus
}
#endif

#endif /* _SOUNDTYPES_H_ */
