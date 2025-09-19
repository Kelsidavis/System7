#!/bin/bash
#
# System 7.1 Portable - VM Image Builder
#
# This script creates a bootable VM image with System 7.1 Portable

set -e

echo "==========================================="
echo "System 7.1 Portable VM Image Builder"
echo "==========================================="

# Configuration
VM_DIR="$(pwd)/vm"
BUILD_DIR="$(pwd)/build_x86_64"
OUTPUT_DIR="${VM_DIR}/output"
INITRD_DIR="${VM_DIR}/initrd"
ISO_DIR="${VM_DIR}/iso"

# Create directories
mkdir -p ${OUTPUT_DIR}
mkdir -p ${INITRD_DIR}
mkdir -p ${ISO_DIR}/boot/grub

# Step 1: Build System 7.1 Portable for x86_64
echo ""
echo "Step 1: Building System 7.1 Portable..."
echo "----------------------------------------"

if [ ! -f "${BUILD_DIR}/lib/libSystem71_x86_64.a" ]; then
    echo "Building System 7.1 libraries..."
    ./build_x86_64.sh
else
    echo "Libraries already built, skipping..."
fi

# Step 2: Compile the init system
echo ""
echo "Step 2: Compiling VM init system..."
echo "----------------------------------------"

gcc -o ${OUTPUT_DIR}/system71_init \
    ${VM_DIR}/system71_init.c \
    -static \
    -Wall -O2

echo "Init binary created: ${OUTPUT_DIR}/system71_init"

# Step 3: Create minimal initramfs
echo ""
echo "Step 3: Creating initramfs..."
echo "----------------------------------------"

# Create directory structure
mkdir -p ${INITRD_DIR}/{bin,sbin,etc,proc,sys,dev,tmp,usr,lib,lib64}
mkdir -p ${INITRD_DIR}/usr/{bin,sbin,lib}
mkdir -p ${INITRD_DIR}/System7

# Copy init binary
cp ${OUTPUT_DIR}/system71_init ${INITRD_DIR}/init
chmod +x ${INITRD_DIR}/init

# Copy System 7.1 resources if they exist
if [ -d "./Resources" ]; then
    cp -r ./Resources ${INITRD_DIR}/System7/
fi

# Copy essential libraries (for dynamic linking if needed)
if [ -f "${BUILD_DIR}/lib/libSystem71_x86_64.so" ]; then
    cp ${BUILD_DIR}/lib/libSystem71_x86_64.so ${INITRD_DIR}/lib/
fi

# Create basic /etc files
cat > ${INITRD_DIR}/etc/passwd << EOF
root:x:0:0:root:/:/bin/sh
EOF

cat > ${INITRD_DIR}/etc/group << EOF
root:x:0:root
EOF

# Create devices (if running as root)
if [ "$EUID" -eq 0 ]; then
    mknod ${INITRD_DIR}/dev/console c 5 1
    mknod ${INITRD_DIR}/dev/null c 1 3
    mknod ${INITRD_DIR}/dev/tty0 c 4 0
    mknod ${INITRD_DIR}/dev/fb0 c 29 0
else
    echo "Warning: Not running as root, skipping device node creation"
fi

# Create initramfs archive
cd ${INITRD_DIR}
find . | cpio -o -H newc | gzip > ${OUTPUT_DIR}/initrd.img
cd - > /dev/null

echo "Initramfs created: ${OUTPUT_DIR}/initrd.img"

# Step 4: Download minimal kernel (if not present)
echo ""
echo "Step 4: Preparing kernel..."
echo "----------------------------------------"

if [ ! -f "${OUTPUT_DIR}/vmlinuz" ]; then
    echo "Downloading minimal Linux kernel..."
    # Use a prebuilt minimal kernel or build one
    # For now, we'll use the system's kernel if available
    if [ -f "/boot/vmlinuz-$(uname -r)" ]; then
        cp "/boot/vmlinuz-$(uname -r)" ${OUTPUT_DIR}/vmlinuz
        echo "Using system kernel: $(uname -r)"
    else
        echo "Error: No kernel found. Please provide a Linux kernel as ${OUTPUT_DIR}/vmlinuz"
        echo "You can download a minimal kernel from: https://kernel.org"
        exit 1
    fi
else
    echo "Kernel already present"
fi

# Step 5: Create GRUB configuration
echo ""
echo "Step 5: Creating bootable ISO..."
echo "----------------------------------------"

# Copy kernel and initrd to ISO directory
cp ${OUTPUT_DIR}/vmlinuz ${ISO_DIR}/boot/
cp ${OUTPUT_DIR}/initrd.img ${ISO_DIR}/boot/

# Create GRUB config
cat > ${ISO_DIR}/boot/grub/grub.cfg << EOF
set default=0
set timeout=5

menuentry "System 7.1 Portable" {
    linux /boot/vmlinuz quiet vga=0x318 fb=vesafb
    initrd /boot/initrd.img
}
EOF

# Create ISO (requires genisoimage or xorriso)
if command -v genisoimage &> /dev/null; then
    genisoimage -R -b boot/grub/stage2_eltorito \
                -no-emul-boot -boot-load-size 4 \
                -boot-info-table -o ${OUTPUT_DIR}/system71.iso ${ISO_DIR}
    echo "ISO created: ${OUTPUT_DIR}/system71.iso"
elif command -v xorriso &> /dev/null; then
    xorriso -as mkisofs -R -b boot/grub/stage2_eltorito \
            -no-emul-boot -boot-load-size 4 \
            -boot-info-table -o ${OUTPUT_DIR}/system71.iso ${ISO_DIR}
    echo "ISO created: ${OUTPUT_DIR}/system71.iso"
else
    echo "Warning: genisoimage/xorriso not found, skipping ISO creation"
fi

# Step 6: Create QEMU launch script
echo ""
echo "Step 6: Creating QEMU launch script..."
echo "----------------------------------------"

cat > ${OUTPUT_DIR}/run_qemu.sh << 'EOF'
#!/bin/bash
#
# Launch System 7.1 Portable in QEMU

# Configuration
MEMORY=512M
CPUS=2
VGA=std
AUDIO=ac97

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL="${SCRIPT_DIR}/vmlinuz"
INITRD="${SCRIPT_DIR}/initrd.img"
ISO="${SCRIPT_DIR}/system71.iso"

# Check for required files
if [ ! -f "$KERNEL" ]; then
    echo "Error: Kernel not found: $KERNEL"
    exit 1
fi

if [ ! -f "$INITRD" ]; then
    echo "Error: Initrd not found: $INITRD"
    exit 1
fi

echo "Starting System 7.1 Portable in QEMU..."
echo "========================================"
echo "Memory: $MEMORY"
echo "CPUs: $CPUS"
echo "Display: $VGA"
echo ""
echo "Controls:"
echo "  - Ctrl+Alt+G: Release mouse"
echo "  - Ctrl+Alt+F: Fullscreen"
echo "  - Ctrl+Alt+2: QEMU Monitor"
echo "  - Ctrl+Alt+1: Return to VM"
echo ""

# Launch QEMU with direct kernel boot
qemu-system-x86_64 \
    -m $MEMORY \
    -smp $CPUS \
    -kernel "$KERNEL" \
    -initrd "$INITRD" \
    -append "console=tty0 quiet" \
    -vga $VGA \
    -display gtk \
    -device $AUDIO \
    -enable-kvm \
    2>/dev/null

# Alternative: Boot from ISO if available
if [ -f "$ISO" ]; then
    echo ""
    echo "Alternatively, you can boot from ISO:"
    echo "qemu-system-x86_64 -m $MEMORY -cdrom $ISO -boot d"
fi
EOF

chmod +x ${OUTPUT_DIR}/run_qemu.sh

# Step 7: Create VirtualBox script
cat > ${OUTPUT_DIR}/run_virtualbox.sh << 'EOF'
#!/bin/bash
#
# Create and run System 7.1 Portable VM in VirtualBox

VM_NAME="System71Portable"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ISO="${SCRIPT_DIR}/system71.iso"

# Check if VM already exists
if VBoxManage list vms | grep -q "\"${VM_NAME}\""; then
    echo "VM ${VM_NAME} already exists. Starting it..."
    VBoxManage startvm ${VM_NAME}
    exit 0
fi

# Create VM
echo "Creating VirtualBox VM: ${VM_NAME}"

VBoxManage createvm --name ${VM_NAME} --ostype Linux26_64 --register

# Configure VM
VBoxManage modifyvm ${VM_NAME} \
    --memory 512 \
    --vram 128 \
    --cpus 2 \
    --ioapic on \
    --boot1 dvd \
    --audio pulse \
    --audiocontroller ac97 \
    --usb on \
    --graphicscontroller vboxvga

# Create and attach storage
VBoxManage storagectl ${VM_NAME} --name "IDE" --add ide

if [ -f "$ISO" ]; then
    VBoxManage storageattach ${VM_NAME} \
        --storagectl "IDE" \
        --port 1 --device 0 \
        --type dvddrive \
        --medium "$ISO"
fi

# Start VM
echo "Starting VM..."
VBoxManage startvm ${VM_NAME}
EOF

chmod +x ${OUTPUT_DIR}/run_virtualbox.sh

# Summary
echo ""
echo "==========================================="
echo "VM Image Build Complete!"
echo "==========================================="
echo ""
echo "Generated files:"
echo "  - Init binary: ${OUTPUT_DIR}/system71_init"
echo "  - Initramfs: ${OUTPUT_DIR}/initrd.img"
echo "  - Kernel: ${OUTPUT_DIR}/vmlinuz"
if [ -f "${OUTPUT_DIR}/system71.iso" ]; then
    echo "  - ISO image: ${OUTPUT_DIR}/system71.iso"
fi
echo ""
echo "To run in QEMU:"
echo "  cd ${OUTPUT_DIR}"
echo "  ./run_qemu.sh"
echo ""
echo "To run in VirtualBox:"
echo "  cd ${OUTPUT_DIR}"
echo "  ./run_virtualbox.sh"
echo ""
echo "Direct QEMU command:"
echo "  qemu-system-x86_64 -m 512M -kernel ${OUTPUT_DIR}/vmlinuz -initrd ${OUTPUT_DIR}/initrd.img -append \"console=tty0\""
echo ""