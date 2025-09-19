#!/bin/bash
#
# Build Multiboot-Compliant Mac OS System 7.1 Kernel for QEMU
#

set -e

echo "=============================================="
echo " Building Multiboot Mac OS 7.1 Kernel"
echo "=============================================="

cd /home/k/System7.1-Portable/vm

# Compile assembly
echo "Assembling multiboot header..."
as --32 multiboot_header.s -o multiboot_header.o

# Compile C code
echo "Compiling Mac OS GUI kernel..."
gcc -m32 -c multiboot_gui.c -o multiboot_gui.o \
    -ffreestanding -O2 -Wall -Wextra -fno-stack-protector \
    -fno-pic -fno-pie -nostdlib

# Link into kernel
echo "Linking Mac OS kernel..."
ld -m elf_i386 -T multiboot.ld -o MacOS71_Multiboot.elf \
    multiboot_header.o multiboot_gui.o \
    -nostdlib

# Copy to output
cp MacOS71_Multiboot.elf output/

echo ""
echo "✅ Multiboot kernel built: output/MacOS71_Multiboot.elf"
echo ""
echo "Test with QEMU:"
echo "  qemu-system-i386 -kernel output/MacOS71_Multiboot.elf -serial stdio"
echo ""
echo "Or with VGA display:"
echo "  qemu-system-i386 -kernel output/MacOS71_Multiboot.elf"