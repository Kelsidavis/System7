# Cleanup and migration notes for efi-rework branch

This branch (efi-rework) is the start of the migration from a GRUB/Multiboot2
boot model to a UEFI/EFI-based boot model, and a platform cleanup to focus on
64-bit, EFI-capable architectures.

What has been done in this commit set (cleanups are incremental):
- Replaced the active grub.cfg with a note indicating GRUB is removed on this branch.
- Converted qemu run scripts to call qemu-system-x86_64 with OVMF (UEFI) firmware.
- Added this CLEANUP.md to summarise the planned migration steps in the branch.

Planned follow-up steps (next commits on this branch):
1) Remove multiboot-specific boot sources or move them to a legacy area.
2) Add an EFI loader (small UEFI PE/COFF stub) and build rules to produce BOOTX64.EFI,
   BOOTAA64.EFI, and (best-effort) BOOTRISCV64.EFI.
3) Remove 32-bit platform build targets and scripts (i386, arm, riscv32).
4) Remove PowerPC platform sources and references entirely (per your instruction).
5) Update Makefile and CI to build and test x86_64, aarch64, and riscv64 only.
6) Update README and Getting Started with EFI build/run instructions.

Notes about deletion of PowerPC sources:
- You chose to delete PowerPC sources. To avoid accidental data loss during
  the migration, I will remove them in a dedicated commit so the change is
  reviewable and reversible via Git history if necessary. Expect that commit
  next.

Security & tooling:
- EFI binaries created here will be unsigned — Secure Boot will need to be
  disabled on test hardware or VMs.
- Later we can integrate GNU-EFI or EDK2 if you want a more full-featured UEFI
  build chain; for now the plan is a minimal custom EFI stub to get booting.

If anything above needs to be adjusted (for example you change your mind about
keeping PPC in legacy/, or you want EDK2 integration immediately), tell me and
I will adapt the plan.
