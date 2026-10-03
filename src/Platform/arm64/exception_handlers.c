/*
 * ARM64 Exception Handlers
 * Provides diagnostic information for exceptions
 */

#include <stdint.h>
#include "exception_handlers.h"
#include "uart.h"

/*
 * Install exception vector table
 */
void exceptions_init(void) {
    __asm__ volatile("msr vbar_el1, %0" :: "r"(exception_vectors));
    __asm__ volatile("isb");
}

/* Hex digits for printing */
static const char hex_digits[] = "0123456789ABCDEF";

/*
 * Print hex value to UART
 */
static void print_hex(uint64_t value) {
    int i;

    uart_putc('0');
    uart_putc('x');

    for (i = 15; i >= 0; i--) {
        uart_putc(hex_digits[(value >> (i * 4)) & 0xF]);
    }
}

/*
 * Synchronous exception handler
 */
void handle_sync_exception(exception_context_t *ctx) {
    uint64_t esr;

    /* Read exception syndrome register */
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));

    uart_puts("\n*** SYNC EXCEPTION ***\n");
    uart_puts("ESR: ");
    print_hex(esr);
    uart_puts("\nELR: ");
    print_hex(ctx->elr);
    uart_puts("\n");

    /* Hang forever */
    while (1) {
        __asm__ volatile("wfe");
    }
}

/*
 * IRQ exception handler
 */
void handle_irq_exception(exception_context_t *ctx) {
    (void)ctx;
    /* IRQ handling - for now just return */
}

/*
 * FIQ exception handler
 */
void handle_fiq_exception(exception_context_t *ctx) {
    (void)ctx;
    /* FIQ handling - for now just return */
}

/*
 * SError exception handler
 */
void handle_serror_exception(exception_context_t *ctx) {
    uint64_t esr;

    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));

    uart_puts("\n*** SERROR EXCEPTION ***\n");
    uart_puts("ESR: ");
    print_hex(esr);
    uart_puts("\nELR: ");
    print_hex(ctx->elr);
    uart_puts("\n");

    /* Hang forever */
    while (1) {
        __asm__ volatile("wfe");
    }
}
