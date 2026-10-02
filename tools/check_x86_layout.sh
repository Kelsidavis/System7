#!/usr/bin/env bash
# Verify the x86 Multiboot2 header and ELF memory permissions.
set -euo pipefail

readelf=${READELF:-}
if [[ -z "$readelf" ]]; then
    for candidate in i686-elf-readelf readelf; do
        if command -v "$candidate" >/dev/null 2>&1; then
            readelf=$candidate
            break
        fi
    done
fi

if [[ -z "$readelf" ]]; then
    echo "ERROR: readelf is required to validate the x86 kernel layout" >&2
    exit 1
fi

entry_hex=$($readelf -W -h kernel.elf | awk '/Entry point address:/ { print $4 }')
boot_offset_hex=$($readelf -W -S kernel.elf | awk '$3 == ".boot" { print "0x" $6 }')
program_headers=$($readelf -W -l kernel.elf)

if [[ -z "$entry_hex" || -z "$boot_offset_hex" ]]; then
    echo "ERROR: x86 kernel is missing its entry point or .boot section" >&2
    exit 1
fi

entry=$((entry_hex))
boot_offset=$((boot_offset_hex))
if (( entry < 0x100000 )); then
    echo "ERROR: x86 entry point is below the 1 MiB load address ($entry_hex)" >&2
    exit 1
fi

if (( boot_offset >= 32768 )); then
    echo "ERROR: Multiboot2 header is outside the first 32 KiB (offset $boot_offset_hex)" >&2
    exit 1
fi

read -r magic architecture header_length checksum < <(
    od -An -N16 -j "$boot_offset" -tx4 kernel.elf
)
if [[ "$magic" != "e85250d6" || -z "$architecture" || -z "$header_length" || -z "$checksum" ]]; then
    echo "ERROR: x86 .boot section does not start with a complete Multiboot2 header" >&2
    exit 1
fi
header_sum=$((0x$magic + 0x$architecture + 0x$header_length + 0x$checksum))
if (( (header_sum & 0xffffffff) != 0 )); then
    echo "ERROR: Multiboot2 header checksum is invalid" >&2
    exit 1
fi

if grep -q 'RWE' <<<"$program_headers"; then
    echo "ERROR: x86 kernel has a writable, executable load segment" >&2
    exit 1
fi

if ! grep -Eq 'GNU_STACK.* RW ' <<<"$program_headers"; then
    echo "ERROR: x86 kernel lacks a non-executable stack declaration" >&2
    exit 1
fi

echo "x86 Multiboot2 placement and ELF permissions OK."
