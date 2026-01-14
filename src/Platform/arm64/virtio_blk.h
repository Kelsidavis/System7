/*
 * VirtIO Block Driver Header
 * For QEMU virtio-blk device support
 */

#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize virtio-blk device */
bool virtio_blk_init(void);

/* Read sectors from device
 * Returns number of sectors read, or -1 on error */
int virtio_blk_read(uint64_t sector, uint32_t count, void *buffer);

/* Write sectors to device
 * Returns number of sectors written, or -1 on error */
int virtio_blk_write(uint64_t sector, uint32_t count, const void *buffer);

/* Get total capacity in sectors */
uint64_t virtio_blk_get_capacity(void);

/* Get sector size (always 512) */
uint32_t virtio_blk_get_sector_size(void);

/* Check if device is read-only */
bool virtio_blk_is_readonly(void);

/* Check if device is initialized */
bool virtio_blk_is_initialized(void);

/* Flush cached writes */
bool virtio_blk_flush(void);

#endif /* VIRTIO_BLK_H */
