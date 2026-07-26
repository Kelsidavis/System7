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

    /* Three heartbeats, then silence forever. Enough to prove on an unfamiliar
     * machine that IRQ0 is actually being delivered, without leaving a
     * multi-millisecond blocking UART write on a 1 kHz interrupt path - that
     * alone is enough to make the desktop stutter. */
    if (g_irq0_ticks <= 3000u && (g_irq0_ticks % 1000u) == 0) {
        serial_puts("[HAL] timer heartbeat: IRQ0 alive\n");
    }
}

/* Registered but currently unreachable: IRQ1/IRQ12 are left masked because
 * PollPS2Input() is not reentrancy-safe (see hal_boot_init). Kept so that
 * enabling interrupt-driven input later is a one-line unmask once that path is
 * made safe. */
static void irq_ps2_handler(uint8_t irq) {
    (void)irq;
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
     *
     * PS/2 deliberately stays on the polled path. PollPS2Input() drains the
     * controller to empty while mutating the shared mouse-packet state machine
     * and posting to the event queue, none of which is guarded against
     * reentrancy - running it from IRQ1/IRQ12 while the main loop is reading
     * that same state loses mouse-moved events (breaking window drags, which
     * need an unbroken stream) and eventually wedges the queue. Making input
     * interrupt-driven means making that path reentrancy-safe first; until
     * then IRQ2 is left masked too, since nothing on the slave PIC is used. */
    pic_unmask_irq(0);   /* PIT timer - the only line we currently service */
    serial_puts("[HAL] IRQ0 (timer) unmasked; PS/2 stays on the polled path\n");
    PS2_SetIRQDriven(false);
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
