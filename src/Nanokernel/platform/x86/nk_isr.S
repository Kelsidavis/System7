/* nk_isr.S - System 7X Nanokernel ISR Stubs (x86-32 Assembly)
 *
 * Interrupt Service Routine stubs for hardware interrupts.
 * These stubs save CPU state, call C handlers, and restore state.
 */

.section .text

/* IRQ0 (Timer) stub - Build interrupt frame and call C handler */
.global irq0_stub
.type irq0_stub, @function
irq0_stub:
    /* CPU has already pushed: EIP, CS, EFLAGS (and possibly SS, ESP for ring change) */

    /* Save all general-purpose registers (creates bottom part of interrupt frame) */
    pusha           /* Pushes: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI */

    /* Save segment registers (creates top part of interrupt frame) */
    push %ds
    push %es
    push %fs
    push %gs

    /* ESP now points to complete interrupt frame (gs is at lowest address) */

    /* Set up kernel data segments */
    mov $0x18, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* Output 'T' to serial to show timer interrupt fired */
    mov $0x3F8, %dx
    mov $'T', %al
    out %al, %dx

    /* Set interrupt context flag: nk_in_interrupt = true */
    movl $1, nk_in_interrupt

    /* Save pointer to interrupt frame: nk_current_frame = ESP */
    movl %esp, nk_current_frame

    /* Call C handler with frame pointer as argument */
    push %esp       /* Argument: pointer to interrupt frame */
    call irq0_handler
    add $4, %esp    /* Clean up argument */

    /* Clear interrupt context flag: nk_in_interrupt = false */
    movl $0, nk_in_interrupt

    /* Restore segment registers */
    pop %gs
    pop %fs
    pop %es
    pop %ds

    /* Restore general-purpose registers */
    popa

    /* Return from interrupt (pops EIP, CS, EFLAGS) */
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

    mov $0x18, %ax
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

    mov $0x18, %ax
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

    mov $0x18, %ax
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

/* Default exception/interrupt handler - catches unhandled vectors */
.global default_isr_stub
.type default_isr_stub, @function
default_isr_stub:
    /* Just send EOI and return - don't call any C code */
    /* Send EOI to master PIC */
    mov $0x20, %al
    out %al, $0x20
    iret

/* C handlers are external - no need to declare them global here */
/* They are defined in nk_irq_handlers.c and will be found by the linker */
