#!/bin/sh
# Build the 68K test applications and the disk image that carries them.
#
# Needs gcc-m68k-linux-gnu, binutils-m68k-linux-gnu and hfsutils. Writes tests/m68k/out/apps.img, an
# HFS volume "Test Apps"; give it to QEMU as an IDE disk and it mounts on the
# desktop beside the startup disk:
#     qemu-system-i386 -cdrom system71.iso -hda tests/m68k/out/apps.img -boot d ...
set -e
here=$(dirname "$0")
out=${1:-tests/m68k/out}
mkdir -p "$out"

# Hello is assembly
m68k-linux-gnu-as -m68000 -o "$out/Hello.o" "$here/Hello.s"
m68k-linux-gnu-objcopy -O binary -j .text "$out/Hello.o" "$out/Hello.code"
python3 "$here/mkapp.py" "$out/Hello.code" "$out/Hello.bin" Hello

# The rest are C. Code and data are one block addressed relative to the PC
# (-mpcrel), A5 is left alone (-ffixed-a5): it is the application's.
CFLAGS="-Wno-multichar -m68000 -mpcrel -ffixed-a5 -Os -ffreestanding -fno-builtin -nostdlib -fno-zero-initialized-in-bss -Wall"
# libgcc is built for the 68020; runtime.c has what a 68000 needs instead.
m68k-linux-gnu-as -m68000 -o "$out/crt0.o" "$here/crt0.s"
m68k-linux-gnu-gcc $CFLAGS -O2 -c -o "$out/runtime.o" "$here/runtime.c"
for app in Sampler Notes Gallery Keeper; do
    m68k-linux-gnu-gcc $CFLAGS -c -o "$out/$app.o" "$here/$app.c"
    m68k-linux-gnu-ld -T "$here/app.ld" -o "$out/$app.elf" "$out/crt0.o" "$out/$app.o" "$out/runtime.o"
    if m68k-linux-gnu-objdump -r "$out/crt0.o" "$out/$app.o" "$out/runtime.o" | grep -q R_68K_32; then
        echo "$app: absolute relocations - it would not run where it is loaded" >&2
        exit 1
    fi
    m68k-linux-gnu-objcopy -O binary -j .text "$out/$app.elf" "$out/$app.code"
    python3 "$here/mkapp.py" "$out/$app.code" "$out/$app.bin" "$app"
done

rm -f "$out/apps.img"
dd if=/dev/zero of="$out/apps.img" bs=1024 count=2048 2>/dev/null
hformat -l "Test Apps" "$out/apps.img" >/dev/null
hmount "$out/apps.img" >/dev/null
for app in Hello Sampler Notes Gallery Keeper; do
    hcopy -m "$out/$app.bin" ":$app"
done
hls -l
humount
