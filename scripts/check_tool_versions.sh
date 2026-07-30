#!/bin/bash

# Check tool versions (efi-rework branch)
# Simplified: focus on tools required for EFI-based build & testing.

set -euo pipefail

RED="\033[0;31m"
GREEN="\033[0;32m"
YELLOW="\033[0;33m"
NC="\033[0m"

# Check basic build tools
if ! command -v gcc >/dev/null 2>&1; then
    echo -e "${RED}✗ GCC not found${NC}"
    exit 1
else
    echo -e "${GREEN}✓ GCC found${NC}"
fi

if ! command -v make >/dev/null 2>&1; then
    echo -e "${RED}✗ GNU Make not found${NC}"
    exit 1
else
    echo -e "${GREEN}✓ Make available${NC}"
fi

# Check QEMU (for testing)
if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo -e "${YELLOW}⚠ qemu-system-x86_64 not found${NC}"
    echo "  Install: sudo apt-get install qemu-system-x86";
else
    echo -e "${GREEN}✓ qemu-system-x86_64 available${NC}"
fi

# Check OVMF (UEFI firmware for QEMU)
if [ -f "/usr/share/ovmf/OVMF_CODE.fd" ] || [ -f "/usr/share/ovmf/OVMF.fd" ]; then
    echo -e "${GREEN}✓ OVMF firmware found${NC}"
else
    echo -e "${YELLOW}⚠ OVMF firmware not found (UEFI testing may fail)${NC}"
    echo "  On Debian/Ubuntu: sudo apt-get install ovmf";
fi

# Note: GRUB/multiboot dependencies were removed in efi-rework branch.

echo ""
echo -e "${GREEN}Environment sanity checks complete (efi-rework).${NC}"
