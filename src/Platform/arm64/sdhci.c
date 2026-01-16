/*
 * SDHCI SD/eMMC Card Driver for Raspberry Pi ARM64
 * Implements block device interface for SD card storage
 */

#include "sdhci.h"
#include "timer.h"
#include "uart.h"
#include "cache.h"
#include <string.h>

/* MMIO helpers */
static inline void mmio_write32(uint64_t addr, uint32_t value) {
    *(volatile uint32_t *)addr = value;
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline uint32_t mmio_read32(uint64_t addr) {
    uint32_t value = *(volatile uint32_t *)addr;
    __asm__ volatile("dsb sy" ::: "memory");
    return value;
}

static inline void mmio_write16(uint64_t addr, uint16_t value) {
    *(volatile uint16_t *)addr = value;
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline uint16_t mmio_read16(uint64_t addr) {
    uint16_t value = *(volatile uint16_t *)addr;
    __asm__ volatile("dsb sy" ::: "memory");
    return value;
}

static inline void mmio_write8(uint64_t addr, uint8_t value) {
    *(volatile uint8_t *)addr = value;
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline uint8_t mmio_read8(uint64_t addr) {
    uint8_t value = *(volatile uint8_t *)addr;
    __asm__ volatile("dsb sy" ::: "memory");
    return value;
}

/* DMA buffer - 4KB aligned for ARM64 cache coherency */
static uint8_t __attribute__((aligned(4096))) dma_buffer[4096];

/* SDHCI state */
static uint64_t sdhci_base = 0;
static bool sdhci_initialized = false;
static uint32_t card_rca = 0;
static bool card_high_capacity = false;
static uint64_t card_capacity = 0;  /* In blocks */

/*
 * Detect Raspberry Pi model and set SDHCI base address
 */
static bool sdhci_detect_base(void) {
    /* Try Pi 4 EMMC2 first (most common for ARM64) */
    uint64_t bases[] = {
        SDHCI_BASE_PI4,     /* Pi 4 EMMC2 */
        SDHCI_BASE_PI3,     /* Pi 3 EMMC */
        ARASAN_BASE_PI4,    /* Pi 4 ARASAN (alternate) */
    };

    for (int i = 0; i < 3; i++) {
        uint32_t ver = mmio_read16(bases[i] + SDHCI_HOST_VERSION);
        uint32_t caps = mmio_read32(bases[i] + SDHCI_CAPABILITIES);

        /* Check for valid SDHCI version (should be 0x00-0x05) */
        if ((ver & 0xFF) <= 0x05 && caps != 0 && caps != 0xFFFFFFFF) {
            sdhci_base = bases[i];
            uart_puts("[SDHCI] Found controller at 0x");
            /* Simple hex print */
            char hex[17];
            uint64_t val = sdhci_base;
            for (int j = 15; j >= 0; j--) {
                hex[j] = "0123456789ABCDEF"[val & 0xF];
                val >>= 4;
            }
            hex[16] = '\0';
            uart_puts(hex);
            uart_puts("\n");
            return true;
        }
    }

    return false;
}

/*
 * Wait for command inhibit to clear
 */
static bool sdhci_wait_cmd_ready(uint32_t timeout_ms) {
    uint32_t timeout = timeout_ms * 100;
    while (timeout--) {
        uint32_t state = mmio_read32(sdhci_base + SDHCI_PRESENT_STATE);
        if (!(state & PRESENT_CMD_INHIBIT)) {
            return true;
        }
        timer_usleep(10);
    }
    return false;
}

/*
 * Wait for data inhibit to clear
 */
static bool sdhci_wait_data_ready(uint32_t timeout_ms) {
    uint32_t timeout = timeout_ms * 100;
    while (timeout--) {
        uint32_t state = mmio_read32(sdhci_base + SDHCI_PRESENT_STATE);
        if (!(state & PRESENT_DAT_INHIBIT)) {
            return true;
        }
        timer_usleep(10);
    }
    return false;
}

/*
 * Send a command to the SD card
 */
static int sdhci_send_command(uint8_t cmd_index, uint32_t arg, uint8_t resp_type, uint32_t *response) {
    /* Wait for command ready */
    if (!sdhci_wait_cmd_ready(1000)) {
        uart_puts("[SDHCI] Command inhibit timeout\n");
        return -1;
    }

    /* Clear all interrupt status */
    mmio_write32(sdhci_base + SDHCI_INT_STATUS, 0xFFFFFFFF);

    /* Set argument */
    mmio_write32(sdhci_base + SDHCI_ARGUMENT, arg);

    /* Build command register value */
    uint16_t cmd = (cmd_index << 8);

    switch (resp_type) {
        case RESP_NONE:
            /* No response */
            break;
        case RESP_R1:
        case RESP_R6:
        case RESP_R7:
            cmd |= 0x1A;  /* 48-bit, CRC check, index check */
            break;
        case RESP_R1B:
            cmd |= 0x1B;  /* 48-bit with busy, CRC, index */
            break;
        case RESP_R2:
            cmd |= 0x09;  /* 136-bit, CRC check */
            break;
        case RESP_R3:
            cmd |= 0x02;  /* 48-bit, no checks */
            break;
    }

    /* Issue command */
    mmio_write16(sdhci_base + SDHCI_COMMAND, cmd);

    /* Wait for command complete or error */
    uint32_t timeout = 100000;
    while (timeout--) {
        uint32_t status = mmio_read32(sdhci_base + SDHCI_INT_STATUS);

        if (status & INT_ERROR_MASK) {
            mmio_write32(sdhci_base + SDHCI_INT_STATUS, status);
            return -1;
        }

        if (status & INT_CMD_COMPLETE) {
            mmio_write32(sdhci_base + SDHCI_INT_STATUS, INT_CMD_COMPLETE);

            /* Read response if needed */
            if (response && resp_type != RESP_NONE) {
                if (resp_type == RESP_R2) {
                    response[0] = mmio_read32(sdhci_base + SDHCI_RESPONSE + 0);
                    response[1] = mmio_read32(sdhci_base + SDHCI_RESPONSE + 4);
                    response[2] = mmio_read32(sdhci_base + SDHCI_RESPONSE + 8);
                    response[3] = mmio_read32(sdhci_base + SDHCI_RESPONSE + 12);
                } else {
                    response[0] = mmio_read32(sdhci_base + SDHCI_RESPONSE);
                }
            }
            return 0;
        }
    }

    uart_puts("[SDHCI] Command timeout\n");
    return -1;
}

/*
 * Send application command (CMD55 + ACMD)
 */
static int sdhci_send_app_command(uint8_t cmd_index, uint32_t arg, uint8_t resp_type, uint32_t *response) {
    uint32_t resp[4];

    /* CMD55 - prepare for ACMD */
    if (sdhci_send_command(SD_CMD55_APP_CMD, card_rca << 16, RESP_R1, resp) != 0) {
        return -1;
    }

    /* Send actual ACMD */
    return sdhci_send_command(cmd_index, arg, resp_type, response);
}

/*
 * Reset SDHCI controller
 */
static bool sdhci_reset(void) {
    /* Reset all */
    mmio_write8(sdhci_base + SDHCI_SOFTWARE_RESET, 0x07);

    /* Wait for reset to complete */
    uint32_t timeout = 10000;
    while (timeout--) {
        if ((mmio_read8(sdhci_base + SDHCI_SOFTWARE_RESET) & 0x07) == 0) {
            return true;
        }
        timer_usleep(10);
    }

    return false;
}

/*
 * Set clock frequency
 */
static void sdhci_set_clock(uint32_t freq_khz) {
    /* Disable clock first */
    mmio_write16(sdhci_base + SDHCI_CLOCK_CONTROL, 0);

    /* Calculate divisor (assuming 100MHz base clock) */
    uint32_t base_clock = 100000;  /* 100 MHz in kHz */
    uint32_t divisor = base_clock / freq_khz;
    if (divisor > 0) divisor--;
    if (divisor > 255) divisor = 255;

    /* Set clock divisor and enable internal clock */
    uint16_t clk = (divisor << 8) | 0x01;
    mmio_write16(sdhci_base + SDHCI_CLOCK_CONTROL, clk);

    /* Wait for internal clock stable */
    uint32_t timeout = 10000;
    while (timeout--) {
        if (mmio_read16(sdhci_base + SDHCI_CLOCK_CONTROL) & 0x02) {
            break;
        }
        timer_usleep(10);
    }

    /* Enable SD clock */
    clk |= 0x04;
    mmio_write16(sdhci_base + SDHCI_CLOCK_CONTROL, clk);
}

/*
 * Initialize SD card
 */
static bool sdhci_init_card(void) {
    uint32_t response[4];
    int retry;

    uart_puts("[SDHCI] Initializing SD card...\n");

    /* Set initial slow clock (400 kHz) */
    sdhci_set_clock(400);
    timer_usleep(10000);

    /* CMD0 - Go idle */
    sdhci_send_command(SD_CMD0_GO_IDLE, 0, RESP_NONE, NULL);
    timer_usleep(10000);

    /* CMD8 - Send interface condition (check SD v2.0) */
    if (sdhci_send_command(SD_CMD8_SEND_IF_COND, 0x1AA, RESP_R7, response) == 0) {
        if ((response[0] & 0xFF) == 0xAA) {
            uart_puts("[SDHCI] SD v2.0 card detected\n");
            card_high_capacity = true;
        }
    } else {
        uart_puts("[SDHCI] SD v1.0 card\n");
        card_high_capacity = false;
    }

    /* ACMD41 - Initialize card */
    retry = 100;
    while (retry--) {
        uint32_t ocr_arg = 0x00FF8000;  /* Voltage range */
        if (card_high_capacity) {
            ocr_arg |= 0x40000000;  /* HCS bit */
        }

        if (sdhci_send_app_command(SD_ACMD41_SEND_OP_COND, ocr_arg, RESP_R3, response) == 0) {
            if (response[0] & 0x80000000) {  /* Card ready */
                if (response[0] & 0x40000000) {
                    card_high_capacity = true;
                    uart_puts("[SDHCI] High capacity (SDHC/SDXC) card\n");
                }
                break;
            }
        }
        timer_usleep(10000);
    }

    if (retry < 0) {
        uart_puts("[SDHCI] Card init timeout\n");
        return false;
    }

    /* CMD2 - Get CID */
    if (sdhci_send_command(SD_CMD2_ALL_SEND_CID, 0, RESP_R2, response) != 0) {
        uart_puts("[SDHCI] CMD2 failed\n");
        return false;
    }

    /* CMD3 - Get RCA */
    if (sdhci_send_command(SD_CMD3_SEND_RCA, 0, RESP_R6, response) != 0) {
        uart_puts("[SDHCI] CMD3 failed\n");
        return false;
    }
    card_rca = (response[0] >> 16) & 0xFFFF;

    /* CMD9 - Get CSD */
    if (sdhci_send_command(SD_CMD9_SEND_CSD, card_rca << 16, RESP_R2, response) != 0) {
        uart_puts("[SDHCI] CMD9 failed\n");
        return false;
    }

    /* Calculate capacity from CSD */
    uint8_t csd_structure = (response[3] >> 22) & 0x3;
    if (csd_structure == 1) {
        /* CSD v2.0 (SDHC/SDXC) */
        uint32_t c_size = ((response[2] & 0x3F) << 16) | ((response[1] >> 16) & 0xFFFF);
        card_capacity = (uint64_t)(c_size + 1) * 1024;  /* In 512-byte blocks */
    } else {
        /* CSD v1.0 */
        uint32_t c_size = ((response[2] & 0x3FF) << 2) | ((response[1] >> 30) & 0x3);
        uint32_t c_size_mult = (response[1] >> 15) & 0x7;
        uint32_t read_bl_len = (response[2] >> 16) & 0xF;
        card_capacity = ((uint64_t)(c_size + 1) << (c_size_mult + 2 + read_bl_len)) / 512;
    }

    uart_puts("[SDHCI] Card capacity: ");
    /* Print capacity in MB */
    uint32_t mb = (uint32_t)(card_capacity / 2048);
    char num[16];
    int i = 0;
    if (mb == 0) {
        num[i++] = '0';
    } else {
        uint32_t tmp = mb;
        int digits = 0;
        while (tmp > 0) {
            digits++;
            tmp /= 10;
        }
        i = digits;
        num[i] = '\0';
        while (mb > 0) {
            num[--i] = '0' + (mb % 10);
            mb /= 10;
        }
        i = digits;
    }
    num[i] = '\0';
    uart_puts(num);
    uart_puts(" MB\n");

    /* CMD7 - Select card */
    if (sdhci_send_command(SD_CMD7_SELECT_CARD, card_rca << 16, RESP_R1B, response) != 0) {
        uart_puts("[SDHCI] CMD7 failed\n");
        return false;
    }

    /* Increase clock to 25 MHz */
    sdhci_set_clock(25000);

    /* Set block size */
    mmio_write16(sdhci_base + SDHCI_BLOCK_SIZE, SDHCI_BLOCK_SIZE_512);

    uart_puts("[SDHCI] Card initialized successfully\n");
    return true;
}

/*
 * Initialize SDHCI controller
 */
bool sdhci_init(void) {
    if (sdhci_initialized) {
        return true;
    }

    uart_puts("[SDHCI] Initializing controller...\n");

    /* Detect controller */
    if (!sdhci_detect_base()) {
        uart_puts("[SDHCI] No controller found\n");
        return false;
    }

    /* Reset controller */
    if (!sdhci_reset()) {
        uart_puts("[SDHCI] Reset failed\n");
        return false;
    }

    /* Enable power (3.3V) */
    mmio_write8(sdhci_base + SDHCI_POWER_CONTROL, 0x0F);
    timer_usleep(10000);

    /* Set timeout */
    mmio_write8(sdhci_base + SDHCI_TIMEOUT_CONTROL, 0x0E);

    /* Enable interrupts */
    mmio_write32(sdhci_base + SDHCI_INT_ENABLE, 0x017F00FF);
    mmio_write32(sdhci_base + SDHCI_INT_SIGNAL_ENABLE, 0);

    /* Check if card is present */
    uint32_t state = mmio_read32(sdhci_base + SDHCI_PRESENT_STATE);
    if (!(state & PRESENT_CARD_INSERTED)) {
        uart_puts("[SDHCI] No card inserted\n");
        /* Don't fail - card may be inserted later */
        sdhci_initialized = true;
        return true;
    }

    /* Initialize the card */
    if (!sdhci_init_card()) {
        uart_puts("[SDHCI] Card init failed\n");
        /* Controller is still usable */
    }

    sdhci_initialized = true;
    return true;
}

/*
 * Check if SDHCI is initialized
 */
bool sdhci_is_initialized(void) {
    return sdhci_initialized;
}

/*
 * Check if card is present
 */
bool sdhci_card_present(void) {
    if (!sdhci_initialized || sdhci_base == 0) {
        return false;
    }

    uint32_t state = mmio_read32(sdhci_base + SDHCI_PRESENT_STATE);
    return (state & PRESENT_CARD_INSERTED) != 0;
}

/*
 * Get card capacity in blocks
 */
uint64_t sdhci_get_capacity(void) {
    return card_capacity;
}

/*
 * Get sector size
 */
uint32_t sdhci_get_sector_size(void) {
    return SDHCI_BLOCK_SIZE_512;
}

/*
 * Wait for data transfer complete
 */
static int sdhci_wait_data_complete(uint32_t timeout_ms) {
    uint32_t timeout = timeout_ms * 100;

    while (timeout--) {
        uint32_t status = mmio_read32(sdhci_base + SDHCI_INT_STATUS);

        if (status & INT_ERROR_MASK) {
            mmio_write32(sdhci_base + SDHCI_INT_STATUS, status);
            return -1;
        }

        if (status & INT_DATA_END) {
            mmio_write32(sdhci_base + SDHCI_INT_STATUS, INT_DATA_END);
            return 0;
        }

        timer_usleep(10);
    }

    return -1;
}

/*
 * Read blocks from SD card
 */
int sdhci_read_blocks(uint64_t lba, uint32_t count, void *buffer) {
    if (!sdhci_initialized || !buffer || count == 0) {
        return -1;
    }

    if (!sdhci_card_present() || card_capacity == 0) {
        return -1;
    }

    uint8_t *dest = (uint8_t *)buffer;
    uint32_t blocks_read = 0;

    while (count > 0) {
        /* Read up to 8 blocks at a time (limited by DMA buffer) */
        uint32_t chunk = (count > 8) ? 8 : count;

        /* Wait for data line ready */
        if (!sdhci_wait_data_ready(1000)) {
            return blocks_read > 0 ? (int)blocks_read : -1;
        }

        /* Set up transfer */
        mmio_write16(sdhci_base + SDHCI_BLOCK_SIZE, SDHCI_BLOCK_SIZE_512);
        mmio_write16(sdhci_base + SDHCI_BLOCK_COUNT, chunk);

        /* Clear interrupts */
        mmio_write32(sdhci_base + SDHCI_INT_STATUS, 0xFFFFFFFF);

        /* Calculate address (block or byte depending on card type) */
        uint32_t addr = card_high_capacity ? (uint32_t)lba : (uint32_t)(lba * 512);

        /* Send read command */
        uint8_t cmd = (chunk == 1) ? SD_CMD17_READ_SINGLE : SD_CMD18_READ_MULTIPLE;

        /* Set transfer mode (read, single/multi block) */
        uint16_t mode = 0x0010;  /* Read direction */
        if (chunk > 1) {
            mode |= 0x0020 | 0x0002 | 0x0004;  /* Multi-block, block count enable, auto CMD12 */
        }
        mmio_write16(sdhci_base + SDHCI_TRANSFER_MODE, mode);

        /* Send command */
        uint32_t response[4];
        if (sdhci_send_command(cmd, addr, RESP_R1, response) != 0) {
            return blocks_read > 0 ? (int)blocks_read : -1;
        }

        /* Wait for transfer complete */
        if (sdhci_wait_data_complete(5000) != 0) {
            /* Send stop command on error */
            if (chunk > 1) {
                sdhci_send_command(SD_CMD12_STOP_TRANS, 0, RESP_R1B, NULL);
            }
            return blocks_read > 0 ? (int)blocks_read : -1;
        }

        /* Read data from buffer */
        for (uint32_t i = 0; i < chunk; i++) {
            /* Wait for buffer read ready */
            uint32_t timeout = 10000;
            while (timeout--) {
                uint32_t state = mmio_read32(sdhci_base + SDHCI_PRESENT_STATE);
                if (state & PRESENT_BUFFER_READ) {
                    break;
                }
            }

            /* Read 512 bytes (128 x 32-bit words) */
            uint32_t *dst32 = (uint32_t *)(dest + i * 512);
            for (int j = 0; j < 128; j++) {
                dst32[j] = mmio_read32(sdhci_base + SDHCI_BUFFER_DATA_PORT);
            }
        }

        dest += chunk * 512;
        lba += chunk;
        count -= chunk;
        blocks_read += chunk;
    }

    return (int)blocks_read;
}

/*
 * Write blocks to SD card
 */
int sdhci_write_blocks(uint64_t lba, uint32_t count, const void *buffer) {
    if (!sdhci_initialized || !buffer || count == 0) {
        return -1;
    }

    if (!sdhci_card_present() || card_capacity == 0) {
        return -1;
    }

    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t blocks_written = 0;

    while (count > 0) {
        /* Write up to 8 blocks at a time */
        uint32_t chunk = (count > 8) ? 8 : count;

        /* Wait for data line ready */
        if (!sdhci_wait_data_ready(1000)) {
            return blocks_written > 0 ? (int)blocks_written : -1;
        }

        /* Set up transfer */
        mmio_write16(sdhci_base + SDHCI_BLOCK_SIZE, SDHCI_BLOCK_SIZE_512);
        mmio_write16(sdhci_base + SDHCI_BLOCK_COUNT, chunk);

        /* Clear interrupts */
        mmio_write32(sdhci_base + SDHCI_INT_STATUS, 0xFFFFFFFF);

        /* Calculate address */
        uint32_t addr = card_high_capacity ? (uint32_t)lba : (uint32_t)(lba * 512);

        /* Send write command */
        uint8_t cmd = (chunk == 1) ? SD_CMD24_WRITE_SINGLE : SD_CMD25_WRITE_MULTIPLE;

        /* Set transfer mode (write, single/multi block) */
        uint16_t mode = 0x0000;  /* Write direction */
        if (chunk > 1) {
            mode |= 0x0020 | 0x0002 | 0x0004;  /* Multi-block, block count enable, auto CMD12 */
        }
        mmio_write16(sdhci_base + SDHCI_TRANSFER_MODE, mode);

        /* Send command */
        uint32_t response[4];
        if (sdhci_send_command(cmd, addr, RESP_R1, response) != 0) {
            return blocks_written > 0 ? (int)blocks_written : -1;
        }

        /* Write data to buffer */
        for (uint32_t i = 0; i < chunk; i++) {
            /* Wait for buffer write ready */
            uint32_t timeout = 10000;
            while (timeout--) {
                uint32_t state = mmio_read32(sdhci_base + SDHCI_PRESENT_STATE);
                if (state & PRESENT_BUFFER_WRITE) {
                    break;
                }
            }

            /* Write 512 bytes (128 x 32-bit words) */
            const uint32_t *src32 = (const uint32_t *)(src + i * 512);
            for (int j = 0; j < 128; j++) {
                mmio_write32(sdhci_base + SDHCI_BUFFER_DATA_PORT, src32[j]);
            }
        }

        /* Wait for transfer complete */
        if (sdhci_wait_data_complete(5000) != 0) {
            /* Send stop command on error */
            if (chunk > 1) {
                sdhci_send_command(SD_CMD12_STOP_TRANS, 0, RESP_R1B, NULL);
            }
            return blocks_written > 0 ? (int)blocks_written : -1;
        }

        src += chunk * 512;
        lba += chunk;
        count -= chunk;
        blocks_written += chunk;
    }

    return (int)blocks_written;
}
