#!/usr/bin/env bash
# [WM-056] Required public symbol manifest + CI diff
# Ensures required Toolbox APIs remain exported from kernel.elf.
# The freestanding kernel has no visibility boundary, so `nm -g` also reports
# internal globals; this manifest is deliberately a required subset rather
# than an exhaustive list of every linker-visible symbol.
# [Audit B] Platform layer must not define WM_ symbols except WDEF refs
set -euo pipefail

OBJ_DIR="${OBJ_DIR:-build/obj}"

# Generate current exports
nm -g --defined-only kernel.elf | awk '{print $3}' | sort -u > build/symbols.exports.txt

# Compare required APIs against the current exports.
comm -13 build/symbols.exports.txt docs/symbols_allowlist.txt > build/symbols.missing.txt || true

# Check for missing intended APIs
if [ -s build/symbols.missing.txt ]; then
  echo "WARNING: Expected symbols missing from kernel.elf:"
  cat build/symbols.missing.txt
  echo ""
  echo "Hint: These symbols are in the allowlist but not exported."
  echo "      Either implement them or remove from docs/symbols_allowlist.txt"
  exit 1
fi

echo "Required export surface OK."

# [Audit B] Check Platform layer for WM_ symbol definitions
# Platform/*.o may reference WM_*DefProc (WDEF handles) but must not define other WM_ symbols
if [ -f "$OBJ_DIR/WindowPlatform.o" ]; then
  if ! platform_symbols=$(nm -o "$OBJ_DIR/WindowPlatform.o" 2>/dev/null); then
    echo "ERROR: Could not inspect $OBJ_DIR/WindowPlatform.o with nm" >&2
    exit 1
  fi
  violations=$(printf '%s\n' "$platform_symbols" | \
    grep -E ' T WM_' | grep -v 'WM_.*DefProc' || true)

  if [ -n "$violations" ]; then
    echo "ERROR: Platform layer defines WM_ symbols (should only reference WM_*DefProc):"
    printf '%s\n' "$violations"
    echo
    echo "Hint: Platform layer should not define WM_ symbols. Move to src/WindowManager/"
    exit 1
  fi
fi

echo "Audit B: Platform layer WM_ separation OK."
