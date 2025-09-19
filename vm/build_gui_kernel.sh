#!/bin/bash
#
# Build GUI-enabled Mac OS 7.1 Multiboot Kernel
#

set -e

echo "=============================================="
echo " Building Mac OS 7.1 GUI Kernel"
echo "=============================================="

cd /home/k/System7.1-Portable/vm

# Assemble multiboot header
echo "Assembling multiboot header..."
as --32 multiboot_macos.S -o multiboot_macos.o

# Compile GUI kernel
echo "Compiling Mac OS GUI kernel..."
gcc -m32 -c multiboot_gui.c -o multiboot_gui.o \
    -ffreestanding -O2 -Wall -Wextra -fno-stack-protector \
    -fno-pic -fno-pie -nostdlib

# Link everything
echo "Linking Mac OS GUI kernel..."
ld -m elf_i386 -T multiboot.ld -o MacOS71_GUI.elf \
    multiboot_macos.o multiboot_gui.o \
    -nostdlib

# Copy to output
cp MacOS71_GUI.elf output/

echo ""
echo "✅ GUI kernel built: output/MacOS71_GUI.elf"
echo ""
echo "Test with QEMU (graphics mode):"
echo "  qemu-system-i386 -kernel output/MacOS71_GUI.elf -vga std"
echo ""
echo "With serial debug output:"
echo "  qemu-system-i386 -kernel output/MacOS71_GUI.elf -vga std -serial stdio"