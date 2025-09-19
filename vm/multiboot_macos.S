/*
 * Multiboot-compliant kernel wrapper for Mac OS System 7.1 Portable
 * This allows the native Mac OS to boot in QEMU using -kernel option
 */

.section .multiboot
.align 4

/* Multiboot header */
multiboot_header:
    .long   0x1BADB002      /* MULTIBOOT_HEADER_MAGIC */
    .long   0x00000003      /* MULTIBOOT_HEADER_FLAGS (PAGE_ALIGN | MEMORY_INFO) */
    .long   -(0x1BADB002 + 0x00000003)  /* MULTIBOOT_CHECKSUM */

.section .text
.global _start
.type _start, @function
_start:
    /* We're now in 32-bit protected mode */
    cli

    /* Set up stack */
    movl    $stack_top, %esp

    /* Save multiboot info pointer */
    pushl   %ebx    /* Multiboot info structure */
    pushl   %eax    /* Multiboot magic */

    /* Clear BSS */
    movl    $__bss_start, %edi
    movl    $__bss_end, %ecx
    subl    %edi, %ecx
    xorl    %eax, %eax
    rep     stosb

    /* Call Mac OS GUI with multiboot info */
    call    macos_gui_main

    /* Halt if Mac OS returns */
hang:
    hlt
    jmp     hang

.section .bss
.align 16
stack_bottom:
.skip 16384 # 16 KiB stack
stack_top: