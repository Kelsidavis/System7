#!/bin/bash
# Make an HFS disk image holding a file split across more extents than the
# catalog record has room for, so reading it needs the extents overflow
# B-tree. Attach it as an IDE drive to an INTEGRATION_TESTS=1 build and
# File_ReadThroughExtentsOverflow reads it back:
#
#   tools/make_fragmented_hfs.sh frag.img
#   qemu-system-i386 -cdrom system71.iso -drive file=frag.img,format=raw,if=ide ...
#
# Needs hfsutils (hformat, hcopy, hdel). The volume is filled with 4 KB files
# and every other one deleted, so the only free space is 8-block holes and
# the 80000-byte file has to be laid across about twenty of them.
set -e
out=${1:-frag.img}
tmp=$(mktemp -d)
trap 'humount >/dev/null 2>&1 || true; rm -rf "$tmp"' EXIT

dd if=/dev/zero of="$out" bs=1k count=2048 status=none
hformat -l ITestHFS "$out" >/dev/null
hmount "$out" >/dev/null

head -c 4096 /dev/zero > "$tmp/small"
n=0
while hcopy -r "$tmp/small" ":f$((n + 1))" 2>/dev/null; do n=$((n + 1)); done
for i in $(seq 1 2 "$n"); do hdel ":f$i"; done

# The bytes the test expects: ((i * 7) ^ (i >> 9)) & 0xFF.
python3 -c '
import sys
sys.stdout.buffer.write(bytes(((i * 7) ^ (i >> 9)) & 0xFF for i in range(80000)))' > "$tmp/big"
hcopy -r "$tmp/big" :Fragmented
humount >/dev/null

# Refuse an image that would not test anything: the extents overflow tree's
# header node must count some leaf records.
python3 - "$out" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read()
mdb = d[1024:1536]
blk, = struct.unpack('>I', mdb[0x14:0x18])
first, = struct.unpack('>H', mdb[0x1C:0x1E])
start, = struct.unpack('>H', mdb[0x86:0x88])
header = first * 512 + start * blk + 14
leaves, = struct.unpack('>I', d[header + 6:header + 10])
if leaves == 0:
    sys.exit("no extents overflow records: the file was not fragmented")
print(f"{sys.argv[1]}: {leaves} extents overflow records")
PY
