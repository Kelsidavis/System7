# Multiboot header for SVGA 640x480 kernel
.section .multiboot
.align 4

# Multiboot header constants
.set MULTIBOOT_MAGIC,     0x1BADB002
.set MULTIBOOT_FLAGS,     0x00000003  # ALIGN | MEMINFO
.set MULTIBOOT_CHECKSUM,  -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

# The multiboot header
multiboot_header:
    .long MULTIBOOT_MAGIC
    .long MULTIBOOT_FLAGS
    .long MULTIBOOT_CHECKSUM

.section .text
.global _start
.extern macos_svga_main

_start:
    # Set up stack
    movl $stack_top, %esp

    # Push multiboot info pointer and magic number
    pushl %ebx  # Multiboot info structure
    pushl %eax  # Multiboot magic number

    # Call the C main function
    call macos_svga_main

    # Halt if main returns
hang:
    cli
    hlt
    jmp hang

# Stack setup
.section .bss
.align 16
stack_bottom:
.skip 16384  # 16 KB stack
stack_top: