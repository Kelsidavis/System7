#!/bin/bash
#
# Build VESA VBE 640x480 Mac OS System 7.1 Kernel for QEMU
#

set -e

echo "=============================================="
echo " Building VESA 640x480 Mac OS 7.1 Kernel"
echo "=============================================="

cd /home/k/System7.1-Portable/vm

# Compile assembly
echo "Assembling multiboot header with VBE support..."
as --32 multiboot_vesa.s -o multiboot_vesa_asm.o

# Compile C code
echo "Compiling Mac OS VESA kernel..."
gcc -m32 -c multiboot_vesa.c -o multiboot_vesa.o \
    -ffreestanding -O2 -Wall -Wextra -fno-stack-protector \
    -fno-pic -fno-pie -nostdlib

# Link into kernel
echo "Linking Mac OS VESA kernel..."
ld -m elf_i386 -T multiboot.ld -o MacOS71_VESA.elf \
    multiboot_vesa_asm.o multiboot_vesa.o \
    -nostdlib

# Copy to output
mkdir -p output
cp MacOS71_VESA.elf output/

echo ""
echo "✅ VESA kernel built: output/MacOS71_VESA.elf"
echo ""
echo "Test with QEMU (640x480 display):"
echo "  qemu-system-i386 -kernel output/MacOS71_VESA.elf -vga std -serial stdio"
echo ""