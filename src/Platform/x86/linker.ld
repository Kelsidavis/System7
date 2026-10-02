/* Linker script for System 7.1 kernel with Multiboot2 */
/* Ensures multiboot header is in first 32KB */

ENTRY(_start)

PHDRS
{
    boot PT_LOAD FLAGS(5); /* Read + execute */
    text PT_LOAD FLAGS(5); /* Read + execute */
    data PT_LOAD FLAGS(6); /* Read + write */
    stack PT_GNU_STACK FLAGS(6); /* Read + write, never execute */
}

SECTIONS
{
    /* Kernel loads at 1MB */
    . = 0x100000;

    /* Multiboot2 scans the first 32 KiB of the image for this header. Keep it
     * and the bootstrap entry code together in the first load segment. */
    .boot :
    {
        KEEP(*(.multiboot2))
        KEEP(*(.text.boot))
    } :boot

    /* Text section */
    . = ALIGN(4K);
    .text :
    {
        *(.text)
        *(.text.*)
    } :text

    /* Read-only data */
    . = ALIGN(4K);
    .rodata :
    {
        *(.rodata)
        *(.rodata.*)
        *(.eh_frame*)
    } :text

    /* Data section */
    . = ALIGN(4K);
    .data :
    {
        *(.data)
        *(.data.*)
        *(.dtors)
    } :data

    /* BSS section */
    . = ALIGN(4K);
    .bss (NOLOAD) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    } :data

    /* Kernel end marker */
    kernel_end = .;
}
