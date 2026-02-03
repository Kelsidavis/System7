/*
 * ATA_Driver.h - ATA/IDE disk driver interface
 *
 * Provides PIO mode access to ATA/IDE hard disks for bare-metal System 7.1.
 * Supports LBA28 addressing mode for drives up to 128GB.
 */

#ifndef ATA_DRIVER_H
#define ATA_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "SystemTypes.h"

/* ATA I/O Port Definitions */

/* Primary ATA bus (master/slave) */
#define ATA_PRIMARY_IO          0x1F0
#define ATA_PRIMARY_CONTROL     0x3F6
#define ATA_PRIMARY_IRQ         14

/* Secondary ATA bus (master/slave) */
#define ATA_SECONDARY_IO        0x170
#define ATA_SECONDARY_CONTROL   0x376
#define ATA_SECONDARY_IRQ       15

/* ATA Register Offsets (from base I/O port) */
#define ATA_REG_DATA            0x00    /* Data register (16-bit) */
#define ATA_REG_ERROR           0x01    /* Error register (read) */
#define ATA_REG_FEATURES        0x01    /* Features register (write) */
#define ATA_REG_SECCOUNT        0x02    /* Sector count */
#define ATA_REG_LBA_LOW         0x03    /* LBA bits 0-7 */
#define ATA_REG_LBA_MID         0x04    /* LBA bits 8-15 */
#define ATA_REG_LBA_HIGH        0x05    /* LBA bits 16-23 */
#define ATA_REG_DRIVE_HEAD      0x06    /* Drive/Head register */
#define ATA_REG_STATUS          0x07    /* Status register (read) */
#define ATA_REG_COMMAND         0x07    /* Command register (write) */

/* Control Register Offsets (from control port) */
#define ATA_REG_ALT_STATUS      0x00    /* Alternate status (read) */
#define ATA_REG_DEV_CONTROL     0x00    /* Device control (write) */

/* ATA Status Register Bits */
#define ATA_STATUS_ERR          0x01    /* Error */
#define ATA_STATUS_IDX          0x02    /* Index (obsolete) */
#define ATA_STATUS_CORR         0x04    /* Corrected data */
#define ATA_STATUS_DRQ          0x08    /* Data request */
#define ATA_STATUS_DSC          0x10    /* Drive seek complete */
#define ATA_STATUS_DF           0x20    /* Drive fault */
#define ATA_STATUS_DRDY         0x40    /* Drive ready */
#define ATA_STATUS_BSY          0x80    /* Busy */

/* ATA Commands */
#define ATA_CMD_READ_SECTORS    0x20    /* Read sectors with retry */
#define ATA_CMD_WRITE_SECTORS   0x30    /* Write sectors with retry */
#define ATA_CMD_IDENTIFY        0xEC    /* Identify device */
#define ATA_CMD_IDENTIFY_PACKET 0xA1    /* Identify packet device */
#define ATA_CMD_PACKET          0xA0    /* Packet command (ATAPI) */
#define ATA_CMD_FLUSH_CACHE     0xE7    /* Flush write cache */

/* Drive Selection Bits (for ATA_REG_DRIVE_HEAD) */
#define ATA_DRIVE_MASTER        0xA0    /* Master drive, LBA mode */
#define ATA_DRIVE_SLAVE         0xB0    /* Slave drive, LBA mode */
#define ATA_DRIVE_LBA           0x40    /* LBA mode bit */

/* Device Control Register Bits */
#define ATA_CTRL_NIEN           0x02    /* Disable interrupts */
#define ATA_CTRL_SRST           0x04    /* Software reset */
#define ATA_CTRL_HOB            0x80    /* High order byte (48-bit LBA) */

/* ATA Device Types */
typedef enum {
    ATA_DEVICE_NONE = 0,
    ATA_DEVICE_PATA,        /* Parallel ATA (IDE) */
    ATA_DEVICE_PATAPI,      /* ATAPI (CD-ROM, etc.) */
    ATA_DEVICE_SATA,        /* Serial ATA */
    ATA_DEVICE_SATAPI       /* SATAPI */
} ATADeviceType;

/* ATA Device Information */
typedef struct {
    bool present;           /* Device is present and detected */
    bool is_slave;          /* True if slave, false if master */
    ATADeviceType type;     /* Device type */
    uint16_t base_io;       /* Base I/O port */
    uint16_t control_io;    /* Control I/O port */
    uint32_t sectors;       /* Total number of sectors (LBA28) */
    uint64_t sectors_48;    /* Total sectors (LBA48, if supported) */
    char model[41];         /* Model string (40 chars + null) */
    char serial[21];        /* Serial number (20 chars + null) */
    char firmware[9];       /* Firmware revision (8 chars + null) */
    bool lba48_supported;   /* LBA48 addressing supported */
    bool dma_supported;     /* DMA transfers supported */
} ATADevice;

/* Maximum devices (primary master/slave + secondary master/slave) */
#define ATA_MAX_DEVICES 4

/* ATA Driver Initialization */
OSErr ATA_Init(void);
void ATA_Shutdown(void);

/* Device Detection */
bool ATA_DetectDevice(uint16_t base_io, bool is_slave, ATADevice* device);
int ATA_GetDeviceCount(void);
ATADevice* ATA_GetDevice(int index);

/* Sector I/O Operations */
OSErr ATA_ReadSectors(ATADevice* device, uint32_t lba, uint8_t count, void* buffer);
OSErr ATA_WriteSectors(ATADevice* device, uint32_t lba, uint8_t count, const void* buffer);
OSErr ATA_FlushCache(ATADevice* device);

/* Low-level ATA Operations */
void ATA_WaitBusy(uint16_t base_io);
void ATA_WaitReady(uint16_t base_io);
bool ATA_WaitDRQ(uint16_t base_io);
uint8_t ATA_ReadStatus(uint16_t base_io);
void ATA_SelectDrive(uint16_t base_io, bool is_slave);

/* Utility Functions */
void ATA_PrintDeviceInfo(ATADevice* device);
const char* ATA_GetDeviceTypeName(ATADeviceType type);

#endif /* ATA_DRIVER_H */
