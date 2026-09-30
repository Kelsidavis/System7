/*
 * File: SpeechManager.h
 *
 * Contains: Main Speech Manager API for System 7.1 Portable
 *
 * Written by: Claude Code (Portable Implementation)
 *
 *
 * Description: This header provides the complete Speech Manager API
 *              for text-to-speech synthesis and voice management.
 */

#ifndef _SPEECHMANAGER_H_
#define _SPEECHMANAGER_H_

#include "SystemTypes.h"

/* Forward declarations */

#include "SystemTypes.h"

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

/* Constants for StopSpeechAt and PauseSpeechAt */

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

/* ===== Basic Types ===== */

/* OSType is defined in MacTypes.h */
/* OSErr is defined in MacTypes.h *//* Fixed is defined in MacTypes.h *//* Ptr is defined in MacTypes.h */

/* Str31 is defined in MacTypes.h */

/* ===== Core Structures ===== */

/* Speech Channel - opaque handle to speech synthesis channel */

/* Voice specification structure */

/* Detailed voice description */

/* File specification for voices stored in files */

/* Speech status information */

/* Speech error tracking */

/* Speech synthesis version information */

/* Phoneme information */

/* Phoneme descriptor */

/* Speech extension data */

/* Delimiter configuration */

/* ===== Callback Function Types ===== */

/* Text-done callback routine */

/* Speech-done callback routine */

/* Sync callback routine */

/* Error callback routine */

/* Phoneme callback routine */

/* Word callback routine */

/* ===== Core Speech Manager API ===== */

/* Version and initialization */
UInt32 SpeechManagerVersion(void);
OSErr SpeechManagerInit(void);

/* Voice management */
OSErr GetVoiceInfo(VoiceSpec *voice, OSType selector, void *voiceInfo);

/* Channel management */
OSErr DisposeSpeechChannel(SpeechChannel chan);

/* Speech synthesis */
OSErr SpeakString(StringPtr textString);
OSErr SpeakText(SpeechChannel chan, void *textBuf, long textBytes);
OSErr SpeakBuffer(SpeechChannel chan, void *textBuf, long textBytes, long controlFlags);

/* Speech control */
OSErr StopSpeechAt(SpeechChannel chan, long whereToStop);
OSErr PauseSpeechAt(SpeechChannel chan, long whereToPause);

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

/* ===== Testing Functions ===== */

void RunSpeechSmokeTest(void);

/* ===== Error Codes ===== */

/* noErr is defined in MacTypes.h */
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