### **Porting System 7 to Other Platforms: A Comprehensive Plan**

> **Status: partly done (October 2026).** `src/Platform/` has `x86/`, `arm/`
> (Raspberry Pi), `arm64/` (QEMU `virt` and Raspberry Pi) and `ppc/` trees.
> x86 runs fully; arm64 builds and starts under QEMU but stops while creating
> the boot volume; `arm/` and `ppc/` are earlier. The 68K and PowerPC
> interpreters live in `src/CPU/`.

This document outlines a plan for porting the System 7 reimplementation to other platforms, including ARM (Raspberry Pi, Apple Silicon) and PowerPC.

**1. High-Level Strategy: Abstraction and Modularization**

The key to a successful multi-platform port is to abstract away all platform-specific code into a well-defined Hardware Abstraction Layer (HAL). This will allow the vast majority of the System 7 codebase (the "core") to remain platform-independent.

The HAL will be organized into a new directory structure:

```
src/
├── Platform/
│   ├── x86/
│   │   ├── boot.S
│   │   ├── io.c
│   │   ├── ata.c
│   │   ├── ps2.c
│   │   └── ...
│   ├── arm/
│   │   ├── rpi/
│   │   │   ├── boot.S
│   │   │   ├── gpio.c
│   │   │   ├── framebuffer.c
│   │   │   ├── sd.c
│   │   │   └── ...
│   │   └── apple_silicon/
│   │       ├── ...
│   └── ppc/
│       ├── ...
└── ...
```

The build system will be updated to select the appropriate platform-specific HAL at compile time.

**2. Core Code Modifications**

Before porting to any new platform, the following changes need to be made to the core codebase to improve portability:

*   **Remove all inline assembly:** All inline assembly should be moved into the HAL.
*   **Replace direct hardware access with HAL calls:** All direct hardware access (e.g., `inb`, `outb`) should be replaced with calls to HAL functions.
*   **Abstract the boot process:** The boot process should be abstracted to allow for different bootloaders and boot methods on different platforms.
*   **Abstract the storage system:** The storage system should be abstracted to allow for different storage devices (e.g., SD card, NVMe) on different platforms.
*   **Abstract the input system:** The input system should be abstracted to allow for different input devices (e.g., USB, I2C) on different platforms.

**3. Porting to ARM (Raspberry Pi)**

The Raspberry Pi is a good first target for porting, as it is a well-documented and widely available platform.

*   **Bootloader:**
    *   Create a new boot script for the Raspberry Pi's bootloader. This will involve creating a `config.txt` file and a minimal bootloader that can load the kernel.
    *   The bootloader will need to set up the framebuffer and pass a pointer to it to the kernel.
*   **Hardware Abstraction Layer (HAL):**
    *   **`boot.S`:** Create a new `boot.S` file for the ARM architecture. This will be responsible for setting up the initial stack, initializing the BSS section, and calling the kernel's `main` function.
    *   **`gpio.c`:** Create a new `gpio.c` file to provide access to the Raspberry Pi's GPIO pins. This will be used for serial I/O and other purposes.
    *   **`framebuffer.c`:** Create a new `framebuffer.c` file to provide access to the Raspberry Pi's framebuffer.
    *   **`sd.c`:** Create a new `sd.c` file to provide access to the Raspberry Pi's SD card controller. This will replace the ATA driver.
    *   **`usb.c`:** Create a new `usb.c` file to provide support for USB keyboards and mice. This will replace the PS/2 driver.
*   **Compiler Toolchain:**
    *   Use the `arm-none-eabi-gcc` cross-compiler to compile the kernel.
*   **Linker Script:**
    *   Create a new linker script for the ARM architecture. This will need to be tailored to the Raspberry Pi's memory layout.
*   **Build System:**
    *   Update the `Makefile` to support the ARM architecture. This will involve adding a new `PLATFORM` variable and using it to select the appropriate compiler, linker script, and HAL.

**4. Porting to ARM (Apple Silicon)**

Porting to Apple Silicon will be a significant challenge due to the lack of public documentation.

*   **Bootloader:**
    *   Research how to load a custom kernel on Apple Silicon. This will likely involve using a custom bootloader like Asahi Linux's `m1n1`.
*   **Hardware Abstraction Layer (HAL):**
    *   This will be the most difficult part of the port. It will require reverse-engineering the Apple Silicon hardware to write drivers for the framebuffer, storage, and input devices.
*   **Compiler Toolchain:**
    *   Use the `aarch64-none-elf-gcc` cross-compiler to compile the kernel.
*   **Linker Script:**
    *   Create a new linker script for the AArch64 architecture.
*   **Build System:**
    *   Update the `Makefile` to support the AArch64 architecture.

**5. Porting to PowerPC**

Porting to PowerPC will be similar to porting to ARM, but with different hardware and a different bootloader.

*   **Bootloader:**
    *   Create a bootloader that can be loaded from Open Firmware.
*   **Hardware Abstraction Layer (HAL):**
    *   Write a new HAL for the PowerPC platform. This will involve writing drivers for the specific hardware found in PowerPC-based Macs.
*   **Compiler Toolchain:**
    *   Use a `powerpc-linux-gnu` cross-compiler toolchain (current makefile default) to build ELF32 PPC binaries.
*   **Linker Script:**
    *   Maintain a PPC-specific linker script (initial skeleton lives in `src/Platform/ppc/linker.ld`).
*   **Build System:**
    *   Update the `Makefile` to support the PowerPC architecture. (Completed: `PLATFORM=ppc` selects the new HAL, toolchain, and flags.)
*   **Boot Strap:**
    *   Ensure the PPC entry stub clears `.bss`, aligns the stack, and forwards Open Firmware arguments into `boot_main` for later HAL consumption. (Completed in the scaffold.)
*   **Firmware Console:**
    *   Use the Open Firmware client interface (`Platform/ppc/open_firmware.c`) to discover `stdout` and plumb early serial logging through the firmware console. (Initial implementation landed.)
*   **Firmware Memory Map:**
    *   Read `/memory` `reg` data from Open Firmware to seed the HAL’s reported RAM size instead of relying on a placeholder. (Initial support implemented; HAL now caches multiple ranges for inspection.)
*   **Framebuffer Discovery:**
    *   Attempt to read display node properties (`address`, `width`, `height`, `linebytes`, `AAPL,boot-display`) from the firmware console path to pre-populate framebuffer info. (Primitive probe added; falls back gracefully when console is serial-only.)

**6. Timeline and Milestones**

This is a large and complex project, and it will take a significant amount of time and effort to complete. Here is a rough timeline with milestones:

*   **Phase 1: Core Code Refactoring (1-2 months)**
    *   Abstract the HAL.
    *   Remove all platform-specific code from the core codebase.
*   **Phase 2: Raspberry Pi Port (2-3 months)**
    *   Implement the Raspberry Pi HAL.
    *   Get the system to boot and display the desktop on the Raspberry Pi.
*   **Phase 3: PowerPC Port (3-4 months)**
    *   Implement the PowerPC HAL.
    *   Get the system to boot and display the desktop on a PowerPC-based Mac.
*   **Phase 4: Apple Silicon Port (6-12 months)**
    *   Reverse-engineer the Apple Silicon hardware.
    *   Implement the Apple Silicon HAL.
    *   Get the system to boot and display the desktop on an Apple Silicon Mac.

This plan is ambitious, but it is achievable with a dedicated effort. By following this plan, we can port the System 7 reimplementation to a wide range of platforms and ensure its long-term viability.
