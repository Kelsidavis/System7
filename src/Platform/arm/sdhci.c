/*
 * SDHCI SD/eMMC Card Driver for Raspberry Pi
 * Implements block device interface for storage access with DMA support
 *
 * SDHCI = SD Host Controller Interface (standard specification)
 * This driver supports:
 * - Pi 3: SDHCI over Broadcom DMA
 * - Pi 4: SDHCI over PCIe (similar interface)
 * - Pi 5: SDHCI over PCIe (faster)
 *
 * DMA Buffer Management:
 * - Pre-allocated 4MB DMA buffer for bulk transfers
 * - Aligned to 4KB pages for optimal DMA performance
 * - Supports up to 8 concurrent blocks per transfer
 *
 * References:
 *   - JEDEC SD Specification
 *   - SD Host Controller Standard Specification
 *   - Raspberry Pi Firmware source
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "System71StdLib.h"
#include "mmio.h"
#include "sdhci.h"

/* SDHCI register base addresses
 * These vary by Raspberry Pi model
 */
#define SDHCI_BASE_PI34         0x3F300000
#define SDHCI_BASE_PI5          0xFE330000

static uint32_t sdhci_base = 0;
static int sdhci_initialized = 0;

/* DMA Buffer Management
 * Allocate a static buffer for DMA operations
 * Size: 4MB (sufficient for 8192 blocks of 512 bytes)
 */
#define DMA_BUFFER_SIZE         (4 * 1024 * 1024)
#define DMA_BLOCK_SIZE          512
#define DMA_MAX_BLOCKS          (DMA_BUFFER_SIZE / DMA_BLOCK_SIZE)

/* Global DMA buffer - aligned to 4KB for ARM DMA */
__attribute__((aligned(4096)))
static uint8_t dma_buffer[DMA_BUFFER_SIZE];

/* SDHCI register offsets */
#define SDHCI_DMA_ADDRESS       0x00
#define SDHCI_BLOCK_SIZE        0x04
#define SDHCI_BLOCK_COUNT       0x06
#define SDHCI_COMMAND           0x0C
#define SDHCI_ARGUMENT          0x08
#define SDHCI_RESPONSE          0x10
#define SDHCI_BUFFER_DATA_PORT  0x20
#define SDHCI_PRESENT_STATE     0x24
#define SDHCI_HOST_CONTROL      0x28
#define SDHCI_POWER_CONTROL     0x29
#define SDHCI_BLOCK_GAP_CONTROL 0x2A
#define SDHCI_WAKE_UP_CONTROL   0x2B
#define SDHCI_CLOCK_CONTROL     0x2C
#define SDHCI_TIMEOUT_CONTROL   0x2E
#define SDHCI_SOFTWARE_RESET    0x2F
#define SDHCI_INT_STATUS        0x30
#define SDHCI_INT_ENABLE        0x34
#define SDHCI_INT_SIGNAL_ENABLE 0x38
#define SDHCI_CAPABILITIES      0x40
#define SDHCI_CAPABILITIES_1    0x44

/* SDHCI Interrupt Status Bits */
#define INT_RESPONSE            (1 << 0)    /* Command complete */
#define INT_DATA_END            (1 << 1)    /* Data transfer complete */
#define INT_DMA_INT             (1 << 3)    /* DMA interrupt */
#define INT_SPACE_AVAIL         (1 << 4)    /* Buffer space available */
#define INT_DATA_AVAIL          (1 << 5)    /* Data available */
#define INT_CARD_INSERT         (1 << 6)    /* Card inserted */
#define INT_CARD_REMOVE         (1 << 7)    /* Card removed */
#define INT_ERROR               (1 << 16)   /* Error occurred */

/* SD Card Commands */
#define CMD0    0   /* GO_IDLE_STATE */
#define CMD17   17  /* READ_SINGLE_BLOCK */
#define CMD18   18  /* READ_MULTIPLE_BLOCK */
#define CMD24   24  /* WRITE_BLOCK */
#define CMD25   25  /* WRITE_MULTIPLE_BLOCK */
#define CMD55   55  /* APP_CMD */

/* SD Card Info Structure */
typedef struct {
    uint32_t card_rca;
    uint32_t card_ocr;
    int card_version_2;
    uint32_t card_csd[4];
    uint32_t block_count;
} sd_card_info_t;

/*
 * Initialize SDHCI controller with DMA support
 */
int sdhci_init(void) {
    Serial_WriteString("[SDHCI] Initializing SD card controller with DMA support\n");

    /* Detect Pi model and set base address */
    /* TODO: Read device tree to determine correct base
     * For now, try Pi 3/4 first (most common)
     */
    sdhci_base = SDHCI_BASE_PI34;

    /* Verify we can read registers */
    uint32_t caps = mmio_read32(sdhci_base + SDHCI_CAPABILITIES);
    if (caps == 0) {
        Serial_WriteString("[SDHCI] Error: Cannot read SDHCI capabilities\n");
        return -1;
    }

    Serial_Printf("[SDHCI] Base address: 0x%lx, Capabilities: 0x%lx\n",
                  (unsigned long)sdhci_base, (unsigned long)caps);
    Serial_Printf("[SDHCI] DMA buffer: 0x%lx (size: %lu MB)\n",
                  (unsigned long)(uintptr_t)&dma_buffer[0],
                  (unsigned long)(DMA_BUFFER_SIZE / (1024 * 1024)));

    /* Reset controller */
    uint8_t reset = mmio_read8(sdhci_base + SDHCI_SOFTWARE_RESET);
    reset |= 0x01;  /* Reset all */
    mmio_write8(sdhci_base + SDHCI_SOFTWARE_RESET, reset);

    /* Wait for reset to complete */
    uint32_t timeout = 10000;
    while (timeout > 0) {
        reset = mmio_read8(sdhci_base + SDHCI_SOFTWARE_RESET);
        if ((reset & 0x01) == 0) {
            break;
        }
        timeout--;
    }

    if (timeout == 0) {
        Serial_WriteString("[SDHCI] Error: Reset timeout\n");
        return -1;
    }

    Serial_WriteString("[SDHCI] Controller reset complete\n");

    /* Set block size to 512 bytes (standard SD block size) */
    mmio_write16(sdhci_base + SDHCI_BLOCK_SIZE, 512);

    /* Enable DMA support */
    uint8_t host_ctrl = mmio_read8(sdhci_base + SDHCI_HOST_CONTROL);
    host_ctrl |= 0x04;  /* Enable DMA (bit 2) */
    mmio_write8(sdhci_base + SDHCI_HOST_CONTROL, host_ctrl);

    /* Enable relevant interrupts for DMA operation */
    uint32_t int_enable = INT_RESPONSE | INT_DATA_END | INT_DMA_INT | INT_ERROR;
    mmio_write32(sdhci_base + SDHCI_INT_ENABLE, int_enable);
    mmio_write32(sdhci_base + SDHCI_INT_SIGNAL_ENABLE, int_enable);

    /* Initialize the SD card */
    if (sdhci_init_card(sdhci_base) != 0) {
        Serial_WriteString("[SDHCI] Warning: Card initialization failed\n");
        /* Don't fail - card may be inserted later */
    } else {
        Serial_WriteString("[SDHCI] Card initialized successfully\n");
    }

    sdhci_initialized = 1;
    return 0;
}

/*
 * Wait for SDHCI interrupt
 * Returns interrupt status bits when interrupt occurs
 */
static uint32_t sdhci_wait_interrupt(uint32_t timeout_ms) {
    uint32_t timeout = timeout_ms * 1000;  /* Convert to iterations */

    while (timeout > 0) {
        uint32_t status = mmio_read32(sdhci_base + SDHCI_INT_STATUS);

        /* Check for data complete or error */
        if (status & (INT_DATA_END | INT_ERROR)) {
            /* Clear interrupt */
            mmio_write32(sdhci_base + SDHCI_INT_STATUS, status);
            return status;
        }

        /* Small delay */
        __asm__ __volatile__("nop");
        timeout--;
    }

    return 0;  /* Timeout */
}

/*
 * Read blocks from SD card using DMA
 * addr: Starting LBA (Logical Block Address)
 * count: Number of 512-byte blocks to read
 * buffer: Destination buffer (must be 4KB aligned for optimal DMA)
 * Returns: Number of blocks read on success, -1 on error
 */
int sdhci_read_blocks(uint32_t addr, uint32_t count, void *buffer) {
    if (!sdhci_initialized || !buffer) {
        return -1;
    }

    if (count == 0 || count > DMA_MAX_BLOCKS) {
        Serial_Printf("[SDHCI] Error: Invalid block count %lu\n", (unsigned long)count);
        return -1;
    }

    Serial_Printf("[SDHCI] Reading %lu blocks from LBA 0x%lx to 0x%lx\n",
                  (unsigned long)count, (unsigned long)addr, (unsigned long)(uintptr_t)buffer);

    /* Use DMA buffer for transfer */
    void *xfer_buffer = &dma_buffer[0];
    uint32_t xfer_addr = (uint32_t)xfer_buffer;

    /* Set DMA address (physical address for DMA) */
    mmio_write32(sdhci_base + SDHCI_DMA_ADDRESS, xfer_addr);

    /* Set block size (512 bytes) and block count */
    mmio_write16(sdhci_base + SDHCI_BLOCK_SIZE, 512);
    mmio_write16(sdhci_base + SDHCI_BLOCK_COUNT, count);

    /* Issue read command based on block count */
    uint32_t response[4] = {0};
    uint8_t cmd = (count == 1) ? CMD17 : CMD18;

    if (sdhci_send_command(sdhci_base, cmd, addr, 1, response) != 0) {
        Serial_Printf("[SDHCI] Read command failed for block 0x%lx\n", (unsigned long)addr);
        return -1;
    }

    /* Wait for data transfer to complete */
    uint32_t status = sdhci_wait_interrupt(5000);  /* 5 second timeout */

    if (!(status & INT_DATA_END)) {
        Serial_Printf("[SDHCI] Data transfer timeout (status: 0x%lx)\n", (unsigned long)status);
        return -1;
    }

    if (status & INT_ERROR) {
        Serial_Printf("[SDHCI] Read error (status: 0x%lx)\n", (unsigned long)status);
        return -1;
    }

    /* If CMD18 (multiple blocks), send CMD12 to stop transmission */
    if (cmd == CMD18) {
        uint32_t stop_response[4] = {0};
        sdhci_send_command(sdhci_base, 12, 0, 1, stop_response);  /* CMD12: STOP_TRANSMISSION */
    }

    /* Copy data from DMA buffer to user buffer */
    uint32_t bytes_to_copy = count * 512;
    memcpy(buffer, xfer_buffer, bytes_to_copy);

    Serial_Printf("[SDHCI] Successfully read %lu blocks\n", (unsigned long)count);
    return count;
}

/*
 * Write blocks to SD card using DMA
 * addr: Starting LBA
 * count: Number of 512-byte blocks to write
 * buffer: Source buffer
 * Returns: Number of blocks written on success, -1 on error
 */
int sdhci_write_blocks(uint32_t addr, uint32_t count, const void *buffer) {
    if (!sdhci_initialized || !buffer) {
        return -1;
    }

    if (count == 0 || count > DMA_MAX_BLOCKS) {
        Serial_Printf("[SDHCI] Error: Invalid block count %lu\n", (unsigned long)count);
        return -1;
    }

    Serial_Printf("[SDHCI] Writing %lu blocks to LBA 0x%lx from 0x%lx\n",
                  (unsigned long)count, (unsigned long)addr, (unsigned long)(uintptr_t)buffer);

    /* Copy data to DMA buffer first */
    void *xfer_buffer = &dma_buffer[0];
    uint32_t bytes_to_write = count * 512;
    memcpy(xfer_buffer, buffer, bytes_to_write);

    uint32_t xfer_addr = (uint32_t)xfer_buffer;

    /* Set DMA address (physical address for DMA) */
    mmio_write32(sdhci_base + SDHCI_DMA_ADDRESS, xfer_addr);

    /* Set block size (512 bytes) and block count */
    mmio_write16(sdhci_base + SDHCI_BLOCK_SIZE, 512);
    mmio_write16(sdhci_base + SDHCI_BLOCK_COUNT, count);

    /* Issue write command based on block count */
    uint32_t response[4] = {0};
    uint8_t cmd = (count == 1) ? CMD24 : CMD25;

    if (sdhci_send_command(sdhci_base, cmd, addr, 1, response) != 0) {
        Serial_Printf("[SDHCI] Write command failed for block 0x%lx\n", (unsigned long)addr);
        return -1;
    }

    /* Wait for data transfer to complete */
    uint32_t status = sdhci_wait_interrupt(5000);  /* 5 second timeout */

    if (!(status & INT_DATA_END)) {
        Serial_Printf("[SDHCI] Data transfer timeout (status: 0x%lx)\n", (unsigned long)status);
        return -1;
    }

    if (status & INT_ERROR) {
        Serial_Printf("[SDHCI] Write error (status: 0x%lx)\n", (unsigned long)status);
        return -1;
    }

    /* If CMD25 (multiple blocks), send CMD12 to stop transmission */
    if (cmd == CMD25) {
        uint32_t stop_response[4] = {0};
        sdhci_send_command(sdhci_base, 12, 0, 1, stop_response);  /* CMD12: STOP_TRANSMISSION */
    }

    Serial_Printf("[SDHCI] Successfully wrote %lu blocks\n", (unsigned long)count);
    return count;
}

/*
 * Get SD card information
 * Returns card block count and block size
 */
int sdhci_get_card_info(uint32_t *block_count) {
    if (!sdhci_initialized || !block_count) {
        return -1;
    }

    /* Calculate block count from CSD register
     * For now, use a default value (typical 2GB for test cards)
     * In production, parse CSD register to get actual capacity
     */
    *block_count = (UINT32_C(2) * 1024 * 1024 * 1024) / 512;

    return 0;
}

/*
 * Get card ready status
 */
int sdhci_card_ready(void) {
    if (!sdhci_initialized) {
        return 0;
    }

    uint32_t state = mmio_read32(sdhci_base + SDHCI_PRESENT_STATE);
    /* Bit 16: Card inserted, Bit 17: Card stable, Bit 20: Card ready */
    return ((state & (1 << 16)) != 0) && ((state & (1 << 20)) == 0);  /* Bit 20=0 means ready */
}

/*
 * Shutdown SDHCI controller
 */
void sdhci_shutdown(void) {
    Serial_WriteString("[SDHCI] Shutting down SD card controller\n");

    if (sdhci_base) {
        /* Disable interrupts */
        mmio_write32(sdhci_base + SDHCI_INT_ENABLE, 0);
        mmio_write32(sdhci_base + SDHCI_INT_SIGNAL_ENABLE, 0);
    }

    sdhci_initialized = 0;
}

/*
 * Check if card is detected
 */
int sdhci_card_present(void) {
    if (!sdhci_initialized) {
        return 0;
    }

    uint32_t state = mmio_read32(sdhci_base + SDHCI_PRESENT_STATE);
    /* Bit 16: Card inserted */
    return (state & (1 << 16)) != 0;
}
