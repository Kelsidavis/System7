#!/bin/bash
# Automated PowerPC boot script for QEMU
# Sends boot commands to OpenBIOS to load and execute System 7 kernel

set -euo pipefail

DISK_IMAGE="${1:-ppc_system71.img}"
KERNEL="${2:-kernel.elf}"

if [ ! -f "$DISK_IMAGE" ]; then
    echo "Error: Disk image not found: $DISK_IMAGE"
    exit 1
fi

if [ ! -f "$KERNEL" ]; then
    echo "Error: Kernel not found: $KERNEL"
    exit 1
fi

echo "================================================"
echo "System 7 PowerPC QEMU Boot"
echo "================================================"
echo "Disk:   $DISK_IMAGE"
echo "Kernel: $KERNEL"
echo ""
echo "Starting QEMU with OpenBIOS..."
echo "Boot commands will be sent automatically"
echo "================================================"
echo ""

# Start QEMU and feed commands via stdin
{
    # Wait for OpenBIOS prompt
    sleep 2

    # Send boot commands
    echo "load hd:,\\kernel.elf"
    sleep 1
    echo "go"
    sleep 5

    # Keep connection open for a while to see output
    sleep 10

} | qemu-system-ppc -M mac99 -m 512 -serial stdio -monitor none -nographic \
    -drive file="$DISK_IMAGE",format=raw,if=ide

echo ""
echo "Boot sequence completed"
