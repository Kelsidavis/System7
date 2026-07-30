#!/bin/bash

# Run with display - open graphical window using QEMU + OVMF (UEFI)
# Adjust OVMF paths on your distribution if needed.

EFI_BIOS="/usr/share/ovmf/OVMF_CODE.fd"
DISK_IMAGE="test_disk.img"
MEM=256M

echo "Starting System 7.1 with graphical display (UEFI)..."

if [ ! -f "$EFI_BIOS" ]; then
    echo "Warning: OVMF firmware not found at $EFI_BIOS. Install the 'ovmf' package or adjust EFI_BIOS path." >&2
fi

qemu-system-x86_64 \
    -m $MEM \
    -bios "$EFI_BIOS" \
    -drive file="$DISK_IMAGE",format=raw,if=ide \
    -serial stdio \
    -vga std \
    -display sdl \
    2>&1
