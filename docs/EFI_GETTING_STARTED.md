# EFI Quick Start (branch: efi-rework)

This is a short, temporary getting-started guide for the efi-rework branch.
The repo is being migrated to EFI boot images and 64-bit-only targets. Use
this guide to run the tree once the EFI image is available.

Prerequisites (development machine):
- GCC/clang for building the kernel for the target architecture(s)
- QEMU with support for the required architectures: qemu-system-x86_64,
  qemu-system-aarch64, qemu-system-riscv64 (as applicable)
- OVMF (UEFI firmware for QEMU) installed (package usually named 'ovmf')

Testing with QEMU (x86_64 example):
1) Build the kernel for x86_64 (TBD: new Makefile entries will be added on this branch).
2) Create an EFI FAT image with an EFI payload at EFI/BOOT/BOOTX64.EFI.
   Example (local testing):

   # Create an empty 32M image and format as FAT
   dd if=/dev/zero of=efiboot.img bs=1M count=32
   mkfs.vfat efiboot.img

   # Mount and copy EFI payload (requires root)
   mkdir -p /mnt/efi
   sudo mount -o loop efiboot.img /mnt/efi
   sudo mkdir -p /mnt/efi/EFI/BOOT
   sudo cp BOOTX64.EFI /mnt/efi/EFI/BOOT/
   sudo umount /mnt/efi

3) Boot with QEMU + OVMF:
   qemu-system-x86_64 -bios /usr/share/ovmf/OVMF_CODE.fd -drive file=efiboot.img,format=raw -m 1024 -serial stdio

Notes:
- Secure Boot: unsigned EFI apps will not load with Secure Boot enabled. Disable
  Secure Boot in the VM/firmware for testing.
- Kernel integration: the tree currently expects multiboot entry points in a
  few locations; I will add a compatibility shim so the kernel can be launched
  from the EFI loader. That work will be added in a follow-up commit.
