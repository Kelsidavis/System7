/*
 * CPU Exception Handler Infrastructure
 * System 7.1 Portable
 *
 * Provides exception handlers for common CPU faults that integrate
 * with the kernel panic system to provide full diagnostic output.
 *
 * Supported exceptions:
 * - INT 0: Divide by Zero
 * - INT 6: Invalid Opcode
 * - INT 12: Stack Segment Fault
 * - INT 13: General Protection Fault (GPF)
 * - INT 14: Page Fault
 */

#ifndef SYSTEM_EXCEPTION_HANDLERS_H
#define SYSTEM_EXCEPTION_HANDLERS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Exception context captured by exception handlers
 * Contains the CPU state at the time of the exception
 */
typedef struct {
    /* Exception information */
    uint32_t exception_number;
    uint32_t error_code;        /* For exceptions that push error code */
    uint32_t fault_address;     /* CR2 for page faults, 0 for others */

    /* CPU state at exception */
    uint32_t eip, cs, eflags;   /* Pushed by CPU on exception */
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint16_t ds, es, fs, gs, ss;
} ExceptionContext;

/**
 * Initialize and install CPU exception handlers
 *
 * Installs handlers for:
 * - Divide by zero (INT 0)
 * - Invalid opcode (INT 6)
 * - Stack segment fault (INT 12)
 * - General protection fault (INT 13)
 * - Page fault (INT 14)
 *
 * All handlers integrate with the kernel panic system to provide
 * full register dumps, stack traces, and diagnostic information.
 */
void exception_handlers_init(void);

/**
 * Check if exception handlers are installed
 * @return true if handlers are installed, false otherwise
 */
bool exception_handlers_installed(void);

/**
 * Get the number of exceptions caught since boot
 * @return Exception count
 */
uint32_t exception_handlers_get_count(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_EXCEPTION_HANDLERS_H */
