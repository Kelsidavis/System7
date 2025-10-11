/* ISO 9660 Filesystem Driver for System 7X VFS
 *
 * This driver implements the FileSystemOps interface for ISO 9660 (CD-ROM).
 * Supports ISO 9660 Level 1/2, Rock Ridge, and Joliet extensions.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/FS/iso9660_types.h"
#include "../../../include/FS/hfs_diskio.h"
#include "../../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* Read 16-bit both-endian value (use LSB) */
static uint16_t iso_read16(const uint8_t* lsb, const uint8_t* msb) {
    (void)msb;
    return lsb[0] | ((uint16_t)lsb[1] << 8);
}

/* Read 32-bit both-endian value (use LSB) */
static uint32_t iso_read32(const uint8_t* lsb, const uint8_t* msb) {
    (void)msb;
    return lsb[0] | ((uint32_t)lsb[1] << 8) |
           ((uint32_t)lsb[2] << 16) | ((uint32_t)lsb[3] << 24);
}

/* ISO 9660 private volume data */
typedef struct {
    ISO9660_PrimaryVolumeDescriptor pvd;           /* Primary volume descriptor */
    ISO9660_SupplementaryVolumeDescriptor svd;     /* Joliet supplementary descriptor */
    BlockDevice*     vfs_blockdev;
    uint64_t         total_bytes;
    uint32_t         block_size;
    uint32_t         root_extent;
    uint32_t         root_size;
    uint32_t         joliet_root_extent;           /* Joliet root extent */
    uint32_t         joliet_root_size;             /* Joliet root size */
    uint8_t          extensions;                    /* Extension flags */
    bool             volume_initialized;
    char             volume_name[65];
} ISO9660Private;

/* Detect Joliet extensions in supplementary volume descriptor */
static bool iso_detect_joliet(const ISO9660_SupplementaryVolumeDescriptor* svd) {
    /* Check for Joliet escape sequences in unused3 field (offset 88) */
    const uint8_t* escape = svd->unused3;

    if (memcmp(escape, JOLIET_ESCAPE_SEQ_LEVEL1, 3) == 0 ||
        memcmp(escape, JOLIET_ESCAPE_SEQ_LEVEL2, 3) == 0 ||
        memcmp(escape, JOLIET_ESCAPE_SEQ_LEVEL3, 3) == 0) {
        return true;
    }
    return false;
}

/* Convert Joliet UCS-2 Big-Endian name to ASCII */
static void joliet_to_ascii(const uint8_t* ucs2, uint8_t len, char* ascii, size_t max_len) {
    size_t out_pos = 0;

    for (uint8_t i = 0; i < len && i + 1 < len && out_pos < max_len - 1; i += 2) {
        uint16_t ch = (ucs2[i] << 8) | ucs2[i + 1];

        /* Convert to ASCII (simplified - just use low byte if < 128) */
        if (ch < 128 && ch != 0) {
            ascii[out_pos++] = (char)ch;
        } else if (ch >= 128) {
            ascii[out_pos++] = '?';  /* Non-ASCII character */
        }
    }
    ascii[out_pos] = '\0';
}

/* Parse Rock Ridge NM (alternate name) entry */
static bool parse_rr_nm(const uint8_t* susp_data, uint8_t susp_len, char* name, size_t max_len) {
    const uint8_t* ptr = susp_data;
    const uint8_t* end = susp_data + susp_len;
    size_t name_pos = 0;

    while (ptr + 4 <= end) {
        if (memcmp(ptr, RR_SIGNATURE_NM, 2) == 0) {
            uint8_t entry_len = ptr[2];
            uint8_t flags = ptr[4];

            if (ptr + entry_len > end) break;

            /* Skip special entries (current/parent dir) */
            if (flags & (RR_NM_CURRENT | RR_NM_PARENT)) {
                ptr += entry_len;
                continue;
            }

            /* Copy name content */
            const char* nm_content = (const char*)&ptr[5];
            uint8_t content_len = entry_len - 5;

            for (uint8_t i = 0; i < content_len && name_pos < max_len - 1; i++) {
                name[name_pos++] = nm_content[i];
            }

            /* If not continued, we're done */
            if (!(flags & RR_NM_CONTINUE)) {
                name[name_pos] = '\0';
                return true;
            }
        }

        ptr += ptr[2];  /* Move to next SUSP entry */
    }

    if (name_pos > 0) {
        name[name_pos] = '\0';
        return true;
    }
    return false;
}

/* Detect Rock Ridge extensions by checking root directory */
static bool iso_detect_rock_ridge(ISO9660Private* priv) {
    uint8_t buffer[ISO_SECTOR_SIZE];
    BlockDevice* dev = priv->vfs_blockdev;

    /* Read first block of root directory */
    uint32_t root_block = (priv->root_extent * priv->block_size) / dev->block_size;
    if (!dev->read_block(dev, root_block, buffer)) {
        return false;
    }

    /* Look for "SP" System Use Sharing Protocol indicator in first entry */
    ISO9660_DirectoryRecord* rec = (ISO9660_DirectoryRecord*)buffer;
    if (rec->length < sizeof(ISO9660_DirectoryRecord)) {
        return false;
    }

    /* SUSP area starts after name (padded to even byte) */
    uint8_t name_len = rec->name_len;
    uint8_t susp_offset = 33 + name_len;  /* 33 = sizeof fixed part */
    if (name_len % 2 == 0) susp_offset++;  /* Padding byte */

    if (susp_offset + 7 > rec->length) {
        return false;
    }

    const uint8_t* susp = buffer + susp_offset;

    /* Look for "SP" signature */
    if (memcmp(susp, RR_SIGNATURE_SP, 2) == 0) {
        return true;
    }

    return false;
}

/* Probe: Detect ISO 9660 filesystem */
static bool iso9660_probe(BlockDevice* dev) {
    uint8_t buffer[ISO_SECTOR_SIZE];

    uint32_t sector = ISO_VD_START_SECTOR;
    uint32_t block = (sector * ISO_SECTOR_SIZE) / dev->block_size;

    if (!dev->read_block(dev, block, buffer)) {
        return false;
    }

    if (buffer[0] == ISO_VD_PRIMARY &&
        memcmp(&buffer[1], ISO_STANDARD_ID, 5) == 0 &&
        buffer[6] == ISO_STANDARD_VERSION) {
        serial_printf("[ISO9660] ISO 9660 filesystem detected\n");
        return true;
    }

    return false;
}

/* Mount: Initialize ISO 9660 volume */
static void* iso9660_mount(VFSVolume* vol, BlockDevice* dev) {
    uint8_t buffer[ISO_SECTOR_SIZE];

    ISO9660Private* priv = (ISO9660Private*)malloc(sizeof(ISO9660Private));
    if (!priv) {
        serial_printf("[ISO9660] Failed to allocate private data\n");
        return NULL;
    }

    memset(priv, 0, sizeof(ISO9660Private));
    priv->vfs_blockdev = dev;
    priv->extensions = ISO_EXT_NONE;

    /* Read primary volume descriptor at sector 16 */
    uint32_t sector = ISO_VD_START_SECTOR;
    uint32_t start_block = (sector * ISO_SECTOR_SIZE) / dev->block_size;

    if (!dev->read_block(dev, start_block, buffer)) {
        serial_printf("[ISO9660] Failed to read primary volume descriptor\n");
        free(priv);
        return NULL;
    }

    memcpy(&priv->pvd, buffer, sizeof(ISO9660_PrimaryVolumeDescriptor));

    if (priv->pvd.type != ISO_VD_PRIMARY ||
        memcmp(priv->pvd.id, ISO_STANDARD_ID, 5) != 0) {
        serial_printf("[ISO9660] Invalid primary volume descriptor\n");
        free(priv);
        return NULL;
    }

    /* Get block size */
    priv->block_size = iso_read16((uint8_t*)&priv->pvd.logical_block_size_lsb,
                                  (uint8_t*)&priv->pvd.logical_block_size_msb);

    /* Get volume size */
    uint32_t volume_blocks = iso_read32((uint8_t*)&priv->pvd.volume_space_size_lsb,
                                        (uint8_t*)&priv->pvd.volume_space_size_msb);
    priv->total_bytes = (uint64_t)volume_blocks * priv->block_size;

    /* Extract primary volume name */
    memcpy(priv->volume_name, priv->pvd.volume_id, 32);
    priv->volume_name[32] = '\0';
    for (int i = 31; i >= 0 && (priv->volume_name[i] == ' ' || priv->volume_name[i] == '\0'); i--) {
        priv->volume_name[i] = '\0';
    }

    /* Extract root directory from PVD */
    ISO9660_DirectoryRecord* root_rec = (ISO9660_DirectoryRecord*)priv->pvd.root_directory_record;
    priv->root_extent = iso_read32((uint8_t*)&root_rec->extent_lsb, (uint8_t*)&root_rec->extent_msb);
    priv->root_size = iso_read32((uint8_t*)&root_rec->size_lsb, (uint8_t*)&root_rec->size_msb);

    /* Scan for supplementary volume descriptors (Joliet) */
    sector = ISO_VD_START_SECTOR + 1;
    for (int i = 0; i < 10; i++) {  /* Check up to 10 descriptors */
        start_block = (sector * ISO_SECTOR_SIZE) / dev->block_size;

        if (!dev->read_block(dev, start_block, buffer)) {
            break;
        }

        if (buffer[0] == ISO_VD_TERMINATOR) {
            break;  /* End of volume descriptors */
        }

        if (buffer[0] == ISO_VD_SUPPLEMENTARY &&
            memcmp(&buffer[1], ISO_STANDARD_ID, 5) == 0) {
            memcpy(&priv->svd, buffer, sizeof(ISO9660_SupplementaryVolumeDescriptor));

            if (iso_detect_joliet(&priv->svd)) {
                priv->extensions |= ISO_EXT_JOLIET;

                /* Extract Joliet root directory */
                ISO9660_DirectoryRecord* joliet_root = (ISO9660_DirectoryRecord*)priv->svd.root_directory_record;
                priv->joliet_root_extent = iso_read32((uint8_t*)&joliet_root->extent_lsb,
                                                      (uint8_t*)&joliet_root->extent_msb);
                priv->joliet_root_size = iso_read32((uint8_t*)&joliet_root->size_lsb,
                                                    (uint8_t*)&joliet_root->size_msb);

                /* Extract Joliet volume name (UCS-2) */
                joliet_to_ascii((uint8_t*)priv->svd.volume_id, 32, priv->volume_name, sizeof(priv->volume_name));

                serial_printf("[ISO9660] Joliet extensions detected\n");
            }
        }

        sector++;
    }

    /* Detect Rock Ridge extensions */
    if (iso_detect_rock_ridge(priv)) {
        priv->extensions |= ISO_EXT_ROCK_RIDGE;
        serial_printf("[ISO9660] Rock Ridge extensions detected\n");
    }

    priv->volume_initialized = true;

    serial_printf("[ISO9660] Mounted '%s' - %llu bytes\n",
                  priv->volume_name[0] ? priv->volume_name : "NO_LABEL",
                  priv->total_bytes);
    serial_printf("[ISO9660]   Block size: %u, Extensions:", priv->block_size);
    if (priv->extensions & ISO_EXT_ROCK_RIDGE) serial_printf(" RockRidge");
    if (priv->extensions & ISO_EXT_JOLIET) serial_printf(" Joliet");
    if (priv->extensions == ISO_EXT_NONE) serial_printf(" None (ISO9660 Level1/2)");
    serial_printf("\n");

    return priv;
}

/* Unmount */
static void iso9660_unmount(VFSVolume* vol) {
    if (vol->fs_private) {
        free(vol->fs_private);
        vol->fs_private = NULL;
    }
    serial_printf("[ISO9660] Unmounted volume\n");
}

/* Get statistics */
static bool iso9660_get_stats(VFSVolume* vol, uint64_t* total_bytes, uint64_t* free_bytes) {
    ISO9660Private* priv = (ISO9660Private*)vol->fs_private;
    if (!priv) return false;

    if (total_bytes) *total_bytes = priv->total_bytes;
    if (free_bytes) *free_bytes = 0;
    return true;
}

/* Read operation - implemented */
static bool iso9660_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                         void* buffer, size_t length, size_t* bytes_read) {
    ISO9660Private* priv = (ISO9660Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized) {
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    /* file_id is the extent (block number) */
    uint32_t extent = (uint32_t)file_id;
    uint32_t byte_offset = extent * priv->block_size + (uint32_t)offset;

    /* Simple block-aligned read */
    uint32_t block = byte_offset / priv->vfs_blockdev->block_size;
    size_t to_read = length;
    size_t total_read = 0;
    uint8_t temp_buffer[4096];

    while (to_read > 0 && total_read < length) {
        if (!priv->vfs_blockdev->read_block(priv->vfs_blockdev, block, temp_buffer)) {
            break;
        }

        size_t chunk = (to_read < sizeof(temp_buffer)) ? to_read : sizeof(temp_buffer);
        memcpy((uint8_t*)buffer + total_read, temp_buffer, chunk);

        total_read += chunk;
        to_read -= chunk;
        block++;
    }

    if (bytes_read) *bytes_read = total_read;
    return total_read > 0;
}

/* Write operation - not supported */
static bool iso9660_write(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                          const void* buffer, size_t length, size_t* bytes_written) {
    (void)vol; (void)file_id; (void)offset; (void)buffer; (void)length;
    if (bytes_written) *bytes_written = 0;
    return false;
}

/* Enumerate - implemented */
static bool iso9660_enumerate(VFSVolume* vol, uint64_t dir_id,
                              bool (*callback)(void* user_data, const char* name,
                                               uint64_t id, bool is_dir),
                              void* user_data) {
    ISO9660Private* priv = (ISO9660Private*)vol->fs_private;
    if (!priv || !priv->volume_initialized || !callback) {
        return false;
    }

    /* Use Joliet root if available and dir_id matches */
    uint32_t extent = (uint32_t)dir_id;
    if (extent == 0) {
        extent = (priv->extensions & ISO_EXT_JOLIET) ? priv->joliet_root_extent : priv->root_extent;
    }

    uint8_t buffer[ISO_SECTOR_SIZE];
    uint32_t block = (extent * priv->block_size) / priv->vfs_blockdev->block_size;

    if (!priv->vfs_blockdev->read_block(priv->vfs_blockdev, block, buffer)) {
        return false;
    }

    uint32_t offset = 0;
    while (offset < ISO_SECTOR_SIZE) {
        ISO9660_DirectoryRecord* rec = (ISO9660_DirectoryRecord*)(buffer + offset);

        if (rec->length == 0) break;

        /* Skip "." and ".." entries */
        if (rec->name_len == 1 && (rec->name[0] == 0 || rec->name[0] == 1)) {
            offset += rec->length;
            continue;
        }

        char name[256];
        bool got_name = false;

        /* Try Rock Ridge NM first */
        if (priv->extensions & ISO_EXT_ROCK_RIDGE) {
            uint8_t susp_offset = 33 + rec->name_len;
            if (rec->name_len % 2 == 0) susp_offset++;

            if (susp_offset < rec->length) {
                uint8_t susp_len = rec->length - susp_offset;
                got_name = parse_rr_nm(buffer + offset + susp_offset, susp_len, name, sizeof(name));
            }
        }

        /* Try Joliet UCS-2 name */
        if (!got_name && (priv->extensions & ISO_EXT_JOLIET)) {
            joliet_to_ascii((uint8_t*)rec->name, rec->name_len, name, sizeof(name));
            got_name = true;
        }

        /* Fall back to ISO 9660 name */
        if (!got_name) {
            memcpy(name, rec->name, rec->name_len);
            name[rec->name_len] = '\0';

            /* Remove version suffix (;1) */
            char* semicolon = strchr(name, ';');
            if (semicolon) *semicolon = '\0';
        }

        uint32_t file_extent = iso_read32((uint8_t*)&rec->extent_lsb, (uint8_t*)&rec->extent_msb);
        bool is_dir = (rec->flags & ISO_FILE_ISDIR) != 0;

        if (!callback(user_data, name, file_extent, is_dir)) {
            return true;  /* Callback requested stop */
        }

        offset += rec->length;
    }

    return true;
}

/* Lookup - stub */
static bool iso9660_lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                           uint64_t* entry_id, bool* is_dir) {
    (void)vol; (void)dir_id; (void)name;
    if (entry_id) *entry_id = 0;
    if (is_dir) *is_dir = false;
    return false;
}

/* Get file info */
static bool iso9660_get_file_info(VFSVolume* vol, uint64_t entry_id,
                                   uint64_t* size, bool* is_dir, uint64_t* mod_time) {
    ISO9660Private* priv = (ISO9660Private*)vol->fs_private;

    if (priv && priv->volume_initialized && entry_id == priv->root_extent) {
        if (size) *size = priv->root_size;
        if (is_dir) *is_dir = true;
        if (mod_time) *mod_time = 0;
        return true;
    }

    if (size) *size = 0;
    if (is_dir) *is_dir = false;
    if (mod_time) *mod_time = 0;
    return true;
}

/* ISO 9660 Filesystem Operations */
static FileSystemOps ISO9660_ops = {
    .fs_name = "iso9660",
    .fs_version = 2,  /* Version 2 with extensions */
    .probe = iso9660_probe,
    .mount = iso9660_mount,
    .unmount = iso9660_unmount,
    .read = iso9660_read,
    .write = iso9660_write,
    .enumerate = iso9660_enumerate,
    .lookup = iso9660_lookup,
    .get_stats = iso9660_get_stats,
    .get_file_info = iso9660_get_file_info,
    .format = NULL,
    .mkdir = NULL,
    .create_file = NULL,
    .delete = NULL,
    .rename = NULL
};

FileSystemOps* ISO9660_GetOps(void) {
    return &ISO9660_ops;
}
