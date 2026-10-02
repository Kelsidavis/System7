/* ARM Linker Script for QEMU virt machine */

ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x40000000, LENGTH = 512M
}

SECTIONS
{
    . = ORIGIN(RAM);

    .boot ALIGN(8) :
    {
        KEEP(build/obj/arm/platform_boot.o(.text.boot))
        KEEP(build/obj/arm/platform_boot.o(.text.entry))
    } > RAM

    .text ALIGN(4K) :
    {
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
    } > RAM

    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
    } > RAM

    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
    } > RAM

    .eh_frame ALIGN(4) :
    {
        *(.eh_frame)
    } > RAM

    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    } > RAM

    kernel_end = .;
}
