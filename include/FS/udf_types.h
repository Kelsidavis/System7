/* UDF (Universal Disk Format) Filesystem Types and Structures
 * Based on ECMA-167 and ISO 13346 standards
 * Supports UDF 1.02 through 2.60
 */
#ifndef UDF_TYPES_H
#define UDF_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* UDF Constants */
#define UDF_SECTOR_SIZE                 2048
#define UDF_AVDP_SECTOR                 256     /* Anchor Volume Descriptor Pointer location */
#define UDF_AVDP_BACKUP_SECTOR          512     /* Backup AVDP location */

/* Descriptor Tag Identifiers */
#define UDF_TAG_PVD                     1       /* Primary Volume Descriptor */
#define UDF_TAG_ANCHOR                  2       /* Anchor Volume Descriptor Pointer */
#define UDF_TAG_VDP                     3       /* Volume Descriptor Pointer */
#define UDF_TAG_IMPL_USE                4       /* Implementation Use Volume Descriptor */
#define UDF_TAG_PARTITION               5       /* Partition Descriptor */
#define UDF_TAG_LOGICAL_VOLUME          6       /* Logical Volume Descriptor */
#define UDF_TAG_UNALLOCATED_SPACE       7       /* Unallocated Space Descriptor */
#define UDF_TAG_TERMINATING             8       /* Terminating Descriptor */
#define UDF_TAG_LOGICAL_VOLUME_INTEGRITY 9      /* Logical Volume Integrity Descriptor */
#define UDF_TAG_FILE_SET                256     /* File Set Descriptor */
#define UDF_TAG_FILE_ID                 257     /* File Identifier Descriptor */
#define UDF_TAG_ALLOC_EXTENT            258     /* Allocation Extent Descriptor */
#define UDF_TAG_FILE_ENTRY              261     /* File Entry */
#define UDF_TAG_EXTENDED_FILE_ENTRY     266     /* Extended File Entry */

/* File Characteristics (File Identifier Descriptor) */
#define UDF_FID_HIDDEN                  0x01    /* Hidden file */
#define UDF_FID_DIRECTORY               0x02    /* Directory */
#define UDF_FID_DELETED                 0x04    /* Deleted file */
#define UDF_FID_PARENT                  0x08    /* Parent directory */
#define UDF_FID_METADATA                0x10    /* Metadata file */

/* ICB File Type */
#define UDF_ICB_FILETYPE_DIRECTORY      4       /* Directory */
#define UDF_ICB_FILETYPE_FILE           5       /* Regular file */
#define UDF_ICB_FILETYPE_SYMLINK        12      /* Symbolic link */

/* Allocation Descriptor Types */
#define UDF_ICBTAG_AD_SHORT             0       /* Short allocation descriptors */
#define UDF_ICBTAG_AD_LONG              1       /* Long allocation descriptors */
#define UDF_ICBTAG_AD_EXTENDED          2       /* Extended allocation descriptors */
#define UDF_ICBTAG_AD_INLINE            3       /* Data stored in descriptor */

/* UDF Timestamp (ECMA-167 1/7.3) */
#pragma pack(push, 1)
typedef struct {
    uint16_t type_timezone;     /* Type and timezone */
    uint16_t year;              /* Year */
    uint8_t  month;             /* Month (1-12) */
    uint8_t  day;               /* Day (1-31) */
    uint8_t  hour;              /* Hour (0-23) */
    uint8_t  minute;            /* Minute (0-59) */
    uint8_t  second;            /* Second (0-59) */
    uint8_t  centiseconds;      /* Centiseconds */
    uint8_t  hundreds_microsec; /* Hundreds of microseconds */
    uint8_t  microseconds;      /* Microseconds */
} UDF_Timestamp;
#pragma pack(pop)

/* Entity Identifier (ECMA-167 1/7.4) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  flags;             /* Flags */
    char     identifier[23];    /* Identifier */
    char     identifier_suffix[8]; /* Identifier suffix */
} UDF_EntityID;
#pragma pack(pop)

/* Extent Descriptor (ECMA-167 3/7.1) */
#pragma pack(push, 1)
typedef struct {
    uint32_t length;            /* Extent length */
    uint32_t location;          /* Extent location (logical sector) */
} UDF_ExtentAD;
#pragma pack(pop)

/* Long Allocation Descriptor (ECMA-167 4/14.14.2) */
#pragma pack(push, 1)
typedef struct {
    uint32_t length;            /* Extent length (and flags in top 2 bits) */
    uint32_t location;          /* Logical block number */
    uint16_t partition;         /* Partition reference number */
    uint8_t  impl_use[6];       /* Implementation use */
} UDF_LongAD;
#pragma pack(pop)

/* Short Allocation Descriptor (ECMA-167 4/14.14.1) */
#pragma pack(push, 1)
typedef struct {
    uint32_t length;            /* Extent length (and flags in top 2 bits) */
    uint32_t position;          /* Extent position */
} UDF_ShortAD;
#pragma pack(pop)

/* Descriptor Tag (ECMA-167 3/7.2) */
#pragma pack(push, 1)
typedef struct {
    uint16_t tag_id;            /* Tag Identifier */
    uint16_t descriptor_version; /* Descriptor Version */
    uint8_t  tag_checksum;      /* Tag Checksum */
    uint8_t  reserved;          /* Reserved */
    uint16_t tag_serial_number; /* Tag Serial Number */
    uint16_t descriptor_crc;    /* Descriptor CRC */
    uint16_t descriptor_crc_length; /* Descriptor CRC Length */
    uint32_t tag_location;      /* Tag Location (logical sector) */
} UDF_DescriptorTag;
#pragma pack(pop)

/* Anchor Volume Descriptor Pointer (ECMA-167 3/10.2) */
#pragma pack(push, 1)
typedef struct {
    UDF_DescriptorTag tag;      /* Tag (identifier = 2) */
    UDF_ExtentAD main_vds;      /* Main Volume Descriptor Sequence extent */
    UDF_ExtentAD reserve_vds;   /* Reserve Volume Descriptor Sequence extent */
    uint8_t reserved[480];      /* Reserved */
} UDF_AnchorVolumeDescriptorPointer;
#pragma pack(pop)

/* Primary Volume Descriptor (ECMA-167 3/10.1) */
#pragma pack(push, 1)
typedef struct {
    UDF_DescriptorTag tag;      /* Tag (identifier = 1) */
    uint32_t vds_number;        /* Volume Descriptor Sequence Number */
    uint32_t pvd_number;        /* Primary Volume Descriptor Number */
    char     volume_id[32];     /* Volume Identifier (dstring) */
    uint16_t volume_seq_number; /* Volume Sequence Number */
    uint16_t max_volume_seq_number; /* Maximum Volume Sequence Number */
    uint16_t interchange_level; /* Interchange Level */
    uint16_t max_interchange_level; /* Maximum Interchange Level */
    uint32_t charset_list;      /* Character Set List */
    uint32_t max_charset_list;  /* Maximum Character Set List */
    char     volume_set_id[128]; /* Volume Set Identifier (dstring) */
    UDF_EntityID descriptor_charset; /* Descriptor Character Set */
    UDF_EntityID explanatory_charset; /* Explanatory Character Set */
    UDF_ExtentAD volume_abstract; /* Volume Abstract */
    UDF_ExtentAD volume_copyright; /* Volume Copyright Notice */
    UDF_EntityID app_id;        /* Application Identifier */
    UDF_Timestamp recording_datetime; /* Recording Date and Time */
    UDF_EntityID impl_id;       /* Implementation Identifier */
    uint8_t  impl_use[64];      /* Implementation Use */
    uint32_t predecessor_vds_location; /* Predecessor Volume Descriptor Sequence Location */
    uint16_t flags;             /* Flags */
    uint8_t  reserved[22];      /* Reserved */
} UDF_PrimaryVolumeDescriptor;
#pragma pack(pop)

/* Partition Descriptor (ECMA-167 3/10.5) */
#pragma pack(push, 1)
typedef struct {
    UDF_DescriptorTag tag;      /* Tag (identifier = 5) */
    uint32_t vds_number;        /* Volume Descriptor Sequence Number */
    uint16_t partition_flags;   /* Partition Flags */
    uint16_t partition_number;  /* Partition Number */
    UDF_EntityID partition_contents; /* Partition Contents */
    uint8_t  partition_contents_use[128]; /* Partition Contents Use */
    uint32_t access_type;       /* Access Type */
    uint32_t partition_start;   /* Partition Starting Location */
    uint32_t partition_length;  /* Partition Length */
    UDF_EntityID impl_id;       /* Implementation Identifier */
    uint8_t  impl_use[128];     /* Implementation Use */
    uint8_t  reserved[156];     /* Reserved */
} UDF_PartitionDescriptor;
#pragma pack(pop)

/* Logical Volume Descriptor (ECMA-167 3/10.6) */
#pragma pack(push, 1)
typedef struct {
    UDF_DescriptorTag tag;      /* Tag (identifier = 6) */
    uint32_t vds_number;        /* Volume Descriptor Sequence Number */
    uint8_t  descriptor_charset[64]; /* Descriptor Character Set */
    char     logical_volume_id[128]; /* Logical Volume Identifier (dstring) */
    uint32_t logical_block_size; /* Logical Block Size */
    UDF_EntityID domain_id;     /* Domain Identifier */
    uint8_t  logical_volume_contents_use[16]; /* Logical Volume Contents Use */
    uint32_t map_table_length;  /* Map Table Length */
    uint32_t num_partition_maps; /* Number of Partition Maps */
    UDF_EntityID impl_id;       /* Implementation Identifier */
    uint8_t  impl_use[128];     /* Implementation Use */
    UDF_ExtentAD integrity_seq_extent; /* Integrity Sequence Extent */
    uint8_t  partition_maps[];  /* Partition Maps (flexible array) */
} UDF_LogicalVolumeDescriptor;
#pragma pack(pop)

/* ICB Tag (ECMA-167 4/14.6) */
#pragma pack(push, 1)
typedef struct {
    uint32_t prior_recorded_num_direct_entries; /* Prior Recorded Number of Direct Entries */
    uint16_t strategy_type;     /* Strategy Type */
    uint8_t  strategy_parameter[2]; /* Strategy Parameter */
    uint16_t max_num_entries;   /* Maximum Number of Entries */
    uint8_t  reserved;          /* Reserved */
    uint8_t  file_type;         /* File Type */
    uint32_t parent_icb_location; /* Parent ICB Location (logical block) */
    uint16_t parent_icb_partition; /* Parent ICB Partition Reference */
    uint16_t flags;             /* Flags */
} UDF_ICBTag;
#pragma pack(pop)

/* File Set Descriptor (ECMA-167 4/14.1) */
#pragma pack(push, 1)
typedef struct {
    UDF_DescriptorTag tag;      /* Tag (identifier = 256) */
    UDF_Timestamp recording_datetime; /* Recording Date and Time */
    uint16_t interchange_level; /* Interchange Level */
    uint16_t max_interchange_level; /* Maximum Interchange Level */
    uint32_t charset_list;      /* Character Set List */
    uint32_t max_charset_list;  /* Maximum Character Set List */
    uint32_t file_set_number;   /* File Set Number */
    uint32_t file_set_desc_number; /* File Set Descriptor Number */
    uint8_t  logical_volume_id_charset[64]; /* Logical Volume Identifier Character Set */
    char     logical_volume_id[128]; /* Logical Volume Identifier (dstring) */
    uint8_t  file_set_charset[64]; /* File Set Character Set */
    char     file_set_id[32];   /* File Set Identifier (dstring) */
    char     copyright_file_id[32]; /* Copyright File Identifier (dstring) */
    char     abstract_file_id[32]; /* Abstract File Identifier (dstring) */
    UDF_LongAD root_dir_icb;    /* Root Directory ICB */
    UDF_EntityID domain_id;     /* Domain Identifier */
    UDF_LongAD next_extent;     /* Next Extent */
    UDF_LongAD system_stream_dir_icb; /* System Stream Directory ICB */
    uint8_t  reserved[32];      /* Reserved */
} UDF_FileSetDescriptor;
#pragma pack(pop)

/* File Entry (ECMA-167 4/14.9) */
#pragma pack(push, 1)
typedef struct {
    UDF_DescriptorTag tag;      /* Tag (identifier = 261) */
    UDF_ICBTag icb_tag;         /* ICB Tag */
    uint32_t uid;               /* UID */
    uint32_t gid;               /* GID */
    uint32_t permissions;       /* Permissions */
    uint16_t file_link_count;   /* File Link Count */
    uint8_t  record_format;     /* Record Format */
    uint8_t  record_display_attrs; /* Record Display Attributes */
    uint32_t record_length;     /* Record Length */
    uint64_t info_length;       /* Information Length */
    uint64_t logical_blocks_recorded; /* Logical Blocks Recorded */
    UDF_Timestamp access_time;  /* Access Date and Time */
    UDF_Timestamp modification_time; /* Modification Date and Time */
    UDF_Timestamp attr_time;    /* Attribute Date and Time */
    uint32_t checkpoint;        /* Checkpoint */
    UDF_LongAD extended_attr_icb; /* Extended Attribute ICB */
    UDF_EntityID impl_id;       /* Implementation Identifier */
    uint64_t unique_id;         /* Unique ID */
    uint32_t length_extended_attrs; /* Length of Extended Attributes */
    uint32_t length_alloc_descs; /* Length of Allocation Descriptors */
    uint8_t  data[];            /* Extended Attributes and Allocation Descriptors */
} UDF_FileEntry;
#pragma pack(pop)

/* Extended File Entry (ECMA-167 4/14.17) - UDF 2.00+ */
#pragma pack(push, 1)
typedef struct {
    UDF_DescriptorTag tag;      /* Tag (identifier = 266) */
    UDF_ICBTag icb_tag;         /* ICB Tag */
    uint32_t uid;               /* UID */
    uint32_t gid;               /* GID */
    uint32_t permissions;       /* Permissions */
    uint16_t file_link_count;   /* File Link Count */
    uint8_t  record_format;     /* Record Format */
    uint8_t  record_display_attrs; /* Record Display Attributes */
    uint32_t record_length;     /* Record Length */
    uint64_t info_length;       /* Information Length */
    uint64_t object_size;       /* Object Size */
    uint64_t logical_blocks_recorded; /* Logical Blocks Recorded */
    UDF_Timestamp access_time;  /* Access Date and Time */
    UDF_Timestamp modification_time; /* Modification Date and Time */
    UDF_Timestamp creation_time; /* Creation Date and Time */
    UDF_Timestamp attr_time;    /* Attribute Date and Time */
    uint32_t checkpoint;        /* Checkpoint */
    uint32_t reserved;          /* Reserved */
    UDF_LongAD extended_attr_icb; /* Extended Attribute ICB */
    UDF_LongAD stream_dir_icb;  /* Stream Directory ICB */
    UDF_EntityID impl_id;       /* Implementation Identifier */
    uint64_t unique_id;         /* Unique ID */
    uint32_t length_extended_attrs; /* Length of Extended Attributes */
    uint32_t length_alloc_descs; /* Length of Allocation Descriptors */
    uint8_t  data[];            /* Extended Attributes and Allocation Descriptors */
} UDF_ExtendedFileEntry;
#pragma pack(pop)

/* File Identifier Descriptor (ECMA-167 4/14.4) */
#pragma pack(push, 1)
typedef struct {
    UDF_DescriptorTag tag;      /* Tag (identifier = 257) */
    uint16_t file_version_number; /* File Version Number */
    uint8_t  file_characteristics; /* File Characteristics */
    uint8_t  length_file_id;    /* Length of File Identifier */
    UDF_LongAD icb;             /* ICB */
    uint16_t length_impl_use;   /* Length of Implementation Use */
    uint8_t  data[];            /* Implementation Use, File Identifier, Padding */
} UDF_FileIdentifierDescriptor;
#pragma pack(pop)

/* Terminating Descriptor (ECMA-167 3/10.9) */
#pragma pack(push, 1)
typedef struct {
    UDF_DescriptorTag tag;      /* Tag (identifier = 8) */
    uint8_t reserved[496];      /* Reserved */
} UDF_TerminatingDescriptor;
#pragma pack(pop)

#endif /* UDF_TYPES_H */
