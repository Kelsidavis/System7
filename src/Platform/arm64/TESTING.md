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

The script captures the complete serial log and succeeds only after the kernel
reaches boot-volume creation. It is an early-boot check, not a full boot test;
the current boot-volume stall is documented in `docs/KNOWN_ISSUES.md`.

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

The kernel currently boots through desktop setup in QEMU and then stops while
creating the boot volume; see `docs/KNOWN_ISSUES.md` for the current status.

## Runtime layout

- QEMU `virt` RAM starts at `0x40000000`.
- The PL011 UART is at `0x09000000`.
- The ARM64 linker separates read/execute code from read/write data.
