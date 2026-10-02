#!/bin/bash
# Quick QEMU virt smoke test for the maintained top-level ARM64 kernel.
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/../../.." && pwd)

make -C "$ROOT_DIR" PLATFORM=arm64 all
timeout 5 qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a53 \
    -m 1G \
    -kernel "$ROOT_DIR/kernel.elf" \
    -serial stdio \
    -display none 2>&1 | head -100
