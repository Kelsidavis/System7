/*
 * SoundTypes.h - Sound Manager Data Types and Structures
 *
 * Defines all data structures, constants, and type definitions used by
 * the Mac OS Sound Manager. This header provides complete compatibility
 * with the original Sound Manager data types.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef _SOUNDTYPES_H_
#define _SOUNDTYPES_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic Mac OS types compatibility */
typedef int16_t             OSErr;
typedef uint32_t            OSType;
typedef uint32_t            UnsignedFixed;
typedef int32_t             Fixed;
typedef uint8_t             Boolean;
typedef int16_t             SInt16;
typedef int32_t             SInt32;
typedef uint16_t            UInt16;
typedef uint32_t            UInt32;
typedef void*               Handle;
typedef unsigned char       Str255[256];

#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

/* Four Character Code macro */
#define FOUR_CHAR_CODE(x) ((uint32_t)(x))

/* Forward declarations */
struct SndChannel;
struct SndCommand;
struct SPB;

/* Sound channel pointer types */
typedef struct SndChannel*      SndChannelPtr;
typedef Handle                  SndListHandle;
typedef struct SPB*             SPBPtr;

/* Point structure (for recording dialogs) */
typedef struct Point {
    int16_t     v;                  /* Vertical coordinate */
    int16_t     h;                  /* Horizontal coordinate */
} Point;

/* Version information structure */
typedef struct NumVersion {
    uint8_t     majorRev;           /* Major revision */
    uint8_t     minorAndBugRev;     /* Minor and bug fix revision */
    uint8_t     stage;              /* Development stage */
    uint8_t     nonRelRev;          /* Non-release revision */
} NumVersion;

/* Sound Resource Formats */
typedef enum {
    soundListRsrc = 1,              /* Sound list resource */
    soundHeaderRsrc = 2             /* Sound header resource */
} SoundResourceType;

/* Sound header format types */
typedef enum {
    standardHeader = 0x00,          /* Standard sound header */
    extendedHeader = 0xFF,          /* Extended sound header */
    compressedHeader = 0xFE         /* Compressed sound header */
} SoundHeaderFormat;

/* Audio encoding types */
typedef enum {
    k8BitOffsetBinaryFormat = 0x00,     /* 8-bit offset binary */
    k16BitBigEndianFormat = 0x01,       /* 16-bit big endian */
    k16BitLittleEndianFormat = 0x02,    /* 16-bit little endian */
    k32BitFormat = 0x03,                /* 32-bit */
    kFloat32Format = 0x04,              /* 32-bit float */
    kFloat64Format = 0x05,              /* 64-bit float */
    k8BitTwosComplementFormat = 0x06,   /* 8-bit two's complement */
    k8BitMuLawFormat = 0x07,            /* 8-bit mu-law */
    kMACE3Compression = 0x08,           /* MACE 3:1 compression */
    kMACE6Compression = 0x09,           /* MACE 6:1 compression */
    kCDXAFormat = 0x0A,                 /* CD-XA format */
    kCDXAMono = 0x0B,                   /* CD-XA mono */
    kCDXAStereo = 0x0C,                 /* CD-XA stereo */
    kDVIIntelFormat = 0x0D,             /* DVI Intel format */
    kDVIMGFormat = 0x0E,                /* DVI MG format */
    k16BitLittleEndianLinear = 0x0F,    /* 16-bit little endian linear */
    k16BitBigEndianLinear = 0x10,       /* 16-bit big endian linear */
    k16BitTwosComplementLE = 0x11,      /* 16-bit two's complement LE */
    k16BitTwosComplementBE = 0x12,      /* 16-bit two's complement BE */
    kMPEGLayer3Format = 0x13,           /* MPEG Layer 3 */
    kFullMPEGLay3Format = 0x14          /* Full MPEG Layer 3 */
} AudioEncodingType;

/* Standard Sound Header - Format 1 */
typedef struct SoundHeader {
    uint32_t    samplePtr;          /* Pointer to samples (obsolete) */
    uint32_t    length;             /* Number of bytes in sample */
    UnsignedFixed sampleRate;       /* Sample rate (Fixed point) */
    uint32_t    loopStart;          /* Loop start point */
    uint32_t    loopEnd;            /* Loop end point */
    uint8_t     encode;             /* Encoding type */
    uint8_t     baseFrequency;      /* Base frequency (middle C = 60) */
    uint8_t     sampleArea[1];      /* Start of sample data */
} SoundHeader;

/* Extended Sound Header - Format 2 */
typedef struct ExtSoundHeader {
    uint32_t    samplePtr;          /* Pointer to samples */
    uint32_t    numChannels;        /* Number of channels */
    UnsignedFixed sampleRate;       /* Sample rate (Fixed point) */
    uint32_t    loopStart;          /* Loop start point */
    uint32_t    loopEnd;            /* Loop end point */
    uint8_t     encode;             /* Encoding type */
    uint8_t     baseFrequency;      /* Base frequency */
    uint32_t    numFrames;          /* Number of sample frames */
    uint8_t     AIFFSampleRate[10]; /* AIFF sample rate (80-bit IEEE) */
    uint32_t    markerChunk;        /* Pointer to marker chunk */
    uint32_t    instrumentChunks;   /* Pointer to instrument chunk */
    uint32_t    AESRecording;       /* AES recording info */
    uint16_t    sampleSize;         /* Sample size in bits */
    uint16_t    futureUse1;         /* Reserved */
    uint32_t    futureUse2;         /* Reserved */
    uint32_t    futureUse3;         /* Reserved */
    uint32_t    futureUse4;         /* Reserved */
    uint8_t     sampleArea[1];      /* Start of sample data */
} ExtSoundHeader;

/* Compressed Sound Header */
typedef struct CmpSoundHeader {
    uint32_t    samplePtr;          /* Pointer to samples */
    uint32_t    numChannels;        /* Number of channels */
    UnsignedFixed sampleRate;       /* Sample rate (Fixed point) */
    uint32_t    loopStart;          /* Loop start point */
    uint32_t    loopEnd;            /* Loop end point */
    uint8_t     encode;             /* Encoding type */
    uint8_t     baseFrequency;      /* Base frequency */
    uint32_t    numFrames;          /* Number of sample frames */
    uint8_t     AIFFSampleRate[10]; /* AIFF sample rate (80-bit IEEE) */
    uint32_t    markerChunk;        /* Pointer to marker chunk */
    uint32_t    format;             /* Compression format */
    uint32_t    futureUse2;         /* Reserved */
    uint32_t    stateVars;          /* State variables */
    uint32_t    leftOverSamples;    /* Leftover samples */
    uint16_t    compressionID;      /* Compression ID */
    uint16_t    packetSize;         /* Packet size */
    uint16_t    snthID;             /* Synthesizer ID */
    uint16_t    sampleSize;         /* Sample size in bits */
    uint8_t     sampleArea[1];      /* Start of sample data */
} CmpSoundHeader;

/* Sound List Resource Header */
typedef struct SndListResource {
    uint16_t    format;             /* Resource format (1 or 2) */
    uint16_t    numModifiers;       /* Number of modifier parts */
    uint16_t    numCommands;        /* Number of command parts */
    uint16_t    modifierPart;       /* Offset to modifier part */
    uint16_t    commandPart;        /* Offset to command part */
    uint8_t     data[1];            /* Start of resource data */
} SndListResource;

/* Sound Command Structure */
typedef struct SndCommand {
    uint16_t    cmd;                /* Command opcode */
    int16_t     param1;             /* First parameter */
    int32_t     param2;             /* Second parameter */
} SndCommand;

/* Sound Channel Status */
typedef struct SCStatus {
    UnsignedFixed scStartTime;      /* Starting time */
    UnsignedFixed scEndTime;        /* Ending time */
    UnsignedFixed scCurrentTime;    /* Current time */
    Boolean     scChannelBusy;      /* Channel busy flag */
    Boolean     scChannelDisposed;  /* Channel disposed flag */
    Boolean     scChannelPaused;    /* Channel paused flag */
    Boolean     scUnused;           /* Unused */
    uint32_t    scChannelAttributes; /* Channel attributes */
    int32_t     scCPULoad;          /* CPU load percentage */
} SCStatus;

/* Sound Manager Status */
typedef struct SMStatus {
    int16_t     smMaxCPULoad;       /* Maximum CPU load */
    int16_t     smNumChannels;      /* Number of sound channels */
    int16_t     smCurCPULoad;       /* Current CPU load */
} SMStatus;

/* Sound Channel Structure */
typedef struct SndChannel {
    struct SndChannel*  nextChan;       /* Next channel in chain */
    void*               firstMod;       /* First modifier */
    void                (*callBack)(struct SndChannel*, struct SndCommand*);
    int32_t             userInfo;       /* User information */
    uint32_t            wait;           /* Wait time */
    struct SndCommand   cmdInProgress;  /* Current command */
    uint16_t            flags;          /* Channel flags */
    uint16_t            qHead;          /* Queue head */
    uint16_t            qTail;          /* Queue tail */
    struct SndCommand*  queue;          /* Command queue */
    uint16_t            queueSize;      /* Queue size */
    uint16_t            channelNumber;  /* Channel number */
    int16_t             synthType;      /* Synthesizer type */
    uint32_t            initBits;       /* Initialization flags */
    void*               userRoutine;    /* User callback routine */
    void*               privateData;    /* Private channel data */
} SndChannel;

/* Audio Selection Structure */
typedef struct AudioSelection {
    int32_t     selStart;           /* Selection start */
    int32_t     selEnd;             /* Selection end */
} AudioSelection;
typedef AudioSelection*     AudioSelectionPtr;

/* File Play Completion UPP */
typedef void (*FilePlayCompletionUPP)(SndChannelPtr chan);

/* Sound Completion UPP */
typedef void (*SoundCompletionUPP)(void);

/* Modal Filter UPP */
typedef Boolean (*ModalFilterProcPtr)(void);

/* Sound Parameter Block for Sound Input */
typedef struct SPB {
    int32_t     inRefNum;           /* Reference number */
    uint32_t    count;              /* Number of bytes to record */
    uint32_t    milliseconds;       /* Milliseconds to record */
    uint32_t    bufferLength;       /* Buffer length */
    void*       bufferPtr;          /* Buffer pointer */
    void        (*completionRoutine)(SPBPtr spb); /* Completion routine */
    void        (*interruptRoutine)(SPBPtr spb, int16_t code, int32_t arg); /* Interrupt routine */
    int32_t     userLong;           /* User data */
    OSErr       error;              /* Error code */
    int32_t     unused1;            /* Unused */
} SPB;

/* Compression Information */
typedef struct CompressionInfo {
    int32_t     recordSize;         /* Size of this record */
    OSType      format;             /* Format ID */
    int16_t     compressionID;      /* Compression ID */
    uint16_t    samplesPerPacket;   /* Samples per packet */
    uint16_t    bytesPerPacket;     /* Bytes per packet */
    uint16_t    bytesPerFrame;      /* Bytes per frame */
    uint16_t    bytesPerSample;     /* Bytes per sample */
    int16_t     futureUse1;         /* Reserved */
} CompressionInfo;
typedef CompressionInfo*    CompressionInfoPtr;

/* MIDI Types */
typedef enum {
    kMIDIDirectionInput = 1,        /* MIDI input */
    kMIDIDirectionOutput = 2,       /* MIDI output */
    kMIDIDirectionInputOutput = 3   /* MIDI input and output */
} MIDIPortDirectionFlags;

typedef struct MIDIPortParams {
    int32_t     flags;              /* Port flags */
    int32_t     refCon;             /* Reference constant */
    void*       timeProc;           /* Time procedure */
    int32_t     timeRefCon;         /* Time reference constant */
    void*       readProc;           /* Read procedure */
    int32_t     readRefCon;         /* Read reference constant */
} MIDIPortParams;

typedef struct MIDIPacket {
    uint64_t    timeStamp;          /* Time stamp */
    uint16_t    length;             /* Data length */
    uint8_t     data[256];          /* MIDI data */
} MIDIPacket;

typedef struct MIDIPacketList {
    uint32_t    numPackets;         /* Number of packets */
    MIDIPacket  packet[1];          /* First packet */
} MIDIPacketList;
typedef MIDIPacketList*     MIDIPacketListPtr;

/* Sound Input Device Information Types */
typedef enum {
    siDeviceType = FOUR_CHAR_CODE('type'),          /* Device type */
    siDeviceName = FOUR_CHAR_CODE('name'),          /* Device name */
    siDeviceIcon = FOUR_CHAR_CODE('icon'),          /* Device icon */
    siHardwareVolume = FOUR_CHAR_CODE('hvol'),      /* Hardware volume */
    siHardwareVolumeSteps = FOUR_CHAR_CODE('hstp'), /* Volume steps */
    siHardwareMute = FOUR_CHAR_CODE('hmut'),        /* Hardware mute */
    siHardwareFormat = FOUR_CHAR_CODE('hfmt'),      /* Hardware format */
    siHardwareBusy = FOUR_CHAR_CODE('hwbs'),        /* Hardware busy */
    siSampleRate = FOUR_CHAR_CODE('srat'),          /* Sample rate */
    siSampleRateAvailable = FOUR_CHAR_CODE('srav'), /* Available rates */
    siSampleSize = FOUR_CHAR_CODE('ssiz'),          /* Sample size */
    siSampleSizeAvailable = FOUR_CHAR_CODE('ssav'), /* Available sizes */
    siNumberChannels = FOUR_CHAR_CODE('chan'),      /* Number of channels */
    siChannelAvailable = FOUR_CHAR_CODE('chav'),    /* Available channels */
    siAsync = FOUR_CHAR_CODE('asyn'),               /* Async support */
    siInputSource = FOUR_CHAR_CODE('sour'),         /* Input source */
    siInputSourceNames = FOUR_CHAR_CODE('snam'),    /* Source names */
    siInputGain = FOUR_CHAR_CODE('gain'),           /* Input gain */
    siInputGainAvailable = FOUR_CHAR_CODE('gnav'),  /* Available gains */
    siPlayThruOnOff = FOUR_CHAR_CODE('plth'),       /* Play-through */
    siPostMixerSoundComponent = FOUR_CHAR_CODE('pmix'), /* Post mixer */
    siPreMixerSoundComponent = FOUR_CHAR_CODE('prmx'), /* Pre mixer */
    siQuality = FOUR_CHAR_CODE('qual'),             /* Recording quality */
    siCompressionFactor = FOUR_CHAR_CODE('cmpr'),   /* Compression factor */
    siCompressionHeader = FOUR_CHAR_CODE('cmhd'),   /* Compression header */
    siCompressionNames = FOUR_CHAR_CODE('cnam'),    /* Compression names */
    siCompressionParams = FOUR_CHAR_CODE('cpar'),   /* Compression params */
    siCompressionSampleRate = FOUR_CHAR_CODE('crat'), /* Compression rate */
    siCompressionChannels = FOUR_CHAR_CODE('ccha'),   /* Compression channels */
    siCompressionOutputSampleRate = FOUR_CHAR_CODE('cort'), /* Output rate */
    siCompressionInputSampleRate = FOUR_CHAR_CODE('cirt'),  /* Input rate */
    siLevelMeterOnOff = FOUR_CHAR_CODE('lmet'),     /* Level meter */
    siContinuous = FOUR_CHAR_CODE('cont'),          /* Continuous recording */
    siVoxRecordInfo = FOUR_CHAR_CODE('voxr')        /* Voice record info */
} SoundInputInfoType;

/* Audio Quality Types */
typedef enum {
    kAudioQualityMin = 0x00,        /* Minimum quality */
    kAudioQualityLow = 0x20,        /* Low quality */
    kAudioQualityMedium = 0x40,     /* Medium quality */
    kAudioQualityHigh = 0x60,       /* High quality */
    kAudioQualityMax = 0x7F         /* Maximum quality */
} AudioQuality;

/* Fixed Point Utilities */
#define Fixed2Long(f)       ((int32_t)((f) >> 16))
#define Long2Fixed(l)       ((Fixed)((l) << 16))
#define Fixed2Frac(f)       ((int16_t)(f))
#define Frac2Fixed(fr)      ((Fixed)(fr))
#define FixedRound(f)       ((int16_t)(((f) + 0x00008000) >> 16))
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