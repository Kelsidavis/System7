#include "SystemTypes.h"
#include "System71StdLib.h"
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "SoundManager/SoundBackend.h"
#include "SoundManager/SoundBlaster16.h"
#include "SoundManager/SoundLogging.h"
#include "TimeManager/MicrosecondTimer.h"

#ifndef unimpErr
#define unimpErr (-4)
#endif
#ifndef notOpenErr
#define notOpenErr (-28)
#endif
#ifndef qErr
#define qErr (-1)
#endif

#define SB16_DMA_CHUNK_BYTES 120000U

static uint8_t g_sb16ChunkBuffer[SB16_DMA_CHUNK_BYTES] __attribute__((aligned(32)));
static bool g_sb16Ready = false;

static OSErr SoundBackendSB16_Init(void)
{
    return noErr;
}

static void SoundBackendSB16_Shutdown(void)
{
    if (!g_sb16Ready) return;
    SB16_Shutdown();
    g_sb16Ready = false;
}

static OSErr SoundBackendSB16_PlayPCM(const uint8_t* data,
                                      uint32_t sizeBytes,
                                      uint32_t sampleRate,
                                      uint8_t channels,
                                      uint8_t bitsPerSample)
{
    if (!data || sizeBytes == 0) {
        return paramErr;
    }

    if (!g_sb16Ready) {
        if (SB16_Init() != 0) {
            SND_LOG_WARN("SoundBackend(SB16): SB16 init failed\n");
            return notOpenErr;
        }
        g_sb16Ready = true;
        SND_LOG_INFO("SoundBackend(SB16): SB16 hardware initialized\n");
    }

    const uint8_t* src = data;
    uint32_t remaining = sizeBytes;
    const uint32_t frameBytes = (bitsPerSample / 8) * channels;

    uint32_t chunkIndex = 0;
    while (remaining > 0) {
        uint32_t chunk = (remaining > SB16_DMA_CHUNK_BYTES) ? SB16_DMA_CHUNK_BYTES : remaining;
        if (frameBytes > 0) {
            chunk -= chunk % frameBytes;
            if (chunk == 0) {
                break;
            }
        }

        /* chunkIndex/chunk/remaining are uint32_t: %u would pass a
         * 4-byte int where the printf expects a long. */
        SND_LOG_INFO("SoundBackend(SB16): Chunk %lu size=%lu remaining=%lu\n",
                      (unsigned long)chunkIndex, (unsigned long)chunk, (unsigned long)remaining);
        memcpy(g_sb16ChunkBuffer, src, chunk);

        int err = SB16_PlayWAV(g_sb16ChunkBuffer,
                               chunk,
                               sampleRate,
                               channels,
                               bitsPerSample);
        if (err != 0) {
            SND_LOG_WARN("SoundBackend(SB16): Playback chunk failed (err=%d)\n", err);
            SB16_StopPlayback();
            return qErr;
        }

        if (frameBytes > 0) {
            uint64_t frames = chunk / frameBytes;
            uint64_t usec64 = udiv64(frames * 1000000ULL, sampleRate);
            if (usec64 > 0) {
                UInt32 clamped = (usec64 > UINT32_MAX) ? UINT32_MAX : (UInt32)usec64;
                SND_LOG_INFO("SoundBackend(SB16): Waiting %lu us for chunk\n", (unsigned long)clamped);
                MicrosecondDelay(clamped);
            }
        }

        src += chunk;
        remaining -= chunk;
        ++chunkIndex;
    }

    SB16_StopPlayback();
    SND_LOG_DEBUG("SoundBackend(SB16): Playback complete, speaker stopped\n");
    return noErr;
}

static void SoundBackendSB16_Stop(void)
{
    if (!g_sb16Ready) return;
    SB16_StopPlayback();
    SND_LOG_DEBUG("SoundBackend(SB16): Stop request\n");
}

const SoundBackendOps kSoundBackendOps_SB16 = {
    .type = kSoundBackendSB16,
    .name = "Sound Blaster 16",
    .init = SoundBackendSB16_Init,
    .shutdown = SoundBackendSB16_Shutdown,
    .play_pcm = SoundBackendSB16_PlayPCM,
    .stop = SoundBackendSB16_Stop
};
