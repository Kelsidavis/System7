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

for marker in \
    '[VFS] HFS_VolumeMountMemory' \
    'Finder initialized' \
    'MAIN: Entering event loop'; do
    if ! grep -Fq "$marker" "$log_file"; then
        echo "ARM64 kernel did not reach '$marker' within 15 seconds" >&2
        exit 1
    fi
done

if grep -Fq 'CPU EXCEPTION' "$log_file"; then
    echo "ARM64 kernel took a CPU exception during boot" >&2
    exit 1
fi

echo "ARM64 QEMU boot reached the event loop without a CPU exception."
