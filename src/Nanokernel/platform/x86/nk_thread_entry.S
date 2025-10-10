/* nk_thread_entry.S - System 7X Nanokernel Thread Entry Stub (x86)
 *
 * Assembly bridge between cooperative context switch and C thread entry.
 * Solves ABI mismatch where RET returns to this stub without a call frame.
 */

.section .text
.global nk_thread_entry_stub

/* ============================================================
 *   Thread Entry Stub
 * ============================================================
 *
 * Stack layout when RET arrives here (no return address):
 *   ESP → [entry]        ; Function pointer to thread main
 *         [arg]          ; Argument to pass to thread
 *
 * This stub properly bridges to C by:
 * 1. Popping entry and arg from stack
 * 2. Aligning stack to 16 bytes (x86 ABI requirement)
 * 3. Pushing arg as parameter
 * 4. CALLing entry function with proper call frame
 * 5. Calling nk_thread_exit if function returns
 */

nk_thread_entry_stub:
    /* Pop entry and arg from stack (no return address above them) */
    pop %eax        /* eax = entry function pointer */
    pop %ebx        /* ebx = arg */

    /* Zero EBP so GDB backtrace stops here (optional debug aid) */
    xor %ebp, %ebp

    /* Align stack to 16 bytes for C ABI compliance */
    and $0xFFFFFFF0, %esp

    /* Push argument for entry function */
    push %ebx       /* arg is now at ESP */

    /* Call thread entry function with proper call frame */
    /* This creates a return address on stack, satisfying C ABI */
    call *%eax      /* entry(arg) */

    /* If thread returns instead of calling nk_thread_exit, do it here */
    call nk_thread_exit

.hang:
    /* Should never reach here, but halt if we do */
    hlt
    jmp .hang

.size nk_thread_entry_stub, . - nk_thread_entry_stub
