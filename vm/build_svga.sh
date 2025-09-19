#!/bin/bash
#
# Build SVGA 640x480 Mac OS System 7.1 Kernel for QEMU
#

set -e

echo "=============================================="
echo " Building SVGA 640x480 Mac OS 7.1 Kernel"
echo "=============================================="

cd /home/k/System7.1-Portable/vm

# Compile assembly
echo "Assembling multiboot header..."
as --32 multiboot_svga.s -o multiboot_svga_asm.o

# Compile C code
echo "Compiling Mac OS SVGA kernel..."
gcc -m32 -c multiboot_svga.c -o multiboot_svga.o \
    -ffreestanding -O2 -Wall -Wextra -fno-stack-protector \
    -fno-pic -fno-pie -nostdlib

# Link into kernel
echo "Linking Mac OS SVGA kernel..."
ld -m elf_i386 -T multiboot.ld -o MacOS71_SVGA.elf \
    multiboot_svga_asm.o multiboot_svga.o \
    -nostdlib

# Copy to output
mkdir -p output
cp MacOS71_SVGA.elf output/

echo ""
echo "✅ SVGA kernel built: output/MacOS71_SVGA.elf"
echo ""
echo "Test with QEMU:"
echo "  qemu-system-i386 -kernel output/MacOS71_SVGA.elf -vga std -serial stdio"
echo ""