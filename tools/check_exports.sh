#!/usr/bin/env bash
# [WM-056] Public symbol surface manifest + CI diff
# Ensures only approved symbols are exported from kernel.elf
# [Audit B] Platform layer must not define WM_ symbols except WDEF refs
set -euo pipefail

# Generate current exports
nm -g --defined-only kernel.elf | awk '{print $3}' | sort -u > build/symbols.exports.txt

# Compare against allowlist
# Unexpected: symbols in exports but not in allowlist
comm -23 build/symbols.exports.txt docs/symbols_allowlist.txt > build/symbols.unexpected.txt || true

# Missing: symbols in allowlist but not in exports
comm -13 build/symbols.exports.txt docs/symbols_allowlist.txt > build/symbols.missing.txt || true

# Check for unexpected exports
if [ -s build/symbols.unexpected.txt ]; then
  echo "ERROR: Unexpected exported symbols found (not in allowlist):"
  cat build/symbols.unexpected.txt
  echo ""
  echo "Hint: If these symbols should be public API, add them to docs/symbols_allowlist.txt"
  echo "      If they should be private, make them static or hide them."
  exit 1
fi

# Check for missing intended APIs
if [ -s build/symbols.missing.txt ]; then
  echo "WARNING: Expected symbols missing from kernel.elf:"
  cat build/symbols.missing.txt
  echo ""
  echo "Hint: These symbols are in the allowlist but not exported."
  echo "      Either implement them or remove from docs/symbols_allowlist.txt"
  exit 1
fi

echo "Export surface OK - all exports match allowlist exactly."

# [Audit B] Check Platform layer for WM_ symbol definitions
# Platform/*.o may reference WM_*DefProc (WDEF handles) but must not define other WM_ symbols
if [ -d build/obj ]; then
  nm -o build/obj/WindowPlatform.o 2>/dev/null | \
    grep -E ' T WM_' | \
    grep -v 'WM_.*DefProc' > /tmp/platform_wm_violations.txt || true

  if [ -s /tmp/platform_wm_violations.txt ]; then
    echo "ERROR: Platform layer defines WM_ symbols (should only reference WM_*DefProc):"
    cat /tmp/platform_wm_violations.txt
    echo ""
    echo "Hint: Platform layer should not define WM_ symbols. Move to src/WindowManager/"
    rm -f /tmp/platform_wm_violations.txt
    exit 1
  fi
  rm -f /tmp/platform_wm_violations.txt
fi

echo "Audit B: Platform layer WM_ separation OK."