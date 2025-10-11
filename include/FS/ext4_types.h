/* ext4 Filesystem Types and Structures */
#ifndef EXT4_TYPES_H
#define EXT4_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* ext4 Superblock - starts at byte 1024 */
#pragma pack(push, 1)
typedef struct {
    /* Base ext2 superblock fields */
    uint32_t s_inodes_count;        /* Inodes count */
    uint32_t s_blocks_count_lo;     /* Blocks count (low 32 bits) */
    uint32_t s_r_blocks_count_lo;   /* Reserved blocks count (low) */
    uint32_t s_free_blocks_count_lo;/* Free blocks count (low) */
    uint32_t s_free_inodes_count;   /* Free inodes count */
    uint32_t s_first_data_block;    /* First Data Block */
    uint32_t s_log_block_size;      /* Block size (1024 << s_log_block_size) */
    uint32_t s_log_cluster_size;    /* Obsolete fragment size */
    uint32_t s_blocks_per_group;    /* Blocks per group */
    uint32_t s_clusters_per_group;  /* Clusters per group */
    uint32_t s_inodes_per_group;    /* Inodes per group */
    uint32_t s_mtime;               /* Mount time */
    uint32_t s_wtime;               /* Write time */
    uint16_t s_mnt_count;           /* Mount count */
    uint16_t s_max_mnt_count;       /* Maximal mount count */
    uint16_t s_magic;               /* Magic signature */
    uint16_t s_state;               /* File system state */
    uint16_t s_errors;              /* Behaviour when detecting errors */
    uint16_t s_minor_rev_level;     /* minor revision level */
    uint32_t s_lastcheck;           /* time of last check */
    uint32_t s_checkinterval;       /* max. time between checks */
    uint32_t s_creator_os;          /* OS */
    uint32_t s_rev_level;           /* Revision level */
    uint16_t s_def_resuid;          /* Default uid for reserved blocks */
    uint16_t s_def_resgid;          /* Default gid for reserved blocks */

    /* ext4 extended superblock fields */
    uint32_t s_first_ino;           /* First non-reserved inode */
    uint16_t s_inode_size;          /* size of inode structure */
    uint16_t s_block_group_nr;      /* block group # of this superblock */
    uint32_t s_feature_compat;      /* compatible feature set */
    uint32_t s_feature_incompat;    /* incompatible feature set */
    uint32_t s_feature_ro_compat;   /* readonly-compatible feature set */
    uint8_t  s_uuid[16];            /* 128-bit uuid for volume */
    char     s_volume_name[16];     /* volume name */
    char     s_last_mounted[64];    /* directory where last mounted */
    uint32_t s_algorithm_usage_bitmap; /* For compression */

    /* Performance hints */
    uint8_t  s_prealloc_blocks;     /* Nr of blocks to try to preallocate*/
    uint8_t  s_prealloc_dir_blocks; /* Nr to preallocate for dirs */
    uint16_t s_reserved_gdt_blocks; /* Per group desc for online growth */

    /* Journaling support */
    uint8_t  s_journal_uuid[16];    /* uuid of journal superblock */
    uint32_t s_journal_inum;        /* inode number of journal file */
    uint32_t s_journal_dev;         /* device number of journal file */
    uint32_t s_last_orphan;         /* start of list of inodes to delete */
    uint32_t s_hash_seed[4];        /* HTREE hash seed */
    uint8_t  s_def_hash_version;    /* Default hash version to use */
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;           /* size of group descriptor */
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;       /* First metablock block group */
    uint32_t s_mkfs_time;           /* When the filesystem was created */
    uint32_t s_jnl_blocks[17];      /* Backup of the journal inode */

    /* 64bit support valid if EXT4_FEATURE_COMPAT_64BIT */
    uint32_t s_blocks_count_hi;     /* Blocks count (high 32 bits) */
    uint32_t s_r_blocks_count_hi;   /* Reserved blocks count (high) */
    uint32_t s_free_blocks_count_hi;/* Free blocks count (high) */
    uint16_t s_min_extra_isize;     /* All inodes have at least # bytes */
    uint16_t s_want_extra_isize;    /* New inodes should reserve # bytes */
    uint32_t s_flags;               /* Miscellaneous flags */
    uint16_t s_raid_stride;         /* RAID stride */
    uint16_t s_mmp_interval;        /* # seconds to wait in MMP checking */
    uint64_t s_mmp_block;           /* Block for multi-mount protection */
    uint32_t s_raid_stripe_width;   /* blocks on all data disks (N*stride)*/
    uint8_t  s_log_groups_per_flex; /* FLEX_BG group size */
    uint8_t  s_checksum_type;       /* metadata checksum algorithm used */
    uint8_t  s_encryption_level;    /* versioning level for encryption */
    uint8_t  s_reserved_pad;        /* Padding to next 32bits */
    uint64_t s_kbytes_written;      /* nr of lifetime kilobytes written */
    uint32_t s_snapshot_inum;       /* Inode number of active snapshot */
    uint32_t s_snapshot_id;         /* sequential ID of active snapshot */
    uint64_t s_snapshot_r_blocks_count; /* reserved blocks for active
                                          snapshot's future use */
    uint32_t s_snapshot_list;       /* inode number of the head of the
                                       on-disk snapshot list */
    uint32_t s_error_count;         /* number of fs errors */
    uint32_t s_first_error_time;    /* first time an error happened */
    uint32_t s_first_error_ino;     /* inode involved in first error */
    uint64_t s_first_error_block;   /* block involved of first error */
    uint8_t  s_first_error_func[32]; /* function where the error happened */
    uint32_t s_first_error_line;    /* line number where error happened */
    uint32_t s_last_error_time;     /* most recent time of an error */
    uint32_t s_last_error_ino;      /* inode involved in last error */
    uint32_t s_last_error_line;     /* line number where error happened */
    uint64_t s_last_error_block;    /* block involved of last error */
    uint8_t  s_last_error_func[32]; /* function where the error happened */
    uint8_t  s_mount_opts[64];
    uint32_t s_usr_quota_inum;      /* inode for tracking user quota */
    uint32_t s_grp_quota_inum;      /* inode for tracking group quota */
    uint32_t s_overhead_clusters;   /* overhead blocks/clusters in fs */
    uint32_t s_backup_bgs[2];       /* groups with sparse_super2 SBs */
    uint8_t  s_encrypt_algos[4];    /* Encryption algorithms in use  */
    uint8_t  s_encrypt_pw_salt[16]; /* Salt used for string2key algorithm */
    uint32_t s_lpf_ino;             /* Location of the lost+found inode */
    uint32_t s_prj_quota_inum;      /* inode for tracking project quota */
    uint32_t s_checksum_seed;       /* crc32c(uuid) if csum_seed set */
    uint32_t s_reserved[98];        /* Padding to the end of the block */
    uint32_t s_checksum;            /* crc32c(superblock) */
} EXT4_Superblock;
#pragma pack(pop)

/* ext4 Group Descriptor */
#pragma pack(push, 1)
typedef struct {
    uint32_t bg_block_bitmap_lo;      /* Blocks bitmap block (low) */
    uint32_t bg_inode_bitmap_lo;      /* Inodes bitmap block (low) */
    uint32_t bg_inode_table_lo;       /* Inodes table block (low) */
    uint16_t bg_free_blocks_count_lo; /* Free blocks count (low) */
    uint16_t bg_free_inodes_count_lo; /* Free inodes count (low) */
    uint16_t bg_used_dirs_count_lo;   /* Directories count (low) */
    uint16_t bg_flags;                /* EXT4_BG_flags (INODE_UNINIT, etc) */
    uint32_t bg_exclude_bitmap_lo;    /* Exclude bitmap for snapshots (low) */
    uint16_t bg_block_bitmap_csum_lo; /* crc32c(s_uuid+grp_num+bbitmap) LE */
    uint16_t bg_inode_bitmap_csum_lo; /* crc32c(s_uuid+grp_num+ibitmap) LE */
    uint16_t bg_itable_unused_lo;     /* Unused inodes count (low) */
    uint16_t bg_checksum;             /* crc16(sb_uuid+group+desc) */
} EXT4_GroupDesc;
#pragma pack(pop)

/* ext4 Inode */
#pragma pack(push, 1)
typedef struct {
    uint16_t i_mode;        /* File mode */
    uint16_t i_uid;         /* Low 16 bits of Owner Uid */
    uint32_t i_size_lo;     /* Size in bytes (low) */
    uint32_t i_atime;       /* Access time */
    uint32_t i_ctime;       /* Inode Change time */
    uint32_t i_mtime;       /* Modification time */
    uint32_t i_dtime;       /* Deletion Time */
    uint16_t i_gid;         /* Low 16 bits of Group Id */
    uint16_t i_links_count; /* Links count */
    uint32_t i_blocks_lo;   /* Blocks count (low) */
    uint32_t i_flags;       /* File flags */
    uint32_t i_osd1;        /* OS dependent 1 */
    uint32_t i_block[15];   /* Pointers to blocks */
    uint32_t i_generation;  /* File version (for NFS) */
    uint32_t i_file_acl_lo; /* File ACL (low) */
    uint32_t i_size_high;   /* Size in bytes (high) or dir_acl */
    uint32_t i_obso_faddr;  /* Obsoleted fragment address */
    uint8_t  i_osd2[12];    /* OS dependent 2 */
    uint16_t i_extra_isize; /* Size of extra inode fields */
    uint16_t i_checksum_hi; /* crc32c(uuid+inum+inode) BE */
    uint32_t i_ctime_extra; /* extra Change time (nsec << 2 | epoch) */
    uint32_t i_mtime_extra; /* extra Modification time(nsec << 2 | epoch) */
    uint32_t i_atime_extra; /* extra Access time (nsec << 2 | epoch) */
    uint32_t i_crtime;      /* File Creation time */
    uint32_t i_crtime_extra;/* extra FileCreationtime (nsec << 2 | epoch) */
    uint32_t i_version_hi;  /* high 32 bits for 64-bit version */
    uint32_t i_projid;      /* Project ID */
} EXT4_Inode;
#pragma pack(pop)

/* ext4 Directory Entry */
#pragma pack(push, 1)
typedef struct {
    uint32_t inode;         /* Inode number */
    uint16_t rec_len;       /* Directory entry length */
    uint8_t  name_len;      /* Name length */
    uint8_t  file_type;     /* File type */
    char     name[];        /* File name (flexible array) */
} EXT4_DirEntry;
#pragma pack(pop)

/* ext4 Constants */
#define EXT4_SUPER_MAGIC            0xEF53
#define EXT4_SUPERBLOCK_OFFSET      1024
#define EXT4_SUPERBLOCK_SIZE        1024
#define EXT4_ROOT_INO               2
#define EXT4_GOOD_OLD_INODE_SIZE    128

/* Feature flags */
#define EXT4_FEATURE_INCOMPAT_64BIT 0x0080

/* File type values for directory entries */
#define EXT4_FT_UNKNOWN     0
#define EXT4_FT_REG_FILE    1
#define EXT4_FT_DIR         2
#define EXT4_FT_CHRDEV      3
#define EXT4_FT_BLKDEV      4
#define EXT4_FT_FIFO        5
#define EXT4_FT_SOCK        6
#define EXT4_FT_SYMLINK     7

/* Inode mode flags */
#define EXT4_S_IFMT     0xF000  /* format mask */
#define EXT4_S_IFSOCK   0xC000  /* socket */
#define EXT4_S_IFLNK    0xA000  /* symbolic link */
#define EXT4_S_IFREG    0x8000  /* regular file */
#define EXT4_S_IFBLK    0x6000  /* block device */
#define EXT4_S_IFDIR    0x4000  /* directory */
#define EXT4_S_IFCHR    0x2000  /* character device */
#define EXT4_S_IFIFO    0x1000  /* fifo */

/* Inode flags */
#define EXT4_EXTENTS_FL 0x00080000  /* Inode uses extents */

#endif /* EXT4_TYPES_H */
