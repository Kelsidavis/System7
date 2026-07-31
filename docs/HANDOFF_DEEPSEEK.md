HANDOFF for Deepseek

Repository: kramlat/LazarusOS
Branch: efi-rework

Context / Goal
- Implement EFI loader + toolkit ROM plumbing so the emulated legacy toolkits (PPC, 68k) and modern LE toolkits (x86_64, aarch64, riscv64) are loaded from the ESP and exposed to the kernel/emulator via a BootInfo handoff.
- PRAM persistence: chosen strategy A — kernel writes PRAM to UEFI variable on shutdown. Loader reads UEFI variable LazarusOS:PRAM if present, otherwise falls back to PRAM.BIN on ESP and exposes pram_addr/pram_size in BootInfo.

What’s already done (on efi-rework)
- UTF-8 name groundwork and OSType helpers: src/FS/ostype_utils.c, include/FS/ostype_utils.h
- VFS + CatEntry change to store printable creator/type strings (include/FS/hfs_types.h and vfs changes)
- EFI loader: blessed-folder heuristics with icon detection and Finder scan: src/Platform/efi/efi_loader.c
- Placeholder clean-room toolkit sources for PPC and 68k: src/Toolkit/ppc/toolkit.c and src/Toolkit/68k/toolkit.c
- Build & ESP scripts to create placeholder ROM blobs and FAT image: scripts/build_toolkits.sh and scripts/make_esp_with_toolkits.sh
- Toolkit placeholder blobs are output to out/toolkits via the build scripts (not tracked as large binaries in repo)

Key files / pointers
- Loader: src/Platform/efi/efi_loader.c
- HFS / VFS types + name handling: include/FS/hfs_types.h, include/FS/vfs.h, src/FS/vfs.c
- OSType helpers: include/FS/ostype_utils.h, src/FS/ostype_utils.c
- Toolkits (placeholders): src/Toolkit/ppc/toolkit.c, src/Toolkit/68k/toolkit.c
- Scripts to reproduce: scripts/build_toolkits.sh, scripts/make_esp_with_toolkits.sh
- Toolkits output (after running build scripts): out/toolkits/

Commands to reproduce locally (smoke test)
1) Build placeholder toolkits (may require cross compilers):
   ./scripts/build_toolkits.sh

2) Create FAT ESP image with placeholders:
   ./scripts/make_esp_with_toolkits.sh

3) Boot in QEMU (example x86_64):
   qemu-system-x86_64 -bios OVMF.fd -drive file=out/esp.img,format=raw -serial stdio

4) Observe EFI loader serial output: look for messages like "Blessed check passed" and which toolkit file was loaded.

Remaining prioritized tasks for Deepseek (highest -> lowest)
1) Produce LE toolkit ROM blobs and source stubs
   - Add clean-room LE toolkit sources (x86_64, aarch64, riscv64) exposing a small Gestalt surface.
   - Add build rules to create TOOLKIT_X86_64.ROM, TOOLKIT_AARCH64.ROM, TOOLKIT_RISCV64.ROM and place them under out/toolkits.
   - Update scripts/build_toolkits.sh to build these when cross toolchains are present.
   Estimated: 1–3 hours.

2) BootInfo wiring & per-arch mapping
   - Ensure loader maps chosen toolkit into page-aligned memory and fills BootInfo with per-arch toolkit_addr/toolkit_size, pram_addr/pram_size, and flags.
   - Pass BootInfo pointer in the ABI entry register per arch:
       x86_64: RDI
       aarch64: X0
       riscv64: a0
       ppc: r3
       68k emulation: A0 (emulator reads it)
   - Document BootInfo struct layout in include/Platform/bootinfo.h.
   Estimated: 2–4 hours.

3) PRAM persistence handoff (kernel-side)
   - Provide a small example kernel stub showing how to read BootInfo and write PRAM via UEFI RuntimeServices->SetVariable prior to ExitBootServices. The loader will only read PRAM; kernel handles write-back on shutdown.
   Estimated: 1–2 hours.

4) Kernel/emulator consumer
   - Integrate a BootInfo consumer in the kernel/emulator to map the toolkit region and expose trap/Gestalt dispatch to legacy apps.
   - Implement minimal PRAM accessors and register the toolkit base for trap table lookups.
   Estimated: variable; initial example stub 1–2 hours; fuller integration more.

5) Tests & validation
   - Add QEMU smoke tests and unit tests for BootInfo parsing and toolkit mapping.
   - Attempt to run a small legacy test program in the emulator against the clean-room toolkit to validate basic Gestalt/trap behavior.

Notes, constraints, and caveats
- No Apple proprietary code: toolkits must be clean-room; do NOT add any Apple ROMs to the repo. Placeholders are included for development; replace with your own lawful binaries if you have them.
- Endianness: PPC and 68k toolkits are BE. x86_64, aarch64, riscv64 toolkits are LE.
- PRAM variable name: LazarusOS:PRAM (UEFI variable). The loader reads it (if present); kernel writes it on shutdown.
- BootInfo ABI: prefer register-based pointer handoff (clean per-ABI). If you need a fixed address, we can change later.

Useful quick checklist for Deepseek
- [ ] Add LE toolkit sources + build rules
- [ ] Extend scripts to build LE blobs and include them in ESP image
- [ ] Implement BootInfo struct header and fill/hand off in efi_loader.c
- [ ] Add small kernel example demonstrating PRAM write via SetVariable
- [ ] Run smoke test in QEMU and verify loader output

Troubleshooting tips
- If the loader doesn't find toolkits, check the FAT image contents (mount or unzip esp_contents/ before image creation).
- Cross-compilers: building PPC/68k toolkits requires powerpc/68k cross-toolchains; scripts will produce zero-filled placeholders if compilers are absent.
- EFI runtime variables may be restricted by the environment; in QEMU+OVMF SetVariable should work by default.

If you want, I will now:
- push this HANDOFF file to efi-rework (done),
- implement step #1 (LE toolkits) and #2 (BootInfo wiring) immediately and open a PR, or
- wait and let Deepseek pick up the tasks.

Contact & context
- I pushed the earlier placeholder commits (toolkits + scripts) on branch efi-rework. Deepseek should start from that branch and follow the checklist above.

