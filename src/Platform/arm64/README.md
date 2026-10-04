# ARM64 Platform Support

ARM64/AArch64 implementation for System 7.1 targeting QEMU's `virt` machine.

## Features

- **Bootstrap**: EL2 to EL1 exception level transition
- **Serial I/O**: PL011 UART driver
- **Timing**: ARM Generic Timer with microsecond precision
- **Interrupts**: GICv2 initialization on non-QEMU builds; the QEMU build skips it
- **Memory**: MMU with 4GB identity mapping
- **Cache**: Data and instruction cache management
- **Graphics**: VirtIO-GPU on QEMU builds; Raspberry Pi builds use the VideoCore mailbox
- **Hardware Detection**: Device Tree Blob parser

## Building

### Requirements

- `aarch64-elf-gcc` or `aarch64-none-elf-gcc`; the build also accepts
  `aarch64-linux-gnu-gcc`
- `qemu-system-aarch64` for testing

### Install Toolchain on macOS

```bash
brew install --cask gcc-aarch64-embedded
# or
brew install aarch64-elf-gcc
```

### Build

```bash
make PLATFORM=arm64
```

This produces `kernel.elf` at the repository root. The platform directory's
`make` command is a convenience wrapper around the same build.

## Testing in QEMU

### Generic ARM64 virtual machine

```bash
make -C src/Platform/arm64 qemu-virt
```

The default QEMU launch does not attach a VirtIO-GPU or storage device, so the
kernel continues without graphics and cannot mount a pre-existing boot volume.

## Memory Map

- QEMU `virt` RAM starts at `0x40000000`

Entry point: `0x40000000`

## Architecture

- **platform_boot.S**: Assembly bootstrap, vector table
- **hal_boot.c**: Hardware initialization and detection
- **uart_qemu.c**: PL011 serial console
- **timer.c**: ARM generic timer
- **storage.c**: VirtIO storage initialization
- **display.c**: QEMU framebuffer support
