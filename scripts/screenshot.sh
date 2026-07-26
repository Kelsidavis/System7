#!/bin/bash
# screenshot.sh - Boot the ISO headless and capture the framebuffer as PNG.
#
# Redraw and layout bugs are invisible in the serial log; this makes them
# reviewable without a physical machine. Optionally drives synthetic clicks
# through the QEMU monitor.
#
# Usage:
#   scripts/screenshot.sh out.png [boot_delay_secs] [click_x click_y]
#
# A USB tablet is attached so monitor mouse_move takes ABSOLUTE screen
# coordinates. A PS/2 mouse only accepts relative deltas, which makes it
# effectively impossible to aim at a specific widget.

set -euo pipefail

OUT="${1:-screenshot.png}"
DELAY="${2:-24}"
CLICK_X="${3:-}"
CLICK_Y="${4:-}"

ISO="${ISO:-system71.iso}"
[ -f "$ISO" ] || { echo "No $ISO - run 'make iso' first" >&2; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
SOCK="$WORK/mon.sock"

qemu-system-i386 -cdrom "$ISO" -m 1024 -vga std -display none \
    -device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0 \
    -serial "file:$WORK/serial.log" \
    -monitor "unix:$SOCK,server,nowait" -no-reboot &
QPID=$!
trap 'kill $QPID 2>/dev/null || true; rm -rf "$WORK"' EXIT

mon() { echo "$1" | socat - "UNIX-CONNECT:$SOCK" >/dev/null 2>&1 || true; sleep 0.3; }

echo "Booting (${DELAY}s)..."
sleep "$DELAY"

if [ -n "$CLICK_X" ] && [ -n "$CLICK_Y" ]; then
    echo "Clicking at ($CLICK_X,$CLICK_Y)..."
    mon "mouse_move $CLICK_X $CLICK_Y"
    mon "mouse_button 1"
    mon "mouse_button 0"
    sleep 1
fi

mon "screendump $WORK/shot.ppm"
sleep 1

if [ ! -s "$WORK/shot.ppm" ]; then
    echo "screendump produced nothing" >&2
    exit 1
fi

if command -v convert >/dev/null 2>&1; then
    convert "$WORK/shot.ppm" "$OUT"
elif command -v pnmtopng >/dev/null 2>&1; then
    pnmtopng "$WORK/shot.ppm" > "$OUT"
else
    OUT="${OUT%.png}.ppm"
    cp "$WORK/shot.ppm" "$OUT"
    echo "(no PNG converter; wrote PPM)"
fi

cp "$WORK/serial.log" "${OUT%.*}.serial.log" 2>/dev/null || true
echo "Wrote $OUT and ${OUT%.*}.serial.log"
