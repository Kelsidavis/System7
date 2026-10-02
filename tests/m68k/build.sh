#!/bin/sh
# Build the 68K test applications and the disk image that carries them.
#
# Needs binutils-m68k-linux-gnu and hfsutils. Writes tests/m68k/out/apps.img, an
# HFS volume "Test Apps"; give it to QEMU as an IDE disk and it mounts on the
# desktop beside the startup disk:
#     qemu-system-i386 -cdrom system71.iso -hda tests/m68k/out/apps.img -boot d ...
set -e
here=$(dirname "$0")
out=${1:-tests/m68k/out}
mkdir -p "$out"

for app in Hello; do
    m68k-linux-gnu-as -m68000 -o "$out/$app.o" "$here/$app.s"
    m68k-linux-gnu-objcopy -O binary -j .text "$out/$app.o" "$out/$app.code"
    python3 "$here/mkapp.py" "$out/$app.code" "$out/$app.bin" "$app"
done

rm -f "$out/apps.img"
dd if=/dev/zero of="$out/apps.img" bs=1024 count=2048 2>/dev/null
hformat -l "Test Apps" "$out/apps.img" >/dev/null
hmount "$out/apps.img" >/dev/null
for app in Hello; do
    hcopy -m "$out/$app.bin" ":$app"
done
hls -l
humount
