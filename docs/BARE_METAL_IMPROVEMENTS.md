# Bare-Metal Validation Status

This guide distinguishes code that exists from hardware behavior that has
actually been verified. The x86 platform is the primary target; the presence of
a driver or a successful cross-build is not evidence that every machine using
that device works.

## Validation evidence

| Target | Verified here | Remaining limits |
|--------|---------------|------------------|
| x86 | CI builds the kernel and boots the ISO in QEMU under BIOS and UEFI. The project README records a physical UEFI ThinkPad test. | The physical test covers one machine, not a compatibility matrix; storage and peripherals vary by hardware. |
| ARM64 | CI builds and runs static analysis. | Runtime startup stops during boot-volume creation; see [Known Issues](KNOWN_ISSUES.md). |
| 32-bit ARM | CI builds and runs static analysis. | No hardware boot is currently recorded in CI. |
| PowerPC | An experimental Open Firmware-oriented target exists. | No current CI build or verified boot path. |

For the current platform build status and remaining port work, see the
[Platform Porting Status](future/PORTING_PLAN.md).

## x86 hardware interfaces

The x86 HAL contains serial, framebuffer, interrupt-controller, timer, PS/2,
ATA, USB, and network code under `src/Platform/x86/`. These components support
the current platform target, but their presence does not establish support for
every physical controller, firmware, or peripheral. QEMU tests validate the
configured virtual machines; physical test results should name the machine and
configuration that were actually exercised.

## Safe hardware testing

- Run `make check` and `make analyze` before testing an image on hardware.
- Record the machine model, firmware mode, boot medium, display mode, input
  devices, and serial output for each physical test.
- Use disposable media for storage experiments. Do not test disk-writing paths
  against a drive containing data that must be preserved.
- Separate build success, emulator boot success, and physical-hardware success
  in reports; they are different levels of evidence.
- When a device is unsupported, capture the failure and keep the limitation in
  [Known Issues](KNOWN_ISSUES.md) until a fix is verified.

## Current validation commands

```sh
make PLATFORM=x86
make check
make analyze
make iso
```

CI runs the x86 quality gate and BIOS/UEFI QEMU boot tests. Building another
platform does not replace a runtime test on that platform.
