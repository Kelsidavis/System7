# Platform Refactoring Status

## Phase 1: shared kernel and platform HAL — complete

The initial portability refactor is implemented. `src/boot.c` contains shared
boot flow, the public platform interfaces are under `src/Platform/include/`,
and platform implementations are selected by `PLATFORM` in the root
`Makefile`.

The x86 implementation lives in `src/Platform/x86/` (including
`platform_boot.S`, `io.c`, `ata.c`, `ps2.c`, and `hal_boot.c`). ARM, ARM64, and
PowerPC implementations live in their respective `src/Platform/` directories.
Shared subsystems remain under `src/` and should call the HAL rather than reach
into architecture-specific drivers.

The old step-by-step instructions to create the HAL and move files into it have
been retired because those changes are already present. For remaining platform
work, use the current [porting plan](PORTING_PLAN.md) and
[known issues](../KNOWN_ISSUES.md). Current build targets and validation are
listed in the [project README](../../README.md).
