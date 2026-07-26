/*
 * hal_boot.c - x86 Boot HAL Implementation
 *
 * Platform-specific boot initialization for x86.
 */

#include "Platform/include/boot.h"
#include "Platform/include/serial.h"
#include "pic.h"
#include "pit.h"
#include "rtc.h"
#include "gdt.h"
#include "idt.h"
#include <stddef.h>
#include "PS2Controller.h"
#include "TimeManager/TimeManager.h"
#include "xhci.h"
#include "ehci.h"
#include "uhci.h"

extern void* framebuffer;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern uint8_t fb_bpp;
extern uint8_t fb_red_pos;
extern uint8_t fb_red_size;
extern uint8_t fb_green_pos;
extern uint8_t fb_green_size;
extern uint8_t fb_blue_pos;
extern uint8_t fb_blue_size;
extern uint32_t g_total_memory_kb;

static volatile uint32_t g_irq0_ticks = 0;

uint32_t hal_get_irq0_ticks(void) {
    return g_irq0_ticks;
}

/* Boot-critical diagnostics go through serial_puts, not serial_printf:
 * serial_printf is classified and filtered by the SystemLog level machinery, so
 * anything emitted through it can silently vanish - useless for proving whether
 * an interrupt line is alive on an unfamiliar machine. */
static void irq_timer_handler(uint8_t irq) {
    (void)irq;
    g_irq0_ticks++;

    /* Heartbeat: one line per ~1000 ticks (1s at 1 kHz) for the first few
     * seconds, then stay quiet so the log stays readable. */
    if ((g_irq0_ticks % 1000u) == 0 && g_irq0_ticks <= 5000u) {
        serial_puts("[HAL] timer heartbeat: IRQ0 alive\n");
    }
}

static volatile uint32_t g_ps2_irq_count = 0;

static void irq_ps2_handler(uint8_t irq) {
    (void)irq;
    /* Announce only the first delivery - enough to prove the line is wired,
     * without flooding the log on every keystroke. */
    if (++g_ps2_irq_count == 1) {
        serial_puts("[HAL] first PS/2 interrupt delivered\n");
    }
    PollPS2Input();
}

/*
 * hal_boot_init - Initialize platform-specific boot components
 *
 * For x86, most initialization is handled in kernel_main.
 * This function can be extended to handle x86-specific early init.
 */
void hal_boot_init(void *boot_arg) {
    (void)boot_arg;
    /* GDT must come first: the IDT's gate selectors are resolved against
     * whatever GDT is live when an interrupt fires, and GRUB's temporary one
     * does not survive kernel memory allocation. */
    gdt_init();
    pic_init();
    pit_init_hz(1000);
    rtc_init();
    idt_init();
    irq_register_handler(0, irq_timer_handler);
    irq_register_handler(1, irq_ps2_handler);
    irq_register_handler(12, irq_ps2_handler);

    /* pic_init() leaves every line masked, so unmask exactly what we handle.
     * IRQ2 is the master<-slave cascade: without it, nothing on the second PIC
     * (including the IRQ12 mouse) can ever reach the CPU. */
    pic_unmask_irq(0);   /* PIT timer */
    pic_unmask_irq(1);   /* PS/2 keyboard */
    pic_unmask_irq(2);   /* slave PIC cascade - required for IRQ8-15 */
    pic_unmask_irq(12);  /* PS/2 mouse */
    serial_puts("[HAL] IRQs unmasked: 0 timer, 1 kbd, 2 cascade, 12 mouse\n");
    PS2_SetIRQDriven(true);
    xhci_init_x86();
    ehci_init_x86();
    uhci_init_x86();
}

int hal_get_framebuffer_info(hal_framebuffer_info_t *info) {
    if (!info || !framebuffer || fb_width == 0 || fb_height == 0) {
        return -1;
    }

    info->framebuffer = framebuffer;
    info->width = fb_width;
    info->height = fb_height;
    info->pitch = fb_pitch;
    info->depth = fb_bpp;
    info->red_offset = fb_red_pos;
    info->red_size = fb_red_size;
    info->green_offset = fb_green_pos;
    info->green_size = fb_green_size;
    info->blue_offset = fb_blue_pos;
    info->blue_size = fb_blue_size;
    return 0;
}

uint32_t hal_get_memory_size(void) {
    return g_total_memory_kb * 1024;
}

int hal_platform_init(void) {
    return 0;
}

void hal_platform_shutdown(void) {
}

int hal_framebuffer_present(void) {
    return framebuffer != NULL;
}
