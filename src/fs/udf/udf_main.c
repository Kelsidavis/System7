/* UDF (Universal Disk Format) Filesystem Driver for System 7X VFS
 *
 * Implements UDF filesystem support for DVD, Blu-ray, and modern optical media.
 * Based on ECMA-167 and ISO 13346 standards.
 * Supports UDF 1.02 through 2.60 revisions.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/FS/udf_types.h"
#include "../../../include/System71StdLib.h"

/* UDF Private Data */
typedef struct {
    BlockDevice* vfs_blockdev;
    uint32_t partition_start;       /* Partition starting logical sector */
    uint32_t partition_length;      /* Partition length in sectors */
    uint32_t logical_block_size;    /* Logical block size (usually 2048) */
    uint32_t root_icb_location;     /* Root directory ICB location */
    uint16_t root_icb_partition;    /* Root directory ICB partition */
    uint64_t total_bytes;
    char     volume_id[128];
    bool     volume_initialized;
} UDFPrivate;

/* Helper: Convert UDF dstring to C string */
static void udf_dstring_to_cstring(const char* dstring, size_t dstring_len, char* output, size_t output_max) {
    if (dstring_len == 0 || output_max == 0) {
        if (output_max > 0) output[0] = '\0';
        return;
    }

    /* UDF dstrings have length byte at beginning */
    uint8_t len = (uint8_t)dstring[dstring_len - 1];
    if (len == 0 || len >= dstring_len) {
        output[0] = '\0';
        return;
    }

    /* Check compression ID (first byte of string data) */
    uint8_t comp_id = (uint8_t)dstring[0];
    const char* str_data = dstring + 1;
    size_t actual_len = len - 1;

    if (comp_id == 8) {
        /* Uncompressed (ASCII/Latin-1) */
        size_t copy_len = (actual_len < output_max - 1) ? actual_len : output_max - 1;
        memcpy(output, str_data, copy_len);
        output[copy_len] = '\0';
    } else if (comp_id == 16) {
        /* Compressed Unicode (UCS-2) */
        size_t out_pos = 0;
        for (size_t i = 0; i < actual_len && i + 1 < actual_len && out_pos < output_max - 1; i += 2) {
            uint16_t ch = ((uint8_t)str_data[i] << 8) | (uint8_t)str_data[i + 1];
            if (ch < 128 && ch != 0) {
                output[out_pos++] = (char)ch;
            } else if (ch >= 128) {
                output[out_pos++] = '?';
            }
        }
        output[out_pos] = '\0';
    } else {
        /* Unknown compression */
        output[0] = '\0';
    }
}

/* Helper: Verify descriptor tag */
static bool udf_verify_tag(const UDF_DescriptorTag* tag, uint16_t expected_id, uint32_t expected_location) {
    if (tag->tag_id != expected_id) {
        return false;
    }

    /* Verify tag location if specified */
    if (expected_location != 0xFFFFFFFF && tag->tag_location != expected_location) {
        return false;
    }

    /* Verify checksum */
    const uint8_t* tag_bytes = (const uint8_t*)tag;
    uint8_t checksum = 0;
    for (int i = 0; i < 16; i++) {
        if (i != 4) {  /* Skip checksum byte itself */
            checksum += tag_bytes[i];
        }
    }

    if (checksum != tag->tag_checksum) {
        return false;
    }

    return true;
}

/* Helper: Convert logical block number to physical sector */
static uint32_t udf_lbn_to_sector(UDFPrivate* priv, uint32_t lbn, uint16_t partition) {
    /* For now, assume single partition (partition 0) */
    if (partition != 0) {
        return 0;  /* Error */
    }
    return priv->partition_start + lbn;
}

/* Mount UDF Volume */
static void* udf_mount(VFSVolume* vol, BlockDevice* dev) {
    serial_printf("[UDF] Attempting to mount UDF filesystem...\n");

    /* Allocate private data */
    UDFPrivate* priv = (UDFPrivate*)malloc(sizeof(UDFPrivate));
    if (!priv) {
        serial_printf("[UDF] ERROR: Failed to allocate private data\n");
        return NULL;
    }

    memset(priv, 0, sizeof(UDFPrivate));
    priv->vfs_blockdev = dev;

    /* Read Anchor Volume Descriptor Pointer (AVDP) at sector 256 */
    uint8_t buffer[UDF_SECTOR_SIZE];
    uint32_t avdp_block = (UDF_AVDP_SECTOR * UDF_SECTOR_SIZE) / dev->block_size;

    serial_printf("[UDF] Reading AVDP at sector %u (block %u)...\n", UDF_AVDP_SECTOR, avdp_block);

    if (!dev->read_block(dev, avdp_block, buffer)) {
        serial_printf("[UDF] ERROR: Failed to read AVDP\n");
        free(priv);
        return NULL;
    }

    UDF_AnchorVolumeDescriptorPointer* avdp = (UDF_AnchorVolumeDescriptorPointer*)buffer;

    if (!udf_verify_tag(&avdp->tag, UDF_TAG_ANCHOR, UDF_AVDP_SECTOR)) {
        serial_printf("[UDF] ERROR: Invalid AVDP tag (id=%u, expected=%u)\n",
                     avdp->tag.tag_id, UDF_TAG_ANCHOR);
        free(priv);
        return NULL;
    }

    serial_printf("[UDF] AVDP found, main VDS at sector %u (length %u)\n",
                 avdp->main_vds.location, avdp->main_vds.length);

    /* Parse Volume Descriptor Sequence */
    uint32_t vds_sector = avdp->main_vds.location;
    uint32_t vds_length = avdp->main_vds.length;
    uint32_t vds_end = vds_sector + (vds_length / UDF_SECTOR_SIZE);

    bool found_pvd = false;
    bool found_partition = false;
    bool found_lvd = false;
    uint32_t fsd_location = 0;
    uint16_t fsd_partition = 0;

    for (uint32_t sector = vds_sector; sector < vds_end; sector++) {
        uint32_t block = (sector * UDF_SECTOR_SIZE) / dev->block_size;

        if (!dev->read_block(dev, block, buffer)) {
            continue;
        }

        UDF_DescriptorTag* tag = (UDF_DescriptorTag*)buffer;

        serial_printf("[UDF] VDS descriptor at sector %u: tag_id=%u\n", sector, tag->tag_id);

        switch (tag->tag_id) {
            case UDF_TAG_PVD: {
                /* Primary Volume Descriptor */
                UDF_PrimaryVolumeDescriptor* pvd = (UDF_PrimaryVolumeDescriptor*)buffer;
                udf_dstring_to_cstring(pvd->volume_id, sizeof(pvd->volume_id),
                                      priv->volume_id, sizeof(priv->volume_id));
                serial_printf("[UDF] Primary Volume Descriptor found: '%s'\n", priv->volume_id);
                found_pvd = true;
                break;
            }

            case UDF_TAG_PARTITION: {
                /* Partition Descriptor */
                UDF_PartitionDescriptor* pd = (UDF_PartitionDescriptor*)buffer;
                priv->partition_start = pd->partition_start;
                priv->partition_length = pd->partition_length;
                priv->total_bytes = (uint64_t)priv->partition_length * UDF_SECTOR_SIZE;
                serial_printf("[UDF] Partition Descriptor: start=%u, length=%u\n",
                             priv->partition_start, priv->partition_length);
                found_partition = true;
                break;
            }

            case UDF_TAG_LOGICAL_VOLUME: {
                /* Logical Volume Descriptor */
                UDF_LogicalVolumeDescriptor* lvd = (UDF_LogicalVolumeDescriptor*)buffer;
                priv->logical_block_size = lvd->logical_block_size;

                /* Extract File Set Descriptor location from logical_volume_contents_use */
                UDF_LongAD* fsd_ad = (UDF_LongAD*)lvd->logical_volume_contents_use;
                fsd_location = fsd_ad->location;
                fsd_partition = fsd_ad->partition;

                serial_printf("[UDF] Logical Volume Descriptor: block_size=%u, FSD at LBN %u (partition %u)\n",
                             priv->logical_block_size, fsd_location, fsd_partition);
                found_lvd = true;
                break;
            }

            case UDF_TAG_TERMINATING:
                serial_printf("[UDF] Terminating descriptor found\n");
                sector = vds_end;  /* Exit loop */
                break;
        }
    }

    if (!found_pvd || !found_partition || !found_lvd) {
        serial_printf("[UDF] ERROR: Missing required volume descriptors (PVD=%d, PD=%d, LVD=%d)\n",
                     found_pvd, found_partition, found_lvd);
        free(priv);
        return NULL;
    }

    /* Read File Set Descriptor */
    uint32_t fsd_sector = udf_lbn_to_sector(priv, fsd_location, fsd_partition);
    uint32_t fsd_block = (fsd_sector * UDF_SECTOR_SIZE) / dev->block_size;

    serial_printf("[UDF] Reading File Set Descriptor at LBN %u (sector %u, block %u)...\n",
                 fsd_location, fsd_sector, fsd_block);

    if (!dev->read_block(dev, fsd_block, buffer)) {
        serial_printf("[UDF] ERROR: Failed to read File Set Descriptor\n");
        free(priv);
        return NULL;
    }

    UDF_FileSetDescriptor* fsd = (UDF_FileSetDescriptor*)buffer;

    if (!udf_verify_tag(&fsd->tag, UDF_TAG_FILE_SET, fsd_location)) {
        serial_printf("[UDF] ERROR: Invalid File Set Descriptor tag (id=%u, expected=%u)\n",
                     fsd->tag.tag_id, UDF_TAG_FILE_SET);
        free(priv);
        return NULL;
    }

    /* Extract root directory ICB location */
    priv->root_icb_location = fsd->root_dir_icb.location;
    priv->root_icb_partition = fsd->root_dir_icb.partition;

    serial_printf("[UDF] Root directory ICB at LBN %u (partition %u)\n",
                 priv->root_icb_location, priv->root_icb_partition);

    priv->volume_initialized = true;

    serial_printf("[UDF] Mount successful!\n");
    serial_printf("[UDF]   Volume: '%s'\n", priv->volume_id);
    serial_printf("[UDF]   Block size: %u\n", priv->logical_block_size);
    serial_printf("[UDF]   Partition: %u sectors starting at %u\n",
                 priv->partition_length, priv->partition_start);

    return priv;
}

/* Unmount UDF Volume */
static void udf_unmount(VFSVolume* vol) {
    UDFPrivate* priv = (UDFPrivate*)vol->fs_private;
    if (priv) {
        free(priv);
    }
}

/* Read File Entry or Extended File Entry */
static bool udf_read_file_entry(UDFPrivate* priv, uint32_t icb_location, uint16_t icb_partition,
                                uint8_t* file_type, uint64_t* file_size,
                                uint8_t** alloc_descs, uint32_t* alloc_descs_len) {
    BlockDevice* dev = priv->vfs_blockdev;
    uint8_t buffer[4096];

    uint32_t sector = udf_lbn_to_sector(priv, icb_location, icb_partition);
    uint32_t block = (sector * UDF_SECTOR_SIZE) / dev->block_size;

    if (!dev->read_block(dev, block, buffer)) {
        return false;
    }

    UDF_DescriptorTag* tag = (UDF_DescriptorTag*)buffer;

    if (tag->tag_id == UDF_TAG_FILE_ENTRY) {
        /* Standard File Entry */
        UDF_FileEntry* fe = (UDF_FileEntry*)buffer;
        *file_type = fe->icb_tag.file_type;
        *file_size = fe->info_length;
        *alloc_descs_len = fe->length_alloc_descs;

        /* Allocation descriptors start after extended attributes */
        *alloc_descs = fe->data + fe->length_extended_attrs;

    } else if (tag->tag_id == UDF_TAG_EXTENDED_FILE_ENTRY) {
        /* Extended File Entry (UDF 2.00+) */
        UDF_ExtendedFileEntry* efe = (UDF_ExtendedFileEntry*)buffer;
        *file_type = efe->icb_tag.file_type;
        *file_size = efe->info_length;
        *alloc_descs_len = efe->length_alloc_descs;

        /* Allocation descriptors start after extended attributes */
        *alloc_descs = efe->data + efe->length_extended_attrs;

    } else {
        serial_printf("[UDF] ERROR: Unknown file entry tag %u at LBN %u\n", tag->tag_id, icb_location);
        return false;
    }

    return true;
}

/* Enumerate directory entries */
static bool udf_enumerate(VFSVolume* vol, uint64_t dir_id,
                         bool (*callback)(void* user_data, const char* name,
                                          uint64_t id, bool is_dir),
                         void* user_data) {
    UDFPrivate* priv = (UDFPrivate*)vol->fs_private;
    BlockDevice* dev = priv->vfs_blockdev;

    if (!priv->volume_initialized) {
        return false;
    }

    /* Determine ICB location */
    uint32_t icb_location;
    uint16_t icb_partition;

    if (dir_id == 0) {
        /* Root directory */
        icb_location = priv->root_icb_location;
        icb_partition = priv->root_icb_partition;
    } else {
        /* Use dir_id as ICB location (encoded as location | (partition << 32)) */
        icb_location = (uint32_t)(dir_id & 0xFFFFFFFF);
        icb_partition = (uint16_t)((dir_id >> 32) & 0xFFFF);
    }

    /* Read file entry for directory */
    uint8_t file_type;
    uint64_t dir_size __attribute__((unused));
    uint8_t* alloc_descs;
    uint32_t alloc_descs_len;

    uint8_t fe_buffer[4096];
    uint32_t fe_sector = udf_lbn_to_sector(priv, icb_location, icb_partition);
    uint32_t fe_block = (fe_sector * UDF_SECTOR_SIZE) / dev->block_size;

    if (!dev->read_block(dev, fe_block, fe_buffer)) {
        return false;
    }

    UDF_DescriptorTag* fe_tag = (UDF_DescriptorTag*)fe_buffer;

    if (fe_tag->tag_id == UDF_TAG_FILE_ENTRY) {
        UDF_FileEntry* fe = (UDF_FileEntry*)fe_buffer;
        file_type = fe->icb_tag.file_type;
        dir_size = fe->info_length;
        alloc_descs_len = fe->length_alloc_descs;
        alloc_descs = fe->data + fe->length_extended_attrs;
    } else if (fe_tag->tag_id == UDF_TAG_EXTENDED_FILE_ENTRY) {
        UDF_ExtendedFileEntry* efe = (UDF_ExtendedFileEntry*)fe_buffer;
        file_type = efe->icb_tag.file_type;
        dir_size = efe->info_length;
        alloc_descs_len = efe->length_alloc_descs;
        alloc_descs = efe->data + efe->length_extended_attrs;
    } else {
        return false;
    }

    if (file_type != UDF_ICB_FILETYPE_DIRECTORY) {
        serial_printf("[UDF] ERROR: ICB is not a directory (type=%u)\n", file_type);
        return false;
    }

    /* Read directory data using allocation descriptors */
    /* For simplicity, assume single short allocation descriptor */
    if (alloc_descs_len < sizeof(UDF_ShortAD)) {
        return false;
    }

    UDF_ShortAD* short_ad = (UDF_ShortAD*)alloc_descs;
    uint32_t extent_length = short_ad->length & 0x3FFFFFFF;  /* Lower 30 bits */
    uint32_t extent_position = short_ad->position;

    /* Read directory contents */
    uint8_t dir_buffer[8192];
    uint32_t dir_sector = udf_lbn_to_sector(priv, extent_position, icb_partition);
    uint32_t dir_block = (dir_sector * UDF_SECTOR_SIZE) / dev->block_size;

    size_t bytes_to_read = (extent_length < sizeof(dir_buffer)) ? extent_length : sizeof(dir_buffer);
    size_t blocks_to_read = (bytes_to_read + dev->block_size - 1) / dev->block_size;

    for (size_t i = 0; i < blocks_to_read; i++) {
        if (!dev->read_block(dev, dir_block + i, dir_buffer + (i * dev->block_size))) {
            return false;
        }
    }

    /* Parse File Identifier Descriptors */
    size_t offset = 0;
    while (offset < bytes_to_read) {
        if (offset + sizeof(UDF_FileIdentifierDescriptor) > bytes_to_read) {
            break;
        }

        UDF_FileIdentifierDescriptor* fid = (UDF_FileIdentifierDescriptor*)(dir_buffer + offset);

        if (fid->tag.tag_id != UDF_TAG_FILE_ID) {
            break;
        }

        /* Skip parent directory entry */
        if (fid->file_characteristics & UDF_FID_PARENT) {
            offset += ((sizeof(UDF_FileIdentifierDescriptor) + fid->length_impl_use +
                       fid->length_file_id + 3) & ~3);  /* 4-byte alignment */
            continue;
        }

        /* Extract filename */
        char filename[256];
        const char* file_id_start = (const char*)fid->data + fid->length_impl_use;
        udf_dstring_to_cstring(file_id_start, fid->length_file_id, filename, sizeof(filename));

        if (filename[0] != '\0') {
            /* Determine if directory */
            bool is_dir = (fid->file_characteristics & UDF_FID_DIRECTORY) ? true : false;

            /* Encode ICB location as file_id (location | (partition << 32)) */
            uint64_t file_id = (uint64_t)fid->icb.location | ((uint64_t)fid->icb.partition << 32);

            /* Invoke callback */
            if (!callback(user_data, filename, file_id, is_dir)) {
                return true;  /* Callback requested stop */
            }
        }

        /* Move to next FID (4-byte aligned) */
        offset += ((sizeof(UDF_FileIdentifierDescriptor) + fid->length_impl_use +
                   fid->length_file_id + 3) & ~3);
    }

    return true;
}

/* Read file data */
static bool udf_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                    void* buffer, size_t length, size_t* bytes_read) {
    UDFPrivate* priv = (UDFPrivate*)vol->fs_private;
    BlockDevice* dev = priv->vfs_blockdev;

    if (!priv->volume_initialized) {
        return false;
    }

    /* Decode file_id to ICB location and partition */
    uint32_t icb_location = (uint32_t)(file_id & 0xFFFFFFFF);
    uint16_t icb_partition = (uint16_t)((file_id >> 32) & 0xFFFF);

    /* Read file entry */
    uint8_t file_type __attribute__((unused));
    uint64_t file_size;
    uint8_t* alloc_descs;
    uint32_t alloc_descs_len;

    uint8_t fe_buffer[4096];
    uint32_t fe_sector = udf_lbn_to_sector(priv, icb_location, icb_partition);
    uint32_t fe_block = (fe_sector * UDF_SECTOR_SIZE) / dev->block_size;

    if (!dev->read_block(dev, fe_block, fe_buffer)) {
        return false;
    }

    UDF_DescriptorTag* fe_tag = (UDF_DescriptorTag*)fe_buffer;

    if (fe_tag->tag_id == UDF_TAG_FILE_ENTRY) {
        UDF_FileEntry* fe = (UDF_FileEntry*)fe_buffer;
        file_type = fe->icb_tag.file_type;
        file_size = fe->info_length;
        alloc_descs_len = fe->length_alloc_descs;
        alloc_descs = fe->data + fe->length_extended_attrs;
    } else if (fe_tag->tag_id == UDF_TAG_EXTENDED_FILE_ENTRY) {
        UDF_ExtendedFileEntry* efe = (UDF_ExtendedFileEntry*)fe_buffer;
        file_type = efe->icb_tag.file_type;
        file_size = efe->info_length;
        alloc_descs_len = efe->length_alloc_descs;
        alloc_descs = efe->data + efe->length_extended_attrs;
    } else {
        return false;
    }

    if (offset >= file_size) {
        *bytes_read = 0;
        return true;
    }

    size_t to_read = length;
    if (offset + to_read > file_size) {
        to_read = file_size - offset;
    }

    /* Read data using allocation descriptors */
    /* For simplicity, assume single short allocation descriptor */
    if (alloc_descs_len < sizeof(UDF_ShortAD)) {
        return false;
    }

    UDF_ShortAD* short_ad = (UDF_ShortAD*)alloc_descs;
    uint32_t extent_length __attribute__((unused)) = short_ad->length & 0x3FFFFFFF;
    uint32_t extent_position = short_ad->position;

    uint32_t data_sector = udf_lbn_to_sector(priv, extent_position, icb_partition);
    uint32_t byte_offset = (data_sector * UDF_SECTOR_SIZE) + offset;
    uint32_t start_block = byte_offset / dev->block_size;

    uint8_t temp_buffer[4096];
    size_t total_read = 0;

    while (total_read < to_read) {
        if (!dev->read_block(dev, start_block, temp_buffer)) {
            break;
        }

        size_t chunk = (to_read - total_read < sizeof(temp_buffer)) ?
                       to_read - total_read : sizeof(temp_buffer);
        memcpy((uint8_t*)buffer + total_read, temp_buffer, chunk);

        total_read += chunk;
        start_block++;
    }

    *bytes_read = total_read;
    return total_read > 0;
}

/* UDF Filesystem Operations */
static FileSystemOps udf_ops = {
    .fs_name = "UDF",
    .fs_version = 1,
    .probe = NULL,        /* TODO: Implement probe */
    .mount = udf_mount,
    .unmount = udf_unmount,
    .read = udf_read,
    .write = NULL,        /* Read-only for now */
    .enumerate = udf_enumerate,
    .lookup = NULL,       /* TODO: Implement lookup */
    .delete = NULL,       /* Read-only for now */
    .mkdir = NULL         /* Read-only for now */
};

/* Get UDF operations structure */
FileSystemOps* UDF_GetOps(void) {
    return &udf_ops;
}
