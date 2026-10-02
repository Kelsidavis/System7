/*
 * SDHCI Command Implementation
 * SD card communication protocol and command sequences
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "System71StdLib.h"
#include "mmio.h"
#include "sdhci.h"

/* ===== SD Card Command Codes ===== */
#define CMD0    0   /* GO_IDLE_STATE */
#define CMD1    1   /* SEND_OP_COND */
#define CMD2    2   /* ALL_SEND_CID */
#define CMD3    3   /* SET_RELATIVE_ADDR */
#define CMD7    7   /* SELECT_CARD */
#define CMD8    8   /* SEND_IF_COND */
#define CMD9    9   /* SEND_CSD */
#define CMD10   10  /* SEND_CID */
#define CMD13   13  /* SEND_STATUS */
#define CMD17   17  /* READ_SINGLE_BLOCK */
#define CMD18   18  /* READ_MULTIPLE_BLOCK */
#define CMD24   24  /* WRITE_BLOCK */
#define CMD25   25  /* WRITE_MULTIPLE_BLOCK */
#define CMD55   55  /* APP_CMD */
#define ACMD41  41  /* SEND_OP_COND (with APP_CMD prefix) */

/* ===== SD Card Response Types ===== */
#define RESP_NONE       0   /* No response */
#define RESP_R1         1   /* 48-bit response */
#define RESP_R1B        2   /* 48-bit response with busy signal */
#define RESP_R2         3   /* 136-bit response (CID/CSD) */
#define RESP_R3         4   /* 48-bit response (OCR) */
#define RESP_R6         6   /* 48-bit response (RCA) */
#define RESP_R7         7   /* 48-bit response (IF_COND) */

/* ===== SD Card Status Register Bits ===== */
#define SDCARD_STATUS_READY_FOR_DATA    (1 << 8)
#define SDCARD_STATUS_CURRENT_STATE     (0xF << 9)

/* Global SD card info */
static uint32_t card_rca = 0;           /* Card relative address */
static uint32_t card_ocr = 0;           /* Operating conditions register */
static uint32_t card_csd[4] = {0};      /* Card-specific data */
static int card_version_2 = 0;          /* v2.0 vs v1.0 */

/*
 * Build command word for SDHCI_COMMAND register
 */
static uint32_t build_command(uint8_t index, uint16_t arg, uint8_t resp_type) {
    (void)arg;
    uint32_t cmd = 0;

    /* Command index (bits 13:8) */
    cmd |= (index & 0x3F) << 8;

    /* Response type (bits 1:0) */
    switch (resp_type) {
        case RESP_NONE:
            cmd |= 0;  /* No response */
            break;
        case RESP_R1:
        case RESP_R1B:
        case RESP_R6:
        case RESP_R7:
            cmd |= 1;  /* 48-bit response */
            break;
        case RESP_R2:
            cmd |= 2;  /* 136-bit response */
            break;
        case RESP_R3:
            cmd |= 1;  /* 48-bit response */
            break;
    }

    /* Busy bit for R1B responses */
    if (resp_type == RESP_R1B) {
        cmd |= (1 << 3);
    }

    return cmd;
}

/*
 * Send SD card command
 * Returns: 0 on success, -1 on timeout/error
 */
int sdhci_send_command(uint32_t sdhci_base, uint8_t cmd_index, uint32_t cmd_arg,
                       uint8_t resp_type, uint32_t *response) {
    if (!sdhci_base) {
        return -1;
    }

    volatile uint32_t *cmd_reg = (volatile uint32_t *)(sdhci_base + 0x0E);
    volatile uint32_t *arg_reg = (volatile uint32_t *)(sdhci_base + 0x08);
    volatile uint32_t *resp_reg = (volatile uint32_t *)(sdhci_base + 0x10);
    volatile uint32_t *int_status = (volatile uint32_t *)(sdhci_base + 0x30);

    /* Set command argument */
    mmio_write32((uint32_t)arg_reg, cmd_arg);

    /* Build and send command */
    uint32_t cmd = build_command(cmd_index, cmd_arg, resp_type);
    cmd |= (1 << 7);  /* Execute bit */
    mmio_write32((uint32_t)cmd_reg, cmd);

    /* Wait for command complete or error */
    uint32_t timeout = 100000;
    while (timeout--) {
        uint32_t status = mmio_read32((uint32_t)int_status);

        if (status & (1 << 0)) {
            /* Command complete */
            if (response) {
                /* Read response based on type */
                if (resp_type == RESP_R2) {
                    /* 136-bit response (4 x 32-bit words) */
                    response[0] = mmio_read32((uint32_t)resp_reg);
                    response[1] = mmio_read32((uint32_t)(resp_reg + 1));
                    response[2] = mmio_read32((uint32_t)(resp_reg + 2));
                    response[3] = mmio_read32((uint32_t)(resp_reg + 3));
                } else {
                    /* 48-bit response (1 x 32-bit word) */
                    response[0] = mmio_read32((uint32_t)resp_reg);
                }
            }

            /* Clear interrupt */
            mmio_write32((uint32_t)int_status, 1);
            return 0;
        }

        if (status & (1 << 16)) {
            /* Command timeout */
            Serial_Printf("[SDHCI] Command %u timeout\n", cmd_index);
            mmio_write32((uint32_t)int_status, (1 << 16));
            return -1;
        }

        if (status & (1 << 15)) {
            /* Command CRC error */
            Serial_Printf("[SDHCI] Command %u CRC error\n", cmd_index);
            mmio_write32((uint32_t)int_status, (1 << 15));
            return -1;
        }
    }

    Serial_Printf("[SDHCI] Command %u timeout (no interrupt)\n", cmd_index);
    return -1;
}

/*
 * Initialize SD card
 * Performs card detection, negotiation, and setup sequence
 */
int sdhci_init_card(uint32_t sdhci_base) {
    Serial_WriteString("[SDHCI] Initializing SD card...\n");

    uint32_t response[4] = {0};
    int retry = 10;

    /* CMD0: GO_IDLE_STATE - Reset card */
    if (sdhci_send_command(sdhci_base, CMD0, 0, RESP_NONE, NULL) != 0) {
        Serial_WriteString("[SDHCI] CMD0 failed\n");
        return -1;
    }
    Serial_WriteString("[SDHCI] Card reset (CMD0 OK)\n");

    /* CMD8: SEND_IF_COND - Check voltage and get version */
    if (sdhci_send_command(sdhci_base, CMD8, 0x000001AA, RESP_R7, response) == 0) {
        Serial_Printf("[SDHCI] SD v2.0 card detected (CMD8 response: 0x%lx)\n",
                      (unsigned long)response[0]);
        card_version_2 = 1;
    } else {
        Serial_WriteString("[SDHCI] SD v1.0 card detected (CMD8 failed)\n");
        card_version_2 = 0;
    }

    /* CMD55/ACMD41: Initialize card voltage and capacity
     * This is an application-specific command sequence
     */
    while (retry--) {
        uint32_t ocr_arg = 0x00300000;  /* 3.2-3.3V range */
        if (card_version_2) {
            ocr_arg |= 0x40000000;  /* HCS (High Capacity Support) for v2.0 */
        }

        /* CMD55: Prepare for ACMD */
        if (sdhci_send_command(sdhci_base, CMD55, 0, RESP_R1, response) != 0) {
            continue;
        }

        /* ACMD41: Send operating condition */
        if (sdhci_send_command(sdhci_base, ACMD41, ocr_arg, RESP_R3, response) != 0) {
            continue;
        }

        card_ocr = response[0];

        /* Check if card is ready (bit 31 = ready) */
        if (card_ocr & 0x80000000) {
            Serial_Printf("[SDHCI] Card ready, OCR: 0x%lx\n", (unsigned long)card_ocr);
            break;
        }

        /* Small delay before retry */
        for (volatile int i = 0; i < 10000; i++) {
            __asm__ __volatile__("nop");
        }
    }

    if (retry < 0) {
        Serial_WriteString("[SDHCI] Card initialization timeout (ACMD41)\n");
        return -1;
    }

    /* CMD2: ALL_SEND_CID - Get card ID */
    if (sdhci_send_command(sdhci_base, CMD2, 0, RESP_R2, response) != 0) {
        Serial_WriteString("[SDHCI] CMD2 failed\n");
        return -1;
    }
    Serial_WriteString("[SDHCI] Card ID received (CMD2 OK)\n");

    /* CMD3: SET_RELATIVE_ADDR - Get card RCA */
    if (sdhci_send_command(sdhci_base, CMD3, 0, RESP_R6, response) != 0) {
        Serial_WriteString("[SDHCI] CMD3 failed\n");
        return -1;
    }
    card_rca = (response[0] >> 16) & 0xFFFF;
    Serial_Printf("[SDHCI] Card RCA: 0x%lx (CMD3 OK)\n", (unsigned long)card_rca);

    /* CMD7: SELECT_CARD - Select card for data transfer */
    if (sdhci_send_command(sdhci_base, CMD7, (card_rca << 16), RESP_R1, response) != 0) {
        Serial_WriteString("[SDHCI] CMD7 failed\n");
        return -1;
    }
    Serial_WriteString("[SDHCI] Card selected (CMD7 OK)\n");

    /* CMD9: SEND_CSD - Get card-specific data */
    if (sdhci_send_command(sdhci_base, CMD9, (card_rca << 16), RESP_R2, card_csd) != 0) {
        Serial_WriteString("[SDHCI] CMD9 failed\n");
        return -1;
    }
    Serial_WriteString("[SDHCI] Card CSD received (CMD9 OK)\n");

    Serial_WriteString("[SDHCI] Card initialization complete!\n");
    return 0;
}

/*
 * Read a single block from SD card
 */
int sdhci_read_block(uint32_t sdhci_base, uint32_t block_addr, void *buffer) {
    if (!buffer) {
        return -1;
    }

    uint32_t response[4] = {0};

    /* Convert byte address to block address for v2.0 cards */
    uint32_t addr = block_addr;
    if (!card_version_2) {
        addr = block_addr * 512;  /* v1.0 uses byte addressing */
    }

    /* CMD17: READ_SINGLE_BLOCK */
    if (sdhci_send_command(sdhci_base, CMD17, addr, RESP_R1, response) != 0) {
        Serial_Printf("[SDHCI] CMD17 failed for block 0x%lx\n", (unsigned long)block_addr);
        return -1;
    }

    /* TODO: Wait for data interrupt and read 512 bytes into buffer
     * For now, just return success
     */

    return 512;  /* Bytes read */
}

/*
 * Write a single block to SD card
 */
int sdhci_write_block(uint32_t sdhci_base, uint32_t block_addr, const void *buffer) {
    if (!buffer) {
        return -1;
    }

    uint32_t response[4] = {0};

    /* Convert byte address to block address for v2.0 cards */
    uint32_t addr = block_addr;
    if (!card_version_2) {
        addr = block_addr * 512;  /* v1.0 uses byte addressing */
    }

    /* CMD24: WRITE_BLOCK */
    if (sdhci_send_command(sdhci_base, CMD24, addr, RESP_R1, response) != 0) {
        Serial_Printf("[SDHCI] CMD24 failed for block 0x%lx\n", (unsigned long)block_addr);
        return -1;
    }

    /* TODO: Write 512 bytes from buffer to data register
     * For now, just return success
     */

    return 512;  /* Bytes written */
}
