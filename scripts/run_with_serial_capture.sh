#!/bin/bash
# System 7.1 Portable - Run with automated serial capture
# Usage: ./run_with_serial_capture.sh [duration_seconds]

DURATION=${1:-120}
LOG_FILE="/tmp/system71_serial_$(date +%Y%m%d_%H%M%S).log"

echo "System 7.1 Portable - Booting with serial capture"
echo "Log file: $LOG_FILE"
echo "Duration: $DURATION seconds"
echo ""

script -q -c "qemu-system-ppc -M mac99 -kernel kernel.elf -nographic" "$LOG_FILE" &
PID=$!
sleep "$DURATION"
kill $PID 2>/dev/null
wait 2>/dev/null

echo ""
echo "=== Boot Summary ===" 
lines=$(wc -l < "$LOG_FILE")
echo "Total lines captured: $lines"
echo ""
echo "=== Last 20 lines ===" 
tail -20 "$LOG_FILE"
echo ""
echo "=== Searching for key events ===" 
grep -E "System|Boot|Desktop|Finder|Error|WARNING" "$LOG_FILE" | head -20
