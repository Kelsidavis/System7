#!/bin/bash
#
# Create bootable ISO with GRUB2 for VESA VBE support
#

set -e

echo "=============================================="
echo " Creating Bootable ISO with VESA Support"
echo "=============================================="

cd /home/k/System7.1-Portable/vm

# Create directory structure
mkdir -p iso_vesa/boot/grub

# Copy kernel
echo "Copying VESA kernel..."
cp output/MacOS71_VESA.elf iso_vesa/boot/

# Create GRUB configuration
echo "Creating GRUB configuration..."
cat > iso_vesa/boot/grub/grub.cfg << 'EOF'
set timeout=3
set default=0

menuentry "Mac OS System 7.1 - 640x480 VESA" {
    insmod all_video
    set gfxmode=640x480x32
    set gfxpayload=keep
    multiboot /boot/MacOS71_VESA.elf
    boot
}

menuentry "Mac OS System 7.1 - 800x600 VESA" {
    insmod all_video
    set gfxmode=800x600x32
    set gfxpayload=keep
    multiboot /boot/MacOS71_VESA.elf
    boot
}

menuentry "Mac OS System 7.1 - 1024x768 VESA" {
    insmod all_video
    set gfxmode=1024x768x32
    set gfxpayload=keep
    multiboot /boot/MacOS71_VESA.elf
    boot
}
EOF

# Create ISO with GRUB
echo "Building ISO image..."
grub-mkrescue -o MacOS71_VESA.iso iso_vesa/ 2>/dev/null

# Copy to output
cp MacOS71_VESA.iso output/

echo ""
echo "✅ Bootable ISO created: output/MacOS71_VESA.iso"
echo ""
echo "Test with QEMU:"
echo "  qemu-system-i386 -cdrom output/MacOS71_VESA.iso -vga std"
echo ""
echo "Or with higher memory:"
echo "  qemu-system-i386 -cdrom output/MacOS71_VESA.iso -vga std -m 256"
echo ""