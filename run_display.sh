#!/bin/bash

# Run with display - this will open a graphical window
echo "Starting System 7.1 with display..."
echo "A graphical window should open showing the system"
echo "Serial output will appear in this terminal"
echo ""

# Run QEMU with SDL display
qemu-system-i386 \
    -m 256M \
    -kernel kernel.elf \
    -serial stdio \
    -vga std \
    -display sdl \
    2>&1