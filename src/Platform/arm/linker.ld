/* ARM Linker Script for System 7.1 Kernel
 * Raspberry Pi 3/4/5 (ARMv7-A)
 *
 * Memory layout:
 * 0x00000000 - Kernel starts here (GPU firmware relocated to high memory on Pi)
 * 0x00008000 - Typical kernel load address
 * 0x0C000000+ - High kernel area (if MMU enabled)
 */

ENTRY(_start)

MEMORY
{
    /* ARM physical memory starts at 0x00000000
     * GPU firmware places kernel at 0x8000 (32KB offset)
     */
    RAM (rwx) : ORIGIN = 0x00008000, LENGTH = 256M
}

SECTIONS
{
    /* Kernel entry point and boot code */
    . = 0x00008000;

    .boot ALIGN(8) :
    {
        /* ARM reset vector - must be first */
        KEEP(build/obj/arm/platform_boot.o(.text.boot))
        KEEP(build/obj/arm/platform_boot.o(.text.entry))
    } > RAM

    /* Text section - kernel code */
    .text ALIGN(4K) :
    {
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
    } > RAM

    /* Read-only data - constants, strings */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
    } > RAM

    /* Data section - initialized globals */
    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
    } > RAM

    /* Exception handling frame info */
    .eh_frame ALIGN(4) :
    {
        *(.eh_frame)
    } > RAM

    /* BSS section - uninitialized globals (zeroed by bootstrap)
     * Note: This should be cleared early in boot
     */
    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    } > RAM

    /* Kernel end marker */
    kernel_end = .;
    kernel_size = kernel_end - ORIGIN(RAM);

    /* Remove unused sections */
    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.gnu_debuglink)
        *(.gnu_debugaltlink)
    }
}

/* Export symbols for bootstrap code */
PROVIDE(_kernel_size = kernel_size);
PROVIDE(_bss_start = __bss_start);
PROVIDE(_bss_end = __bss_end);
