/*
 * Kernel Panic Implementation
 * System 7.1 Portable
 *
 * Platform-agnostic panic handling with platform-specific support
 * for register capture, stack walking, and CPU halt.
 */

#include "System/Panic.h"
#include "System/Platform/PanicPlatform.h"
#include <string.h>

/* Forward declarations for platform-specific functions */
extern void serial_puts(const char* str);
extern void serial_putchar(char c);
extern uint64_t nk_get_ticks(void);

static inline uint32_t get_tick_count(void) {
    return (uint32_t)nk_get_ticks();
}

/* Global panic context for post-mortem analysis */
static PanicContext g_last_panic;
static bool g_panic_in_progress = false;

/* === Helper Functions === */

static void print_hex32(uint32_t val) {
    const char hex[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        serial_putchar(hex[(val >> i) & 0xF]);
    }
}

static void print_hex16(uint16_t val) {
    const char hex[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 12; i >= 0; i -= 4) {
        serial_putchar(hex[(val >> i) & 0xF]);
    }
}

static void print_decimal(uint32_t val) {
    if (val == 0) {
        serial_putchar('0');
        return;
    }
    char buf[16];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        serial_putchar(buf[--i]);
    }
}

/* === Register State Capture (Platform-Agnostic Wrapper) === */

void panic_capture_registers(RegisterState* regs) {
    /* Delegate to platform-specific implementation */
    panic_platform_capture_registers(regs);
}

/* === Stack Backtrace (Platform-Agnostic Wrapper) === */

uint32_t panic_collect_backtrace(StackFrame* frames, uint32_t max_frames) {
    /* Delegate to platform-specific implementation */
    return panic_platform_collect_backtrace(frames, max_frames);
}

/* === Panic Display === */

static void display_register_state(const RegisterState* regs) {
    serial_puts("\n=== REGISTER STATE ===\n");

    serial_puts("EAX="); print_hex32(regs->eax);
    serial_puts("  EBX="); print_hex32(regs->ebx);
    serial_puts("  ECX="); print_hex32(regs->ecx);
    serial_puts("  EDX="); print_hex32(regs->edx);
    serial_puts("\n");

    serial_puts("ESI="); print_hex32(regs->esi);
    serial_puts("  EDI="); print_hex32(regs->edi);
    serial_puts("  EBP="); print_hex32(regs->ebp);
    serial_puts("  ESP="); print_hex32(regs->esp);
    serial_puts("\n");

    serial_puts("EIP="); print_hex32(regs->eip);
    serial_puts("  EFLAGS="); print_hex32(regs->eflags);
    serial_puts("\n");

    serial_puts("CS="); print_hex16(regs->cs);
    serial_puts("  DS="); print_hex16(regs->ds);
    serial_puts("  ES="); print_hex16(regs->es);
    serial_puts("  FS="); print_hex16(regs->fs);
    serial_puts("  GS="); print_hex16(regs->gs);
    serial_puts("  SS="); print_hex16(regs->ss);
    serial_puts("\n");
}

static void display_backtrace(const StackFrame* frames, uint32_t depth) {
    serial_puts("\n=== STACK BACKTRACE ===\n");
    if (depth == 0) {
        serial_puts("  (no backtrace available)\n");
        return;
    }

    for (uint32_t i = 0; i < depth; i++) {
        serial_puts("  #");
        print_decimal(i);
        serial_puts(": ");
        print_hex32(frames[i].return_address);
        serial_puts(" (FP=");
        print_hex32(frames[i].frame_pointer);
        serial_puts(")\n");
    }
}

void panic_display_context(const PanicContext* ctx) {
    if (!ctx) return;

    serial_puts("\n");
    serial_puts("================================================================================\n");
    serial_puts("                         KERNEL PANIC DETECTED\n");
    serial_puts("================================================================================\n\n");

    /* Panic details */
    serial_puts("Code: "); print_hex32(ctx->code);
    serial_puts(" (");
    switch (ctx->code) {
        case PANIC_CODE_HEAP_CORRUPTION:  serial_puts("HEAP_CORRUPTION"); break;
        case PANIC_CODE_DOUBLE_FREE:      serial_puts("DOUBLE_FREE"); break;
        case PANIC_CODE_BAD_POINTER:      serial_puts("BAD_POINTER"); break;
        case PANIC_CODE_BLOCK_OVERFLOW:   serial_puts("BLOCK_OVERFLOW"); break;
        case PANIC_CODE_FREELIST_CORRUPT: serial_puts("FREELIST_CORRUPT"); break;
        case PANIC_CODE_STACK_OVERFLOW:   serial_puts("STACK_OVERFLOW"); break;
        case PANIC_CODE_NULL_DEREF:       serial_puts("NULL_DEREFERENCE"); break;
        case PANIC_CODE_ASSERT_FAILED:    serial_puts("ASSERT_FAILED"); break;
        default:                          serial_puts("UNKNOWN"); break;
    }
    serial_puts(")\n");

    serial_puts("Message: ");
    serial_puts(ctx->message ? ctx->message : "(no message)");
    serial_puts("\n");

    if (ctx->file) {
        serial_puts("Location: ");
        serial_puts(ctx->file);
        serial_puts(":");
        print_decimal(ctx->line);
        if (ctx->function) {
            serial_puts(" in ");
            serial_puts(ctx->function);
            serial_puts("()");
        }
        serial_puts("\n");
    }

    serial_puts("Timestamp: ");
    print_decimal(ctx->timestamp);
    serial_puts(" ticks\n");

    /* Register state */
    display_register_state(&ctx->registers);

    /* Stack trace */
    display_backtrace(ctx->backtrace, ctx->backtrace_depth);

    /* Context data */
    if (ctx->context_data && ctx->context_size > 0) {
        serial_puts("\n=== CONTEXT DATA (");
        print_decimal(ctx->context_size);
        serial_puts(" bytes) ===\n");

        uint8_t* data = (uint8_t*)ctx->context_data;
        for (uint32_t i = 0; i < ctx->context_size && i < 256; i++) {
            if (i % 16 == 0) {
                serial_puts("  ");
                print_hex32(i);
                serial_puts(": ");
            }
            uint8_t byte = data[i];
            const char hex[] = "0123456789ABCDEF";
            serial_putchar(hex[(byte >> 4) & 0xF]);
            serial_putchar(hex[byte & 0xF]);
            serial_putchar(' ');
            if ((i + 1) % 16 == 0 || i == ctx->context_size - 1) {
                serial_puts("\n");
            }
        }
    }

    serial_puts("\n");
    serial_puts("================================================================================\n");
    serial_puts("                   SYSTEM HALTED - DEBUGGING REQUIRED\n");
    serial_puts("================================================================================\n\n");
}

/* === Panic Halt (Platform-Agnostic Wrapper) === */

void panic_halt(void) {
    /* Delegate to platform-specific halt implementation */
    panic_platform_halt();
}

/* === Main Panic Functions === */

void _kernel_panic(PanicCode code, const char* file, int line,
                   const char* func, const char* message) {
    _kernel_panic_with_context(code, file, line, func, message, NULL, 0);
}

void _kernel_panic_with_context(PanicCode code, const char* file, int line,
                                const char* func, const char* message,
                                void* context, uint32_t context_size) {
    /* Prevent recursive panics */
    if (g_panic_in_progress) {
        serial_puts("\n*** RECURSIVE PANIC DETECTED - HALTING ***\n");
        panic_halt();
    }
    g_panic_in_progress = true;

    /* Build panic context */
    memset(&g_last_panic, 0, sizeof(g_last_panic));
    g_last_panic.code = code;
    g_last_panic.severity = PANIC_FATAL;
    g_last_panic.message = message;
    g_last_panic.file = file;
    g_last_panic.line = line;
    g_last_panic.function = func;
    g_last_panic.context_data = context;
    g_last_panic.context_size = context_size;
    g_last_panic.timestamp = get_tick_count();

    /* Capture registers and stack trace */
    panic_capture_registers(&g_last_panic.registers);
    g_last_panic.backtrace_depth = panic_collect_backtrace(
        g_last_panic.backtrace, MAX_STACK_FRAMES);

    /* Display panic information */
    panic_display_context(&g_last_panic);

    /* Halt the system */
    panic_halt();
}
