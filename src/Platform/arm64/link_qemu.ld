/*
 * ARM64 Linker Script - QEMU virt machine
 * Memory starts at 0x40000000 on QEMU virt
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

/* QEMU virt machine memory starts at 0x40000000 */
SECTIONS
{
    . = 0x40000000;

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .fini_array : {
        *(.fini_array*)
    }

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    .stack (NOLOAD) : {
        *(.stack)
    }

    /* Discard unneeded sections */
    /DISCARD/ : {
        *(.comment)
        *(.gnu*)
        *(.note*)
        *(.eh_frame*)
    }
}
