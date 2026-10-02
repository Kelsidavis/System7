/*
 * ARM64 Linker Script - QEMU virt machine
 * Memory starts at 0x40000000 on QEMU virt
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5); /* Read + execute */
    data PT_LOAD FLAGS(6); /* Read + write */
}

/* QEMU virt machine memory starts at 0x40000000 */
SECTIONS
{
    . = 0x40000000;

    .text : {
        KEEP(*(.text.boot))
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
    } :text

    .data : {
        *(.data*)
    } :data

    .fini_array : {
        *(.fini_array*)
    } :data

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } :data

    .stack (NOLOAD) : {
        *(.stack)
    } :data

    /* Discard unneeded sections */
    /DISCARD/ : {
        *(.comment)
        *(.gnu*)
        *(.note*)
        *(.eh_frame*)
    }
}
