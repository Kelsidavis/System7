/* One block, entered at its first byte, everything in it addressed relative
 * to the program counter: a single CODE resource with nothing to relocate. */
SECTIONS
{
    . = 0;
    .text : { *(.text.start) *(.text*) *(.rodata*) *(.data*) *(.bss*) *(COMMON) . = ALIGN(4); }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
