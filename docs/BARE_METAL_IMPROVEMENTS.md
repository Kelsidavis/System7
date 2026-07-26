# Bare Metal Support Improvements

This document outlines a plan to improve System 7's functionality on real hardware (as opposed to QEMU-only).

## Current Reality

The project has been developed and tested exclusively in QEMU. While platform code exists for x86 (boot, interrupts, PIC, PS/2, ATA, etc.), most of it is either:
- Untested on real hardware
- QEMU-specific (assumes certain devices at fixed ports)
- Incomplete (just stubs)
- Broken under real hardware conditions

## Immediate Blockers for Bare Metal

### 1. Hardware Detection & Initialization
**Status**: QEMU-specific hardcoded addresses

**Issues**:
- Serial output hardcoded to `0x3F8` (COM1)
- VGA framebuffer base hardcoded to `0xA0000`
- No real video mode detection
- No PCI enumeration verification

**Needs**:
- Multiboot bootloader that provides hardware info
- Proper VESA BIOS detection/fallback
- Real serial port detection (check if port exists)
- PCI enumeration to find devices

**Priority**: CRITICAL - Can't debug without serial output

### 2. Interrupt & Exception Handling  
**Status**: Partially implemented, untested

**Issues**:
- IDT initialized but handlers might be incomplete
- PIC routing not verified on real hardware
- Exception handlers don't properly save/restore CPU state
- No APIC support (needed for modern x86)
- Timer interrupt missing or non-functional

**Needs**:
- Verify IDT installation (CPU should use our descriptors)
- Test PIC initialization on real hardware
- Add proper CPU context preservation in interrupt handlers
- Implement timer interrupt (IRQ0) for scheduling
- Test keyboard interrupt (IRQ1)
- Add error handling for spurious interrupts

**Priority**: CRITICAL - System can't function without this

### 3. Timing & Scheduling
**Status**: RDTSC-based but not functional

**Issues**:
- No timer interrupt means no preemptive scheduling
- RDTSC calibration untested on real hardware
- PIT might not be configured correctly
- No sleep/delay functionality

**Needs**:
- Configure PIT for regular timer interrupts
- Implement proper sleep/delay functions
- Test on real hardware to verify timing accuracy

**Priority**: HIGH - Needed for responsive system

### 4. Input (PS/2 Keyboard/Mouse)
**Status**: Extensive driver, likely QEMU-specific

**Issues**:
- Probably works in QEMU but not on real hardware
- Controller port (0x60/0x64) detection might fail
- No interrupt-driven input (probably polling)
- Keyboard controller might need different initialization
- Mouse detection and protocol handling untested

**Needs**:
- Verify PS/2 controller presence before using
- Implement interrupt-driven input (IRQ1 for keyboard, IRQ12 for mouse)
- Test on real hardware
- Add fallback for missing controllers

**Priority**: HIGH - User can't interact with system

### 5. Storage (ATA/IDE)
**Status**: Driver exists, completely untested

**Issues**:
- ATA driver exists but never tested on real hardware
- Likely assumes QEMU's disk behavior
- No DMA (all PIO, probably slow)
- Error handling probably incomplete
- Hangs likely if tried on real hardware

**Needs**:
- Never try without serial debugging setup
- Verify IDE controller detection
- Implement proper error handling
- Add timeouts for all operations
- Test with small disk operations first

**Priority**: MEDIUM - File system depends on this

### 6. Memory Management
**Status**: Zone allocator, untested on real hardware

**Issues**:
- Assumes flat physical memory
- Might not handle various memory layouts
- No page table management for paging
- Physical memory fragmentation not handled

**Needs**:
- Verify physical memory layout at boot
- Add bounds checking
- Test with different RAM configurations

**Priority**: MEDIUM - Memory corruption is hard to debug

## Improvement Roadmap

### Phase 1: Core System Stability (Weeks 1-2)

**Goal**: Get a bootable system that runs without crashing

**Tasks**:
1. [ ] Add comprehensive serial debugging output
   - Detect serial port presence
   - Test serial output with different ports
   - Add boot-time diagnostics

2. [ ] Verify interrupt handling
   - Add IDT installation verification
   - Trap into debugger on exceptions
   - Test with intentional divide-by-zero

3. [ ] Fix timer interrupt
   - Configure PIT for 100Hz interrupts
   - Implement IRQ0 handler
   - Add timer tick counter

4. [ ] Test on real hardware (if available)
   - Document hardware configuration
   - Capture serial output
   - Report any crashes/hangs

**Testing Approach**:
- Start with serial-only (no display)
- Use breakpoints to verify code paths
- Add panic/halt on errors
- Document each test result

### Phase 2: User Interaction (Weeks 3-4)

**Goal**: User can interact with system via keyboard

**Tasks**:
1. [ ] Verify PS/2 controller
   - Detect keyboard presence
   - Test keyboard interrupt (IRQ1)
   - Handle keyboard input

2. [ ] Add video output
   - Detect VESA support
   - Fall back to VGA if needed
   - Verify framebuffer is writable

3. [ ] Test desktop rendering
   - Render to real framebuffer
   - Handle video mode differences
   - Test on different GPUs

**Testing Approach**:
- Use serial debugging while testing video
- Test keyboard input with simple echo
- Verify framebuffer write-ability

### Phase 3: Persistent Storage (Weeks 5-6)

**Goal**: Read/write files from real disk

**Tasks**:
1. [ ] ATA/IDE initialization
   - Detect IDE controllers via PCI
   - Send IDENTIFY command to drive
   - Handle drive errors

2. [ ] Basic disk I/O
   - Read boot sector
   - Verify disk structure
   - Test with small transfers first

3. [ ] File system access
   - Mount HFS volume
   - Read boot blocks
   - Access files on disk

**Testing Approach**:
- Start with read-only operations
- Test with external USB drives first
- Use small test disk images
- Never test on valuable data

### Phase 4: Advanced Features (Ongoing)

**Goal**: Multi-processor, networking, advanced I/O

**Tasks**:
1. [ ] APIC support for modern systems
2. [ ] USB driver (currently stubs)
3. [ ] Network driver initialization
4. [ ] M68K code execution testing

## Key Principles

### 1. Serial Debugging First
Every hardware interaction needs serial output logging:
```c
serial_printf("Initializing PIC...\n");
pic_init();
serial_printf("PIC initialized\n");
```

### 2. Never Assume QEMU
Remove hardcoded addresses and assumptions:
- ❌ `uart_port = 0x3F8` (assumes COM1)
- ✅ Detect and verify serial port presence
- ❌ `framebuffer = 0xA0000` (assumes VGA at fixed address)
- ✅ Query BIOS/VESA for framebuffer location

### 3. Add Error Handling
Every operation should have timeout/error path:
```c
// BAD - just hangs if device missing
while (!(inb(0x60) & 0x01));  // Wait forever

// GOOD - timeout after reasonable wait
int timeout = 1000;  // milliseconds
while (!(inb(0x60) & 0x01) && timeout--) {
    delay_ms(1);
}
if (timeout <= 0) {
    serial_printf("ERROR: Keyboard timeout\n");
    return -1;
}
```

### 4. Verify Before Using
Check hardware existence before assuming:
```c
// Verify PIC is working
outb(0x20, 0x11);  // Initialize PIC
// Read back to verify it accepted command
if (!(can_read_from_pic())) {
    serial_printf("WARNING: PIC might not exist\n");
}
```

### 5. Document What Works
After testing on real hardware, document:
- Hardware tested
- What works/doesn't work
- Configuration requirements
- Known limitations

## Hardware for Testing

### Recommended Setup
1. **Older laptop** (2000s-era)
   - Easier to debug
   - Familiar hardware
   - IDE drives available
   - Serial port or USB adapter

2. **USB Live Boot**
   - Can test without affecting drive
   - Easy to recover from crashes
   - Can use modern hardware

3. **Virtual Machine (hypervisor)**
   - VirtualBox/VMware with real hardware pass-through
   - More controlled than bare metal
   - Can take snapshots

### What NOT to Test
- ❌ Storage access without serial debugging
- ❌ Advanced features before basic ones work
- ❌ DMA without proper error handling
- ❌ On valuable systems or drives

## Current Code Status

### Ready for Bare Metal
- Boot loader (platform_boot.S)
- Interrupt descriptor table (idt.c/S)
- Memory allocator (zone-based)
- Basic drivers (PIC, PIT, RTC)

### Needs Work
- PS/2 controller (extensive but untested)
- ATA driver (exists, never tested)
- Serial I/O (hardcoded ports)
- Video (assumes VESA works)
- Exception handling (incomplete)

### Stubs/Not Ready
- USB drivers (ehci.c, xhci.c, uhci.c)
- Network drivers (e1000.c, network.c)
- Advanced scheduling

## Getting Started

1. **Set up serial debugging**
   - Add comprehensive logging to all init code
   - Verify serial output works on test machine

2. **Test interrupt handling**
   - Boot from USB
   - Trigger intentional exceptions
   - Verify they're caught and logged

3. **Fix timer interrupt**
   - Implement PIT configuration
   - Verify timer ticks appear on serial
   - Test scheduling works

4. **Test keyboard input**
   - Verify PS/2 controller can be accessed
   - Implement IRQ1 handler
   - Echo keystrokes to serial

5. **Document everything**
   - What hardware was tested
   - What worked/failed
   - What needs fixing

## Resources

- **OSDev.org**: Comprehensive OS development guides
- **Intel SDM**: CPU and chipset documentation
- **ACPI Spec**: Hardware enumeration
- **Inside Macintosh**: Already referenced in code

## Success Criteria

**Phase 1**: System boots to serial prompt, no crashes
**Phase 2**: User can type on keyboard, see characters
**Phase 3**: Files can be read from disk
**Phase 4**: System is stable and usable

## Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Hangs on real hardware | Serial debugging, timeouts, safe defaults |
| Data corruption | Test on non-valuable hardware/USB only |
| Hardware incompatibility | Test on multiple machines, document findings |
| Interrupt conflicts | Verify IRQ routing, handle spurious interrupts |
| Forgotten QEMU-isms | Search code for hardcoded addresses, device paths |

---

**Next Step**: Start with serial debugging setup and boot-time diagnostics.
