/*
 * DMA_Controller.c - ISA DMA controller for Sound Blaster 16
 *
 * Implements DMA channel setup for audio playback.
 */

#include <stdint.h>
#include <stdbool.h>
#include "SoundManager/SoundLogging.h"
#include "SoundManager/SoundBlaster16.h"

/* I/O port access */
#include "Platform/include/io.h"

#define inb(port) hal_inb(port)
#define outb(port, value) hal_outb(port, value)

/* DMA Controller ports */
#define DMA1_STATUS         0x08  /* DMA 1 status (channels 0-3) */
#define DMA1_COMMAND        0x08  /* DMA 1 command */
#define DMA1_REQUEST        0x09  /* DMA 1 request */
#define DMA1_MASK           0x0A  /* DMA 1 single channel mask */
#define DMA1_MODE           0x0B  /* DMA 1 mode */
#define DMA1_CLEAR_FF       0x0C  /* DMA 1 clear flip-flop */
#define DMA1_RESET          0x0D  /* DMA 1 master reset */
#define DMA1_MASK_ALL       0x0F  /* DMA 1 mask all channels */

#define DMA2_STATUS         0xD0  /* DMA 2 status (channels 4-7) */
#define DMA2_COMMAND        0xD0  /* DMA 2 command */
#define DMA2_REQUEST        0xD2  /* DMA 2 request */
#define DMA2_MASK           0xD4  /* DMA 2 single channel mask */
#define DMA2_MODE           0xD6  /* DMA 2 mode */
#define DMA2_CLEAR_FF       0xD8  /* DMA 2 clear flip-flop */
#define DMA2_RESET          0xDA  /* DMA 2 master reset */
#define DMA2_MASK_ALL       0xDE  /* DMA 2 mask all channels */

/* DMA channel specific ports (channels 0-3) */
static const uint16_t dma1_addr_ports[] = { 0x00, 0x02, 0x04, 0x06 };
static const uint16_t dma1_count_ports[] = { 0x01, 0x03, 0x05, 0x07 };
static const uint16_t dma1_page_ports[] = { 0x87, 0x83, 0x81, 0x82 };

/* DMA channel specific ports (channels 4-7, note: addresses are word-based) */
static const uint16_t dma2_addr_ports[] = { 0xC0, 0xC4, 0xC8, 0xCC };
static const uint16_t dma2_count_ports[] = { 0xC2, 0xC6, 0xCA, 0xCE };
static const uint16_t dma2_page_ports[] = { 0x8F, 0x8B, 0x89, 0x8A };

/* DMA modes */
#define DMA_MODE_READ       0x48  /* Read from memory (device->memory) */
#define DMA_MODE_WRITE      0x44  /* Write to memory (memory->device) */
#define DMA_MODE_AUTO       0x10  /* Auto-init mode */
#define DMA_MODE_SINGLE     0x40  /* Single transfer mode */

/*
 * Set up DMA for audio playback (8-bit, channel 1)
 */
static int DMA_Setup8Bit(const void* buffer, uint32_t size) {

    const uint8_t channel = 1;  /* SB16 uses DMA channel 1 for 8-bit */
    uint32_t addr = (uint32_t)(uintptr_t)buffer;

    SND_LOG_DEBUG("DMA: Setting up 8-bit DMA on channel %d\n", channel);
    SND_LOG_DEBUG("DMA: Buffer at 0x%08x, size %u bytes\n", addr, size);

    /* Check alignment */
    if (size > 65536) {
        SND_LOG_DEBUG("DMA: Size too large (max 64KB)\n");
        return -1;
    }

    /* Disable DMA on channel */
    outb(DMA1_MASK, 0x04 | channel);  /* Set mask bit */

    /* Clear flip-flop */
    outb(DMA1_CLEAR_FF, 0xFF);

    /* Set DMA mode (single cycle, write) - no auto-repeat */
    outb(DMA1_MODE, DMA_MODE_WRITE | channel);

    /* Set address (low byte, high byte) */
    outb(dma1_addr_ports[channel], addr & 0xFF);
    outb(dma1_addr_ports[channel], (addr >> 8) & 0xFF);

    /* Set page (bits 16-23 of address) */
    outb(dma1_page_ports[channel], (addr >> 16) & 0xFF);

    /* Set count (length - 1, low byte, high byte) */
    uint16_t count = size - 1;
    outb(dma1_count_ports[channel], count & 0xFF);
    outb(dma1_count_ports[channel], (count >> 8) & 0xFF);

    /* Enable DMA on channel */
    outb(DMA1_MASK, channel);  /* Clear mask bit */

    SND_LOG_DEBUG("DMA: 8-bit DMA setup complete\n");
    return 0;
}

/*
 * Set up DMA for audio playback (16-bit, channel 5)
 */
static int DMA_Setup16Bit(const void* buffer, uint32_t size) {

    const uint8_t channel = 5;  /* SB16 uses DMA channel 5 for 16-bit */
    const uint8_t channel_offset = channel - 4;  /* DMA2 uses channels 4-7 */
    uint32_t addr = (uint32_t)(uintptr_t)buffer;

    SND_LOG_DEBUG("DMA: Setting up 16-bit DMA on channel %d\n", channel);
    SND_LOG_DEBUG("DMA: Buffer at 0x%08x, size %u bytes\n", addr, size);

    /* 16-bit DMA works in words, so size and address must be word-aligned */
    if (addr & 1) {
        SND_LOG_DEBUG("DMA: Address not word-aligned\n");
        return -1;
    }

    if (size & 1) {
        SND_LOG_DEBUG("DMA: Size not word-aligned\n");
        return -1;
    }

    if (size > 131072) {  /* 64K words = 128KB */
        SND_LOG_DEBUG("DMA: Size too large (max 128KB)\n");
        return -1;
    }

    /* Convert to word address and word count */
    uint32_t word_addr = addr >> 1;
    uint32_t word_count = (size >> 1) - 1;

    /* Disable DMA on channel */
    outb(DMA2_MASK, 0x04 | channel_offset);  /* Set mask bit */

    /* Clear flip-flop */
    outb(DMA2_CLEAR_FF, 0xFF);

    /* Set DMA mode (single cycle, write) - no auto-repeat */
    outb(DMA2_MODE, DMA_MODE_WRITE | channel_offset);

    /* Set address (low byte, high byte) - in WORDS */
    outb(dma2_addr_ports[channel_offset], word_addr & 0xFF);
    outb(dma2_addr_ports[channel_offset], (word_addr >> 8) & 0xFF);

    /* Set page (bits 16-23 of BYTE address) */
    outb(dma2_page_ports[channel_offset], (addr >> 16) & 0xFF);

    /* Set count (length - 1, low byte, high byte) - in WORDS */
    outb(dma2_count_ports[channel_offset], word_count & 0xFF);
    outb(dma2_count_ports[channel_offset], (word_count >> 8) & 0xFF);

    /* Enable DMA on channel */
    outb(DMA2_MASK, channel_offset);  /* Clear mask bit */

    SND_LOG_DEBUG("DMA: 16-bit DMA setup complete\n");
    return 0;
}

/*
 * Play audio via DMA (called from SB16 driver)
 */
int SB16_PlayDMA(const uint8_t* data, uint32_t size,
                 uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample) {

    /* DSP commands (single-cycle) */
    #define DSP_CMD_DMA16_MONO_SINGLE      0xB0
    #define DSP_CMD_DMA16_STEREO_SINGLE    0xB2
    #define DSP_CMD_DMA8_MONO_SINGLE       0xC0
    #define DSP_CMD_DMA8_STEREO_SINGLE     0xC2

    SND_LOG_DEBUG("DMA: Starting playback\n");

    /* Set up DMA */
    int dma_result;
    if (bits_per_sample == 16) {
        dma_result = DMA_Setup16Bit(data, size);
    } else {
        dma_result = DMA_Setup8Bit(data, size);
    }

    if (dma_result != 0) {
        SND_LOG_DEBUG("DMA: Setup failed\n");
        return -1;
    }

    /* Calculate transfer size (in samples) */
    uint32_t sample_size = (bits_per_sample / 8) * channels;
    uint32_t dma_count;

    if (bits_per_sample == 16) {
        dma_count = (size / 2) - 1;  /* Length in 16-bit words */
    } else {
        dma_count = size - 1;        /* Length in bytes */
    }

    SND_LOG_DEBUG("DMA: size=%u bytes (%u bytes per frame)\n", size, sample_size);

    /* Select DSP command based on bit depth and channels */
    uint8_t dsp_cmd;
    if (bits_per_sample == 16) {
        dsp_cmd = (channels == 2) ? DSP_CMD_DMA16_STEREO_SINGLE : DSP_CMD_DMA16_MONO_SINGLE;
    } else {
        dsp_cmd = (channels == 2) ? DSP_CMD_DMA8_STEREO_SINGLE : DSP_CMD_DMA8_MONO_SINGLE;
    }

    /* Send DMA playback command */
    if (!sb16_dsp_write(dsp_cmd)) {
        SND_LOG_DEBUG("DMA: Failed to send DSP command\n");
        return -1;
    }

    /* Send mode (unsigned PCM, stereo/mono) */
    uint8_t mode = (channels == 2) ? 0x20 : 0x00;  /* Bit 5 = stereo */
    if (bits_per_sample == 16) {
        mode |= 0x10;  /* Signed data */
    }
    if (!sb16_dsp_write(mode)) {
        SND_LOG_DEBUG("DMA: Failed to send mode\n");
        return -1;
    }

    /* Send count (low byte, high byte) */
    if (!sb16_dsp_write(dma_count & 0xFF)) {
        SND_LOG_DEBUG("DMA: Failed to send count low\n");
        return -1;
    }

    if (!sb16_dsp_write((dma_count >> 8) & 0xFF)) {
        SND_LOG_DEBUG("DMA: Failed to send count high\n");
        return -1;
    }

    SND_LOG_DEBUG("DMA: Playback started\n");

    return 0;
}
