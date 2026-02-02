/* Multiboot2 header structures */
#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

/* Multiboot2 magic value */
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289

/* Tag types */
#define MULTIBOOT_TAG_TYPE_END               0
#define MULTIBOOT_TAG_TYPE_CMDLINE           1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME  2
#define MULTIBOOT_TAG_TYPE_MODULE            3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO     4
#define MULTIBOOT_TAG_TYPE_BOOTDEV           5
#define MULTIBOOT_TAG_TYPE_MMAP              6
#define MULTIBOOT_TAG_TYPE_VBE               7
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER       8
#define MULTIBOOT_TAG_TYPE_ELF_SECTIONS      9
#define MULTIBOOT_TAG_TYPE_APM               10

/* Basic tag structure */
struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

/* Command line tag */
struct multiboot_tag_string {
    uint32_t type;
    uint32_t size;
    char string[0];  /* Null-terminated command line string */
};

/* Basic memory info tag */
struct multiboot_tag_basic_meminfo {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;  /* KB of lower memory (0-640KB) */
    uint32_t mem_upper;  /* KB of memory above 1MB */
};

/* Framebuffer tag */
struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;

    /* Color info follows for RGB mode */
    union {
        struct {
            uint8_t framebuffer_red_field_position;
            uint8_t framebuffer_red_mask_size;
            uint8_t framebuffer_green_field_position;
            uint8_t framebuffer_green_mask_size;
            uint8_t framebuffer_blue_field_position;
            uint8_t framebuffer_blue_mask_size;
        };
        struct {
            uint32_t framebuffer_palette_num_colors;
            /* Palette entries follow */
        };
    };
};

/* Framebuffer types */
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED  0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB       1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT 2

#endif /* MULTIBOOT_H */