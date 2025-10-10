/*
 * x86-Specific Panic Support Implementation
 * System 7.1 Portable
 */

#include "System/Platform/PanicPlatform.h"
#include "System/Panic.h"
#include <stdint.h>

/* === x86 Register State Capture === */

void panic_platform_capture_registers(RegisterState* regs) {
    if (!regs) return;

    /* Save general purpose registers using inline assembly */
    __asm__ volatile (
        "movl %%eax, %0\n\t"
        "movl %%ebx, %1\n\t"
        "movl %%ecx, %2\n\t"
        "movl %%edx, %3\n\t"
        : "=m"(regs->eax), "=m"(regs->ebx),
          "=m"(regs->ecx), "=m"(regs->edx)
        :
        : "memory"
    );

    __asm__ volatile (
        "movl %%esi, %0\n\t"
        "movl %%edi, %1\n\t"
        "movl %%ebp, %2\n\t"
        "movl %%esp, %3\n\t"
        : "=m"(regs->esi), "=m"(regs->edi),
          "=m"(regs->ebp), "=m"(regs->esp)
        :
        : "memory"
    );

    /* Capture instruction pointer (approximate - this is the panic handler) */
    void* eip_val;
    __asm__ volatile ("1: leal 1b, %0" : "=r"(eip_val));
    regs->eip = (uint32_t)(uintptr_t)eip_val;

    /* Capture eflags */
    __asm__ volatile (
        "pushfl\n\t"
        "popl %0\n\t"
        : "=r"(regs->eflags)
    );

    /* Capture segment registers */
    __asm__ volatile (
        "movw %%cs, %0\n\t"
        "movw %%ds, %1\n\t"
        "movw %%es, %2\n\t"
        "movw %%fs, %3\n\t"
        "movw %%gs, %4\n\t"
        "movw %%ss, %5\n\t"
        : "=m"(regs->cs), "=m"(regs->ds), "=m"(regs->es),
          "=m"(regs->fs), "=m"(regs->gs), "=m"(regs->ss)
    );
}

/* === x86 Stack Backtrace (EBP Frame Pointer Walking) === */

uint32_t panic_platform_collect_backtrace(StackFrame* frames, uint32_t max_frames) {
    if (!frames || max_frames == 0) return 0;

    uint32_t depth = 0;
    uint32_t* ebp;

    /* Get current frame pointer */
    __asm__ volatile ("movl %%ebp, %0" : "=r"(ebp));

    /* Walk the stack using EBP chain */
    while (depth < max_frames && ebp != NULL) {
        /* Validate frame pointer is aligned and reasonable */
        if ((uintptr_t)ebp < 0x1000 ||
            (uintptr_t)ebp > 0xFFFF0000 ||
            ((uintptr_t)ebp & 0x3) != 0) {
            break;  /* Invalid frame pointer */
        }

        /* x86 stack frame layout:
         * [ebp+4] = return address
         * [ebp+0] = previous ebp
         */
        uint32_t ret_addr = *(ebp + 1);
        uint32_t* prev_ebp = (uint32_t*)*ebp;

        /* Validate return address is reasonable */
        if (ret_addr < 0x100000 || ret_addr > 0xFFFFFF00) {
            break;  /* Invalid return address */
        }

        frames[depth].return_address = ret_addr;
        frames[depth].frame_pointer = (uint32_t)(uintptr_t)ebp;
        depth++;

        /* Detect cycles */
        if (prev_ebp == ebp || prev_ebp < ebp) {
            break;  /* Stack corrupted or end reached */
        }

        ebp = prev_ebp;
    }

    return depth;
}

/* === x86 CPU Halt === */

void panic_platform_halt(void) {
    /* Disable interrupts */
    __asm__ volatile ("cli");

    /* Halt CPU in infinite loop */
    while (1) {
        __asm__ volatile ("hlt");
    }

    __builtin_unreachable();
}
