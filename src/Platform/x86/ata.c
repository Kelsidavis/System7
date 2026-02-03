/*
 * ATA_Driver.c - ATA/IDE disk driver implementation
 *
 * Bare-metal ATA/IDE driver using PIO mode for System 7.1.
 * Supports LBA28 addressing for drives up to 128GB.
 */

#include "ATA_Driver.h"
#include "Platform/include/storage.h"
#include "Platform/include/io.h"
#include "FileManagerTypes.h"
#include <stddef.h>
#include "Platform/PlatformLogging.h"

/* External functions */
extern void* memset(void* s, int c, size_t n);
extern void* memcpy(void* dest, const void* src, size_t n);
extern size_t strlen(const char* s);

#define ATAPI_SECTOR_SIZE 2048
#define ATAPI_CMD_READ_10 0x28
#define ATAPI_PACKET_SIZE 12

static inline uint32_t iso_read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline int ascii_upper(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

static OSErr ATA_ReadATAPISectors(ATADevice* device, uint32_t lba, uint16_t count, void* buffer);

static int iso_name_match(const uint8_t *name, uint8_t name_len, const char *target) {
    int i = 0;
    while (i < name_len && name[i] != ';') {
        int a = ascii_upper(name[i]);
        int b = ascii_upper((unsigned char)target[i]);
        if (b == 0) {
            return 0;
        }
        if (a != b) {
            return 0;
        }
        i++;
    }
    return target[i] == '\0';
}

static OSErr iso_read_dir_record(ATADevice *device, uint32_t dir_lba, uint32_t dir_size,
                                 const char *target, uint32_t *out_lba, uint32_t *out_size,
                                 Boolean *out_is_dir) {
    if (!device || !target || !out_lba || !out_size || !out_is_dir) {
        return paramErr;
    }

    uint32_t bytes_remaining = dir_size;
    uint32_t sector_offset = 0;
    uint8_t sector[ATAPI_SECTOR_SIZE];

    while (bytes_remaining > 0) {
        OSErr err = ATA_ReadATAPISectors(device, dir_lba + sector_offset, 1, sector);
        if (err != noErr) {
            return err;
        }

        uint32_t offset = 0;
        while (offset < ATAPI_SECTOR_SIZE && bytes_remaining > 0) {
            uint8_t len = sector[offset];
            if (len == 0) {
                uint32_t advance = ATAPI_SECTOR_SIZE - offset;
                if (advance > bytes_remaining) {
                    bytes_remaining = 0;
                } else {
                    bytes_remaining -= advance;
                }
                break;
            }

            if (offset + len > ATAPI_SECTOR_SIZE) {
                break;
            }

            const uint8_t *record = &sector[offset];
            uint32_t extent = iso_read_le32(record + 2);
            uint32_t data_len = iso_read_le32(record + 10);
            uint8_t flags = record[25];
            uint8_t name_len = record[32];
            const uint8_t *name = record + 33;

            if (name_len == 1 && name[0] == 0) {
                /* '.' */
            } else if (name_len == 1 && name[0] == 1) {
                /* '..' */
            } else if (iso_name_match(name, name_len, target)) {
                *out_lba = extent;
                *out_size = data_len;
                *out_is_dir = (flags & 0x02) != 0;
                return noErr;
            }

            offset += len;
            bytes_remaining -= len;
        }

        sector_offset++;
    }

    return fnfErr;
}

static OSErr iso_find_path(ATADevice *device, const char *path, uint32_t *out_lba, uint32_t *out_size,
                           Boolean *out_is_dir) {
    uint8_t pvd[ATAPI_SECTOR_SIZE];
    OSErr err = ATA_ReadATAPISectors(device, 16, 1, pvd);
    if (err != noErr) {
        return err;
    }

    if (!(pvd[1] == 'C' && pvd[2] == 'D' && pvd[3] == '0' && pvd[4] == '0' && pvd[5] == '1')) {
        return fnfErr;
    }

    const uint8_t *root = pvd + 156;
    uint32_t root_lba = iso_read_le32(root + 2);
    uint32_t root_size = iso_read_le32(root + 10);

    char segment[64];
    uint32_t seg_len = 0;
    uint32_t cur_lba = root_lba;
    uint32_t cur_size = root_size;
    Boolean cur_is_dir = true;

    for (const char *p = path; ; p++) {
        if (*p == '/' || *p == '\0') {
            if (seg_len > 0) {
                segment[seg_len] = '\0';
                err = iso_read_dir_record(device, cur_lba, cur_size, segment, &cur_lba, &cur_size, &cur_is_dir);
                if (err != noErr) {
                    return err;
                }
                if (!cur_is_dir && *p != '\0') {
                    return fnfErr;
                }
                seg_len = 0;
            }
            if (*p == '\0') {
                break;
            }
            continue;
        }

        if (seg_len + 1 < sizeof(segment)) {
            segment[seg_len++] = *p;
        }
    }

    *out_lba = cur_lba;
    *out_size = cur_size;
    *out_is_dir = cur_is_dir;
    return noErr;
}

/* Global device table */
static ATADevice g_ata_devices[ATA_MAX_DEVICES];
static int g_device_count = 0;
static bool g_ata_initialized = false;

/* 400ns delay by reading alternate status register */
static inline void ata_io_delay(uint16_t control_io) {
    for (int i = 0; i < 4; i++) {
        hal_inb(control_io + ATA_REG_ALT_STATUS);
    }
}

/*
 * ATA_ReadStatus - Read status register
 */
uint8_t ATA_ReadStatus(uint16_t base_io) {
    return hal_inb(base_io + ATA_REG_STATUS);
}

/*
 * ATA_WaitBusy - Wait for BSY bit to clear
 */
void ATA_WaitBusy(uint16_t base_io) {
    uint8_t status = 0;
    int timeout = 100000;

    while (timeout-- > 0) {
        status = ATA_ReadStatus(base_io);
        if (!(status & ATA_STATUS_BSY)) {
            return;
        }
    }

    PLATFORM_LOG_DEBUG("ATA: Timeout waiting for BSY to clear (status=0x%02x)\n", status);
}

/*
 * ATA_WaitReady - Wait for DRDY bit to set
 */
void ATA_WaitReady(uint16_t base_io) {
    uint8_t status = 0;
    int timeout = 100000;

    ATA_WaitBusy(base_io);

    while (timeout-- > 0) {
        status = ATA_ReadStatus(base_io);
        if (status & ATA_STATUS_DRDY) {
            return;
        }
    }

    PLATFORM_LOG_DEBUG("ATA: Timeout waiting for DRDY (status=0x%02x)\n", status);
}

/*
 * ATA_WaitDRQ - Wait for DRQ bit to set
 */
bool ATA_WaitDRQ(uint16_t base_io) {
    uint8_t status = 0;
    int timeout = 100000;

    ATA_WaitBusy(base_io);

    while (timeout-- > 0) {
        status = ATA_ReadStatus(base_io);
        if (status & ATA_STATUS_DRQ) {
            return true;
        }
        if (status & ATA_STATUS_ERR) {
            PLATFORM_LOG_DEBUG("ATA: Error waiting for DRQ (status=0x%02x)\n", status);
            return false;
        }
    }

    PLATFORM_LOG_DEBUG("ATA: Timeout waiting for DRQ (status=0x%02x)\n", status);
    return false;
}

/*
 * ATA_SelectDrive - Select master or slave drive
 */
void ATA_SelectDrive(uint16_t base_io, bool is_slave) {
    uint8_t drive_select = is_slave ? ATA_DRIVE_SLAVE : ATA_DRIVE_MASTER;
    hal_outb(base_io + ATA_REG_DRIVE_HEAD, drive_select);
    ata_io_delay(base_io == ATA_PRIMARY_IO ? ATA_PRIMARY_CONTROL : ATA_SECONDARY_CONTROL);
}

/*
 * ATA_SoftReset - Perform software reset on ATA bus
 */
static void ATA_SoftReset(uint16_t control_io) {
    /* Set SRST bit */
    hal_outb(control_io + ATA_REG_DEV_CONTROL, ATA_CTRL_SRST | ATA_CTRL_NIEN);
    ata_io_delay(control_io);

    /* Clear SRST bit */
    hal_outb(control_io + ATA_REG_DEV_CONTROL, ATA_CTRL_NIEN);
    ata_io_delay(control_io);

    /* Wait for reset to complete */
    uint16_t base_io = (control_io == ATA_PRIMARY_CONTROL) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
    ATA_WaitReady(base_io);
}

/*
 * ATA_IdentifyDevice - Execute IDENTIFY DEVICE command
 */
static bool ATA_IdentifyDevice(uint16_t base_io, bool is_slave, uint16_t* buffer, uint8_t cmd) {
    uint16_t control_io = (base_io == ATA_PRIMARY_IO) ? ATA_PRIMARY_CONTROL : ATA_SECONDARY_CONTROL;

    /* Select drive */
    ATA_SelectDrive(base_io, is_slave);
    ATA_WaitReady(base_io);

    /* Clear registers */
    hal_outb(base_io + ATA_REG_SECCOUNT, 0);
    hal_outb(base_io + ATA_REG_LBA_LOW, 0);
    hal_outb(base_io + ATA_REG_LBA_MID, 0);
    hal_outb(base_io + ATA_REG_LBA_HIGH, 0);

    /* Send IDENTIFY/IDENTIFY PACKET command */
    hal_outb(base_io + ATA_REG_COMMAND, cmd);
    ata_io_delay(control_io);

    /* Check if drive exists */
    uint8_t status = ATA_ReadStatus(base_io);
    if (status == 0 || status == 0xFF) {
        return false;  /* No drive */
    }

    /* Wait for DRQ or error */
    if (!ATA_WaitDRQ(base_io)) {
        return false;
    }

    /* Read 256 words (512 bytes) of identification data */
    for (int i = 0; i < 256; i++) {
        buffer[i] = hal_inw(base_io + ATA_REG_DATA);
    }

    return true;
}

static void ATA_TestATAPI(ATADevice* device) {
    if (!device || !device->present) {
        return;
    }
    if (device->type != ATA_DEVICE_PATAPI && device->type != ATA_DEVICE_SATAPI) {
        return;
    }

    uint8_t sector[ATAPI_SECTOR_SIZE];
    OSErr err = ATA_ReadATAPISectors(device, 16, 1, sector);
    if (err != noErr) {
        PLATFORM_LOG_DEBUG("ATAPI: Failed to read PVD from device\n");
        return;
    }

    if (sector[1] == 'C' && sector[2] == 'D' && sector[3] == '0' &&
        sector[4] == '0' && sector[5] == '1') {
        PLATFORM_LOG_DEBUG("ATAPI: ISO9660 PVD detected\n");
    } else {
        PLATFORM_LOG_DEBUG("ATAPI: PVD signature not found\n");
        return;
    }

    uint32_t file_lba = 0;
    uint32_t file_size = 0;
    Boolean is_dir = false;
    err = iso_find_path(device, "/boot/grub/grub.cfg", &file_lba, &file_size, &is_dir);
    if (err == noErr && !is_dir) {
        PLATFORM_LOG_DEBUG("ATAPI: Found /boot/grub/grub.cfg at LBA %u size %u\n",
                           file_lba, file_size);
        if (file_size > 0) {
            uint8_t file_sector[ATAPI_SECTOR_SIZE];
            OSErr read_err = ATA_ReadATAPISectors(device, file_lba, 1, file_sector);
            if (read_err == noErr) {
                char preview[65];
                int max_bytes = (file_size < 64) ? (int)file_size : 64;
                for (int i = 0; i < max_bytes; i++) {
                    uint8_t c = file_sector[i];
                    if (c < 0x20 || c > 0x7E) {
                        c = '.';
                    }
                    preview[i] = (char)c;
                }
                preview[max_bytes] = '\0';
                PLATFORM_LOG_DEBUG("ATAPI: grub.cfg preview: %s\n", preview);
            } else {
                PLATFORM_LOG_DEBUG("ATAPI: Failed to read grub.cfg data (err=%d)\n", (int)read_err);
            }
        }
    } else {
        PLATFORM_LOG_DEBUG("ATAPI: /boot/grub/grub.cfg not found (err=%d)\n", (int)err);
    }
}

/*
 * ATA_ParseIdentifyData - Parse IDENTIFY DEVICE response
 */
static void ATA_ParseIdentifyData(uint16_t* id_data, ATADevice* device) {
    /* Model string (words 27-46, 40 chars) */
    for (int i = 0; i < 20; i++) {
        uint16_t word = id_data[27 + i];
        device->model[i * 2] = (word >> 8) & 0xFF;
        device->model[i * 2 + 1] = word & 0xFF;
    }
    device->model[40] = '\0';

    /* Trim trailing spaces from model */
    for (int i = 39; i >= 0 && device->model[i] == ' '; i--) {
        device->model[i] = '\0';
    }

    /* Serial number (words 10-19, 20 chars) */
    for (int i = 0; i < 10; i++) {
        uint16_t word = id_data[10 + i];
        device->serial[i * 2] = (word >> 8) & 0xFF;
        device->serial[i * 2 + 1] = word & 0xFF;
    }
    device->serial[20] = '\0';

    /* Firmware revision (words 23-26, 8 chars) */
    for (int i = 0; i < 4; i++) {
        uint16_t word = id_data[23 + i];
        device->firmware[i * 2] = (word >> 8) & 0xFF;
        device->firmware[i * 2 + 1] = word & 0xFF;
    }
    device->firmware[8] = '\0';

    /* LBA28 sector count (words 60-61) */
    device->sectors = ((uint32_t)id_data[61] << 16) | id_data[60];

    /* Check for LBA48 support (word 83, bit 10) */
    device->lba48_supported = (id_data[83] & (1 << 10)) != 0;

    if (device->lba48_supported) {
        /* LBA48 sector count (words 100-103) */
        device->sectors_48 = ((uint64_t)id_data[103] << 48) |
                            ((uint64_t)id_data[102] << 32) |
                            ((uint64_t)id_data[101] << 16) |
                            id_data[100];
    } else {
        device->sectors_48 = device->sectors;
    }

    /* Check for DMA support (word 49, bit 8) */
    device->dma_supported = (id_data[49] & (1 << 8)) != 0;
}

/*
 * ATA_DetectDevice - Detect and identify a device
 */
bool ATA_DetectDevice(uint16_t base_io, bool is_slave, ATADevice* device) {
    uint16_t id_buffer[256];
    uint16_t control_io = (base_io == ATA_PRIMARY_IO) ? ATA_PRIMARY_CONTROL : ATA_SECONDARY_CONTROL;

    memset(device, 0, sizeof(ATADevice));
    device->base_io = base_io;
    device->control_io = control_io;
    device->is_slave = is_slave;

    /* Try to identify the device (ATA) */
    bool identify_ok = ATA_IdentifyDevice(base_io, is_slave, id_buffer, ATA_CMD_IDENTIFY);

    /* Check device type based on signature */
    uint8_t cl = hal_inb(base_io + ATA_REG_LBA_MID);
    uint8_t ch = hal_inb(base_io + ATA_REG_LBA_HIGH);

    if (!identify_ok) {
        /* Try ATAPI identify if signature matches */
        if ((cl == 0x14 && ch == 0xEB) || (cl == 0x69 && ch == 0x96)) {
            identify_ok = ATA_IdentifyDevice(base_io, is_slave, id_buffer, ATA_CMD_IDENTIFY_PACKET);
        }
    }

    if (!identify_ok) {
        return false;
    }

    if (cl == 0x14 && ch == 0xEB) {
        device->type = ATA_DEVICE_PATAPI;
    } else if (cl == 0x69 && ch == 0x96) {
        device->type = ATA_DEVICE_SATAPI;
    } else if (cl == 0x3C && ch == 0xC3) {
        device->type = ATA_DEVICE_SATA;
    } else if (cl == 0x00 && ch == 0x00) {
        device->type = ATA_DEVICE_PATA;
    } else {
        PLATFORM_LOG_DEBUG("ATA: Unknown device signature: 0x%02x 0x%02x\n", cl, ch);
        return false;
    }

    /* Parse identification data */
    ATA_ParseIdentifyData(id_buffer, device);
    device->present = true;

    return true;
}

/*
 * ATA_Init - Initialize ATA driver and detect devices
 */
OSErr hal_storage_init(void) {
    PLATFORM_LOG_DEBUG("ATA: Initializing ATA/IDE driver\n");

    if (g_ata_initialized) {
        PLATFORM_LOG_DEBUG("ATA: Already initialized\n");
        return noErr;
    }

    /* Clear device table */
    memset(g_ata_devices, 0, sizeof(g_ata_devices));
    g_device_count = 0;

    /* Reset primary and secondary buses */
    PLATFORM_LOG_DEBUG("ATA: Resetting primary bus\n");
    ATA_SoftReset(ATA_PRIMARY_CONTROL);

    PLATFORM_LOG_DEBUG("ATA: Resetting secondary bus\n");
    ATA_SoftReset(ATA_SECONDARY_CONTROL);

    /* Detect devices on primary bus */
    PLATFORM_LOG_DEBUG("ATA: Detecting primary master\n");
    if (ATA_DetectDevice(ATA_PRIMARY_IO, false, &g_ata_devices[g_device_count])) {
        PLATFORM_LOG_DEBUG("ATA: Found primary master\n");
        ATA_PrintDeviceInfo(&g_ata_devices[g_device_count]);
        g_device_count++;
    }

    PLATFORM_LOG_DEBUG("ATA: Detecting primary slave\n");
    if (ATA_DetectDevice(ATA_PRIMARY_IO, true, &g_ata_devices[g_device_count])) {
        PLATFORM_LOG_DEBUG("ATA: Found primary slave\n");
        ATA_PrintDeviceInfo(&g_ata_devices[g_device_count]);
        g_device_count++;
    }

    /* Detect devices on secondary bus */
    PLATFORM_LOG_DEBUG("ATA: Detecting secondary master\n");
    if (ATA_DetectDevice(ATA_SECONDARY_IO, false, &g_ata_devices[g_device_count])) {
        PLATFORM_LOG_DEBUG("ATA: Found secondary master\n");
        ATA_PrintDeviceInfo(&g_ata_devices[g_device_count]);
        g_device_count++;
    }

    PLATFORM_LOG_DEBUG("ATA: Detecting secondary slave\n");
    if (ATA_DetectDevice(ATA_SECONDARY_IO, true, &g_ata_devices[g_device_count])) {
        PLATFORM_LOG_DEBUG("ATA: Found secondary slave\n");
        ATA_PrintDeviceInfo(&g_ata_devices[g_device_count]);
        g_device_count++;
    }

    PLATFORM_LOG_DEBUG("ATA: Detected %d device(s)\n", g_device_count);

    for (int i = 0; i < g_device_count; i++) {
        ATA_TestATAPI(&g_ata_devices[i]);
    }

    g_ata_initialized = true;
    return noErr;
}

/*
 * ATA_Shutdown - Shut down ATA driver
 */
OSErr hal_storage_shutdown(void) {
    if (!g_ata_initialized) {
        return noErr;
    }

    /* Flush all drives */
    for (int i = 0; i < g_device_count; i++) {
        if (g_ata_devices[i].present && g_ata_devices[i].type == ATA_DEVICE_PATA) {
            ATA_FlushCache(&g_ata_devices[i]);
        }
    }

    g_ata_initialized = false;
    g_device_count = 0;
    return noErr;
}

/*
 * ATA_GetDeviceCount - Get number of detected devices
 */
int hal_storage_get_drive_count(void) {
    return g_device_count;
}

/*
 * ATA_GetDevice - Get device by index
 */
ATADevice* ATA_GetDevice(int index) {
    if (index < 0 || index >= g_device_count) {
        return NULL;
    }
    return &g_ata_devices[index];
}

/*
 * ATA_ReadSectors - Read sectors using PIO mode (LBA28)
 */
OSErr ATA_ReadSectors(ATADevice* device, uint32_t lba, uint8_t count, void* buffer) {
    if (!device || !device->present) {
        return paramErr;
    }

    if (count == 0) {
        return noErr;
    }

    uint16_t base_io = device->base_io;
    uint16_t control_io = device->control_io;
    uint16_t* buf16 = (uint16_t*)buffer;

    /* Select drive and set LBA mode */
    uint8_t drive_head = (device->is_slave ? ATA_DRIVE_SLAVE : ATA_DRIVE_MASTER) |
                         ATA_DRIVE_LBA | ((lba >> 24) & 0x0F);
    hal_outb(base_io + ATA_REG_DRIVE_HEAD, drive_head);
    ata_io_delay(control_io);

    /* Set sector count and LBA (all three LBA registers required for 28-bit LBA) */
    hal_outb(base_io + ATA_REG_SECCOUNT, count);
    hal_outb(base_io + ATA_REG_LBA_LOW, lba & 0xFF);
    hal_outb(base_io + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    hal_outb(base_io + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    hal_outb(base_io + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
    ata_io_delay(control_io);

    /* Read each sector */
    for (int sector = 0; sector < count; sector++) {
        /* Wait for DRQ */
        if (!ATA_WaitDRQ(base_io)) {
            PLATFORM_LOG_DEBUG("ATA: Read failed at sector %d\n", sector);
            return ioErr;
        }

        /* Read 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            buf16[sector * 256 + i] = hal_inw(base_io + ATA_REG_DATA);
        }

        /* Check for errors */
        uint8_t status = ATA_ReadStatus(base_io);
        if (status & ATA_STATUS_ERR) {
            uint8_t error = hal_inb(base_io + ATA_REG_ERROR);
            PLATFORM_LOG_DEBUG("ATA: Read error (status=0x%02x, error=0x%02x)\n", status, error);
            return ioErr;
        }
    }

    PLATFORM_LOG_DEBUG("ATA: Read complete\n");
    return noErr;
}

/*
 * ATA_WriteSectors - Write sectors using PIO mode (LBA28)
 */
OSErr ATA_WriteSectors(ATADevice* device, uint32_t lba, uint8_t count, const void* buffer) {
    if (!device || !device->present) {
        return paramErr;
    }

    if (count == 0) {
        return noErr;
    }

    uint16_t base_io = device->base_io;
    uint16_t control_io = device->control_io;
    const uint16_t* buf16 = (const uint16_t*)buffer;

    PLATFORM_LOG_DEBUG("ATA: Writing %u sector(s) to LBA %u\n", count, lba);

    /* Wait for drive to be ready */
    ATA_WaitReady(base_io);

    /* Select drive and set LBA mode */
    uint8_t drive_head = (device->is_slave ? ATA_DRIVE_SLAVE : ATA_DRIVE_MASTER) |
                         ATA_DRIVE_LBA | ((lba >> 24) & 0x0F);
    hal_outb(base_io + ATA_REG_DRIVE_HEAD, drive_head);
    ata_io_delay(control_io);

    /* Set sector count and LBA */
    hal_outb(base_io + ATA_REG_SECCOUNT, count);
    hal_outb(base_io + ATA_REG_LBA_LOW, lba & 0xFF);
    hal_outb(base_io + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    hal_outb(base_io + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);

    /* Send WRITE SECTORS command */
    hal_outb(base_io + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);
    ata_io_delay(control_io);

    /* Write each sector */
    for (int sector = 0; sector < count; sector++) {
        /* Wait for DRQ */
        if (!ATA_WaitDRQ(base_io)) {
            PLATFORM_LOG_DEBUG("ATA: Write failed at sector %d\n", sector);
            return ioErr;
        }

        /* Write 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            hal_outw(base_io + ATA_REG_DATA, buf16[sector * 256 + i]);
        }

        /* Wait for completion */
        ATA_WaitBusy(base_io);

        /* Check for errors */
        uint8_t status = ATA_ReadStatus(base_io);
        if (status & ATA_STATUS_ERR) {
            uint8_t error = hal_inb(base_io + ATA_REG_ERROR);
            PLATFORM_LOG_DEBUG("ATA: Write error (status=0x%02x, error=0x%02x)\n", status, error);
            return ioErr;
        }
    }

    /* Flush write cache */
    ATA_FlushCache(device);

    PLATFORM_LOG_DEBUG("ATA: Write complete\n");
    return noErr;
}

/*
 * ATA_ReadATAPISectors - Read sectors from ATAPI device (PIO, READ(10))
 */
static OSErr ATA_ReadATAPISectors(ATADevice* device, uint32_t lba, uint16_t count, void* buffer) {
    if (!device || !device->present) {
        return paramErr;
    }

    if (count == 0) {
        return noErr;
    }

    uint16_t base_io = device->base_io;
    uint16_t control_io = device->control_io;
    uint16_t* buf16 = (uint16_t*)buffer;

    /* Select drive */
    ATA_SelectDrive(base_io, device->is_slave);
    ATA_WaitReady(base_io);

    /* Set expected transfer size (2048 bytes per sector) */
    hal_outb(base_io + ATA_REG_FEATURES, 0);
    hal_outb(base_io + ATA_REG_LBA_MID, (uint8_t)(ATAPI_SECTOR_SIZE & 0xFF));
    hal_outb(base_io + ATA_REG_LBA_HIGH, (uint8_t)((ATAPI_SECTOR_SIZE >> 8) & 0xFF));

    /* Send PACKET command */
    hal_outb(base_io + ATA_REG_COMMAND, ATA_CMD_PACKET);
    ata_io_delay(control_io);

    if (!ATA_WaitDRQ(base_io)) {
        return ioErr;
    }

    /* Build READ(10) packet */
    uint8_t packet[ATAPI_PACKET_SIZE] = {0};
    packet[0] = ATAPI_CMD_READ_10;
    packet[2] = (uint8_t)((lba >> 24) & 0xFF);
    packet[3] = (uint8_t)((lba >> 16) & 0xFF);
    packet[4] = (uint8_t)((lba >> 8) & 0xFF);
    packet[5] = (uint8_t)(lba & 0xFF);
    packet[7] = (uint8_t)((count >> 8) & 0xFF);
    packet[8] = (uint8_t)(count & 0xFF);

    /* Send packet (12 bytes = 6 words) */
    for (int i = 0; i < (ATAPI_PACKET_SIZE / 2); i++) {
        uint16_t word = (uint16_t)packet[i * 2] | ((uint16_t)packet[i * 2 + 1] << 8);
        hal_outw(base_io + ATA_REG_DATA, word);
    }

    uint32_t total_words = (uint32_t)count * (ATAPI_SECTOR_SIZE / 2);
    uint32_t words_read = 0;

    while (words_read < total_words) {
        if (!ATA_WaitDRQ(base_io)) {
            return ioErr;
        }

        uint32_t chunk_words = ATAPI_SECTOR_SIZE / 2;
        for (uint32_t i = 0; i < chunk_words; i++) {
            buf16[words_read + i] = hal_inw(base_io + ATA_REG_DATA);
        }
        words_read += chunk_words;

        uint8_t status = ATA_ReadStatus(base_io);
        if (status & ATA_STATUS_ERR) {
            return ioErr;
        }
    }

    return noErr;
}

/*
 * ATA_FlushCache - Flush write cache
 */
OSErr ATA_FlushCache(ATADevice* device) {
    if (!device || !device->present) {
        return paramErr;
    }

    uint16_t base_io = device->base_io;
    uint16_t control_io = device->control_io;

    /* Wait for drive to be ready */
    ATA_WaitReady(base_io);

    /* Select drive */
    ATA_SelectDrive(base_io, device->is_slave);

    /* Send FLUSH CACHE command */
    hal_outb(base_io + ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE);
    ata_io_delay(control_io);

    /* Wait for completion */
    ATA_WaitReady(base_io);

    return noErr;
}

/*
 * ATA_GetDeviceTypeName - Get device type name string
 */
const char* ATA_GetDeviceTypeName(ATADeviceType type) {
    switch (type) {
        case ATA_DEVICE_PATA:   return "PATA";
        case ATA_DEVICE_PATAPI: return "PATAPI";
        case ATA_DEVICE_SATA:   return "SATA";
        case ATA_DEVICE_SATAPI: return "SATAPI";
        default:                return "Unknown";
    }
}

/*
 * ATA_PrintDeviceInfo - Print device information
 */
void ATA_PrintDeviceInfo(ATADevice* device) {
    if (!device || !device->present) {
        return;
    }

    PLATFORM_LOG_DEBUG("ATA: Device Info:\n");
    PLATFORM_LOG_DEBUG("ATA:   Type: %s (%s)\n",
                 ATA_GetDeviceTypeName(device->type),
                 device->is_slave ? "Slave" : "Master");
    PLATFORM_LOG_DEBUG("ATA:   Model: %s\n", device->model);
    PLATFORM_LOG_DEBUG("ATA:   Serial: %s\n", device->serial);
    PLATFORM_LOG_DEBUG("ATA:   Firmware: %s\n", device->firmware);
    PLATFORM_LOG_DEBUG("ATA:   Sectors: %u (%u MB)\n",
                 device->sectors,
                 (device->sectors / 2048));  /* sectors * 512 / 1024 / 1024 */
    PLATFORM_LOG_DEBUG("ATA:   LBA48: %s\n", device->lba48_supported ? "Yes" : "No");
    PLATFORM_LOG_DEBUG("ATA:   DMA: %s\n", device->dma_supported ? "Yes" : "No");
}

/*
 * HAL Storage Interface Implementation
 */

OSErr hal_storage_get_drive_info(int drive_index, hal_storage_info_t* info) {
    if (!info) {
        return paramErr;
    }

    ATADevice* device = ATA_GetDevice(drive_index);
    if (!device) {
        return paramErr;
    }

    if (device->type == ATA_DEVICE_PATAPI || device->type == ATA_DEVICE_SATAPI) {
        info->block_size = ATAPI_SECTOR_SIZE;
        info->block_count = device->sectors;
    } else {
        info->block_size = 512;  /* ATA sectors are always 512 bytes */
        info->block_count = device->sectors;
    }

    return noErr;
}

OSErr hal_storage_read_blocks(int drive_index, uint64_t start_block, uint32_t block_count, void* buffer) {
    ATADevice* device = ATA_GetDevice(drive_index);
    if (!device) {
        return paramErr;
    }

    if (device->type == ATA_DEVICE_PATAPI || device->type == ATA_DEVICE_SATAPI) {
        uint8_t* buf = (uint8_t*)buffer;
        uint32_t remaining = block_count;
        uint32_t current_lba = (uint32_t)start_block;

        while (remaining > 0) {
            uint16_t count = (remaining > 0xFFFFu) ? 0xFFFFu : (uint16_t)remaining;
            OSErr err = ATA_ReadATAPISectors(device, current_lba, count, buf);
            if (err != noErr) {
                return err;
            }
            remaining -= count;
            current_lba += count;
            buf += (count * ATAPI_SECTOR_SIZE);
        }
        return noErr;
    }

    /* ATA uses LBA28, so we can only address 28 bits */
    if (start_block > 0x0FFFFFFF) {
        return paramErr;
    }

    /* Read in chunks of 255 sectors (max for LBA28) */
    uint8_t* buf = (uint8_t*)buffer;
    uint32_t remaining = block_count;
    uint32_t current_lba = (uint32_t)start_block;

    while (remaining > 0) {
        uint8_t count = (remaining > 255) ? 255 : (uint8_t)remaining;
        OSErr err = ATA_ReadSectors(device, current_lba, count, buf);
        if (err != noErr) {
            return err;
        }

        remaining -= count;
        current_lba += count;
        buf += (count * 512);
    }

    return noErr;
}

OSErr hal_storage_write_blocks(int drive_index, uint64_t start_block, uint32_t block_count, const void* buffer) {
    ATADevice* device = ATA_GetDevice(drive_index);
    if (!device) {
        return paramErr;
    }

    if (device->type == ATA_DEVICE_PATAPI || device->type == ATA_DEVICE_SATAPI) {
        return wPrErr;
    }

    /* ATA uses LBA28, so we can only address 28 bits */
    if (start_block > 0x0FFFFFFF) {
        return paramErr;
    }

    /* Write in chunks of 255 sectors (max for LBA28) */
    const uint8_t* buf = (const uint8_t*)buffer;
    uint32_t remaining = block_count;
    uint32_t current_lba = (uint32_t)start_block;

    while (remaining > 0) {
        uint8_t count = (remaining > 255) ? 255 : (uint8_t)remaining;
        OSErr err = ATA_WriteSectors(device, current_lba, count, buf);
        if (err != noErr) {
            return err;
        }

        remaining -= count;
        current_lba += count;
        buf += (count * 512);
    }

    /* Flush cache after write */
    return ATA_FlushCache(device);
}
