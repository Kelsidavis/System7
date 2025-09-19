#!/bin/bash
#
# Build Complete Mac OS 7.1 GUI Kernel with Text and Icons
#

set -e

echo "=============================================="
echo " Building Complete Mac OS 7.1 GUI Kernel"
echo "=============================================="

cd /home/k/System7.1-Portable/vm

# Compile assembly
echo "Assembling multiboot header..."
as --32 multiboot_complete.s -o multiboot_complete_asm.o

# Compile C code
echo "Compiling Mac OS complete GUI kernel..."
gcc -m32 -c multiboot_gui_complete.c -o multiboot_gui_complete.o \
    -ffreestanding -O2 -Wall -Wextra -fno-stack-protector \
    -fno-pic -fno-pie -nostdlib

# Link into kernel
echo "Linking Mac OS complete GUI kernel..."
ld -m elf_i386 -T multiboot.ld -o MacOS71_Complete.elf \
    multiboot_complete_asm.o multiboot_gui_complete.o \
    -nostdlib

# Copy to output
mkdir -p output
cp MacOS71_Complete.elf output/

echo ""
echo "✅ Complete GUI kernel built: output/MacOS71_Complete.elf"
echo ""

# Create ISO with GRUB
echo "Creating bootable ISO..."
mkdir -p iso_complete/boot/grub
cp output/MacOS71_Complete.elf iso_complete/boot/

cat > iso_complete/boot/grub/grub.cfg << 'EOF'
set timeout=3
set default=0

menuentry "Mac OS System 7.1 Complete GUI" {
    insmod all_video
    set gfxmode=640x480x32
    set gfxpayload=keep
    multiboot /boot/MacOS71_Complete.elf
    boot
}
EOF

grub-mkrescue -o MacOS71_Complete.iso iso_complete/ 2>/dev/null
cp MacOS71_Complete.iso output/

echo "✅ Bootable ISO created: output/MacOS71_Complete.iso"
echo ""
echo "Test with QEMU:"
echo "  qemu-system-i386 -cdrom output/MacOS71_Complete.iso -vga std -m 256"
echo ""