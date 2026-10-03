# Platform Porting Status

This document tracks work against the platform layout and build targets that
exist today. It replaces the original prospective plan to create the HAL and
platform directories; those scaffolds are already present.

## Current status

- **x86** is the primary supported target. CI builds it, runs the quality gate,
  and boots the generated image under QEMU in BIOS and UEFI modes.
- **ARM64** has a build and static-analysis job. Runtime startup remains
  incomplete: the kernel stops while creating the boot volume; see
  [Known Issues](../KNOWN_ISSUES.md).
- **32-bit ARM** has a Raspberry Pi-oriented build target and HAL sources, but
  this repository's CI does not currently validate a hardware boot.
- **PowerPC** has an experimental Open Firmware-oriented build scaffold. It is
  not included in CI and has no verified hardware or QEMU boot path.

Platform code is selected by `PLATFORM` in the root `Makefile`. Shared kernel
code should use the interfaces in `src/Platform/include/`; implementations are
kept in `src/Platform/x86/`, `arm/`, `arm64/`, and `ppc/`.

## Remaining work

1. Diagnose and fix the ARM64 boot-volume creation failure, then add a runtime
   boot check to CI.
2. Define repeatable runtime validation for 32-bit ARM and PowerPC before
   describing either target as supported; keep their current experimental
   status explicit until that evidence exists.
3. As platform implementations mature, consolidate genuinely shared behavior
   behind the HAL. Keep hardware-specific alternatives separate where their
   behavior or registers differ; do not duplicate shared policy in each port.

## Build and validation

```sh
make PLATFORM=x86
make check
make analyze
make PLATFORM=arm64
make PLATFORM=arm
make PLATFORM=ppc
```

The ARM and PowerPC builds require their cross-compilers. Build success alone
does not establish that a target boots; use runtime evidence before changing
the status above.
