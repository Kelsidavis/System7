/*
 * Platform-Specific Panic Support Interface
 * System 7.1 Portable
 *
 * Defines platform-agnostic interface for panic system diagnostics.
 * Each platform (x86, ARM, PowerPC, etc.) provides its own implementation.
 */

#ifndef PANIC_PLATFORM_H
#define PANIC_PLATFORM_H

#include "System/Panic.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Platform-specific: Capture current CPU register state
 *
 * Each platform implements this to save its architecture-specific registers.
 * The RegisterState structure in Panic.h should be adapted per-platform.
 *
 * @param regs Output register state structure
 */
void panic_platform_capture_registers(RegisterState* regs);

/**
 * Platform-specific: Walk the call stack and collect backtrace
 *
 * Implementations use platform-specific stack frame walking:
 * - x86: EBP-based frame pointer walking
 * - ARM: FP register or DWARF unwinding
 * - PowerPC: Stack frame backchain
 *
 * @param frames Output array for stack frames
 * @param max_frames Maximum number of frames to capture
 * @return Number of frames actually captured
 */
uint32_t panic_platform_collect_backtrace(StackFrame* frames, uint32_t max_frames);

/**
 * Platform-specific: Halt the CPU (never returns)
 *
 * Implementations should:
 * 1. Disable all interrupts
 * 2. Enter lowest-power halt state
 * 3. Loop forever if halt instruction not available
 */
void panic_platform_halt(void) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* PANIC_PLATFORM_H */
