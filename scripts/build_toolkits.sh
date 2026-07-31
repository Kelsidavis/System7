#!/bin/sh
# scripts/build_toolkits.sh
# Simple helper to produce placeholder ROM blobs. Requires appropriate cross-compilers
# If you don't have cross-compilers installed, this script will still produce
# raw placeholders by concatenating zero bytes.

set -e
OUTDIR=out/toolkits
mkdir -p "$OUTDIR"

# PPC: Expect ppc-none-elf-gcc / ld to exist; otherwise create zeros
if command -v powerpc-linux-gnu-gcc >/dev/null 2>&1; then
  echo "Building PPC placeholder..."
  powerpc-linux-gnu-gcc -march=powerpc -mabi=32 -nostdlib -ffreestanding -c src/Toolkit/ppc/toolkit.c -o "$OUTDIR/toolkit_ppc.o"
  powerpc-linux-gnu-ld -Ttext=0x0 --oformat binary "$OUTDIR/toolkit_ppc.o" -o "$OUTDIR/TOOLKIT_PPC.ROM"
else
  echo "No PPC cross-compiler found; creating zero-filled PPC placeholder (8K)"
  dd if=/dev/zero of="$OUTDIR/TOOLKIT_PPC.ROM" bs=1 count=8192
fi

# 68k
if command -v m68k-elf-gcc >/dev/null 2>&1; then
  echo "Building 68k placeholder..."
  m68k-elf-gcc -mcpu=68020 -nostdlib -ffreestanding -c src/Toolkit/68k/toolkit.c -o "$OUTDIR/toolkit_68k.o"
  m68k-elf-ld -Ttext=0x0 --oformat binary "$OUTDIR/toolkit_68k.o" -o "$OUTDIR/TOOLKIT_68K_BE.ROM"
else
  echo "No 68k cross-compiler found; creating zero-filled 68k placeholder (8K)"
  dd if=/dev/zero of="$OUTDIR/TOOLKIT_68K_BE.ROM" bs=1 count=8192
fi

# Touch optional LE toolkits as empty placeholders (can be replaced by real blobs)
mkdir -p "$OUTDIR"
for f in TOOLKIT_X86_64.ROM TOOLKIT_AARCH64.ROM TOOLKIT_RISCV64.ROM TOOLKIT_PPC.ROM; do
  if [ ! -f "$OUTDIR/$f" ]; then
    dd if=/dev/zero of="$OUTDIR/$f" bs=1 count=4096
  fi
done

# PRAM placeholder
if [ ! -f "$OUTDIR/PRAM.BIN" ]; then
  printf '\0' > "$OUTDIR/PRAM.BIN"
fi

echo "Toolkits written to $OUTDIR"
