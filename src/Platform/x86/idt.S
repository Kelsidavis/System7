/*
 * idt.S - x86 IRQ stubs
 */

    .section .text
    .globl irq0
    .globl irq1
    .globl irq2
    .globl irq3
    .globl irq4
    .globl irq5
    .globl irq6
    .globl irq7
    .globl irq8
    .globl irq9
    .globl irq10
    .globl irq11
    .globl irq12
    .globl irq13
    .globl irq14
    .globl irq15
    .globl isr_default

irq0:
    pushl $0
    jmp irq_common
irq1:
    pushl $1
    jmp irq_common
irq2:
    pushl $2
    jmp irq_common
irq3:
    pushl $3
    jmp irq_common
irq4:
    pushl $4
    jmp irq_common
irq5:
    pushl $5
    jmp irq_common
irq6:
    pushl $6
    jmp irq_common
irq7:
    pushl $7
    jmp irq_common
irq8:
    pushl $8
    jmp irq_common
irq9:
    pushl $9
    jmp irq_common
irq10:
    pushl $10
    jmp irq_common
irq11:
    pushl $11
    jmp irq_common
irq12:
    pushl $12
    jmp irq_common
irq13:
    pushl $13
    jmp irq_common
irq14:
    pushl $14
    jmp irq_common
irq15:
    pushl $15
    jmp irq_common

irq_common:
    pusha
    mov 32(%esp), %eax
    push %eax
    call irq_dispatch
    add $4, %esp
    popa
    add $4, %esp
    iret

isr_default:
    iret

/* CPU exception stubs (vectors 0-31).
 *
 * A bare `iret` is wrong for exceptions: the ones that push an error code
 * (8, 10-14, 17, 21) leave it on the stack, so `iret` reads the wrong frame and
 * the machine triple-faults with no diagnostic at all. Each stub below
 * normalises the frame to <vector, error-code> and reports it, which turns a
 * silent reset into a printable fault - the difference between "it froze" and
 * "it took a #GP at 0x...".
 */
.macro EXCEPTION_NOERR vec
    .globl exc\vec
exc\vec:
    pushl $0
    pushl $\vec
    jmp exc_common
.endm

.macro EXCEPTION_ERR vec
    .globl exc\vec
exc\vec:
    /* CPU already pushed the error code */
    pushl $\vec
    jmp exc_common
.endm

    EXCEPTION_NOERR 0
    EXCEPTION_NOERR 1
    EXCEPTION_NOERR 2
    EXCEPTION_NOERR 3
    EXCEPTION_NOERR 4
    EXCEPTION_NOERR 5
    EXCEPTION_NOERR 6
    EXCEPTION_NOERR 7
    EXCEPTION_ERR   8
    EXCEPTION_NOERR 9
    EXCEPTION_ERR   10
    EXCEPTION_ERR   11
    EXCEPTION_ERR   12
    EXCEPTION_ERR   13
    EXCEPTION_ERR   14
    EXCEPTION_NOERR 15
    EXCEPTION_NOERR 16
    EXCEPTION_ERR   17
    EXCEPTION_NOERR 18
    EXCEPTION_NOERR 19

exc_common:
    pusha
    /* After pusha: esp+32 = vector, esp+36 = error code, esp+40 = faulting EIP */
    mov 40(%esp), %eax      /* EIP */
    push %eax
    mov 40(%esp), %eax      /* error code (stack shifted by the push above) */
    push %eax
    mov 40(%esp), %eax      /* vector (shifted by two pushes) */
    push %eax
    call exception_dispatch
    add $12, %esp
    popa
    add $8, %esp            /* drop vector + error code */
    iret

    .section .note.GNU-stack,"",@progbits
