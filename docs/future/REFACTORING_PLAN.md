### **Phase 1: Core Codebase Refactoring for Portability**

> **Status: executed (verified 2026-10-01).** `src/Platform/include/`
> (`boot.h`, `io.h`, `storage.h`, `input.h`, `serial.h`, `network.h`) and
> `src/Platform/x86/` (`io.c`, `ata.c`, `ps2.c`, `hal_boot.c`) all exist, as
> does `src/boot.c`. Paths in this plan refer to the *target* layout and are
> accurate.

This is the most critical phase, establishing the foundation for all future porting. The primary objective is to abstract all platform-specific code into a well-defined Hardware Abstraction Layer (HAL), making the core OS platform-agnostic.

**Module 1.1: Establish the HAL Directory Structure and API**

1.  **Create HAL Directory Structure:**
    *   Create `src/Platform/include` for the public HAL API headers.
    *   Create `src/Platform/x86` for the existing x86-specific implementation.

2.  **Define Core HAL APIs:**
    *   Create `src/Platform/include/boot.h`: Defines the interface for the boot process, including initialization and hand-off to the kernel.
    *   Create `src/Platform/include/io.h`: Defines the interface for low-level I/O operations (e.g., `hal_inb`, `hal_outb`).
    *   Create `src/Platform/include/storage.h`: Defines a generic block storage interface (e.g., `hal_storage_read_sectors`, `hal_storage_write_sectors`).
    *   Create `src/Platform/include/input.h`: Defines a generic input device interface (e.g., `hal_input_poll`, `hal_input_get_mouse`).

**Module 1.2: Abstract the Boot Process**

1.  **Isolate Platform-Independent Boot Logic:**
    *   Create `src/boot.c` to house the platform-independent boot logic. This will call a new `hal_boot_init()` function to handle platform-specific setup.

2.  **Move x86-Specific Boot Code:**
    *   Move the contents of `src/multiboot2.S` to `src/Platform/x86/boot.S`.

3.  **Update Build System:**
    *   Modify the `Makefile` to compile `src/boot.c` and the platform-specific boot file based on the selected `PLATFORM`.

**Module 1.3: Abstract I/O Operations**

1.  **Implement the x86 I/O HAL:**
    *   Move the inline assembly functions from `src/Platform/x86_io.c` to `src/Platform/x86/io.c`.
    *   Update `src/Platform/x86/io.c` to implement the functions defined in `src/Platform/include/io.h`.

2.  **Refactor Core Code:**
    *   Replace all direct calls to `inb`, `outb`, etc., throughout the codebase with calls to the new HAL functions (`hal_inb`, `hal_outb`).

**Module 1.4: Abstract the Storage System**

1.  **Implement the x86 Storage HAL:**
    *   Move the ATA driver code from `src/Platform/ATA_Driver.c` to `src/Platform/x86/ata.c`.
    *   Modify the ATA driver to implement the generic storage interface defined in `src/Platform/include/storage.h`.

2.  **Refactor Core Code:**
    *   Replace all direct calls to the ATA driver with calls to the new HAL storage functions.

**Module 1.5: Abstract the Input System**

1.  **Implement the x86 Input HAL:**
    *   Move the PS/2 driver code from `src/PS2Controller.c` to `src/Platform/x86/ps2.c`.
    *   Modify the PS/2 driver to implement the generic input interface defined in `src/Platform/include/input.h`.

2.  **Refactor Core Code:**
    *   Replace all direct calls to the PS/2 driver with calls to the new HAL input functions.

**Module 1.6: Update and Generalize the Build System**

1.  **Introduce `PLATFORM` Variable:**
    *   Add a `PLATFORM` variable to the `Makefile` to allow selection of the target platform (e.g., `make PLATFORM=x86`).

2.  **Conditional Compilation:**
    *   Use the `PLATFORM` variable to conditionally include the correct platform-specific source files and linker script.

3.  **Abstract Toolchain:**
    *   Define toolchain variables (e.g., `CC`, `LD`) based on the selected `PLATFORM`.

With the completion of Phase 1, the core OS will be decoupled from the x86 architecture, making it significantly easier to port to new platforms. The next phase would be to implement a new HAL for a different platform, such as the Raspberry Pi.

> **Note:** the earlier revision of this plan referred to `src/System/Kernel/…`,
> `src/StringMgr/`, and `src/SerialManager/`. None of those paths exist; the
> Memory Manager lives in `src/MemoryMgr/`, and `printf`/`snprintf` live in
> `src/System71StdLib.c`. Don't go looking for files that aren't there.
