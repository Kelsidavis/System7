# Project Guide

This file collects repository-specific guidance for AI coding assistants. The
README and `docs/` are the source of truth for project status and user-facing
documentation; keep this guide focused on development workflow.

## Start here

- [`README.md`](README.md): project overview, supported workflows, and current
  status.
- [`docs/INDEX.md`](docs/INDEX.md): documentation map.
- [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md): confirmed bugs and limitations.
- [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md): contribution conventions.

The code is organized by subsystem under `src/` and `include/`; hardware-specific
implementations live under `src/Platform/`. Check the active `PLATFORM` and
Makefile source lists before assuming a file is built.

## Build and test

The default platform is x86. Useful commands:

```sh
make                         # Build the default x86 kernel
make run                     # Boot the x86 ISO in QEMU
make debug                   # Boot under QEMU and wait for a debugger
make check                   # Build plus local x86 quality checks
make analyze                 # x86 build with GCC's static analyzer
make analyze-arm             # ARM32 analyzer build
make analyze-arm64           # ARM64 analyzer build
make PLATFORM=arm64          # Build the ARM64 kernel
make PLATFORM=ppc            # Build the experimental PowerPC target
make clean                   # Remove generated build artifacts
```

`make check` covers the strict build, x86 ELF layout, allocator policy,
standard-library differential tests, and required exports. CI additionally
builds supported configurations and platforms and boots the x86 ISO with BIOS
and UEFI firmware. Run the narrowest relevant checks while iterating, then run
`make check` for changes that affect shared code. Do not claim that a build proves
runtime behavior on hardware that was not tested.

`INTEGRATION_TESTS=1` includes tests that execute during boot. To build, run
QEMU, and parse their serial results, use `python3 tests/run_integration_tests.py`.

## Change guidelines

- Read the owning component's guide and nearby implementation before editing.
- Preserve the project's freestanding kernel constraints; direct `malloc` and
  `free` use in kernel code is rejected by the quality gate.
- Prefer shared implementations over platform-local copies when behavior is
  architecture-independent. Keep platform-specific hardware access in the
  platform layer.
- Update documentation when commands, source paths, supported behavior, or
  limitations change. Prefer links to the maintained overview over copying
  volatile status into multiple documents.
- Search with `rg`; the repository uses it in its current quick-reference docs.
- Keep changes focused, run relevant checks, and report what was and was not
  verified.

## Resource generation

`gen_rsrc.py` accepts a JSON manifest and an output path:

```sh
python3 gen_rsrc.py manifest.json output.rsrc
```

Check its module documentation and the manifest examples before changing the
supported resource formats.
