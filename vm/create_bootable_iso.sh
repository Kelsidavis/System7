#!/bin/bash
#
# System 7.1 Portable - Create Bootable ISO
#
# Creates a minimal Linux ISO that boots directly into System 7.1 Portable

set -e

echo "============================================"
echo "System 7.1 Portable - Bootable ISO Creator"
echo "============================================"

# Configuration
WORK_DIR="/tmp/system71_iso_build"
OUTPUT_DIR="$(pwd)/vm/output"
ISO_NAME="System71Portable.iso"

# Clean and create work directory
rm -rf ${WORK_DIR}
mkdir -p ${WORK_DIR}/{boot,isolinux,system}
mkdir -p ${OUTPUT_DIR}

# Build the init binaries
echo ""
echo "Step 1: Building System 7.1 binaries..."

# Build text version
gcc -o ${WORK_DIR}/system/init vm/system71_init.c -static -O2

# Try to build framebuffer version
gcc -o ${WORK_DIR}/system/init_fb vm/system71_fb.c -static -O2 2>/dev/null || {
    echo "Note: Framebuffer version build failed (expected on non-Linux)"
}

echo "Binaries built successfully"

# Create init script
echo ""
echo "Step 2: Creating init script..."

cat > ${WORK_DIR}/system/startup.sh << 'INITSCRIPT'
#!/bin/sh
#
# System 7.1 Portable Boot Script

# Mount essential filesystems
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

# Create device nodes if missing
[ -e /dev/console ] || mknod /dev/console c 5 1
[ -e /dev/null ] || mknod /dev/null c 1 3
[ -e /dev/tty0 ] || mknod /dev/tty0 c 4 0
[ -e /dev/fb0 ] || mknod /dev/fb0 c 29 0

# Clear screen
clear

echo "================================"
echo "System 7.1 Portable Boot Loader"
echo "================================"
echo ""

# Try framebuffer version first
if [ -e /dev/fb0 ] && [ -x /system/init_fb ]; then
    echo "Starting graphical mode..."
    /system/init_fb
else
    echo "Starting text mode..."
    /system/init
fi

# Fallback shell if init exits
echo ""
echo "System halted. Dropping to shell..."
/bin/sh
INITSCRIPT

chmod +x ${WORK_DIR}/system/startup.sh

# Create minimal initramfs
echo ""
echo "Step 3: Creating initramfs..."

cd ${WORK_DIR}

# Create directory structure
mkdir -p initramfs/{bin,dev,etc,lib,proc,sys,system}

# Copy binaries
cp system/init initramfs/system/
[ -f system/init_fb ] && cp system/init_fb initramfs/system/
cp system/startup.sh initramfs/init

# Add busybox for basic utilities (if available)
if command -v busybox &> /dev/null; then
    cp $(which busybox) initramfs/bin/
    cd initramfs/bin
    for cmd in sh ls cat echo mount umount; do
        ln -s busybox $cmd
    done
    cd ../..
else
    echo "Warning: busybox not found, ISO will have limited functionality"
fi

# Create initramfs archive
cd initramfs
find . | cpio -o -H newc | gzip > ../boot/initrd.gz
cd ..

echo "Initramfs created: $(du -h boot/initrd.gz | cut -f1)"

# Download or use minimal kernel
echo ""
echo "Step 4: Preparing kernel..."

# Try to find a kernel
if [ -f "/boot/vmlinuz-$(uname -r)" ]; then
    echo "Using system kernel: $(uname -r)"
    cp "/boot/vmlinuz-$(uname -r)" boot/vmlinuz 2>/dev/null || {
        echo "Note: Cannot copy system kernel (permission denied)"

        # Try to download a minimal kernel
        echo "Downloading minimal kernel..."
        wget -q -O boot/vmlinuz \
            "https://github.com/ivandavidov/minimal/releases/download/15-Dec-2019/kernel.xz" \
            2>/dev/null || {
                echo "Warning: Could not download kernel"
            }
    }
fi

# Create isolinux configuration
echo ""
echo "Step 5: Creating boot configuration..."

# Download isolinux if not present
if [ ! -f isolinux/isolinux.bin ]; then
    echo "Downloading isolinux..."
    wget -q -O isolinux.tar.gz \
        "https://www.kernel.org/pub/linux/utils/boot/syslinux/syslinux-6.03.tar.gz" \
        2>/dev/null || {
            echo "Warning: Could not download isolinux"
        }

    if [ -f isolinux.tar.gz ]; then
        tar -xzf isolinux.tar.gz --strip-components=3 \
            -C isolinux/ \
            syslinux-6.03/bios/core/isolinux.bin \
            syslinux-6.03/bios/com32/elflink/ldlinux/ldlinux.c32 \
            2>/dev/null || true
    fi
fi

# Create isolinux config
cat > isolinux/isolinux.cfg << 'ISOLINUX'
DEFAULT system71
PROMPT 0
TIMEOUT 50

LABEL system71
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initrd.gz quiet vga=788 console=tty0

LABEL text
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initrd.gz quiet console=tty0

LABEL vga
    KERNEL /boot/vmlinuz
    APPEND initrd=/boot/initrd.gz vga=ask
ISOLINUX

# Create ISO
echo ""
echo "Step 6: Creating ISO image..."

cd ${WORK_DIR}

if command -v genisoimage &> /dev/null; then
    genisoimage -R -J -c boot.cat \
        -b isolinux/isolinux.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        -o ${OUTPUT_DIR}/${ISO_NAME} . 2>/dev/null || {
            echo "ISO creation failed"
        }
elif command -v mkisofs &> /dev/null; then
    mkisofs -R -J -c boot.cat \
        -b isolinux/isolinux.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        -o ${OUTPUT_DIR}/${ISO_NAME} . 2>/dev/null || {
            echo "ISO creation failed"
        }
else
    echo "Warning: No ISO creation tool found (genisoimage/mkisofs)"
    echo "Creating tarball instead..."
    tar -czf ${OUTPUT_DIR}/system71_bootfiles.tar.gz *
fi

# Clean up
cd - > /dev/null
rm -rf ${WORK_DIR}

# Create QEMU test script
cat > ${OUTPUT_DIR}/test_iso_qemu.sh << 'QEMU_SCRIPT'
#!/bin/bash
#
# Test System 7.1 Portable ISO in QEMU

ISO="System71Portable.iso"

if [ ! -f "$ISO" ]; then
    echo "Error: $ISO not found"
    exit 1
fi

echo "Testing System 7.1 Portable ISO..."
echo "==================================="
echo ""
echo "Boot options:"
echo "  - Default: Graphical mode (if available)"
echo "  - Type 'text' for text mode"
echo "  - Type 'vga' to select video mode"
echo ""

qemu-system-x86_64 \
    -cdrom "$ISO" \
    -boot d \
    -m 512M \
    -vga std \
    -display gtk \
    2>/dev/null || \
qemu-system-x86_64 \
    -cdrom "$ISO" \
    -boot d \
    -m 512M \
    -nographic
QEMU_SCRIPT

chmod +x ${OUTPUT_DIR}/test_iso_qemu.sh

# Create VirtualBox script
cat > ${OUTPUT_DIR}/test_iso_virtualbox.sh << 'VBOX_SCRIPT'
#!/bin/bash
#
# Test System 7.1 Portable ISO in VirtualBox

ISO="System71Portable.iso"
VM_NAME="System71Test"

if [ ! -f "$ISO" ]; then
    echo "Error: $ISO not found"
    exit 1
fi

# Create VM if it doesn't exist
if ! VBoxManage list vms | grep -q "\"$VM_NAME\""; then
    echo "Creating VirtualBox VM..."
    VBoxManage createvm --name "$VM_NAME" --ostype Linux26_64 --register
    VBoxManage modifyvm "$VM_NAME" --memory 512 --vram 32
    VBoxManage storagectl "$VM_NAME" --name IDE --add ide
fi

# Attach ISO
VBoxManage storageattach "$VM_NAME" \
    --storagectl IDE \
    --port 1 --device 0 \
    --type dvddrive \
    --medium "$(pwd)/$ISO"

# Start VM
VBoxManage startvm "$VM_NAME"
VBOX_SCRIPT

chmod +x ${OUTPUT_DIR}/test_iso_virtualbox.sh

# Summary
echo ""
echo "============================================"
echo "Build Complete!"
echo "============================================"
echo ""

if [ -f "${OUTPUT_DIR}/${ISO_NAME}" ]; then
    echo "ISO created: ${OUTPUT_DIR}/${ISO_NAME}"
    echo "Size: $(du -h ${OUTPUT_DIR}/${ISO_NAME} | cut -f1)"
    echo ""
    echo "To test in QEMU:"
    echo "  cd ${OUTPUT_DIR}"
    echo "  ./test_iso_qemu.sh"
    echo ""
    echo "To test in VirtualBox:"
    echo "  cd ${OUTPUT_DIR}"
    echo "  ./test_iso_virtualbox.sh"
    echo ""
    echo "To burn to USB (replace /dev/sdX with your USB device):"
    echo "  sudo dd if=${ISO_NAME} of=/dev/sdX bs=4M status=progress"
elif [ -f "${OUTPUT_DIR}/system71_bootfiles.tar.gz" ]; then
    echo "Boot files created: ${OUTPUT_DIR}/system71_bootfiles.tar.gz"
    echo "Extract to a bootable medium to use."
else
    echo "Warning: No output files created"
fi

echo ""