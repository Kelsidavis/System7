#ifndef HAL_BOOT_H
#define HAL_BOOT_H

#include <stdint.h>

typedef struct {
    void *framebuffer;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t depth;
    uint8_t red_offset;
    uint8_t red_size;
    uint8_t green_offset;
    uint8_t green_size;
    uint8_t blue_offset;
    uint8_t blue_size;
} hal_framebuffer_info_t;

void hal_boot_init(void *boot_arg);
int hal_get_framebuffer_info(hal_framebuffer_info_t *info);
uint32_t hal_get_memory_size(void);
uint32_t hal_get_irq0_ticks(void);
int hal_platform_init(void);
void hal_platform_shutdown(void);
int hal_framebuffer_present(void);

#if defined(__powerpc__) || defined(__powerpc64__)
#include "Platform/PowerPC/OpenFirmware.h"
size_t hal_ppc_get_memory_ranges(ofw_memory_range_t *out_ranges, size_t max_ranges);
size_t hal_ppc_memory_range_count(void);
#endif

#endif /* HAL_BOOT_H */
