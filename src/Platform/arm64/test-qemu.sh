#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/../../.." && pwd)

make -C "$ROOT_DIR" PLATFORM=arm64 all
log_file=$(mktemp)
trap 'rm -f "$log_file"' EXIT

qemu_status=0
timeout 15s qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a53 \
    -m 1G \
    -kernel "$ROOT_DIR/kernel.elf" \
    -serial "file:$log_file" \
    -display none || qemu_status=$?

cat "$log_file"

if [[ "$qemu_status" -ne 0 && "$qemu_status" -ne 124 ]]; then
    echo "QEMU exited with status $qemu_status" >&2
    exit "$qemu_status"
fi

if ! grep -Fq '[VFS] HFS_CreateBlankVolume' "$log_file"; then
    echo "ARM64 kernel did not reach boot-volume creation within 15 seconds" >&2
    exit 1
fi

if [[ "$qemu_status" -eq 124 ]]; then
    echo "ARM64 kernel reached boot-volume creation and remained running until the timeout."
fi
