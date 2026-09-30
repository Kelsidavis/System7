#ifndef PPC_OPEN_FIRMWARE_H
#define PPC_OPEN_FIRMWARE_H

#include <stdint.h>
#include <stddef.h>

void ofw_early_init(void *entry);
void ofw_boot_banner(void *entry);
void ofw_client_init(void *entry);
int ofw_console_available(void);
int ofw_console_write(const char *buffer, uint32_t length);
int ofw_console_putchar(char ch);
int ofw_console_input_available(void);
int ofw_console_poll_char(char *out_char);
int ofw_console_read_char(char *out_char);
int ofw_get_memory_range(uint64_t *out_base, uint64_t *out_size);
typedef struct {
    uint64_t base;
    uint64_t size;
} ofw_memory_range_t;

#define OFW_MAX_MEMORY_RANGES 16

size_t ofw_get_memory_ranges(ofw_memory_range_t *out_ranges, size_t max_ranges);
size_t ofw_memory_range_count(void);
typedef struct {
    uint64_t base;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t stride;
} ofw_framebuffer_info_t;

int ofw_get_framebuffer_info(ofw_framebuffer_info_t *out);

#endif /* PPC_OPEN_FIRMWARE_H */
