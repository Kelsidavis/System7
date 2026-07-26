#!/bin/bash

# Run System 7.1 kernel in QEMU
# Using multiboot protocol

echo "Starting System 7.1 reimplementation in QEMU..."
echo "Press Ctrl+A then X to exit"
echo ""

# Try with multiboot option explicitly
qemu-system-i386 \
    -machine q35 \
    -m 256M \
    -kernel kernel.elf \
    -serial mon:stdio \
    -vga std \
    -display curses \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    2>&1

# Alternative if above fails
if [ $? -ne 0 ]; then
    echo "Trying alternative QEMU configuration..."
    qemu-system-i386 \
        -m 256M \
        -kernel kernel.elf \
        -nographic \
        -serial mon:stdio \
        -append "console=ttyS0" \
        2>&1
fi