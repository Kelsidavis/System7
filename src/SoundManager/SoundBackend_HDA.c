#include "SystemTypes.h"
#include <stdbool.h>

#include "SoundManager/SoundBackend.h"
#include "SoundManager/SoundLogging.h"

static bool g_hdaInitialized = false;

#ifndef unimpErr
#define unimpErr (-4)
#endif

static OSErr SoundBackendHDA_Init(void)
{
    SND_LOG_INFO("SoundBackend(HDA): Initialization not yet implemented\n");
    g_hdaInitialized = false;
    return unimpErr;
}

static void SoundBackendHDA_Shutdown(void)
{
    (void)g_hdaInitialized;
}

static OSErr SoundBackendHDA_PlayPCM(const uint8_t* data,
                                     uint32_t sizeBytes,
                                     uint32_t sampleRate,
                                     uint8_t channels,
                                     uint8_t bitsPerSample)
{
    /* sizeBytes/sampleRate are uint32_t: %u would pass a 4-byte int
     * where the printf expects a long. */
    SND_LOG_WARN("SoundBackend(HDA): PCM playback not implemented (size=%lu, rate=%lu, ch=%u, bits=%u)\n",
                 (unsigned long)sizeBytes, (unsigned long)sampleRate, channels, bitsPerSample);
    return unimpErr;
}

static void SoundBackendHDA_Stop(void)
{
    /* Nothing yet */
}

const SoundBackendOps kSoundBackendOps_HDA = {
    .type = kSoundBackendHDA,
    .name = "Intel HDA",
    .init = SoundBackendHDA_Init,
    .shutdown = SoundBackendHDA_Shutdown,
    .play_pcm = SoundBackendHDA_PlayPCM,
    .stop = SoundBackendHDA_Stop
};
