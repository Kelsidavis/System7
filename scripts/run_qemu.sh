#!/bin/bash

# Run System 7.1 kernel in QEMU (EFI/UEFI)
# This script was converted from the legacy multiboot-based runner to an
# EFI-based runner. It expects an EFI-capable disk image or an EFI payload.
# Note: OVMF (UEFI firmware for QEMU) is usually provided by the package
# 'ovmf' on most distros at /usr/share/ovmf/OVMF_CODE.fd and
# /usr/share/ovmf/OVMF_VARS.fd. Adjust paths as needed.

set -euo pipefail

EFI_BIOS="/usr/share/ovmf/OVMF_CODE.fd"
EFI_VARS="/usr/share/ovmf/OVMF_VARS.fd"
DISK_IMAGE="test_disk.img"
MEM=256M

echo "Starting System 7.1 under QEMU with UEFI..."

if [ ! -f "$EFI_BIOS" ]; then
    echo "Warning: OVMF firmware not found at $EFI_BIOS. Install the 'ovmf' package or adjust EFI_BIOS path." >&2
fi

qemu-system-x86_64 \
    -m $MEM \
    -bios "$EFI_BIOS" \
    -drive file="$DISK_IMAGE",format=raw,if=ide \
    -serial mon:stdio \
    -vga std \
    -display curses \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    2>&1

# If the above fails, check that you have an EFI payload on the disk (EFI/BOOT/BOOTX64.EFI)
