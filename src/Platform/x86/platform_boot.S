/* Multiboot2 header for System 7.1 kernel with SVGA support */

.section .multiboot2,"a"
.align 8

multiboot2_header_start:
    /* Magic number for Multiboot2 */
    .long 0xe85250d6                /* Multiboot2 magic */
    .long 0                          /* Architecture: i386 */
    .long multiboot2_header_end - multiboot2_header_start  /* Header length */
    .long -(0xe85250d6 + 0 + (multiboot2_header_end - multiboot2_header_start))  /* Checksum */

    /* Framebuffer tag - Request SVGA mode */
    .align 8
framebuffer_tag_start:
    .short 5                         /* Type: framebuffer */
    .short 0                         /* Flags: not optional */
    .long framebuffer_tag_end - framebuffer_tag_start
    .long 800                        /* Width */
    .long 600                        /* Height */
    .long 32                         /* Depth (bits per pixel) */
framebuffer_tag_end:

    /* Module alignment tag */
    .align 8
module_align_tag_start:
    .short 6                         /* Type: module alignment */
    .short 0                         /* Flags */
    .long module_align_tag_end - module_align_tag_start
module_align_tag_end:

    /* End tag */
    .align 8
    .short 0                         /* Type: end */
    .short 0                         /* Flags */
    .long 8                          /* Size */

multiboot2_header_end:

/* Stack setup */
.section .bss
.align 16
stack_bottom:
    .space 65536                    /* 64KB stack */
stack_top:

/* Entry point */
.section .text.boot,"ax"
.global _start
#ifndef _WIN32
.type _start, @function
#endif
_start:
    /* Save Multiboot2 magic and info pointer first! */
    movl %eax, %edi                  /* Save magic in %edi */
    movl %ebx, %esi                  /* Save info pointer in %esi */

    /* Set up stack */
    movl $stack_top, %esp

    /* Early serial output for debugging */
    /* Initialize COM1 */
    movw $0x3F8+1, %dx
    xorb %al, %al
    outb %al, %dx       /* Disable interrupts */

    movw $0x3F8+3, %dx
    movb $0x80, %al
    outb %al, %dx       /* Enable DLAB */

    movw $0x3F8, %dx
    movb $0x03, %al
    outb %al, %dx       /* Divisor low byte (38400 baud) */

    movw $0x3F8+1, %dx
    xorb %al, %al
    outb %al, %dx       /* Divisor high byte */

    movw $0x3F8+3, %dx
    movb $0x03, %al
    outb %al, %dx       /* 8 bits, no parity, 1 stop */

    /* Send 'BOOT\n' for debugging */
    movw $0x3F8, %dx
    movb $'B', %al
    outb %al, %dx
    movb $'O', %al
    outb %al, %dx
    movb $'O', %al
    outb %al, %dx
    movb $'T', %al
    outb %al, %dx
    movb $'
', %al
    outb %al, %dx

    /* Reset EFLAGS */
    pushl $0
    popfl

    /* Push Multiboot2 info */
    pushl %esi                       /* Multiboot2 info pointer */
    pushl %edi                       /* Multiboot2 magic */

    /* Call boot main */
    call boot_main

    /* Hang if boot_main returns */
hang:
    cli
    hlt
    jmp hang

#ifndef _WIN32
.size _start, . - _start
#endif

/* Mark stack usage as non-executable to satisfy modern linkers */
.section .note.GNU-stack,"",@progbits
