/* Linker script for System 7.1 kernel with Multiboot2 */
/* Ensures multiboot header is in first 32KB */

ENTRY(_start)

SECTIONS
{
    /* Kernel loads at 1MB */
    . = 0x100000;

    /* Combine multiboot header and entry point in first section */
    .boot ALIGN(8) :
    {
        KEEP(*(.multiboot2))
    }

    /* Text section */
    .text ALIGN(4K) :
    {
        *(.text)
        *(.text.*)
    }

    /* Read-only data */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Data section */
    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }

    /* BSS section */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Kernel end marker */
    kernel_end = .;
}
