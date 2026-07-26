# Bare Metal Implementation TODO

Specific tasks to improve bare metal (real hardware) support.

## CRITICAL - System Won't Run

### Serial I/O
- [ ] **Detect serial port availability** 
  - Current: Hardcoded to 0x3F8 (COM1)
  - Issue: Fails silently if port doesn't exist
  - Fix: Probe multiple ports, verify responsiveness
  - Files: `src/Platform/x86/io.c`, `include/Platform/include/io.h`

- [ ] **Add serial port diagnostics**
  - Print: "Testing serial at 0x3F8..."
  - Print: "Serial OK" or "Serial FAILED"
  - Help: Can't debug without knowing serial works
  - Files: Early boot code, `platform_boot.S`

### Interrupt Handling
- [ ] **Verify IDT installation**
  - Add: Code to check CPU is using our IDT
  - Method: Read IDTR register, compare with expected
  - Files: `src/Platform/x86/idt.c`

- [ ] **Test exception handlers**
  - Implement: Intentional divide-by-zero to verify handler works
  - Implement: Intentional page fault to verify handler works
  - Document: What should happen vs. what does happen
  - Files: `src/Platform/x86/idt.c`, exception handlers

- [ ] **Fix context preservation in interrupt handlers**
  - Issue: CPU state might not be properly saved/restored
  - Check: Stack frame layout in assembly handlers
  - Test: Verify variables survive interrupts
  - Files: `src/Platform/x86/idt.S`

### Timer Interrupt
- [ ] **Configure PIT (Programmable Interval Timer)**
  - Status: `pit.c` exists but might not work on real hardware
  - Verify: PIT controller responds to initialization
  - Set: 100Hz timer (10ms ticks)
  - Files: `src/Platform/x86/pit.c`

- [ ] **Implement IRQ0 (Timer) handler**
  - Create: Function to handle timer ticks
  - Do: Increment global timer counter
  - Test: Verify ticks increment on serial output
  - Files: Main interrupt handler, `idt.c`

- [ ] **Add system clock/sleep functions**
  - Implement: `delay_ms()` based on timer
  - Implement: `get_ticks()` for current time
  - Test: Verify timing accuracy
  - Files: `src/Platform/x86/pit.c` or new file

## HIGH PRIORITY - System Unusable Without These

### Keyboard Input
- [ ] **Detect PS/2 keyboard presence**
  - Issue: Assumes keyboard exists
  - Fix: Try to read from keyboard controller, handle timeout
  - Method: Probe port 0x60 for responsiveness
  - Files: `src/Platform/x86/ps2.c`

- [ ] **Implement interrupt-driven keyboard**
  - Current: Probably polling (busy loop)
  - Change: Use IRQ1 interrupt instead
  - Files: IRQ1 handler, `ps2.c`

- [ ] **Test on real hardware**
  - Use: USB-to-PS/2 adapter on modern systems
  - Document: What keyboard layout is assumed
  - Fix: Handle keyboards without PS/2
  - Files: Keyboard driver

### Video/Framebuffer
- [ ] **Verify framebuffer is accessible**
  - Current: Assumes 0xA0000 (VGA text buffer) or VESA address
  - Issue: Address might be different on real hardware
  - Fix: Detect via BIOS or use bootloader-provided address
  - Files: Graphics initialization

- [ ] **Test framebuffer writes**
  - Write: Pattern to framebuffer
  - Verify: Pattern appears on screen
  - Handle: If framebuffer not writable
  - Files: Graphics driver

- [ ] **Fallback video modes**
  - If: VESA not available
  - Use: VGA text mode (80x25)
  - Document: What video modes are supported
  - Files: Video initialization

## MEDIUM PRIORITY - Advanced Features

### Interrupt Routing
- [ ] **PIC (8259A) initialization**
  - Verify: Master and slave PICs are configured
  - Test: Each IRQ fires correctly
  - Files: `src/Platform/x86/pic.c`

- [ ] **Handle spurious interrupts**
  - Issue: Some systems generate spurious IRQ7/IRQ15
  - Fix: Detect and ignore safely
  - Files: IRQ7/IRQ15 handlers

- [ ] **APIC support (future)**
  - For: Modern multi-core systems
  - Status: Not yet implemented
  - Priority: Lower (for now)
  - Files: New - `apic.c`

### Storage (Dangerous - Don't Test Without Serial Debugging)
- [ ] **Detect ATA/IDE controllers**
  - Via: PCI enumeration
  - Issue: Currently hardcoded or missing
  - Files: `src/Platform/x86/ata.c`

- [ ] **ATA IDENTIFY command**
  - Send: IDENTIFY to device
  - Parse: Response to get drive info
  - Handle: Timeout if no drive present
  - **DANGER**: Only with serial debugging
  - Files: `ata.c`

- [ ] **ATA read with timeout**
  - Read: Single sector
  - Timeout: After reasonable interval
  - Verify: Data is correct
  - **DANGER**: Only with serial debugging
  - Files: `ata.c`

### Memory Management
- [ ] **Verify physical memory layout**
  - At: Boot time
  - Check: Memory size from Multiboot info
  - Bounds: All allocations within bounds
  - Files: Memory initialization

- [ ] **Handle fragmented memory**
  - Current: Assumes flat layout
  - Improve: Track free regions better
  - Handle: Allocation failures gracefully
  - Files: `src/MemoryMgr/MemoryManager.c`

## LOW PRIORITY - Ambitious

### Advanced Hardware Support
- [ ] **USB driver** (currently stub)
  - Status: `uhci.c`, `xhci.c`, `ehci.c` exist but incomplete
  - Work: Implement actual USB support
  - Priority: Lower

- [ ] **Network driver** (currently stub)
  - Status: `e1000.c` minimal
  - Work: Implement network initialization
  - Priority: Lower

- [ ] **Sound output**
  - Current: PC speaker only
  - Issue: Untested on real hardware
  - Priority: Very low

## Testing Checklist

### Before Running on Real Hardware

- [ ] Serial debugging works in QEMU
- [ ] All timer operations work in QEMU
- [ ] Keyboard input works in QEMU
- [ ] Graphics render correctly in QEMU
- [ ] No obvious QEMU-specific code paths

### When Running on Real Hardware

- [ ] Serial output appears (at 115200 baud)
- [ ] Boot messages print correctly
- [ ] No immediate crashes
- [ ] Timer ticks appear on serial
- [ ] Keyboard input registers on serial
- [ ] Graphics appear on screen (if applicable)

### Document Findings

- [ ] Hardware configuration
- [ ] Serial output log
- [ ] What worked
- [ ] What failed
- [ ] Suggested fixes

## File Organization

### Key Files to Modify

**Hardware Initialization:**
- `src/Platform/x86/platform_boot.S` - Assembly boot code
- `src/Platform/x86/hal_boot.c` - HAL initialization
- `src/main.c` - Main kernel entry

**Interrupts:**
- `src/Platform/x86/idt.c` - IDT setup
- `src/Platform/x86/idt.S` - Exception handlers
- `src/Platform/x86/pic.c` - PIC setup

**Devices:**
- `src/Platform/x86/ps2.c` - Keyboard/mouse
- `src/Platform/x86/pit.c` - Timer
- `src/Platform/x86/io.c` - Serial I/O

**Memory/Platform:**
- `src/Platform/x86/pci.c` - PCI enumeration
- `src/MemoryMgr/MemoryManager.c` - Memory management

## Success Milestones

1. **Boots to serial prompt** (Week 1-2)
   - Serial output works
   - No exceptions
   - Timer ticks

2. **User interaction** (Week 3-4)
   - Keyboard works
   - Can type on screen
   - Desktop renders

3. **File access** (Week 5-6)
   - Can read files from disk
   - File system works
   - Boot from real disk

4. **Stability** (Ongoing)
   - No crashes under normal use
   - Handles errors gracefully
   - Works on various hardware

## Notes for Contributors

- **DANGER**: Storage code can corrupt disks - test only on USB/non-valuable media
- **SAFETY**: Always run with serial debugging enabled
- **DOCUMENTATION**: Test results should be documented in detail
- **INCREMENTAL**: Test one thing at a time, verify each step
- **HARDWARE**: Different machines might have different behaviors

---

**Last Updated**: July 2026  
**Priority Level**: CRITICAL - System doesn't work on real hardware  
**Time Estimate**: 4-6 weeks for solid bare metal support
