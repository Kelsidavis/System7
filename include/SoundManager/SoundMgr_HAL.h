/*
 * Copyright (c) 2024 System7 Project
 * MIT License - See LICENSE file
 */

/*
 * SoundMgr_HAL.h - Hardware Abstraction Layer for Sound Manager
 */

#ifndef SOUNDMGR_HAL_H
#define SOUNDMGR_HAL_H

#include <Types.h>
#include "SoundTypes.h"
#include "SoundManager.h"

/* HAL Initialization */
OSErr SoundMgr_HAL_Init(void);
void SoundMgr_HAL_Cleanup(void);

/* Audio Control */
OSErr SoundMgr_HAL_Start(void);
OSErr SoundMgr_HAL_Stop(void);

/* Channel Management */
OSErr SoundMgr_HAL_AllocateChannel(SndChannelPtr *chan, short synth);
OSErr SoundMgr_HAL_DisposeChannel(SndChannelPtr chan);

/* Sound Playback */
OSErr SoundMgr_HAL_PlayResource(Handle sndHandle, SndChannelPtr chan);
OSErr SoundMgr_HAL_SendCommand(SndChannelPtr chan, const SndCommand *cmd, Boolean noWait);

/* System Functions */
OSErr SoundMgr_HAL_SysBeep(short duration);
OSErr SoundMgr_HAL_SetVolume(int16_t volume);
int16_t SoundMgr_HAL_GetCPULoad(void);

/* Waveform Types */
typedef enum {
    sineWave = 0,
    squareWave = 1,
    triangleWave = 2,
    sawtoothWave = 3
} WaveformType;

#endif /* SOUNDMGR_HAL_H */