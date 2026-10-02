#include "SoundManager/SoundEffects.h"

#include <stddef.h>

#include "SoundManager/SoundManager.h"
#include "SoundManager/SoundLogging.h"
#include "sound_effect_data.h"
#include "boot_chime_data.h"

/* PC speaker fallback */
extern void PCSpkr_Beep(uint32_t frequency, uint32_t duration_ms);

extern OSErr SoundManager_PlayPCM(const uint8_t* data,
                                  uint32_t sizeBytes,
                                  uint32_t sampleRate,
                                  uint8_t channels,
                                  uint8_t bitsPerSample);

typedef struct {
    const uint8_t* data;
    uint32_t sizeBytes;
    uint32_t sampleRate;
    uint8_t channels;
    uint8_t bitsPerSample;
    uint32_t fallbackFrequency;
    uint32_t fallbackDurationMs;
} SoundEffectDataEntry;

#define EFFECT_ENTRY(dataName, rateMacro, channelMacro, bitsMacro, sizeMacro, freq, dur) \
    { dataName, sizeMacro, rateMacro, channelMacro, bitsMacro, freq, dur }

static const SoundEffectDataEntry kSoundEffectTable[kSoundEffectCount] = {
    [kSoundEffectStartupChime] = {
        gBootChimePCM,
        BOOT_CHIME_DATA_SIZE,
        BOOT_CHIME_SAMPLE_RATE,
        BOOT_CHIME_CHANNELS,
        BOOT_CHIME_BITS_PER_SAMPLE,
        0, 0  /* No fallback beep for startup chime - keep chime sound only */
    },
    [kSoundEffectBeep] = EFFECT_ENTRY(gSosumiPCM, SOUND_SOSUMI_SAMPLE_RATE, SOUND_SOSUMI_CHANNELS,
                                      SOUND_SOSUMI_BITS_PER_SAMPLE, SOUND_SOSUMI_DATA_SIZE,
                                      1000, 200),
    [kSoundEffectAlertCaution] = EFFECT_ENTRY(gWildEepPCM, SOUND_WILDEEP_SAMPLE_RATE, SOUND_WILDEEP_CHANNELS,
                                              SOUND_WILDEEP_BITS_PER_SAMPLE, SOUND_WILDEEP_DATA_SIZE,
                                              1200, 250),
    [kSoundEffectAlertStop] = EFFECT_ENTRY(gQuackPCM, SOUND_QUACK_SAMPLE_RATE, SOUND_QUACK_CHANNELS,
                                          SOUND_QUACK_BITS_PER_SAMPLE, SOUND_QUACK_DATA_SIZE,
                                          600, 250),
    [kSoundEffectAlertNote] = EFFECT_ENTRY(gIndigoPCM, SOUND_INDIGO_SAMPLE_RATE, SOUND_INDIGO_CHANNELS,
                                          SOUND_INDIGO_BITS_PER_SAMPLE, SOUND_INDIGO_DATA_SIZE,
                                          750, 250),
    [kSoundEffectMenuSelect] = EFFECT_ENTRY(gDropletPCM, SOUND_DROPLET_SAMPLE_RATE, SOUND_DROPLET_CHANNELS,
                                           SOUND_DROPLET_BITS_PER_SAMPLE, SOUND_DROPLET_DATA_SIZE,
                                           900, 120),
    [kSoundEffectDroplet] = EFFECT_ENTRY(gDropletPCM, SOUND_DROPLET_SAMPLE_RATE, SOUND_DROPLET_CHANNELS,
                                         SOUND_DROPLET_BITS_PER_SAMPLE, SOUND_DROPLET_DATA_SIZE,
                                         900, 120),
    [kSoundEffectQuack] = EFFECT_ENTRY(gQuackPCM, SOUND_QUACK_SAMPLE_RATE, SOUND_QUACK_CHANNELS,
                                       SOUND_QUACK_BITS_PER_SAMPLE, SOUND_QUACK_DATA_SIZE,
                                       600, 250),
    [kSoundEffectWildEep] = EFFECT_ENTRY(gWildEepPCM, SOUND_WILDEEP_SAMPLE_RATE, SOUND_WILDEEP_CHANNELS,
                                         SOUND_WILDEEP_BITS_PER_SAMPLE, SOUND_WILDEEP_DATA_SIZE,
                                         1200, 250),
    [kSoundEffectIndigo] = EFFECT_ENTRY(gIndigoPCM, SOUND_INDIGO_SAMPLE_RATE, SOUND_INDIGO_CHANNELS,
                                        SOUND_INDIGO_BITS_PER_SAMPLE, SOUND_INDIGO_DATA_SIZE,
                                        750, 250)
};

OSErr SoundEffects_Play(SoundEffectId effect)
{
    if ((unsigned)effect >= kSoundEffectCount) {
        return paramErr;
    }

    const SoundEffectDataEntry* entry = &kSoundEffectTable[effect];
    if (!entry->data || entry->sizeBytes == 0) {
        return paramErr;
    }

    OSErr err = SoundManager_PlayPCM(entry->data,
                                     entry->sizeBytes,
                                     entry->sampleRate,
                                     entry->channels,
                                     entry->bitsPerSample);
    if (err != noErr) {
        if (entry->fallbackDurationMs > 0) {
            PCSpkr_Beep(entry->fallbackFrequency, entry->fallbackDurationMs);
        }
    }
    return err;
}
