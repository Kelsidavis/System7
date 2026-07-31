# scripts/make_esp_with_toolkits.sh
# Create a FAT image and populate with EFI/BOOT and our toolkit placeholders.
set -e
OUT=out/esp.img
BOOTDIR=esp_contents/EFI/BOOT
mkdir -p "$BOOTDIR"

# Copy placeholders produced by build_toolkits.sh
cp out/toolkits/TOOLKIT_PPC.ROM "$BOOTDIR/TOOLKIT_PPC.ROM" || true
cp out/toolkits/TOOLKIT_68K_BE.ROM "$BOOTDIR/TOOLKIT_68K_BE.ROM" || true
cp out/toolkits/TOOLKIT_X86_64.ROM "$BOOTDIR/TOOLKIT_X86_64.ROM" || true
cp out/toolkits/TOOLKIT_AARCH64.ROM "$BOOTDIR/TOOLKIT_AARCH64.ROM" || true
cp out/toolkits/TOOLKIT_RISCV64.ROM "$BOOTDIR/TOOLKIT_RISCV64.ROM" || true
cp out/toolkits/PRAM.BIN "$BOOTDIR/PRAM.BIN" || true

# Placeholder EFI binary (copy existing BOOTX64.EFI if present)
if [ -f build/efi/BOOTX64.EFI ]; then
  cp build/efi/BOOTX64.EFI "$BOOTDIR/BOOTX64.EFI"
else
  # create a tiny placeholder text file so FAT isn't empty
  printf "Placeholder EFI binary\n" > "$BOOTDIR/BOOTX64.EFI"
fi

# Create a FAT image using mtools or genisoimage + mformat
# Prefer mformat (mtools) if available
if command -v mkfs.vfat >/dev/null 2>&1; then
  echo "Creating FAT image $OUT (32MB)"
  dd if=/dev/zero of="$OUT" bs=1M count=32
  mkfs.vfat "$OUT"
  mkdir -p /tmp/esp_mount
  sudo mount -o loop "$OUT" /tmp/esp_mount
  sudo cp -r esp_contents/* /tmp/esp_mount/
  sync
  sudo umount /tmp/esp_mount
  rmdir /tmp/esp_mount
else
  echo "mkfs.vfat not found; create FAT image manually and copy esp_contents/ to it"
fi

echo "ESP image prepared: $OUT"
