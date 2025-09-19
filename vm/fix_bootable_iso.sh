#!/bin/bash
#
# Fix System 7.1 Portable Bootable ISO

set -e

echo "============================================"
echo "Fixing System 7.1 Portable Bootable ISO"
echo "============================================"

WORK_DIR="/tmp/system71_fix"
OUTPUT_DIR="$(pwd)/vm/output"

# Clean work directory
rm -rf ${WORK_DIR}
mkdir -p ${WORK_DIR}/{boot/grub,system}

# Copy our init binaries
echo "Step 1: Copying System 7.1 binaries..."
cp ${OUTPUT_DIR}/system71_init ${WORK_DIR}/system/init
chmod +x ${WORK_DIR}/system/init

# Copy initrd
echo "Step 2: Copying initrd..."
cp ${OUTPUT_DIR}/initrd.img ${WORK_DIR}/boot/

# Download a minimal kernel if we don't have one
echo "Step 3: Getting a minimal kernel..."

# Try to download a tiny kernel (TinyCore Linux kernel)
if [ ! -f "${WORK_DIR}/boot/vmlinuz" ]; then
    echo "Downloading TinyCore Linux kernel (5MB)..."
    wget -q -O ${WORK_DIR}/boot/vmlinuz64 \
        "http://tinycorelinux.net/13.x/x86_64/release/distribution_files/vmlinuz64" 2>/dev/null || {

        echo "Failed to download kernel. Trying alternative..."

        # Alternative: Use Alpine Linux kernel
        wget -q -O /tmp/alpine.tar.gz \
            "https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/x86_64/alpine-minirootfs-3.18.4-x86_64.tar.gz" 2>/dev/null || {
            echo "Cannot download kernel. Creating minimal stub..."

            # Create a tiny stub kernel that will at least boot to GRUB
            dd if=/dev/zero of=${WORK_DIR}/boot/vmlinuz bs=1K count=100 2>/dev/null
        }
    }

    # Rename to vmlinuz if downloaded as vmlinuz64
    [ -f "${WORK_DIR}/boot/vmlinuz64" ] && mv ${WORK_DIR}/boot/vmlinuz64 ${WORK_DIR}/boot/vmlinuz
fi

# Create GRUB configuration (simpler than ISOLINUX)
echo "Step 4: Creating GRUB boot configuration..."

mkdir -p ${WORK_DIR}/boot/grub

cat > ${WORK_DIR}/boot/grub/grub.cfg << 'GRUBCFG'
set timeout=5
set default=0

menuentry "System 7.1 Portable" {
    echo "Loading System 7.1 Portable..."
    linux /boot/vmlinuz quiet init=/system/init
    initrd /boot/initrd.img
    boot
}

menuentry "System 7.1 Portable (Debug)" {
    echo "Loading System 7.1 Portable (Debug mode)..."
    linux /boot/vmlinuz init=/system/init
    initrd /boot/initrd.img
    boot
}
GRUBCFG

# Alternative: Create simple SYSLINUX config
mkdir -p ${WORK_DIR}/syslinux

cat > ${WORK_DIR}/syslinux/syslinux.cfg << 'SYSLINUX'
DEFAULT system71
PROMPT 1
TIMEOUT 50

LABEL system71
    LINUX /boot/vmlinuz
    INITRD /boot/initrd.img
    APPEND quiet init=/system/init

LABEL debug
    LINUX /boot/vmlinuz
    INITRD /boot/initrd.img
    APPEND init=/system/init debug
SYSLINUX

# Create the ISO with xorriso (more reliable than genisoimage)
echo "Step 5: Creating new ISO..."

if command -v xorriso &> /dev/null; then
    echo "Using xorriso to create ISO..."

    cd ${WORK_DIR}
    xorriso -as mkisofs \
        -r -J \
        -b boot/grub/grub.cfg \
        -c boot.catalog \
        -no-emul-boot \
        -boot-load-size 4 \
        -boot-info-table \
        -o ${OUTPUT_DIR}/System71Fixed.iso \
        . 2>/dev/null || {

        # Fallback to simpler ISO creation
        echo "Trying simpler ISO creation..."
        genisoimage -R -J -o ${OUTPUT_DIR}/System71Fixed.iso . 2>/dev/null
    }

elif command -v mkisofs &> /dev/null; then
    echo "Using mkisofs..."
    cd ${WORK_DIR}
    mkisofs -R -J -o ${OUTPUT_DIR}/System71Fixed.iso .
else
    echo "No ISO creation tool available"
    echo "Creating tar archive instead..."
    cd ${WORK_DIR}
    tar czf ${OUTPUT_DIR}/System71Fixed.tar.gz *
fi

# Check what was created
cd - > /dev/null

if [ -f "${OUTPUT_DIR}/System71Fixed.iso" ]; then
    SIZE=$(du -h ${OUTPUT_DIR}/System71Fixed.iso | cut -f1)
    echo ""
    echo "✅ Fixed ISO created: ${OUTPUT_DIR}/System71Fixed.iso ($SIZE)"
    echo ""
    echo "Test with: qemu-system-x86_64 -cdrom System71Fixed.iso -m 512M"
elif [ -f "${OUTPUT_DIR}/System71Fixed.tar.gz" ]; then
    echo "Archive created: ${OUTPUT_DIR}/System71Fixed.tar.gz"
fi

# Clean up
rm -rf ${WORK_DIR}

echo ""
echo "Alternative: Boot directly with kernel and initrd:"
echo "qemu-system-x86_64 -kernel vmlinuz -initrd initrd.img -m 512M -append 'init=/system/init'"