/* nk_isr.S - System 7X Nanokernel ISR Stubs (x86-32 Assembly)
 *
 * Interrupt Service Routine stubs for hardware interrupts.
 * These stubs save CPU state, call C handlers, and restore state.
 */

.section .text

/* IRQ0 (Timer) stub - called 1000 times per second */
.global irq0_stub
.type irq0_stub, @function
irq0_stub:
    /* Save all general-purpose registers */
    pusha

    /* Save data segments */
    push %ds
    push %es
    push %fs
    push %gs

    /* Load kernel data segment */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* Call C handler */
    call irq0_handler

    /* Restore segments */
    pop %gs
    pop %fs
    pop %es
    pop %ds

    /* Restore general-purpose registers */
    popa

    /* Return from interrupt */
    iret

/* IRQ1 (Keyboard) stub - for future use */
.global irq1_stub
.type irq1_stub, @function
irq1_stub:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    call irq1_handler

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret

/* Spurious IRQ7 stub - for PIC spurious interrupts */
.global irq7_stub
.type irq7_stub, @function
irq7_stub:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    call irq7_handler

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret

/* Software interrupt stub for deferred rescheduling (INT 0x81) */
.global irq_resched_stub
.type irq_resched_stub, @function
irq_resched_stub:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    call irq_resched_handler

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret

/* C handlers are external - no need to declare them global here */
/* They are defined in nk_irq_handlers.c and will be found by the linker */
