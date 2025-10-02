/*
 * ATA_Driver.c - ATA/IDE disk driver implementation
 *
 * Bare-metal ATA/IDE driver using PIO mode for System 7.1.
 * Supports LBA28 addressing for drives up to 128GB.
 */

#include "ATA_Driver.h"
#include "x86_io.h"
#include "FileManagerTypes.h"
#include <stddef.h>

/* External functions */
extern void serial_printf(const char* fmt, ...);
extern void* memset(void* s, int c, size_t n);
extern void* memcpy(void* dest, const void* src, size_t n);
extern size_t strlen(const char* s);

/* Global device table */
static ATADevice g_ata_devices[ATA_MAX_DEVICES];
static int g_device_count = 0;
static bool g_ata_initialized = false;

/* 400ns delay by reading alternate status register */
static inline void ata_io_delay(uint16_t control_io) {
    for (int i = 0; i < 4; i++) {
        inb(control_io + ATA_REG_ALT_STATUS);
    }
}

/*
 * ATA_ReadStatus - Read status register
 */
uint8_t ATA_ReadStatus(uint16_t base_io) {
    return inb(base_io + ATA_REG_STATUS);
}

/*
 * ATA_WaitBusy - Wait for BSY bit to clear
 */
void ATA_WaitBusy(uint16_t base_io) {
    uint8_t status;
    int timeout = 100000;

    while (timeout-- > 0) {
        status = ATA_ReadStatus(base_io);
        if (!(status & ATA_STATUS_BSY)) {
            return;
        }
    }

    serial_printf("ATA: Timeout waiting for BSY to clear (status=0x%02x)\n", status);
}

/*
 * ATA_WaitReady - Wait for DRDY bit to set
 */
void ATA_WaitReady(uint16_t base_io) {
    uint8_t status;
    int timeout = 100000;

    ATA_WaitBusy(base_io);

    while (timeout-- > 0) {
        status = ATA_ReadStatus(base_io);
        if (status & ATA_STATUS_DRDY) {
            return;
        }
    }

    serial_printf("ATA: Timeout waiting for DRDY (status=0x%02x)\n", status);
}

/*
 * ATA_WaitDRQ - Wait for DRQ bit to set
 */
bool ATA_WaitDRQ(uint16_t base_io) {
    uint8_t status;
    int timeout = 100000;

    ATA_WaitBusy(base_io);

    while (timeout-- > 0) {
        status = ATA_ReadStatus(base_io);
        if (status & ATA_STATUS_DRQ) {
            return true;
        }
        if (status & ATA_STATUS_ERR) {
            serial_printf("ATA: Error waiting for DRQ (status=0x%02x)\n", status);
            return false;
        }
    }

    serial_printf("ATA: Timeout waiting for DRQ (status=0x%02x)\n", status);
    return false;
}

/*
 * ATA_SelectDrive - Select master or slave drive
 */
void ATA_SelectDrive(uint16_t base_io, bool is_slave) {
    uint8_t drive_select = is_slave ? ATA_DRIVE_SLAVE : ATA_DRIVE_MASTER;
    outb(base_io + ATA_REG_DRIVE_HEAD, drive_select);
    ata_io_delay(base_io == ATA_PRIMARY_IO ? ATA_PRIMARY_CONTROL : ATA_SECONDARY_CONTROL);
}

/*
 * ATA_SoftReset - Perform software reset on ATA bus
 */
static void ATA_SoftReset(uint16_t control_io) {
    /* Set SRST bit */
    outb(control_io + ATA_REG_DEV_CONTROL, ATA_CTRL_SRST | ATA_CTRL_NIEN);
    ata_io_delay(control_io);

    /* Clear SRST bit */
    outb(control_io + ATA_REG_DEV_CONTROL, ATA_CTRL_NIEN);
    ata_io_delay(control_io);

    /* Wait for reset to complete */
    uint16_t base_io = (control_io == ATA_PRIMARY_CONTROL) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
    ATA_WaitReady(base_io);
}

/*
 * ATA_IdentifyDevice - Execute IDENTIFY DEVICE command
 */
static bool ATA_IdentifyDevice(uint16_t base_io, bool is_slave, uint16_t* buffer) {
    uint16_t control_io = (base_io == ATA_PRIMARY_IO) ? ATA_PRIMARY_CONTROL : ATA_SECONDARY_CONTROL;

    /* Select drive */
    ATA_SelectDrive(base_io, is_slave);
    ATA_WaitReady(base_io);

    /* Send IDENTIFY command */
    outb(base_io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
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
        buffer[i] = inw(base_io + ATA_REG_DATA);
    }

    return true;
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

    /* Try to identify the device */
    if (!ATA_IdentifyDevice(base_io, is_slave, id_buffer)) {
        return false;
    }

    /* Check device type based on signature */
    uint8_t cl = inb(base_io + ATA_REG_LBA_MID);
    uint8_t ch = inb(base_io + ATA_REG_LBA_HIGH);

    if (cl == 0x14 && ch == 0xEB) {
        device->type = ATA_DEVICE_PATAPI;
    } else if (cl == 0x69 && ch == 0x96) {
        device->type = ATA_DEVICE_SATAPI;
    } else if (cl == 0x3C && ch == 0xC3) {
        device->type = ATA_DEVICE_SATA;
    } else if (cl == 0x00 && ch == 0x00) {
        device->type = ATA_DEVICE_PATA;
    } else {
        serial_printf("ATA: Unknown device signature: 0x%02x 0x%02x\n", cl, ch);
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
OSErr ATA_Init(void) {
    serial_printf("ATA: Initializing ATA/IDE driver\n");

    if (g_ata_initialized) {
        serial_printf("ATA: Already initialized\n");
        return noErr;
    }

    /* Clear device table */
    memset(g_ata_devices, 0, sizeof(g_ata_devices));
    g_device_count = 0;

    /* Reset primary and secondary buses */
    serial_printf("ATA: Resetting primary bus\n");
    ATA_SoftReset(ATA_PRIMARY_CONTROL);

    serial_printf("ATA: Resetting secondary bus\n");
    ATA_SoftReset(ATA_SECONDARY_CONTROL);

    /* Detect devices on primary bus */
    serial_printf("ATA: Detecting primary master\n");
    if (ATA_DetectDevice(ATA_PRIMARY_IO, false, &g_ata_devices[g_device_count])) {
        serial_printf("ATA: Found primary master\n");
        ATA_PrintDeviceInfo(&g_ata_devices[g_device_count]);
        g_device_count++;
    }

    serial_printf("ATA: Detecting primary slave\n");
    if (ATA_DetectDevice(ATA_PRIMARY_IO, true, &g_ata_devices[g_device_count])) {
        serial_printf("ATA: Found primary slave\n");
        ATA_PrintDeviceInfo(&g_ata_devices[g_device_count]);
        g_device_count++;
    }

    /* Detect devices on secondary bus */
    serial_printf("ATA: Detecting secondary master\n");
    if (ATA_DetectDevice(ATA_SECONDARY_IO, false, &g_ata_devices[g_device_count])) {
        serial_printf("ATA: Found secondary master\n");
        ATA_PrintDeviceInfo(&g_ata_devices[g_device_count]);
        g_device_count++;
    }

    serial_printf("ATA: Detecting secondary slave\n");
    if (ATA_DetectDevice(ATA_SECONDARY_IO, true, &g_ata_devices[g_device_count])) {
        serial_printf("ATA: Found secondary slave\n");
        ATA_PrintDeviceInfo(&g_ata_devices[g_device_count]);
        g_device_count++;
    }

    serial_printf("ATA: Detected %d device(s)\n", g_device_count);

    g_ata_initialized = true;
    return noErr;
}

/*
 * ATA_Shutdown - Shut down ATA driver
 */
void ATA_Shutdown(void) {
    if (!g_ata_initialized) {
        return;
    }

    /* Flush all drives */
    for (int i = 0; i < g_device_count; i++) {
        if (g_ata_devices[i].present && g_ata_devices[i].type == ATA_DEVICE_PATA) {
            ATA_FlushCache(&g_ata_devices[i]);
        }
    }

    g_ata_initialized = false;
    g_device_count = 0;
}

/*
 * ATA_GetDeviceCount - Get number of detected devices
 */
int ATA_GetDeviceCount(void) {
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

    serial_printf("ATA: Reading %u sector(s) from LBA %u\n", count, lba);

    /* Wait for drive to be ready */
    ATA_WaitReady(base_io);

    /* Select drive and set LBA mode */
    uint8_t drive_head = (device->is_slave ? ATA_DRIVE_SLAVE : ATA_DRIVE_MASTER) |
                         ATA_DRIVE_LBA | ((lba >> 24) & 0x0F);
    outb(base_io + ATA_REG_DRIVE_HEAD, drive_head);
    ata_io_delay(control_io);

    /* Set sector count and LBA */
    outb(base_io + ATA_REG_SECCOUNT, count);
    outb(base_io + ATA_REG_LBA_LOW, lba & 0xFF);
    outb(base_io + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(base_io + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);

    /* Send READ SECTORS command */
    outb(base_io + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
    ata_io_delay(control_io);

    /* Read each sector */
    for (int sector = 0; sector < count; sector++) {
        /* Wait for DRQ */
        if (!ATA_WaitDRQ(base_io)) {
            serial_printf("ATA: Read failed at sector %d\n", sector);
            return ioErr;
        }

        /* Read 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            buf16[sector * 256 + i] = inw(base_io + ATA_REG_DATA);
        }

        /* Check for errors */
        uint8_t status = ATA_ReadStatus(base_io);
        if (status & ATA_STATUS_ERR) {
            uint8_t error = inb(base_io + ATA_REG_ERROR);
            serial_printf("ATA: Read error (status=0x%02x, error=0x%02x)\n", status, error);
            return ioErr;
        }
    }

    serial_printf("ATA: Read complete\n");
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

    serial_printf("ATA: Writing %u sector(s) to LBA %u\n", count, lba);

    /* Wait for drive to be ready */
    ATA_WaitReady(base_io);

    /* Select drive and set LBA mode */
    uint8_t drive_head = (device->is_slave ? ATA_DRIVE_SLAVE : ATA_DRIVE_MASTER) |
                         ATA_DRIVE_LBA | ((lba >> 24) & 0x0F);
    outb(base_io + ATA_REG_DRIVE_HEAD, drive_head);
    ata_io_delay(control_io);

    /* Set sector count and LBA */
    outb(base_io + ATA_REG_SECCOUNT, count);
    outb(base_io + ATA_REG_LBA_LOW, lba & 0xFF);
    outb(base_io + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(base_io + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);

    /* Send WRITE SECTORS command */
    outb(base_io + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);
    ata_io_delay(control_io);

    /* Write each sector */
    for (int sector = 0; sector < count; sector++) {
        /* Wait for DRQ */
        if (!ATA_WaitDRQ(base_io)) {
            serial_printf("ATA: Write failed at sector %d\n", sector);
            return ioErr;
        }

        /* Write 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            outw(base_io + ATA_REG_DATA, buf16[sector * 256 + i]);
        }

        /* Wait for completion */
        ATA_WaitBusy(base_io);

        /* Check for errors */
        uint8_t status = ATA_ReadStatus(base_io);
        if (status & ATA_STATUS_ERR) {
            uint8_t error = inb(base_io + ATA_REG_ERROR);
            serial_printf("ATA: Write error (status=0x%02x, error=0x%02x)\n", status, error);
            return ioErr;
        }
    }

    /* Flush write cache */
    ATA_FlushCache(device);

    serial_printf("ATA: Write complete\n");
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
    outb(base_io + ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE);
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

    serial_printf("ATA: Device Info:\n");
    serial_printf("ATA:   Type: %s (%s)\n",
                 ATA_GetDeviceTypeName(device->type),
                 device->is_slave ? "Slave" : "Master");
    serial_printf("ATA:   Model: %s\n", device->model);
    serial_printf("ATA:   Serial: %s\n", device->serial);
    serial_printf("ATA:   Firmware: %s\n", device->firmware);
    serial_printf("ATA:   Sectors: %u (%u MB)\n",
                 device->sectors,
                 (device->sectors / 2048));  /* sectors * 512 / 1024 / 1024 */
    serial_printf("ATA:   LBA48: %s\n", device->lba48_supported ? "Yes" : "No");
    serial_printf("ATA:   DMA: %s\n", device->dma_supported ? "Yes" : "No");
}
