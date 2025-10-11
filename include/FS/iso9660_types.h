/* ISO 9660 Filesystem Types and Structures */
#ifndef ISO9660_TYPES_H
#define ISO9660_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ISO 9660 Volume Descriptor Types */
#define ISO_VD_BOOT_RECORD              0
#define ISO_VD_PRIMARY                  1
#define ISO_VD_SUPPLEMENTARY            2
#define ISO_VD_PARTITION                3
#define ISO_VD_TERMINATOR               255

/* ISO 9660 constants */
#define ISO_SECTOR_SIZE                 2048
#define ISO_VD_START_SECTOR             16
#define ISO_STANDARD_ID                 "CD001"
#define ISO_STANDARD_VERSION            1

/* Directory entry file flags */
#define ISO_FILE_HIDDEN                 0x01
#define ISO_FILE_ISDIR                  0x02
#define ISO_FILE_ASSOCIATED             0x04
#define ISO_FILE_USEEXTATTR             0x08
#define ISO_FILE_USEPERMISSIONS         0x10
#define ISO_FILE_MULTIEXTENT            0x80

/* ISO 9660 Primary Volume Descriptor */
#pragma pack(push, 1)
typedef struct {
    uint8_t  type;                      /* Volume descriptor type (1 for primary) */
    char     id[5];                     /* Standard identifier "CD001" */
    uint8_t  version;                   /* Volume descriptor version (1) */
    uint8_t  unused1;                   /* Unused field */
    char     system_id[32];             /* System identifier */
    char     volume_id[32];             /* Volume identifier */
    uint8_t  unused2[8];                /* Unused field */
    uint32_t volume_space_size_lsb;     /* Volume space size (LSB) */
    uint32_t volume_space_size_msb;     /* Volume space size (MSB) */
    uint8_t  unused3[32];               /* Unused field */
    uint16_t volume_set_size_lsb;       /* Volume set size (LSB) */
    uint16_t volume_set_size_msb;       /* Volume set size (MSB) */
    uint16_t volume_sequence_number_lsb;/* Volume sequence number (LSB) */
    uint16_t volume_sequence_number_msb;/* Volume sequence number (MSB) */
    uint16_t logical_block_size_lsb;    /* Logical block size (LSB) */
    uint16_t logical_block_size_msb;    /* Logical block size (MSB) */
    uint32_t path_table_size_lsb;       /* Path table size (LSB) */
    uint32_t path_table_size_msb;       /* Path table size (MSB) */
    uint32_t type_l_path_table;         /* Type L path table location */
    uint32_t opt_type_l_path_table;     /* Optional Type L path table location */
    uint32_t type_m_path_table;         /* Type M path table location */
    uint32_t opt_type_m_path_table;     /* Optional Type M path table location */
    uint8_t  root_directory_record[34]; /* Root directory record */
    char     volume_set_id[128];        /* Volume set identifier */
    char     publisher_id[128];         /* Publisher identifier */
    char     preparer_id[128];          /* Data preparer identifier */
    char     application_id[128];       /* Application identifier */
    char     copyright_file_id[37];     /* Copyright file identifier */
    char     abstract_file_id[37];      /* Abstract file identifier */
    char     bibliographic_file_id[37]; /* Bibliographic file identifier */
    char     creation_date[17];         /* Volume creation date and time */
    char     modification_date[17];     /* Volume modification date and time */
    char     expiration_date[17];       /* Volume expiration date and time */
    char     effective_date[17];        /* Volume effective date and time */
    uint8_t  file_structure_version;    /* File structure version (1) */
    uint8_t  unused4;                   /* Reserved for future */
    uint8_t  application_data[512];     /* Application use */
    uint8_t  reserved[653];             /* Reserved for future standardization */
} ISO9660_PrimaryVolumeDescriptor;
#pragma pack(pop)

/* ISO 9660 Directory Record */
#pragma pack(push, 1)
typedef struct {
    uint8_t  length;                    /* Length of directory record */
    uint8_t  ext_attr_length;           /* Extended attribute record length */
    uint32_t extent_lsb;                /* Location of extent (LSB) */
    uint32_t extent_msb;                /* Location of extent (MSB) */
    uint32_t size_lsb;                  /* Data length (LSB) */
    uint32_t size_msb;                  /* Data length (MSB) */
    uint8_t  date[7];                   /* Recording date and time */
    uint8_t  flags;                     /* File flags */
    uint8_t  file_unit_size;            /* File unit size (interleaved files) */
    uint8_t  interleave;                /* Interleave gap size */
    uint16_t volume_sequence_number_lsb;/* Volume sequence number (LSB) */
    uint16_t volume_sequence_number_msb;/* Volume sequence number (MSB) */
    uint8_t  name_len;                  /* Length of file identifier */
    char     name[];                    /* File identifier (flexible array) */
} ISO9660_DirectoryRecord;
#pragma pack(pop)

/* ISO 9660 Date/Time format (7 bytes) */
typedef struct {
    uint8_t year;       /* Years since 1900 */
    uint8_t month;      /* Month (1-12) */
    uint8_t day;        /* Day (1-31) */
    uint8_t hour;       /* Hour (0-23) */
    uint8_t minute;     /* Minute (0-59) */
    uint8_t second;     /* Second (0-59) */
    int8_t  gmt_offset; /* GMT offset in 15-minute intervals */
} ISO9660_DateTime;

/* ISO 9660 ASCII Date/Time format (17 bytes) */
typedef struct {
    char year[4];       /* Year (0001-9999) */
    char month[2];      /* Month (01-12) */
    char day[2];        /* Day (01-31) */
    char hour[2];       /* Hour (00-23) */
    char minute[2];     /* Minute (00-59) */
    char second[2];     /* Second (00-59) */
    char hundredths[2]; /* Hundredths of a second */
    int8_t gmt_offset;  /* GMT offset in 15-minute intervals */
} ISO9660_ASCIIDateTime;

#endif /* ISO9660_TYPES_H */
