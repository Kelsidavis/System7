# ARM64 Platform Testing

## Build

The maintained ARM64 build targets QEMU's `virt` machine and produces
`kernel.elf` at the repository root:

```bash
make PLATFORM=arm64
```

The convenience wrapper is equivalent:

```bash
make -C src/Platform/arm64
```

## QEMU smoke test

```bash
src/Platform/arm64/test-qemu.sh
```

The script captures the serial log and succeeds only after the kernel mounts
the boot volume, initializes Finder, and enters the event loop. QEMU remains
running there until the test timeout terminates it.

Or run the kernel interactively:

```bash
qemu-system-aarch64 \
  -M virt \
  -cpu cortex-a53 \
  -m 1G \
  -kernel kernel.elf \
  -serial stdio \
  -display none
```

The headless QEMU configuration has no GPU, storage device, or wall clock. The
kernel continues without the first two; without a platform RTC, date/time
queries return the Mac epoch until `SetDateTime` is called.

## Runtime layout

- QEMU `virt` RAM starts at `0x40000000`.
- The PL011 UART is at `0x09000000`.
- The ARM64 linker separates read/execute code from read/write data.
