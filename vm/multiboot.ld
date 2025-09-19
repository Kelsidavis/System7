/* Linker script for multiboot Mac OS kernel */

ENTRY(_start)

SECTIONS {
    /* Begin at 1 MB - standard for multiboot kernels */
    . = 0x100000;

    /* Multiboot header must be within first 8KB and FIRST in the binary */
    .multiboot ALIGN(4) : {
        KEEP(*(.multiboot))
    }

    /* Code section */
    .text ALIGN(4K) : {
        *(.text)
        *(.text.*)
    }

    /* Read-only data */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /* Data section */
    .data : {
        *(.data)
        *(.data.*)
    }

    /* BSS section */
    __bss_start = .;
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    __bss_end = .;

    /* Discard unwanted sections */
    /DISCARD/ : {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}