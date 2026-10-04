#!/usr/bin/env bash
set -euo pipefail

# Extract Mac OS icons from a classic HFS disk image (qcow2/raw)
#
# Usage:
#   tools/extract_icons_from_image.sh /path/to/macos71-hd.qcow2 OUT_DIR
#
# Requirements: qemu-img, parted, mount (HFS), or hfsutils (hmount/hcopy)

if [ $# -lt 2 ]; then
  echo "Usage: $0 IMAGE.qcow2 OUT_DIR" >&2
  exit 1
fi

IMG="$1"
OUT="$2"
WORK=$(mktemp -d)
RAW="$WORK/disk.raw"
MNT="$WORK/mnt"
LOOP=""

cleanup() {
  sudo umount "$MNT" 2>/dev/null || true
  if [ -n "$LOOP" ]; then
    sudo losetup -d "$LOOP" 2>/dev/null || true
  fi
  rm -rf -- "$WORK"
}
trap cleanup EXIT

mkdir -p "$OUT" "$MNT"

echo "[*] Converting to raw..."
qemu-img convert -O raw "$IMG" "$RAW"

echo "[*] Locating HFS partition..."
PART_INFO=$(parted -s "$RAW" unit B print | awk '/^ [0-9]+/ {print $1, $2, $4}')
START=$(echo "$PART_INFO" | head -n1 | awk '{print $2}' | sed 's/B//')

echo "[*] Mounting HFS partition (requires sudo)..."
LOOP=$(sudo losetup --show -f -o "$START" "$RAW")
sudo mount -t hfs -o ro "$LOOP" "$MNT"

echo "[*] Copying candidate files (System, Finder, Desktop DB)..."
cp -f "$MNT"/System* "$OUT" 2>/dev/null || true
cp -f "$MNT"/Finder* "$OUT" 2>/dev/null || true
cp -f "$MNT"/Desktop* "$OUT" 2>/dev/null || true

echo "[*] Extracting icons from resource forks (best-effort)..."
python3 tools/rez_extract_icons.py "$OUT"/* --out "$OUT/png"

echo "Done. PNGs in: $OUT/png"
