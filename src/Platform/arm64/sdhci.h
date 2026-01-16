/*
 * SDHCI SD/eMMC Card Driver for Raspberry Pi ARM64
 * Implements block device interface for SD card storage
 */

#ifndef ARM64_SDHCI_H
#define ARM64_SDHCI_H

#include <stdint.h>
#include <stdbool.h>

/* SDHCI base addresses for different Pi models (ARM64 memory map) */
#define SDHCI_BASE_PI3      0x3F300000ULL   /* Pi 3 EMMC */
#define SDHCI_BASE_PI4      0xFE340000ULL   /* Pi 4 EMMC2 */
#define SDHCI_BASE_PI5      0x1000FE340000ULL /* Pi 5 (placeholder) */

/* Alternative: ARASAN SDHCI on Pi 4 (for WiFi SD) */
#define ARASAN_BASE_PI4     0xFE300000ULL

/* SDHCI register offsets */
#define SDHCI_DMA_ADDRESS       0x00
#define SDHCI_BLOCK_SIZE        0x04
#define SDHCI_BLOCK_COUNT       0x06
#define SDHCI_ARGUMENT          0x08
#define SDHCI_TRANSFER_MODE     0x0C
#define SDHCI_COMMAND           0x0E
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
#define SDHCI_AUTO_CMD_STATUS   0x3C
#define SDHCI_HOST_CONTROL2     0x3E
#define SDHCI_CAPABILITIES      0x40
#define SDHCI_CAPABILITIES_1    0x44
#define SDHCI_MAX_CURRENT       0x48
#define SDHCI_ADMA_ERROR        0x54
#define SDHCI_ADMA_ADDRESS      0x58
#define SDHCI_SLOT_INT_STATUS   0xFC
#define SDHCI_HOST_VERSION      0xFE

/* SDHCI Present State bits */
#define PRESENT_CMD_INHIBIT     (1 << 0)
#define PRESENT_DAT_INHIBIT     (1 << 1)
#define PRESENT_DAT_ACTIVE      (1 << 2)
#define PRESENT_WRITE_ACTIVE    (1 << 8)
#define PRESENT_READ_ACTIVE     (1 << 9)
#define PRESENT_BUFFER_WRITE    (1 << 10)
#define PRESENT_BUFFER_READ     (1 << 11)
#define PRESENT_CARD_INSERTED   (1 << 16)
#define PRESENT_CARD_STABLE     (1 << 17)
#define PRESENT_CARD_DETECT     (1 << 18)
#define PRESENT_WRITE_PROTECT   (1 << 19)

/* SDHCI Interrupt Status bits */
#define INT_CMD_COMPLETE        (1 << 0)
#define INT_DATA_END            (1 << 1)
#define INT_BLOCK_GAP           (1 << 2)
#define INT_DMA                 (1 << 3)
#define INT_BUFFER_WRITE        (1 << 4)
#define INT_BUFFER_READ         (1 << 5)
#define INT_CARD_INSERT         (1 << 6)
#define INT_CARD_REMOVE         (1 << 7)
#define INT_CARD_INTERRUPT      (1 << 8)
#define INT_ERROR               (1 << 15)
#define INT_CMD_TIMEOUT         (1 << 16)
#define INT_CMD_CRC             (1 << 17)
#define INT_CMD_END_BIT         (1 << 18)
#define INT_CMD_INDEX           (1 << 19)
#define INT_DATA_TIMEOUT        (1 << 20)
#define INT_DATA_CRC            (1 << 21)
#define INT_DATA_END_BIT        (1 << 22)
#define INT_CURRENT_LIMIT       (1 << 23)
#define INT_AUTO_CMD            (1 << 24)
#define INT_ADMA                (1 << 25)

#define INT_ERROR_MASK          (INT_CMD_TIMEOUT | INT_CMD_CRC | INT_CMD_END_BIT | \
                                 INT_CMD_INDEX | INT_DATA_TIMEOUT | INT_DATA_CRC | \
                                 INT_DATA_END_BIT | INT_CURRENT_LIMIT | INT_AUTO_CMD | INT_ADMA)

/* SD Card Commands */
#define SD_CMD0_GO_IDLE         0
#define SD_CMD2_ALL_SEND_CID    2
#define SD_CMD3_SEND_RCA        3
#define SD_CMD7_SELECT_CARD     7
#define SD_CMD8_SEND_IF_COND    8
#define SD_CMD9_SEND_CSD        9
#define SD_CMD12_STOP_TRANS     12
#define SD_CMD13_SEND_STATUS    13
#define SD_CMD17_READ_SINGLE    17
#define SD_CMD18_READ_MULTIPLE  18
#define SD_CMD24_WRITE_SINGLE   24
#define SD_CMD25_WRITE_MULTIPLE 25
#define SD_CMD55_APP_CMD        55
#define SD_ACMD41_SEND_OP_COND  41

/* Response types */
#define RESP_NONE       0
#define RESP_R1         1
#define RESP_R1B        2
#define RESP_R2         3
#define RESP_R3         4
#define RESP_R6         6
#define RESP_R7         7

/* Block size */
#define SDHCI_BLOCK_SIZE_512    512

/* Public API */
bool sdhci_init(void);
bool sdhci_is_initialized(void);
bool sdhci_card_present(void);
uint64_t sdhci_get_capacity(void);
uint32_t sdhci_get_sector_size(void);

/* Block I/O */
int sdhci_read_blocks(uint64_t lba, uint32_t count, void *buffer);
int sdhci_write_blocks(uint64_t lba, uint32_t count, const void *buffer);

#endif /* ARM64_SDHCI_H */
