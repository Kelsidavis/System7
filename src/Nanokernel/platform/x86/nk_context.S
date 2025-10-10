/* nk_context.S - System 7X Nanokernel Context Switching (x86)
 *
 * Low-level context switching for cooperative and preemptive multithreading.
 * Saves and restores CPU context between threads.
 */

.section .text
.global nk_switch_context

/* ============================================================
 *   Context Switch
 * ============================================================
 *
 * void nk_switch_context(nk_context_t *old, nk_context_t *new)
 *
 * Context structure layout (28 bytes):
 *   +0:  edi
 *   +4:  esi
 *   +8:  ebx
 *   +12: ebp
 *   +16: esp
 *   +20: eip
 *   +24: eflags
 */

nk_switch_context:
    /* Get arguments: old=%eax, new=%edx */
    movl 4(%esp), %eax          /* old context */
    movl 8(%esp), %edx          /* new context */

    /* If old context is NULL, skip save (first time) */
    testl %eax, %eax
    jz .restore

.save:
    /* Save current context to old */
    movl %edi, 0(%eax)
    movl %esi, 4(%eax)
    movl %ebx, 8(%eax)
    movl %ebp, 12(%eax)

    /* Save stack pointer (after return address) */
    leal 4(%esp), %ecx          /* ESP after we return */
    movl %ecx, 16(%eax)

    /* Save return address as EIP */
    movl 0(%esp), %ecx          /* Return address */
    movl %ecx, 20(%eax)

    /* Save EFLAGS */
    pushf
    popl %ecx
    movl %ecx, 24(%eax)

.restore:
    /* Restore context from new */
    movl 0(%edx), %edi
    movl 4(%edx), %esi
    movl 8(%edx), %ebx
    movl 12(%edx), %ebp

    /* Restore stack pointer */
    movl 16(%edx), %esp

    /* Push return address (EIP) onto new stack */
    movl 20(%edx), %ecx
    pushl %ecx

    /* Restore EFLAGS */
    movl 24(%edx), %ecx
    pushl %ecx
    popf

    /* Jump to saved EIP (ret pops and jumps) */
    ret

.size nk_switch_context, . - nk_switch_context

/* ============================================================
 *   IRQ-Safe Context Switch
 * ============================================================
 *
 * void nk_switch_context_irq(nk_thread_t *prev, nk_thread_t *next,
 *                           nk_interrupt_frame_t *prev_frame)
 *
 * This function performs a context switch from within an interrupt
 * handler. It saves the interrupt frame pointer for the previous
 * thread and restores the next thread's pre-initialized IRQ frame.
 *
 * Thread structure layout (relevant fields):
 *   +16: context (nk_context_t, 28 bytes)
 *   +44: irq_frame (pointer, 4 bytes)
 *
 * All threads now have a pre-initialized IRQ frame allocated during
 * creation, so we always restore from a valid frame pointer.
 *
 * IMPORTANT: This function MUST return via IRET, not RET, because
 * we're returning from an interrupt context.
 */

.global nk_switch_context_irq
nk_switch_context_irq:
    /* Get arguments from stack */
    movl 4(%esp), %eax      /* prev thread (may be null) */
    movl 8(%esp), %edx      /* next thread (must not be null) */
    movl 12(%esp), %ecx     /* prev interrupt frame pointer */

    /* Save prev interrupt frame pointer if prev is not null */
    testl %eax, %eax
    jz .load_next

    /* Save frame pointer to prev->irq_frame (offset 44 in nk_thread_t) */
    movl %ecx, 44(%eax)

.load_next:
    /* Load next thread's IRQ frame pointer (offset 44) */
    /* All threads have pre-initialized frames, so this is always valid */
    movl 44(%edx), %esi     /* ESI = next->irq_frame */

    /* DEBUG: Print loaded frame pointer */
    push %eax
    push %edx
    mov $0x3F8, %dx
    mov $'L', %al          /* 'L' for Load */
    out %al, %dx

    /* Print ESI value (frame pointer) in hex */
    mov %esi, %eax
    shr $28, %eax
    and $0xF, %al
    cmp $10, %al
    jl .ldigit1
    add $('A'-10), %al
    jmp .lprint1
.ldigit1:
    add $'0', %al
.lprint1:
    mov $0x3F8, %dx
    out %al, %dx

    pop %edx
    pop %eax

    /* Restore from saved interrupt frame */
    /* Frame layout (ESP->): GS, FS, ES, DS, EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX, EIP, CS, EFLAGS */

    /* Point ESP to the saved frame */
    movl %esi, %esp

    /* DEBUG: Print entire frame before touching it */
    push %eax
    push %edx

    /* Print 'F' to mark frame dump start */
    mov $0x3F8, %dx
    mov $'F', %al
    out %al, %dx

    /* Print EIP from frame (offset 48 = 16 seg + 32 gpr) */
    mov 48(%esi), %eax
    shr $28, %eax
    and $0xF, %al
    cmp $10, %al
    jl .digit1
    add $('A'-10), %al
    jmp .print1
.digit1:
    add $'0', %al
.print1:
    mov $0x3F8, %dx
    out %al, %dx

    /* Print CS */
    mov $0x3F8, %dx
    mov $'/', %al
    out %al, %dx

    mov 52(%esi), %eax
    and $0xFF, %eax
    shr $4, %al
    and $0xF, %al
    cmp $10, %al
    jl .digit2
    add $('A'-10), %al
    jmp .print2
.digit2:
    add $'0', %al
.print2:
    mov $0x3F8, %dx
    out %al, %dx

    mov 52(%esi), %eax
    and $0xF, %al
    cmp $10, %al
    jl .digit3
    add $('A'-10), %al
    jmp .print3
.digit3:
    add $'0', %al
.print3:
    mov $0x3F8, %dx
    out %al, %dx

    pop %edx
    pop %eax

    /* DEBUG: Send marker before segment pops */
    push %eax
    mov $0x3F8, %dx
    mov $'1', %al
    out %al, %dx
    pop %eax

    /* Restore segment registers (popped first - they're at lowest addresses) */
    pop %gs
    pop %fs
    pop %es
    pop %ds

    /* DEBUG: Send marker after segment pops */
    push %eax
    mov $0x3F8, %dx
    mov $'2', %al
    out %al, %dx
    pop %eax

    /* Restore general-purpose registers (popa pops EDI first) */
    popa

    /* DEBUG: Send marker before IRET */
    push %eax
    mov $0x3F8, %dx
    mov $'3', %al
    out %al, %dx
    pop %eax

    /* DEBUG: Verify ESP alignment (must be 16-byte aligned on x86) */
    push %eax
    mov %esp, %eax
    add $4, %eax          /* Account for our push */
    and $0xF, %al         /* Get low 4 bits */
    or  $'0', %al         /* Convert to ASCII digit */
    mov $0x3F8, %dx
    out %al, %dx          /* Print alignment (should be '0', '4', '8', or 'C') */
    pop %eax

    /* At this point, stack has: EIP, CS, EFLAGS, [USER_ESP, SS] */
    /* IRET will pop these and return to the interrupted code */
    iret

.size nk_switch_context_irq, . - nk_switch_context_irq
