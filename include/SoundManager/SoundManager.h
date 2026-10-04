/*
 * SoundManager.h - System 7 Sound Manager API declarations
 *
 * The kernel implements a backend-dependent subset of this API; recording
 * and full sound synthesis are not provided by the current implementation.
 *
 * Copyright (c) 2025 - System 7.1 Portable Project
 */

#ifndef _SOUNDMANAGER_H_
#define _SOUNDMANAGER_H_

#include "SystemTypes.h"

/* Forward declarations */

#include "SoundTypes.h"
#include "SoundSynthesis.h"
#include "SoundHardware.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sound Manager Version */
#define kSoundManagerVersion    0x0300      /* Version 3.0 */

/* Sound Manager Error Codes */

/* Sound Manager Command Opcodes */

/* Channel Initialization Flags */

/* Synthesizer Types */

/* Volume Control Constants */
#define kFullVolume         0x0100  /* Maximum volume */
#define kNoVolume           0x0000  /* Silence */
#define kDefaultVolume      0x0100  /* Default volume level */

/* Sample Rate Constants (Fixed Point) */
#define rate22khz           0x56220000UL    /* 22.254 kHz */
#define rate11khz           0x2B110000UL    /* 11.127 kHz */
#define rate44khz           0xAC440000UL    /* 44.100 kHz */
#define rate48khz           0xBB800000UL    /* 48.000 kHz */

/* Sound Manager Command Structure */

/* Sound Channel Callback Function */

/* Sound Manager Global State */

/* Sound Manager API Functions */

/* Initialization and Cleanup */
OSErr SoundManagerInit(void);
OSErr SoundManagerShutdown(void);

/* System Sounds */
void SysBeep(short duration);
void StartupChime(void);
OSErr SoundManager_PlayPCM(const uint8_t* data,
                           uint32_t sizeBytes,
                           uint32_t sampleRate,
                           uint8_t channels,
                           uint8_t bitsPerSample);

/* Channel Management */
OSErr SndNewChannel(SndChannelPtr *chan,
                   SInt16 synth,
                   SInt32 init,
                   SndCallBackProcPtr userRoutine);

OSErr SndDisposeChannel(SndChannelPtr chan, Boolean quietNow);

OSErr SndChannelStatus(SndChannelPtr chan,
                      SInt16 theLength,
                      SCStatus *theStatus);

/* Sound Playback */
OSErr SndPlay(SndChannelPtr chan, SndListHandle sndHandle, Boolean async);
OSErr SetupSndHeader(Handle sndHandle,
                     SInt16 numChannels,
                     Fixed sampleRate,
                     SInt16 sampleSize,
                     OSType compressionType,
                     SInt16 baseFrequency,
                     SInt32 numBytes,
                     SInt16 *headerLen);

OSErr SndStartFilePlay(SndChannelPtr chan,
                      SInt16 fRefNum,
                      SInt16 resNum,
                      SInt32 bufferSize,
                      void *theBuffer,
                      AudioSelectionPtr theSelection,
                      FilePlayCompletionUPP theCompletion,
                      Boolean async);

OSErr SndPauseFilePlay(SndChannelPtr chan);
OSErr SndStopFilePlay(SndChannelPtr chan, Boolean quietNow);

/* Command Processing */
OSErr SndDoCommand(SndChannelPtr chan,
                  const SndCommand *cmd,
                  Boolean noWait);

OSErr SndDoImmediate(SndChannelPtr chan, const SndCommand *cmd);

/* Callback Management */
OSErr SndSetChannelCallback(SndChannelPtr chan, SndCallBackProcPtr callback);

/* Channel Routing and Control */
OSErr SndSetChannelEnabled(SndChannelPtr chan, Boolean enabled);
OSErr SndGetChannelEnabled(SndChannelPtr chan, Boolean* enabled);
OSErr SndSetChannelMute(SndChannelPtr chan, Boolean muted);
OSErr SndGetChannelMute(SndChannelPtr chan, Boolean* muted);
OSErr SndSetChannelPriority(SndChannelPtr chan, SInt16 priority);
OSErr SndGetChannelPriority(SndChannelPtr chan, SInt16* priority);
OSErr SndGetActiveChannel(SndChannelPtr* activeChan);

/* Sound Control */
OSErr SndControl(SInt16 id, SndCommand *cmd);

/* Volume and Settings */
void GetSysBeepVolume(SInt32 *level);
OSErr SetSysBeepVolume(SInt32 level);
void GetDefaultOutputVolume(SInt32 *level);
OSErr SetDefaultOutputVolume(SInt32 level);
void GetSoundHeaderOffset(SndListHandle sndHandle, SInt32 *offset);
UnsignedFixed GetCompressionInfo(SInt16 compressionID,
                                OSType format,
                                SInt16 numChannels,
                                SInt16 sampleSize,
                                CompressionInfoPtr cp);

/* Sound Recording */
OSErr SndRecord(ModalFilterProcPtr filterProc,
               Point corner,
               OSType quality,
               SndListHandle *sndHandle);

OSErr SndRecordToFile(ModalFilterProcPtr filterProc,
                     Point corner,
                     OSType quality,
                     SInt16 fRefNum);

/* Sound Input Management */
OSErr SPBOpenDevice(const Str255 deviceName,
                   SInt16 permission,
                   SInt32 *inRefNum);
OSErr SPBGetIndexedDevice(SInt16 count, Str255 deviceName,
                          Handle *deviceIconHandle);
OSErr SPBSignInDevice(SInt16 deviceRefNum, ConstStr255Param deviceName);
OSErr SPBSignOutDevice(SInt16 deviceRefNum);
OSErr SPBCloseDevice(SInt32 inRefNum);
OSErr SPBRecord(SPBPtr inParamPtr, Boolean asynchFlag);
OSErr SPBRecordToFile(SInt16 fRefNum, SPBPtr inParamPtr, Boolean asynchFlag);
OSErr SPBPauseRecording(SInt32 inRefNum);
OSErr SPBResumeRecording(SInt32 inRefNum);
OSErr SPBStopRecording(SInt32 inRefNum);

OSErr SPBGetRecordingStatus(SInt32 inRefNum,
                           SInt16 *recordingStatus,
                           SInt16 *meterLevel,
                           UInt32 *totalSamplesToRecord,
                           UInt32 *numberOfSamplesRecorded,
                           UInt32 *totalMsecsToRecord,
                           UInt32 *numberOfMsecsRecorded);

OSErr SPBGetDeviceInfo(SInt32 inRefNum,
                      OSType infoType,
                      void *infoData);

OSErr SPBSetDeviceInfo(SInt32 inRefNum,
                      OSType infoType,
                      void *infoData);

OSErr SPBMillisecondsToBytes(SInt32 inRefNum,
                           SInt32 *milliseconds);

OSErr SPBBytesToMilliseconds(SInt32 inRefNum,
                           SInt32 *byteCount);

/* Sound Manager Information */
NumVersion SndSoundManagerVersion(void);
OSErr SndManagerStatus(SInt16 theLength, SMStatus *theStatus);

/* Legacy Sound Manager 1.0 Compatibility */
void StartSound(const void *synthRec,
               SInt32 numBytes,
               SoundCompletionUPP completionRtn);
void StopSound(void);
Boolean SoundDone(void);
void GetSoundVol(SInt16 *level);
void SetSoundVol(SInt16 level);

/* Utility Functions */
OSErr GetSoundPreference(OSType theType,
                        Str255 name,
                        Handle settings);

OSErr SetSoundPreference(OSType theType,
                        const Str255 name,
                        Handle settings);

/* Constants for GetSoundPreference/SetSoundPreference */
#define kSoundPrefsType         FOUR_CHAR_CODE('sout')
#define kGeneralSoundPrefs      FOUR_CHAR_CODE('genr')
#define kSoundEffectsPrefs      FOUR_CHAR_CODE('sfx ')
#define kSpeechPrefs           FOUR_CHAR_CODE('spch')

/* MIDI Support */
OSErr MIDISignIn(OSType clientID,
                SInt32 refCon,
                Handle icon,
                Str255 name);


OSErr MIDIOpenPort(OSType clientID,
                  Str255 name,
                  MIDIPortDirectionFlags direction,
                  MIDIPortParams *params,
                  SInt32 *portRefNum);


OSErr MIDIConnectPort(SInt32 srcPortRefNum,
                     SInt32 dstPortRefNum,
                     OSType connType);


OSErr MIDISendData(SInt32 portRefNum,
                  MIDIPacketListPtr packetList);

/* Internal Functions - Do Not Call Directly */
void _SoundManagerMixerCallback(void *userData,
                               SInt16 *buffer,
                               UInt32 frameCount);

#ifdef __cplusplus
}
#endif

#endif /* _SOUNDMANAGER_H_ */
