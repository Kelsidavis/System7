/*
 * CPU Exception Handler Implementation
 * System 7.1 Portable
 *
 * Implements exception handlers that capture full CPU state and
 * integrate with the kernel panic system.
 */

#include "System/ExceptionHandlers.h"
#include "System/Panic.h"
#include "Nanokernel/nk_idt.h"
#include <string.h>

/* Forward declarations */
extern void serial_puts(const char* str);
extern void serial_printf(const char* fmt, ...);

/* Exception handler state */
static bool g_handlers_installed = false;
static uint32_t g_exception_count = 0;

/* Assembly exception stubs (defined below) */
extern void exception_stub_0(void);   /* Divide by zero */
extern void exception_stub_6(void);   /* Invalid opcode */
extern void exception_stub_12(void);  /* Stack segment fault */
extern void exception_stub_13(void);  /* General protection fault */
extern void exception_stub_14(void);  /* Page fault */

/* === Exception Context Display === */

static void display_exception_context(const ExceptionContext* ctx) {
    serial_puts("\n=== EXCEPTION CONTEXT ===\n");

    serial_puts("Exception: #");
    serial_printf("%u", ctx->exception_number);

    switch (ctx->exception_number) {
        case 0:  serial_puts(" (Divide by Zero)\n"); break;
        case 6:  serial_puts(" (Invalid Opcode)\n"); break;
        case 12: serial_puts(" (Stack Segment Fault)\n"); break;
        case 13: serial_puts(" (General Protection Fault)\n"); break;
        case 14: serial_puts(" (Page Fault)\n"); break;
        default: serial_puts(" (Unknown)\n"); break;
    }

    serial_puts("Error Code: ");
    serial_printf("0x%08X\n", ctx->error_code);

    if (ctx->exception_number == 14) {
        serial_puts("Fault Address (CR2): ");
        serial_printf("0x%08X\n", ctx->fault_address);
    }

    serial_puts("EIP: ");
    serial_printf("0x%08X  ", ctx->eip);
    serial_puts("CS: ");
    serial_printf("0x%04X  ", ctx->cs);
    serial_puts("EFLAGS: ");
    serial_printf("0x%08X\n", ctx->eflags);
}

/* === C Exception Handlers === */

/**
 * Common exception handler - called from assembly stubs
 * Never returns - always triggers kernel panic
 */
void exception_handler_common(ExceptionContext* ctx) {
    g_exception_count++;

    /* Display exception context */
    display_exception_context(ctx);

    /* Map exception number to panic code */
    PanicCode panic_code;
    const char* panic_msg;

    switch (ctx->exception_number) {
        case 0:
            panic_code = PANIC_CODE_DIVIDE_BY_ZERO;
            panic_msg = "CPU Exception: Divide by Zero";
            break;
        case 6:
            panic_code = PANIC_CODE_INVALID_OPCODE;
            panic_msg = "CPU Exception: Invalid Opcode";
            break;
        case 12:
            panic_code = PANIC_CODE_STACK_FAULT;
            panic_msg = "CPU Exception: Stack Segment Fault";
            break;
        case 13:
            panic_code = PANIC_CODE_GPF;
            panic_msg = "CPU Exception: General Protection Fault";
            break;
        case 14:
            panic_code = PANIC_CODE_PAGE_FAULT;
            panic_msg = "CPU Exception: Page Fault";
            break;
        default:
            panic_code = PANIC_CODE_GENERAL;
            panic_msg = "CPU Exception: Unknown";
            break;
    }

    /* Trigger kernel panic with exception context */
    _kernel_panic_with_context(
        panic_code,
        "CPU Exception",  /* file */
        ctx->exception_number,  /* line (reused for exception number) */
        "exception_handler_common",  /* function */
        panic_msg,
        ctx,
        sizeof(ExceptionContext)
    );

    /* Never reached */
}

/* === Assembly Exception Stubs === */

/*
 * Exception stub format (for exceptions WITHOUT error code):
 * - Push dummy error code (0)
 * - Push exception number
 * - Save all registers
 * - Call C handler
 *
 * Exception stub format (for exceptions WITH error code):
 * - CPU automatically pushes error code
 * - Push exception number
 * - Save all registers
 * - Call C handler
 *
 * Stack layout when C handler is called:
 * [esp+0]  = Return address to stub
 * [esp+4]  = Pointer to ExceptionContext structure on stack
 */

/* Divide by Zero (INT 0) - No error code */
__asm__ (
"    .globl exception_stub_0\n"
"exception_stub_0:\n"
"    cli\n"                     /* Disable interrupts */
"    pushl $0\n"                /* Dummy error code */
"    pushl $0\n"                /* Exception number */
"    pushal\n"                  /* Push all GPRs */
"    push %ds\n"
"    push %es\n"
"    push %fs\n"
"    push %gs\n"
"    push %ss\n"
"    \n"
"    /* Read CR2 (fault address) - only relevant for page faults */\n"
"    mov %cr2, %eax\n"
"    pushl %eax\n"
"    \n"
"    /* Call C handler with pointer to context */\n"
"    mov %esp, %eax\n"
"    pushl %eax\n"
"    call exception_handler_common\n"
"    \n"
"    /* Never returns - panic halts */\n"
"1:  hlt\n"
"    jmp 1b\n"
);

/* Invalid Opcode (INT 6) - No error code */
__asm__ (
"    .globl exception_stub_6\n"
"exception_stub_6:\n"
"    cli\n"
"    pushl $0\n"                /* Dummy error code */
"    pushl $6\n"                /* Exception number */
"    pushal\n"
"    push %ds\n"
"    push %es\n"
"    push %fs\n"
"    push %gs\n"
"    push %ss\n"
"    \n"
"    mov %cr2, %eax\n"
"    pushl %eax\n"
"    \n"
"    mov %esp, %eax\n"
"    pushl %eax\n"
"    call exception_handler_common\n"
"    \n"
"1:  hlt\n"
"    jmp 1b\n"
);

/* Stack Segment Fault (INT 12) - Has error code */
__asm__ (
"    .globl exception_stub_12\n"
"exception_stub_12:\n"
"    cli\n"
"    /* CPU already pushed error code */\n"
"    pushl $12\n"               /* Exception number */
"    pushal\n"
"    push %ds\n"
"    push %es\n"
"    push %fs\n"
"    push %gs\n"
"    push %ss\n"
"    \n"
"    mov %cr2, %eax\n"
"    pushl %eax\n"
"    \n"
"    mov %esp, %eax\n"
"    pushl %eax\n"
"    call exception_handler_common\n"
"    \n"
"1:  hlt\n"
"    jmp 1b\n"
);

/* General Protection Fault (INT 13) - Has error code */
__asm__ (
"    .globl exception_stub_13\n"
"exception_stub_13:\n"
"    cli\n"
"    /* CPU already pushed error code */\n"
"    pushl $13\n"               /* Exception number */
"    pushal\n"
"    push %ds\n"
"    push %es\n"
"    push %fs\n"
"    push %gs\n"
"    push %ss\n"
"    \n"
"    mov %cr2, %eax\n"
"    pushl %eax\n"
"    \n"
"    mov %esp, %eax\n"
"    pushl %eax\n"
"    call exception_handler_common\n"
"    \n"
"1:  hlt\n"
"    jmp 1b\n"
);

/* Page Fault (INT 14) - Has error code */
__asm__ (
"    .globl exception_stub_14\n"
"exception_stub_14:\n"
"    cli\n"
"    /* CPU already pushed error code */\n"
"    pushl $14\n"               /* Exception number */
"    pushal\n"
"    push %ds\n"
"    push %es\n"
"    push %fs\n"
"    push %gs\n"
"    push %ss\n"
"    \n"
"    mov %cr2, %eax\n"
"    pushl %eax\n"
"    \n"
"    mov %esp, %eax\n"
"    pushl %eax\n"
"    call exception_handler_common\n"
"    \n"
"1:  hlt\n"
"    jmp 1b\n"
);

/* === Public API === */

void exception_handlers_init(void) {
    if (g_handlers_installed) {
        serial_puts("[EXCEPTION] Handlers already installed\n");
        return;
    }

    serial_puts("[EXCEPTION] Installing CPU exception handlers...\n");

    /* Install handlers in IDT
     * Use kernel code segment (0x08) and interrupt gate with DPL=0
     */
    uint8_t flags = IDT_TYPE_INT32 | IDT_FLAG_PRESENT | IDT_FLAG_DPL0;

    nk_idt_set_gate(0, exception_stub_0, 0x08, flags);    /* Divide by Zero */
    nk_idt_set_gate(6, exception_stub_6, 0x08, flags);    /* Invalid Opcode */
    nk_idt_set_gate(12, exception_stub_12, 0x08, flags);  /* Stack Fault */
    nk_idt_set_gate(13, exception_stub_13, 0x08, flags);  /* GPF */
    nk_idt_set_gate(14, exception_stub_14, 0x08, flags);  /* Page Fault */

    g_handlers_installed = true;
    g_exception_count = 0;

    serial_puts("[EXCEPTION] CPU exception handlers installed successfully\n");
    serial_puts("[EXCEPTION] Monitoring: INT 0 (Divide), INT 6 (Invalid Opcode), INT 12 (Stack), INT 13 (GPF), INT 14 (Page Fault)\n");
}

bool exception_handlers_installed(void) {
    return g_handlers_installed;
}

uint32_t exception_handlers_get_count(void) {
    return g_exception_count;
}
