/*
 * ARM64 Bootloader HAL Implementation
 * Raspberry Pi 3/4/5 AArch64 boot initialization
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uart.h"
#include "timer.h"
#include "dtb.h"
#include "Platform/include/boot.h"

#ifndef QEMU_BUILD
#include "mailbox.h"
#include "framebuffer.h"
#include "gic.h"
#else
#include "virtio_gpu.h"
#endif

/* Minimal snprintf declaration */
extern int snprintf(char *str, size_t size, const char *format, ...);
extern char *strncpy(char *dest, const char *src, size_t n);

/* ARM64-specific boot information */
typedef struct {
    uint64_t dtb_address;
    uint64_t memory_base;
    uint64_t memory_size;
    uint32_t board_revision;
    char board_model[256];
} arm64_boot_info_t;

static arm64_boot_info_t boot_info = {0};

/* Framebuffer state */
static hal_framebuffer_info_t g_fb_info = {0};
static int g_fb_present = 0;

/*
 * Early ARM64 boot entry from assembly
 * Called with DTB address in x0
 */
void arm64_boot_main(void *dtb_ptr) {
    /* Initialize UART for early serial output */
    uart_init();

    /* Initialize timer */
    timer_init();

    /* Initialize exception handlers */
    extern void exceptions_init(void);
    exceptions_init();

    /* Save DTB address */
    boot_info.dtb_address = (uint64_t)dtb_ptr;

    /* Early serial output */
    uart_puts("\n");
    uart_puts("[ARM64] ==========================================================\n");
    uart_puts("[ARM64] System 7.1 Portable - ARM64/AArch64 Boot\n");
    uart_puts("[ARM64] ==========================================================\n");

    /* Report exception level */
    uint64_t current_el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));
    current_el = (current_el >> 2) & 3;

    uart_puts("[ARM64] Running at Exception Level: EL");
    if (current_el == 0) uart_puts("0\n");
    else if (current_el == 1) uart_puts("1\n");
    else if (current_el == 2) uart_puts("2\n");
    else uart_puts("3\n");

    /* If no DTB passed in x0, scan for it at known locations */
    if (!dtb_ptr) {
        /* QEMU virt typically places DTB at fixed locations */
        static const uint64_t dtb_scan_addrs[] = {
            0x44000000,  /* Common offset from RAM start */
            0x48000000,  /* Alternative location */
            0x40000000 + (1024 * 1024 * 1024) - (1024 * 1024),  /* End of 1GB RAM - 1MB */
            0
        };
        for (int i = 0; dtb_scan_addrs[i] != 0; i++) {
            uint32_t *probe = (uint32_t *)dtb_scan_addrs[i];
            /* Check for DTB magic (big-endian 0xD00DFEED) */
            if (*probe == 0xEDFE0DD0) {  /* Little-endian check */
                dtb_ptr = probe;
                boot_info.dtb_address = (uint64_t)dtb_ptr;
                break;
            }
        }
    }

    /* Report timer frequency */
    uart_puts("[ARM64] Timer initialized\n");

    /* Initialize DTB parser */
    uart_puts("[ARM64] Checking Device Tree...\n");
    if (dtb_ptr) {
        uart_puts("[ARM64] DTB pointer provided, attempting init...\n");
        if (dtb_init(dtb_ptr)) {
            uart_puts("[ARM64] Device Tree initialized\n");
            uart_flush();

            /* Get model string from DTB */
            const char *model = dtb_get_model();
            if (model) {
                uart_puts("[ARM64] Model: ");
                uart_puts(model);
                uart_puts("\n");
                strncpy(boot_info.board_model, model, sizeof(boot_info.board_model) - 1);
                boot_info.board_model[sizeof(boot_info.board_model) - 1] = '\0';
            }

            /* Get memory information from DTB */
            uint64_t mem_base, mem_size;
            if (dtb_get_memory(&mem_base, &mem_size)) {
                boot_info.memory_base = mem_base;
                boot_info.memory_size = mem_size;
                uart_puts("[ARM64] Memory from DTB: ");
                /* Print size in MB */
                uint32_t size_mb = (uint32_t)(mem_size / (1024 * 1024));
                char size_buf[32];
                snprintf(size_buf, sizeof(size_buf), "%u MB", size_mb);
                uart_puts(size_buf);
                uart_puts("\n");
            } else {
                uart_puts("[ARM64] DTB memory info unavailable, using 1GB default\n");
                boot_info.memory_base = 0x00000000;
                boot_info.memory_size = 1024 * 1024 * 1024;  /* 1GB default */
            }
        } else {
            uart_puts("[ARM64] DTB init failed\n");
            boot_info.memory_base = 0x00000000;
            boot_info.memory_size = 1024 * 1024 * 1024;  /* 1GB default */
        }
    } else {
        uart_puts("[ARM64] No DTB pointer\n");
        uart_puts("[ARM64] Setting default memory base...\n");
        boot_info.memory_base = 0x00000000;
        uart_puts("[ARM64] Setting default memory size...\n");
        boot_info.memory_size = 1024 * 1024 * 1024;  /* 1GB default */
        uart_puts("[ARM64] Default memory set\n");
    }

    uart_puts("[ARM64] Memory setup complete\n");

#ifndef QEMU_BUILD
    /* Initialize mailbox (not available in QEMU virt) */
    if (mailbox_init()) {
        uart_puts("[ARM64] Mailbox initialized\n");

        /* Get board revision */
        uint32_t revision;
        if (mailbox_get_board_revision(&revision)) {
            boot_info.board_revision = revision;
            uart_puts("[ARM64] Board Revision detected\n");
        }

        /* Get ARM memory info from mailbox as well */
        uint32_t arm_base, arm_size;
        if (mailbox_get_arm_memory(&arm_base, &arm_size)) {
            uart_puts("[ARM64] ARM Memory info from mailbox\n");
        }
    }

    /* Initialize GIC (not available in QEMU virt) */
    if (gic_init()) {
        uart_puts("[ARM64] GIC interrupt controller initialized\n");
    }
#else
    uart_puts("[ARM64] Running in QEMU - skipping mailbox and GIC\n");
#endif

    /* Report processor features */
    uint64_t midr_el1;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(midr_el1));
    uint32_t implementer = (midr_el1 >> 24) & 0xFF;
    uint32_t partnum = (midr_el1 >> 4) & 0xFFF;

    /* Detect Cortex-A53/A72/A76 */
    if (implementer == 0x41) {  /* ARM */
        uart_puts("[ARM64] CPU: ");
        if (partnum == 0xD03) uart_puts("Cortex-A53\n");
        else if (partnum == 0xD08) uart_puts("Cortex-A72\n");
        else if (partnum == 0xD0B) uart_puts("Cortex-A76\n");
        else uart_puts("ARM CPU\n");
    } else {
        uart_puts("[ARM64] CPU detected\n");
    }

    /* Initialize MMU with fixed TCR configuration */
    extern bool mmu_init(void);
    extern void mmu_enable(void);

    uart_puts("[ARM64] Initializing MMU...\n");
    if (mmu_init()) {
        uart_puts("[ARM64] MMU page tables configured\n");
        mmu_enable();
        uart_puts("[ARM64] MMU enabled - virtual memory active\n");
    } else {
        uart_puts("[ARM64] MMU init failed\n");
    }

    /* Initialize framebuffer */
    uart_puts("[ARM64] Initializing framebuffer...\n");
#ifdef QEMU_BUILD
    /* QEMU virt machine uses VirtIO GPU */
    if (virtio_gpu_init()) {
        g_fb_present = 1;
        g_fb_info.framebuffer = virtio_gpu_get_buffer();
        g_fb_info.width = virtio_gpu_get_width();
        g_fb_info.height = virtio_gpu_get_height();
        g_fb_info.pitch = g_fb_info.width * 4;
        g_fb_info.depth = 32;
        /* VirtIO GPU uses BGRX format */
        g_fb_info.blue_offset = 0;
        g_fb_info.blue_size = 8;
        g_fb_info.green_offset = 8;
        g_fb_info.green_size = 8;
        g_fb_info.red_offset = 16;
        g_fb_info.red_size = 8;
        uart_puts("[ARM64] VirtIO GPU framebuffer initialized\n");

        /* Clear to light gray (classic Mac desktop color) */
        uart_puts("[ARM64] Clearing framebuffer...\n");
        virtio_gpu_clear(0xFFCCCCCC);
        uart_puts("[ARM64] Flushing framebuffer...\n");
        virtio_gpu_flush();
        uart_puts("[ARM64] Framebuffer flush complete\n");
    } else {
        uart_puts("[ARM64] VirtIO GPU init failed, continuing without graphics\n");
    }
#else
    /* Raspberry Pi uses VideoCore mailbox framebuffer */
    if (framebuffer_init(640, 480, 32)) {
        g_fb_present = 1;
        g_fb_info.framebuffer = framebuffer_get_buffer();
        g_fb_info.width = framebuffer_get_width();
        g_fb_info.height = framebuffer_get_height();
        g_fb_info.pitch = framebuffer_get_pitch();
        g_fb_info.depth = framebuffer_get_depth();
        /* Pi framebuffer uses RGB format */
        g_fb_info.red_offset = 16;
        g_fb_info.red_size = 8;
        g_fb_info.green_offset = 8;
        g_fb_info.green_size = 8;
        g_fb_info.blue_offset = 0;
        g_fb_info.blue_size = 8;
        uart_puts("[ARM64] Pi framebuffer initialized\n");

        /* Clear to light gray */
        framebuffer_clear(0xFFCCCCCC);
    } else {
        uart_puts("[ARM64] Pi framebuffer init failed, continuing without graphics\n");
    }
#endif

    uart_puts("[ARM64] Early boot complete, entering kernel...\n");
    uart_flush();
    uart_puts("[ARM64] ==========================================================\n");
    uart_flush();

    /* Jump to System7 boot entry point via HAL */
    uart_puts("[ARM64] About to call boot_main()...\n");
    uart_flush();
    extern void boot_main(uint32_t magic, uint32_t* mb2_info);
    boot_main(0, NULL);  /* No multiboot on ARM64, pass NULL */
    uart_puts("[ARM64] boot_main() returned\n");

    /* Should not return */
    while (1) {
        __asm__ volatile("wfe");
    }
}

/*
 * Get detected memory size (returns uint32_t for HAL compatibility)
 */
uint32_t hal_get_memory_size(void) {
    if (boot_info.memory_size > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)boot_info.memory_size;
}

/*
 * Get DTB address
 */
void *hal_get_dtb_address(void) {
    return (void *)boot_info.dtb_address;
}

/*
 * HAL boot init - called by boot_main()
 */
void hal_boot_init(void *boot_arg) {
    (void)boot_arg;
    /* Framebuffer already initialized in arm64_boot_main */
    uart_puts("[ARM64] hal_boot_init called\n");
}

/*
 * Get framebuffer information for System7 GUI
 */
int hal_get_framebuffer_info(hal_framebuffer_info_t *info) {
    if (!info || !g_fb_present) {
        return -1;
    }
    /* Use explicit field assignment to avoid memcpy on ARM64 */
    info->framebuffer = g_fb_info.framebuffer;
    info->width = g_fb_info.width;
    info->height = g_fb_info.height;
    info->pitch = g_fb_info.pitch;
    info->depth = g_fb_info.depth;
    info->red_offset = g_fb_info.red_offset;
    info->red_size = g_fb_info.red_size;
    info->green_offset = g_fb_info.green_offset;
    info->green_size = g_fb_info.green_size;
    info->blue_offset = g_fb_info.blue_offset;
    info->blue_size = g_fb_info.blue_size;
    return 0;
}

/*
 * Platform initialization
 */
int hal_platform_init(void) {
    uart_puts("[ARM64] hal_platform_init called\n");
    return 0;
}

/*
 * Platform shutdown
 */
void hal_platform_shutdown(void) {
    uart_puts("[ARM64] hal_platform_shutdown called\n");
}

/*
 * Present framebuffer (flush to display)
 */
int hal_framebuffer_present(void) {
#ifdef QEMU_BUILD
    if (g_fb_present) {
        virtio_gpu_flush();
    }
#endif
    /* Pi framebuffer doesn't need explicit flush - writes go directly to VideoCore */
    return g_fb_present;
}
