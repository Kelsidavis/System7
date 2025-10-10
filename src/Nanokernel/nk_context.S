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
