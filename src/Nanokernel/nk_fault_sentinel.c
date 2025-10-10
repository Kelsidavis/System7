/* nk_fault_sentinel.c - CPU Exception Handlers for x86 Nanokernel
 *
 * Lightweight handlers for all CPU exceptions (0x00-0x1F).
 * Prevents triple faults by catching and logging all exceptions.
 */

#include <stdint.h>

/* External functions */
extern void serial_puts(const char *s);
extern void nk_printf(const char *fmt, ...);

/* Forward declaration */
void nk_fault_handler_common(uint32_t vector, uint32_t errcode, uint32_t eip, uint32_t cs, uint32_t eflags);
void nk_halt_forever(void);

/* Common fault handler - logs fault details and halts */
__attribute__((cdecl))
void nk_fault_handler_common(uint32_t vector, uint32_t errcode, uint32_t eip, uint32_t cs, uint32_t eflags)
{
    serial_puts("\n[FAULT] ============================\n");
    serial_puts("[FAULT] CPU Exception Detected!\n");
    serial_puts("[FAULT] Vector: 0x");
    if (vector < 16) serial_puts("0");
    /* Manual hex conversion since nk_printf might be broken */
    char hex[3];
    hex[0] = "0123456789ABCDEF"[vector >> 4];
    hex[1] = "0123456789ABCDEF"[vector & 0xF];
    hex[2] = '\0';
    serial_puts(hex);
    serial_puts("\n");

    serial_puts("[FAULT] System halted to prevent triple fault.\n");
    serial_puts("[FAULT] ============================\n\n");

    nk_halt_forever();
}

/* Infinite halt loop */
__attribute__((noreturn))
void nk_halt_forever(void)
{
    serial_puts("[FAULT] HALT: entering infinite loop.\n");
    for (;;) __asm__ volatile("cli; hlt");
}

/* Macro to declare fault stub without error code */
#define DECLARE_FAULT_STUB(name, vec) \
__asm__( \
".globl " #name "\n" \
#name ":\n\t" \
"    pushl $0\n\t" \
"    pushl $" #vec "\n\t" \
"    jmp _nk_fault_trampoline\n");

/* Macro for faults that push error code */
#define DECLARE_FAULT_STUB_ERR(name, vec) \
__asm__( \
".globl " #name "\n" \
#name ":\n\t" \
"    pushl $" #vec "\n\t" \
"    jmp _nk_fault_trampoline\n");

/* Common trampoline */
__asm__(
".globl _nk_fault_trampoline\n"
"_nk_fault_trampoline:\n"
"    pusha\n"
"    mov  36(%esp), %eax\n"  /* vector */
"    mov  40(%esp), %ebx\n"  /* errcode */
"    mov  48(%esp), %ecx\n"  /* eip */
"    mov  52(%esp), %edx\n"  /* cs */
"    mov  56(%esp), %esi\n"  /* eflags */
"    push %esi\n"
"    push %edx\n"
"    push %ecx\n"
"    push %ebx\n"
"    push %eax\n"
"    call nk_fault_handler_common\n"
"    add  $20, %esp\n"
"    popa\n"
"    add  $8, %esp\n"
"    iret\n"
);

/* Fault stubs for all CPU exceptions */
DECLARE_FAULT_STUB(divide_error,          0x00)
DECLARE_FAULT_STUB(debug,                 0x01)
DECLARE_FAULT_STUB(nmi,                   0x02)
DECLARE_FAULT_STUB(breakpoint,            0x03)
DECLARE_FAULT_STUB(overflow,              0x04)
DECLARE_FAULT_STUB(bound_range,           0x05)
DECLARE_FAULT_STUB(invalid_opcode,        0x06)
DECLARE_FAULT_STUB(device_not_available,  0x07)
DECLARE_FAULT_STUB_ERR(double_fault,      0x08)
DECLARE_FAULT_STUB(coprocessor_segment,   0x09)
DECLARE_FAULT_STUB_ERR(invalid_tss,       0x0A)
DECLARE_FAULT_STUB_ERR(segment_not_present, 0x0B)
DECLARE_FAULT_STUB_ERR(stack_fault,       0x0C)
DECLARE_FAULT_STUB_ERR(general_protection, 0x0D)
DECLARE_FAULT_STUB_ERR(page_fault,        0x0E)
DECLARE_FAULT_STUB(x87_fpu,               0x10)
DECLARE_FAULT_STUB_ERR(alignment_check,   0x11)
DECLARE_FAULT_STUB(machine_check,         0x12)
DECLARE_FAULT_STUB(simd_fpu,              0x13)
DECLARE_FAULT_STUB(virtualization,        0x14)
DECLARE_FAULT_STUB(security_exception,    0x1E)

/* Declare externals for use in init */
extern void divide_error(void);
extern void debug(void);
extern void nmi(void);
extern void breakpoint(void);
extern void overflow(void);
extern void bound_range(void);
extern void invalid_opcode(void);
extern void device_not_available(void);
extern void double_fault(void);
extern void coprocessor_segment(void);
extern void invalid_tss(void);
extern void segment_not_present(void);
extern void stack_fault(void);
extern void general_protection(void);
extern void page_fault(void);
extern void x87_fpu(void);
extern void alignment_check(void);
extern void machine_check(void);
extern void simd_fpu(void);
extern void virtualization(void);
extern void security_exception(void);

/* Install fault sentinel handlers into IDT */
void nk_fault_sentinel_install(void)
{
    extern void nk_idt_set_gate(uint8_t num, void (*handler)(void), uint16_t selector, uint8_t flags);

    serial_puts("[FAULT] Installing fault sentinel handlers for CPU exceptions 0x00-0x1F...\n");

    nk_idt_set_gate(0x00, divide_error,          0x10, 0x8E);  /* Use actual CS=0x10 */
    nk_idt_set_gate(0x01, debug,                 0x10, 0x8E);
    nk_idt_set_gate(0x02, nmi,                   0x10, 0x8E);
    nk_idt_set_gate(0x03, breakpoint,            0x10, 0x8E);
    nk_idt_set_gate(0x04, overflow,              0x10, 0x8E);
    nk_idt_set_gate(0x05, bound_range,           0x10, 0x8E);
    nk_idt_set_gate(0x06, invalid_opcode,        0x10, 0x8E);
    nk_idt_set_gate(0x07, device_not_available,  0x10, 0x8E);
    nk_idt_set_gate(0x08, double_fault,          0x10, 0x8E);
    nk_idt_set_gate(0x09, coprocessor_segment,   0x10, 0x8E);
    nk_idt_set_gate(0x0A, invalid_tss,           0x10, 0x8E);
    nk_idt_set_gate(0x0B, segment_not_present,   0x10, 0x8E);
    nk_idt_set_gate(0x0C, stack_fault,           0x10, 0x8E);
    nk_idt_set_gate(0x0D, general_protection,    0x10, 0x8E);
    nk_idt_set_gate(0x0E, page_fault,            0x10, 0x8E);
    nk_idt_set_gate(0x10, x87_fpu,               0x10, 0x8E);
    nk_idt_set_gate(0x11, alignment_check,       0x10, 0x8E);
    nk_idt_set_gate(0x12, machine_check,         0x10, 0x8E);
    nk_idt_set_gate(0x13, simd_fpu,              0x10, 0x8E);
    nk_idt_set_gate(0x14, virtualization,        0x10, 0x8E);
    nk_idt_set_gate(0x1E, security_exception,    0x10, 0x8E);

    serial_puts("[FAULT] Fault sentinel handlers installed.\n");
}
