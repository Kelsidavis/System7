/*
 * File: SpeechManager.h
 *
 * Contains: Main Speech Manager API for System 7.1 Portable
 *
 * Written by: Claude Code (Portable Implementation)
 *
 * Copyright: Based on Apple Computer, Inc. Speech Manager API
 *
 * Description: This header provides the complete Speech Manager API
 *              for text-to-speech synthesis and voice management.
 */

#ifndef _SPEECHMANAGER_H_
#define _SPEECHMANAGER_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Constants and Types ===== */

/* Component types for Text-to-Speech */
#define kTextToSpeechSynthType      0x74747363  /* 'ttsc' */
#define kTextToSpeechVoiceType      0x74747664  /* 'ttvd' */
#define kTextToSpeechVoiceFileType  0x74747666  /* 'ttvf' */
#define kTextToSpeechVoiceBundleType 0x74747662 /* 'ttvb' */

/* Control flags for SpeakBuffer and TextDone callback */
typedef enum {
    kNoEndingProsody    = 1,
    kNoSpeechInterrupt  = 2,
    kPreflightThenPause = 4
} SpeechControlFlags;

/* Constants for StopSpeechAt and PauseSpeechAt */
typedef enum {
    kImmediate      = 0,
    kEndOfWord      = 1,
    kEndOfSentence  = 2
} SpeechStopMode;

/* GetSpeechInfo & SetSpeechInfo selectors */
#define soStatus            0x73746174  /* 'stat' */
#define soErrors            0x6572726F  /* 'erro' */
#define soInputMode         0x696E7074  /* 'inpt' */
#define soCharacterMode     0x63686172  /* 'char' */
#define soNumberMode        0x6E6D6272  /* 'nmbr' */
#define soRate              0x72617465  /* 'rate' */
#define soPitchBase         0x70626173  /* 'pbas' */
#define soPitchMod          0x706D6F64  /* 'pmod' */
#define soVolume            0x766F6C6D  /* 'volm' */
#define soSynthType         0x76657273  /* 'vers' */
#define soRecentSync        0x73796E63  /* 'sync' */
#define soPhonemeSymbols    0x70687379  /* 'phsy' */
#define soCurrentVoice      0x63766F78  /* 'cvox' */
#define soCommandDelimiter  0x646C696D  /* 'dlim' */
#define soReset             0x72736574  /* 'rset' */
#define soCurrentA5         0x6D794135  /* 'myA5' */
#define soRefCon            0x72656663  /* 'refc' */
#define soTextDoneCallBack  0x74646362  /* 'tdcb' */
#define soSpeechDoneCallBack 0x73646362 /* 'sdcb' */
#define soSyncCallBack      0x73796362  /* 'sycb' */
#define soErrorCallBack     0x65726362  /* 'ercb' */
#define soPhonemeCallBack   0x70686362  /* 'phcb' */
#define soWordCallBack      0x77646362  /* 'wdcb' */
#define soSynthExtension    0x78746E64  /* 'xtnd' */
#define soSndInit           0x736E6469  /* 'sndi' */

/* Speaking Mode Constants */
#define modeText        0x54455854  /* 'TEXT' */
#define modePhonemes    0x50484F4E  /* 'PHON' */
#define modeNormal      0x4E4F524D  /* 'NORM' */
#define modeLiteral     0x4C54524C  /* 'LTRL' */

/* GetVoiceInfo selectors */
#define soVoiceDescription  0x696E666F  /* 'info' */
#define soVoiceFile         0x66726566  /* 'fref' */

/* Gender constants */
typedef enum {
    kNeuter = 0,
    kMale = 1,
    kFemale = 2
} VoiceGender;

/* ===== Basic Types ===== */

typedef uint32_t OSType;
typedef int16_t OSErr;
typedef uint32_t Fixed;
typedef unsigned char *StringPtr;
typedef char Str15[16];
typedef char Str31[32];
typedef char Str63[64];
typedef char Str255[256];

/* ===== Core Structures ===== */

/* Speech Channel - opaque handle to speech synthesis channel */
typedef struct SpeechChannelRecord {
    long data[1];
} SpeechChannelRecord;

typedef SpeechChannelRecord *SpeechChannel;

/* Voice specification structure */
typedef struct VoiceSpec {
    OSType creator;     /* creator id of required synthesizer */
    OSType id;          /* voice id on the specified synth */
} VoiceSpec;

/* Detailed voice description */
typedef struct VoiceDescription {
    long length;            /* size of structure - set by application */
    VoiceSpec voice;        /* voice creator and id info */
    long version;           /* version code for voice */
    Str63 name;             /* name of voice */
    Str255 comment;         /* additional text info about voice */
    short gender;           /* neuter, male, or female */
    short age;              /* approximate age in years */
    short script;           /* script code of text voice can process */
    short language;         /* language code of voice output speech */
    short region;           /* region code of voice output speech */
    long reserved[4];       /* always zero - reserved for future use */
} VoiceDescription;

/* File specification for voices stored in files */
typedef struct VoiceFileInfo {
    void *fileSpec;         /* platform-specific file specification */
    short resID;            /* resource id of voice in the file */
} VoiceFileInfo;

/* Speech status information */
typedef struct SpeechStatusInfo {
    bool outputBusy;        /* true if audio is playing */
    bool outputPaused;      /* true if channel is paused */
    long inputBytesLeft;    /* bytes left to process */
    short phonemeCode;      /* opcode for current phoneme */
} SpeechStatusInfo;

/* Speech error tracking */
typedef struct SpeechErrorInfo {
    short count;            /* number of errors since last check */
    OSErr oldest;           /* oldest unread error */
    long oldPos;            /* character position of oldest error */
    OSErr newest;           /* most recent error */
    long newPos;            /* character position of newest error */
} SpeechErrorInfo;

/* Speech synthesis version information */
typedef struct SpeechVersionInfo {
    OSType synthType;           /* always 'ttsc' */
    OSType synthSubType;        /* synth flavor */
    OSType synthManufacturer;   /* synth creator ID */
    long synthFlags;            /* synth feature flags */
    uint32_t synthVersion;      /* synth version number */
} SpeechVersionInfo;

/* Phoneme information */
typedef struct PhonemeInfo {
    short opcode;           /* opcode for the phoneme */
    Str15 phStr;            /* corresponding character string */
    Str31 exampleStr;       /* word that shows use of phoneme */
    short hiliteStart;      /* segment of example word that */
    short hiliteEnd;        /* should be highlighted */
} PhonemeInfo;

/* Phoneme descriptor */
typedef struct PhonemeDescriptor {
    short phonemeCount;     /* number of elements */
    PhonemeInfo thePhonemes[1]; /* element list */
} PhonemeDescriptor;

/* Speech extension data */
typedef struct SpeechXtndData {
    OSType synthCreator;    /* synth creator id */
    unsigned char synthData[2]; /* data defined by synth */
} SpeechXtndData;

/* Delimiter configuration */
typedef struct DelimiterInfo {
    unsigned char startDelimiter[2];    /* defaults to "[[" */
    unsigned char endDelimiter[2];      /* defaults to "]]" */
} DelimiterInfo;

/* ===== Callback Function Types ===== */

/* Text-done callback routine */
typedef void (*SpeechTextDoneProcPtr)(SpeechChannel chan, long refCon,
                                       void **nextBuf, long *nextLen, long *nextFlags);

/* Speech-done callback routine */
typedef void (*SpeechDoneProcPtr)(SpeechChannel chan, long refCon);

/* Sync callback routine */
typedef void (*SpeechSyncProcPtr)(SpeechChannel chan, long refCon, OSType syncMessage);

/* Error callback routine */
typedef void (*SpeechErrorProcPtr)(SpeechChannel chan, long refCon, OSErr theError, long bytePos);

/* Phoneme callback routine */
typedef void (*SpeechPhonemeProcPtr)(SpeechChannel chan, long refCon, short phonemeOpcode);

/* Word callback routine */
typedef void (*SpeechWordProcPtr)(SpeechChannel chan, long refCon, long wordPos, short wordLen);

/* ===== Core Speech Manager API ===== */

/* Version and initialization */
uint32_t SpeechManagerVersion(void);

/* Voice management */
OSErr MakeVoiceSpec(OSType creator, OSType id, VoiceSpec *voice);
OSErr CountVoices(short *numVoices);
OSErr GetIndVoice(short index, VoiceSpec *voice);
OSErr GetVoiceDescription(VoiceSpec *voice, VoiceDescription *info, long infoLength);
OSErr GetVoiceInfo(VoiceSpec *voice, OSType selector, void *voiceInfo);

/* Channel management */
OSErr NewSpeechChannel(VoiceSpec *voice, SpeechChannel *chan);
OSErr DisposeSpeechChannel(SpeechChannel chan);

/* Speech synthesis */
OSErr SpeakString(StringPtr textString);
OSErr SpeakText(SpeechChannel chan, void *textBuf, long textBytes);
OSErr SpeakBuffer(SpeechChannel chan, void *textBuf, long textBytes, long controlFlags);

/* Speech control */
OSErr StopSpeech(SpeechChannel chan);
OSErr StopSpeechAt(SpeechChannel chan, long whereToStop);
OSErr PauseSpeechAt(SpeechChannel chan, long whereToPause);
OSErr ContinueSpeech(SpeechChannel chan);

/* Speech status */
short SpeechBusy(void);
short SpeechBusySystemWide(void);

/* Speech parameters */
OSErr SetSpeechRate(SpeechChannel chan, Fixed rate);
OSErr GetSpeechRate(SpeechChannel chan, Fixed *rate);
OSErr SetSpeechPitch(SpeechChannel chan, Fixed pitch);
OSErr GetSpeechPitch(SpeechChannel chan, Fixed *pitch);
OSErr SetSpeechInfo(SpeechChannel chan, OSType selector, void *speechInfo);
OSErr GetSpeechInfo(SpeechChannel chan, OSType selector, void *speechInfo);

/* Text processing */
OSErr TextToPhonemes(SpeechChannel chan, void *textBuf, long textBytes,
                     void **phonemeBuf, long *phonemeBytes);

/* Dictionary support */
OSErr UseDictionary(SpeechChannel chan, void *dictionary);

/* ===== Error Codes ===== */

#define noErr                   0
#define paramErr               -50
#define memFullErr            -108
#define resNotFound           -192
#define voiceNotFound         -244
#define noSynthFound          -245
#define synthOpenFailed       -246
#define synthNotReady         -247
#define bufTooSmall           -248
#define badInputText          -249

#ifdef __cplusplus
}
#endif

#endif /* _SPEECHMANAGER_H_ */