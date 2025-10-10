/*
 * Kernel Panic and Diagnostic Infrastructure
 * System 7.1 Portable
 *
 * Provides kernel panic handling with:
 * - Stack trace collection
 * - Register dump
 * - Memory state snapshots
 * - Halt/diagnostic output
 */

#ifndef SYSTEM_PANIC_H
#define SYSTEM_PANIC_H

#include "SystemTypes.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Panic severity levels */
typedef enum {
    PANIC_FATAL     = 0,  /* Unrecoverable - halt system */
    PANIC_ERROR     = 1,  /* Serious error - attempt recovery */
    PANIC_WARNING   = 2,  /* Warning - log and continue */
    PANIC_INFO      = 3   /* Informational - diagnostics only */
} PanicSeverity;

/* Panic codes for categorization */
typedef enum {
    PANIC_CODE_GENERAL          = 0x0000,
    PANIC_CODE_HEAP_CORRUPTION  = 0x1000,
    PANIC_CODE_DOUBLE_FREE      = 0x1001,
    PANIC_CODE_BAD_POINTER      = 0x1002,
    PANIC_CODE_BLOCK_OVERFLOW   = 0x1003,
    PANIC_CODE_FREELIST_CORRUPT = 0x1004,
    PANIC_CODE_STACK_OVERFLOW   = 0x2000,
    PANIC_CODE_STACK_UNDERFLOW  = 0x2001,
    PANIC_CODE_NULL_DEREF       = 0x3000,
    PANIC_CODE_DIVIDE_BY_ZERO   = 0x3001,
    PANIC_CODE_ASSERT_FAILED    = 0x4000,
    PANIC_CODE_UNREACHABLE      = 0x4001,
    PANIC_CODE_NANOKERNEL       = 0x5000
} PanicCode;

/* Register state snapshot (x86) */
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip, eflags;
    uint16_t cs, ds, es, fs, gs, ss;
} RegisterState;

/* Stack frame for backtrace */
#define MAX_STACK_FRAMES 32
typedef struct {
    uint32_t return_address;
    uint32_t frame_pointer;
} StackFrame;

/* Panic context - all diagnostic info */
typedef struct {
    PanicCode code;
    PanicSeverity severity;
    const char* message;
    const char* file;
    int line;
    const char* function;

    RegisterState registers;
    StackFrame backtrace[MAX_STACK_FRAMES];
    uint32_t backtrace_depth;

    /* Optional context-specific data */
    void* context_data;
    uint32_t context_size;

    uint32_t timestamp;  /* Tick count at panic */
} PanicContext;

/**
 * Trigger a kernel panic with full diagnostics
 *
 * @param code Panic classification code
 * @param file Source file (__FILE__)
 * @param line Line number (__LINE__)
 * @param func Function name (__func__)
 * @param message Descriptive panic message
 */
void _kernel_panic(PanicCode code, const char* file, int line,
                   const char* func, const char* message);

/**
 * Panic with additional context data
 *
 * @param code Panic code
 * @param file Source file
 * @param line Line number
 * @param func Function name
 * @param message Panic message
 * @param context Additional diagnostic data
 * @param context_size Size of context data in bytes
 */
void _kernel_panic_with_context(PanicCode code, const char* file, int line,
                                const char* func, const char* message,
                                void* context, uint32_t context_size);

/**
 * Capture current register state
 *
 * @param regs Output register structure
 */
void panic_capture_registers(RegisterState* regs);

/**
 * Walk the stack and collect backtrace
 *
 * @param frames Output array for stack frames
 * @param max_frames Maximum frames to capture
 * @return Number of frames captured
 */
uint32_t panic_collect_backtrace(StackFrame* frames, uint32_t max_frames);

/**
 * Print panic context to serial output
 *
 * @param ctx Panic context to display
 */
void panic_display_context(const PanicContext* ctx);

/**
 * Halt the CPU after a fatal panic
 * Never returns
 */
void panic_halt(void) __attribute__((noreturn));

/* Convenience macros */
#define KERNEL_PANIC(code, msg) \
    _kernel_panic((code), __FILE__, __LINE__, __func__, (msg))

#define KERNEL_PANIC_CTX(code, msg, ctx, size) \
    _kernel_panic_with_context((code), __FILE__, __LINE__, __func__, (msg), (ctx), (size))

#define PANIC_ASSERT(condition, msg) \
    do { \
        if (!(condition)) { \
            KERNEL_PANIC(PANIC_CODE_ASSERT_FAILED, msg); \
        } \
    } while(0)

#define PANIC_UNREACHABLE() \
    KERNEL_PANIC(PANIC_CODE_UNREACHABLE, "Reached unreachable code")

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_PANIC_H */
