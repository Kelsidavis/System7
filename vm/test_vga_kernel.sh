#!/bin/bash
# Test script for VGA graphics Mac OS 7.1 kernel

echo "=== Mac OS System 7.1 VGA Graphics Test ==="
echo "Testing the VGA graphics implementation with proper multiboot"

# Create GRUB configuration for VGA kernel
cat > /tmp/grub_vga.cfg << 'EOF'
set timeout=0
set default=0

menuentry "Mac OS System 7.1 VGA Graphics" {
    multiboot /boot/MacOS71_GUI.elf
    boot
}
EOF

# Create ISO directory structure
echo "Creating ISO structure..."
mkdir -p /tmp/vga_iso/boot/grub
cp vm/output/MacOS71_GUI.elf /tmp/vga_iso/boot/
cp /tmp/grub_vga.cfg /tmp/vga_iso/boot/grub/grub.cfg

# Create bootable ISO
echo "Creating bootable ISO..."
grub-mkrescue -o vm/output/MacOS71_VGA.iso /tmp/vga_iso 2>/dev/null

# Test in QEMU with VGA graphics
echo "Starting QEMU with VGA graphics..."
echo "Press Ctrl+A then X to exit QEMU"
echo ""

# Run QEMU with VGA display
qemu-system-i386 \
    -cdrom vm/output/MacOS71_VGA.iso \
    -m 256M \
    -vga std \
    -serial stdio \
    -display gtk \
    2>&1 | tee vm/output/vga_test.log

echo ""
echo "Test complete. Log saved to vm/output/vga_test.log"

# Clean up
rm -rf /tmp/vga_iso /tmp/grub_vga.cfg