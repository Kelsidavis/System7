#!/usr/bin/env bash
# Tool version verification script for System 7.1 build
set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
GCC_MIN_VERSION="${1:-7.0}"
PYTHON_MIN_VERSION="${2:-3.6}"

# Version comparison function
version_ge() {
    # Returns 0 if $1 >= $2
    printf '%s\n%s\n' "$2" "$1" | sort -V -C
}

# Extract version number from string
extract_version() {
    echo "$1" | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1
}

echo "Checking build tool versions..."
echo ""

# Check GCC
if ! command -v gcc >/dev/null 2>&1; then
    echo -e "${RED}✗ GCC not found${NC}"
    exit 1
fi

GCC_VERSION=$(gcc --version | head -1)
GCC_NUM=$(extract_version "$GCC_VERSION")

if version_ge "$GCC_NUM" "$GCC_MIN_VERSION"; then
    echo -e "${GREEN}✓ GCC $GCC_NUM${NC} (minimum: $GCC_MIN_VERSION)"
else
    echo -e "${RED}✗ GCC $GCC_NUM${NC} - requires >= $GCC_MIN_VERSION"
    exit 1
fi

# Check for 32-bit support (skip for non-x86 platforms)
if [ "${PLATFORM:-x86}" = "x86" ]; then
    if ! gcc -m32 -x c -c /dev/null -o /dev/null 2>/dev/null; then
        echo -e "${YELLOW}⚠ GCC 32-bit support not available${NC}"
        echo "  Install: sudo apt-get install gcc-multilib"
        # Don't exit - allow build to proceed, will fail later if 32-bit is actually needed
    else
        echo -e "${GREEN}✓ GCC 32-bit support available${NC}"
    fi
else
    echo -e "${GREEN}✓ Skipping 32-bit check for ${PLATFORM} platform${NC}"
fi

# Check Python
if ! command -v python3 >/dev/null 2>&1; then
    echo -e "${RED}✗ Python3 not found${NC}"
    exit 1
fi

PYTHON_VERSION=$(python3 --version 2>&1)
PYTHON_NUM=$(extract_version "$PYTHON_VERSION")

if version_ge "$PYTHON_NUM" "$PYTHON_MIN_VERSION"; then
    echo -e "${GREEN}✓ Python $PYTHON_NUM${NC} (minimum: $PYTHON_MIN_VERSION)"
else
    echo -e "${RED}✗ Python $PYTHON_NUM${NC} - requires >= $PYTHON_MIN_VERSION"
    exit 1
fi

# Check Make
if ! command -v make >/dev/null 2>&1; then
    echo -e "${RED}✗ GNU Make not found${NC}"
    exit 1
fi

MAKE_VERSION=$(make --version 2>/dev/null | head -1 || echo "GNU Make 0.0")
MAKE_NUM=$(extract_version "$MAKE_VERSION")

if version_ge "$MAKE_NUM" "4.0"; then
    echo -e "${GREEN}✓ Make $MAKE_NUM${NC} (minimum: 4.0)"
else
    echo -e "${YELLOW}⚠ Make $MAKE_NUM${NC} - recommended >= 4.0"
fi

# Check grub-mkrescue
if ! command -v grub-mkrescue >/dev/null 2>&1; then
    echo -e "${YELLOW}⚠ grub-mkrescue not found${NC}"
    echo "  Install: sudo apt-get install grub-pc-bin xorriso"
    echo "  (Optional: only needed for ISO creation)"
else
    echo -e "${GREEN}✓ grub-mkrescue available${NC}"
fi

# Check xxd
if ! command -v xxd >/dev/null 2>&1; then
    echo -e "${RED}✗ xxd not found${NC}"
    echo "  Install: sudo apt-get install vim-common"
    exit 1
else
    echo -e "${GREEN}✓ xxd available${NC}"
fi

# Check QEMU (optional)
if ! command -v qemu-system-i386 >/dev/null 2>&1; then
    echo -e "${YELLOW}⚠ qemu-system-i386 not found${NC}"
    echo "  Install: sudo apt-get install qemu-system-x86"
    echo "  (Optional: only needed for testing)"
else
    echo -e "${GREEN}✓ QEMU available${NC}"
fi

echo ""
echo -e "${GREEN}All required tools are present and versioned correctly${NC}"
