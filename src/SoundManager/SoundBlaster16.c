/*
 * SoundBlaster16.c - Sound Blaster 16 driver for bare-metal x86
 *
 * Implements basic SB16 audio playback for WAV files.
 * Uses DMA for audio buffer transfer.
 */

#include <stdint.h>
#include <stdbool.h>
#include "SoundManager/SoundLogging.h"
#include "SoundManager/SoundBlaster16.h"

/* I/O port access */
#include "Platform/include/io.h"

#define inb(port) hal_inb(port)
#define outb(port, value) hal_outb(port, value)

/* Sound Blaster 16 I/O ports (base 0x220) */
#define SB16_BASE_PORT      0x220

#define SB16_MIXER_ADDR     (SB16_BASE_PORT + 0x04)
#define SB16_MIXER_DATA     (SB16_BASE_PORT + 0x05)
#define SB16_DSP_RESET      (SB16_BASE_PORT + 0x06)
#define SB16_DSP_READ       (SB16_BASE_PORT + 0x0A)
#define SB16_DSP_WRITE      (SB16_BASE_PORT + 0x0C)
#define SB16_DSP_READ_STATUS (SB16_BASE_PORT + 0x0E)
#define SB16_DSP_ACK_16BIT  (SB16_BASE_PORT + 0x0F)

/* DSP Commands */
#define DSP_CMD_SET_TIME_CONSTANT  0x40
#define DSP_CMD_SET_SAMPLE_RATE    0x41
#define DSP_CMD_TURN_ON_SPEAKER    0xD1
#define DSP_CMD_TURN_OFF_SPEAKER   0xD3
#define DSP_CMD_GET_VERSION        0xE1
#define DSP_CMD_DMA_16BIT_STEREO   0xB6
#define DSP_CMD_DMA_16BIT_MONO     0xB4
#define DSP_CMD_DMA_8BIT_STEREO    0xC6
#define DSP_CMD_DMA_8BIT_MONO      0xC4

/* Mixer registers */
#define MIXER_MASTER_VOLUME   0x22
#define MIXER_VOICE_VOLUME    0x04

/* DMA configuration */
#define DMA_CHANNEL_16BIT     5  /* SB16 uses DMA channel 5 for 16-bit */
#define DMA_CHANNEL_8BIT      1  /* SB16 uses DMA channel 1 for 8-bit */

/* State */
static bool g_sb16_initialized = false;
static uint8_t g_dsp_version_major = 0;
static uint8_t g_dsp_version_minor = 0;

/*
 * Wait for DSP to be ready for writing
 */
static bool sb16_dsp_wait_write(void) {
    for (int i = 0; i < 65536; i++) {
        if ((inb(SB16_DSP_WRITE) & 0x80) == 0) {
            return true;
        }
    }
    return false;  /* Timeout */
}

/*
 * Wait for DSP to have data ready for reading
 */
static bool sb16_dsp_wait_read(void) {
    for (int i = 0; i < 65536; i++) {
        if ((inb(SB16_DSP_READ_STATUS) & 0x80) != 0) {
            return true;
        }
    }
    return false;  /* Timeout */
}

/*
 * Write a byte to the DSP (exported for DMA controller)
 */
bool sb16_dsp_write(uint8_t value) {
    if (!sb16_dsp_wait_write()) {
        return false;
    }
    outb(SB16_DSP_WRITE, value);
    return true;
}

/*
 * Read a byte from the DSP
 */
static bool sb16_dsp_read(uint8_t* value) {
    if (!sb16_dsp_wait_read()) {
        return false;
    }
    *value = inb(SB16_DSP_READ);
    return true;
}

/*
 * Reset the DSP
 */
static bool sb16_dsp_reset(void) {

    /* Send reset command */
    outb(SB16_DSP_RESET, 1);

    /* Wait 3 microseconds (busy loop approximation) */
    for (volatile int i = 0; i < 100; i++);

    outb(SB16_DSP_RESET, 0);

    /* Wait for DSP ready (should return 0xAA) */
    uint8_t response;
    if (!sb16_dsp_read(&response)) {
        SND_LOG_DEBUG("SB16: DSP reset timeout\n");
        return false;
    }

    if (response != 0xAA) {
        SND_LOG_DEBUG("SB16: DSP reset failed (got 0x%02x)\n", response);
        return false;
    }

    return true;
}

/*
 * Get DSP version
 */
static bool sb16_get_version(void) {

    if (!sb16_dsp_write(DSP_CMD_GET_VERSION)) {
        return false;
    }

    if (!sb16_dsp_read(&g_dsp_version_major)) {
        return false;
    }

    if (!sb16_dsp_read(&g_dsp_version_minor)) {
        return false;
    }

    SND_LOG_DEBUG("SB16: DSP version %d.%d\n", g_dsp_version_major, g_dsp_version_minor);
    return true;
}

/*
 * Set mixer volume
 */
static void sb16_set_volume(uint8_t left, uint8_t right) {
    uint8_t volume = (left << 4) | right;

    outb(SB16_MIXER_ADDR, MIXER_MASTER_VOLUME);
    outb(SB16_MIXER_DATA, volume);

    outb(SB16_MIXER_ADDR, MIXER_VOICE_VOLUME);
    outb(SB16_MIXER_DATA, volume);
}

/*
 * Initialize Sound Blaster 16
 */
int SB16_Init(void) {

    if (g_sb16_initialized) {
        return 0;
    }

    SND_LOG_DEBUG("SB16: Initializing Sound Blaster 16...\n");

    /* Reset DSP */
    if (!sb16_dsp_reset()) {
        SND_LOG_DEBUG("SB16: Failed to reset DSP\n");
        return -1;
    }

    /* Get version */
    if (!sb16_get_version()) {
        SND_LOG_DEBUG("SB16: Failed to get DSP version\n");
        return -1;
    }

    /* Check if we have at least SB16 (version 4.x) */
    if (g_dsp_version_major < 4) {
        SND_LOG_DEBUG("SB16: DSP version too old (need 4.x, got %d.%d)\n",
                     g_dsp_version_major, g_dsp_version_minor);
        return -1;
    }

    /* Set volume to maximum */
    sb16_set_volume(15, 15);

    /* Turn on speaker */
    sb16_dsp_write(DSP_CMD_TURN_ON_SPEAKER);

    g_sb16_initialized = true;
    SND_LOG_DEBUG("SB16: Initialized successfully\n");

    return 0;
}

/*
 * Shutdown Sound Blaster 16
 */
void SB16_StopPlayback(void) {
    if (!g_sb16_initialized) {
        return;
    }
    sb16_dsp_write(DSP_CMD_TURN_OFF_SPEAKER);
}

void SB16_Shutdown(void) {
    if (!g_sb16_initialized) {
        return;
    }

    SB16_StopPlayback();

    /* Reset DSP */
    sb16_dsp_reset();

    g_sb16_initialized = false;
}

/*
 * Set sample rate (SB16 4.xx and later)
 */
static bool sb16_set_sample_rate(uint32_t sample_rate) {

    /* sample_rate is uint32_t: %u would pass a 4-byte int to printf. */
    SND_LOG_DEBUG("SB16: Setting sample rate to %lu Hz\n", (unsigned long)sample_rate);

    if (!sb16_dsp_write(DSP_CMD_SET_SAMPLE_RATE)) {
        return false;
    }

    /* Send high byte then low byte */
    if (!sb16_dsp_write((sample_rate >> 8) & 0xFF)) {
        return false;
    }

    if (!sb16_dsp_write(sample_rate & 0xFF)) {
        return false;
    }

    return true;
}

/*
 * Play audio using DMA
 * This is declared but implemented separately with DMA support
 */
/*
 * Play WAV data
 */
int SB16_PlayWAV(const uint8_t* data, uint32_t size,
                 uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample) {

    if (!g_sb16_initialized) {
        SND_LOG_DEBUG("SB16: Not initialized\n");
        return -1;
    }

    SND_LOG_DEBUG("SB16: Playing WAV - %lu Hz, %u channels, %u bits, %lu bytes\n",
                 (unsigned long)sample_rate, channels, bits_per_sample, (unsigned long)size);

    /* Set sample rate */
    if (!sb16_set_sample_rate(sample_rate)) {
        SND_LOG_DEBUG("SB16: Failed to set sample rate\n");
        return -1;
    }

    /* Play using DMA */
    return SB16_PlayDMA(data, size, sample_rate, channels, bits_per_sample);
}
