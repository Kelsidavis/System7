# Build EFI loader (x86_64) using GNU-EFI
# Adjust include and library paths for your distribution where needed.

set -euo pipefail

EFI_INC_DIR="/usr/include/efi"
EFI_ARCH_DIR="/usr/include/efi/x86_64"
LD_SCRIPT="/usr/lib/gnuefi/elf_x86_64_efi.lds"

if [ ! -d "$EFI_INC_DIR" ] || [ ! -d "$EFI_ARCH_DIR" ]; then
    echo "GNU-EFI headers not found in $EFI_INC_DIR or $EFI_ARCH_DIR."
    echo "On Debian/Ubuntu: sudo apt-get install libefi-dev gnu-efi";
    exit 1
fi

gcc -I"$EFI_INC_DIR" -I"$EFI_ARCH_DIR" -fno-stack-protector -fshort-wchar -mno-red-zone -DEFI_FUNCTION_WRAPPER -c src/Platform/efi/efi_loader.c -o efi_loader.o

ld -nostdlib -znocombreloc -T "$LD_SCRIPT" efi_loader.o -Bsymbolic -L/usr/lib -lefi -lgnuefi -o BOOTX64.EFI

echo "Built BOOTX64.EFI"
