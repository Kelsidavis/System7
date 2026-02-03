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

    .section .note.GNU-stack,"",@progbits
