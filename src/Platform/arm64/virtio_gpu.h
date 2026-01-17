/*
 * VirtIO GPU Driver for ARM64
 */

#ifndef VIRTIO_GPU_H
#define VIRTIO_GPU_H

#include <stdbool.h>
#include <stdint.h>

bool virtio_gpu_init(void);
void virtio_gpu_flush(void);
void virtio_gpu_clear(uint32_t color);
void virtio_gpu_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
uint32_t* virtio_gpu_get_buffer(void);
uint32_t virtio_gpu_get_width(void);
uint32_t virtio_gpu_get_height(void);
bool virtio_gpu_is_initialized(void);

#endif
