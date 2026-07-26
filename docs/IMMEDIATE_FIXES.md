# Immediate Fixes - YouTube Testing Results

Based on the YouTuber's real hardware testing, here are the critical issues and fixes needed.

## Issue #1: System Freezes After Boot (CRITICAL)

### Root Cause
System 7 is running in **polling mode, not interrupt-driven mode**.

**Current code (src/main.c:1781-1782):**
```c
/* Keep IRQs disabled for now; polling paths are stable. */
serial_puts("KERNEL: IRQs disabled (polling mode)\n");
PS2_SetIRQDriven(false);
```

**What this means:**
- Timer interrupt handler IS registered (hal_boot.c:61)
- BUT interrupts are globally DISABLED
- System manually calls `TimeManager_TimerISR()` in event loop
- If event loop blocks or gets stuck → system freezes

**Why it freezes on real hardware:**
- In QEMU: Timing is loose, loops complete quickly
- On real hardware: `GetNextEvent()` at line 1795 blocks waiting for input
- With interrupts disabled, nothing can interrupt this block
- System is now stuck forever

### Fix

**Step 1: Enable Hardware Interrupts (1-2 hours)**

Replace polling mode with real interrupt-driven mode:

```c
// In main.c around line 1780-1782, change:
#if defined(__i386__) || defined(__x86_64__)
    // ENABLE interrupts (don't keep them disabled)
    serial_puts("KERNEL: Enabling hardware interrupts\n");
    PS2_SetIRQDriven(true);  // Changed from false!
    asm volatile("sti");      // Enable interrupts globally
#endif
```

**Step 2: Remove Manual Timer Polling (1 hour)**

Change the event loop to NOT manually call TimerISR:

```c
// In main.c around line 1787, change:
while (1) {
    // REMOVE this line (timer interrupt will handle it):
    // TimeManager_TimerISR();  // DELETE THIS
    
    // Keep the rest
    TimeManager_DrainDeferred(16, 1000);
    platform_network_poll();
    
    // Rest of event loop...
}
```

**Step 3: Verify Timer Interrupt Works (2-3 hours)**

Test that IRQ0 is firing:

```c
// Add debug output in irq_timer_handler (hal_boot.c:39-42)
static volatile uint32_t g_irq0_count = 0;

static void irq_timer_handler(uint8_t irq) {
    (void)irq;
    g_irq0_ticks++;
    g_irq0_count++;
    
    // Print every 100 ticks (should happen ~10x per second at 1000Hz)
    if ((g_irq0_count % 100) == 0) {
        serial_printf("IRQ0: %u ticks\n", g_irq0_ticks);
    }
}
```

### Testing
1. Compile with interrupt-driven mode
2. Boot in QEMU - should see "IRQ0: 100 ticks" messages
3. Boot on real hardware - should see the same, not freeze

**Expected outcome:** System no longer freezes; continues responding even when waiting for input

---

## Issue #2: No Mouse/Keyboard Input (HIGH PRIORITY)

### Root Cause
PS/2 controller driver is probably working, BUT:
1. IRQ1 (keyboard) and IRQ12 (mouse) are masked (line 64-65 in hal_boot.c)
2. System never gets input events

### Current Code (hal_boot.c:64-65)
```c
pic_mask_irq(1);   // Keyboard disabled
pic_mask_irq(12);  // Mouse disabled
```

### Fix (1-2 hours)

**Step 1: Unmask PS/2 IRQs**

```c
// In hal_boot.c, change:
pic_mask_irq(1);   // Keyboard - REMOVE THIS LINE
pic_mask_irq(12);  // Mouse - REMOVE THIS LINE

// Add unmask instead:
pic_unmask_irq(1);   // Enable keyboard interrupts
pic_unmask_irq(12);  // Enable mouse interrupts
```

**Step 2: Verify IRQ1/12 Handlers**

Check that handlers exist in idt.c:

```c
// These should already exist:
irq_register_handler(1, irq_ps2_handler);   // Line 62
irq_register_handler(12, irq_ps2_handler);  // Line 63
```

**Step 3: Debug PS/2 Interrupts**

Add logging to irq_ps2_handler:

```c
static volatile uint32_t g_ps2_irq_count = 0;

static void irq_ps2_handler(uint8_t irq) {
    g_ps2_irq_count++;
    if ((g_ps2_irq_count % 10) == 0) {
        serial_printf("PS/2 IRQ%d: %u events\n", irq, g_ps2_irq_count);
    }
    PollPS2Input();
}
```

### Testing
1. Compile with PS/2 IRQs unmasked
2. Boot in QEMU - type on keyboard, move mouse
3. Should see "PS/2 IRQ1: 10 events" etc. on serial
4. Boot on real hardware - test keyboard/mouse

**Expected outcome:** Keyboard/mouse input works

---

## Issue #3: 68K Interpreter Not Wired Up (HIGH PRIORITY)

### Current State
- Interpreter exists: `include/CPU/M68KInterp.h`
- Opcode handlers exist: `include/CPU/M68KOpcodes.h`
- BUT: Never integrated into application execution

### Root Cause
Applications load as x86 code via native C reimplementations (SimpleText, MacPaint).
Real 68K Mac applications need the interpreter wired into:
1. Segment loader (load 68K code)
2. Trap dispatcher (convert Mac OS traps to our implementations)
3. Exception handler (handle 68K exceptions)

### Fix (2-3 weeks)

**Phase 1: Wire Up Interpreter (3-4 days)**

1. Create 68K execution entry point:
```c
// New file: src/CPU/M68KExec.c
int execute_68k_code(void *code_ptr, size_t code_len, void *initial_a7, void *initial_pc) {
    // Initialize CPU state
    // Load initial registers
    // Execute interpreter loop
    // Handle traps and exceptions
    // Return exit code
}
```

2. Connect to segment loader (src/SegmentLoader/SegmentLoader.c):
```c
// When loading a 68K segment, call:
result = execute_68k_code(segment_data, segment_size, stack_ptr, entry_point);
```

3. Implement trap dispatcher:
```c
// When 68K code hits a TRAP instruction:
// Parse trap number
// Look up our implementation
// Call it and return result
```

**Phase 2: Implement Core Traps (1-2 weeks)**

Start with most-used traps:
- File I/O (Open, Close, Read, Write)
- Memory (NewPtr, DisposePtr, BlockMove)
- Event Manager (GetNextEvent, WaitNextEvent)
- Window Manager (CreateWindow, DisposeWindow)
- Menu Manager (CreateMenu, GetMenuHandle)

**Phase 3: Test with Real Apps (3-4 days)**

Try loading classic Mac apps and executing them.

### Why This is Hard
- 68K has different calling conventions than x86
- Stack frames need conversion
- Traps need stubbing for ~200+ routines
- Requires careful testing

### Simpler Alternative (1 week)
Build "wrapper" apps in 68K assembly that call x86 reimplementations:
1. Write wrapper.68k that calls GetNextEvent, etc.
2. Implement thin stubs that copy args to x86 calling convention
3. Call x86 function
4. Copy result back
5. Return

This would allow using existing x86 implementations from 68K code.

---

## Priority Order (What to Fix First)

### CRITICAL (Fixes the freeze)
1. ✅ Enable hardware timer interrupt
2. ✅ Remove manual timer polling
3. ✅ Test on real hardware

**Time: 4-5 hours**
**Impact: HUGE - system no longer freezes**

### HIGH (Fixes input)
4. ✅ Unmask PS/2 interrupts (IRQ1, IRQ12)
5. ✅ Test keyboard/mouse

**Time: 2 hours**
**Impact: User can interact with system**

### HIGH (Fixes applications)
6. Wire up 68K interpreter (3-4 days)
7. Implement trap dispatcher
8. Start with file I/O traps

**Time: 3-4 weeks**
**Impact: Can run real Mac applications**

---

## Testing Strategy

### For Bare Metal (Critical)

1. **Add Serial Debugging Everywhere**
   - Every interrupt handler logs to serial
   - Every state change logs to serial
   - Enables debugging without display

2. **Test on Real Hardware Progressively**
   - Boot and verify serial output
   - Verify timer ticks appear
   - Verify keyboard input appears
   - Verify mouse movement appears

3. **Document What Works**
   - Hardware tested
   - Exact configuration
   - What worked/broke
   - Serial output log

### For 68K Interpreter

1. **Unit Tests First**
   - Test individual opcode handlers
   - Test trap dispatching
   - Test register state preservation

2. **Integration Test**
   - Load small 68K program
   - Execute in interpreter
   - Verify results

3. **Real App Test**
   - Try ResEdit
   - Try HyperCard
   - Document failures

---

## Code Files to Modify

### Bare Metal Fixes (High Priority)

| File | Changes | Time |
|------|---------|------|
| src/main.c | Enable interrupts, remove polling | 1 hour |
| src/Platform/x86/hal_boot.c | Unmask PS/2 IRQs | 30 min |
| src/Platform/x86/idt.c | Add debug output | 1 hour |
| tests/test_interrupts.c | Create interrupt tests | 2 hours |

### 68K Interpreter (Major Work)

| File | Changes | Time |
|------|---------|------|
| src/CPU/M68KExec.c | Create execution entry point | 2 days |
| src/SegmentLoader/SegmentLoader.c | Call M68K executor | 1 day |
| src/CPU/M68KTraps.c | Implement trap dispatcher | 3 days |
| src/CPU/M68KTrapStubs.c | Stub all traps | 1-2 weeks |

---

## Estimated Impact

### Fix 1: Timer Interrupt
- Before: System freezes immediately
- After: System stays responsive
- Users can see: Desktop remains responsive

### Fix 2: PS/2 Input
- Before: No input possible
- After: Keyboard/mouse work
- Users can see: Can click, type, navigate

### Fix 3: 68K Interpreter
- Before: Only x86-native apps work
- After: Real Mac applications run
- Users can see: Full compatibility with classic apps

---

## Next Steps

1. **Immediate (This Week):**
   - [ ] Enable hardware timer interrupt
   - [ ] Unmask PS/2 IRQs
   - [ ] Test on QEMU
   - [ ] Test on real hardware

2. **This Month:**
   - [ ] Debug remaining freeze issues
   - [ ] Get full keyboard/mouse working
   - [ ] Document test results

3. **This Quarter:**
   - [ ] Start 68K interpreter integration
   - [ ] Implement core traps
   - [ ] Test with real applications

---

**The YouTuber's feedback is gold.** Real hardware testing revealed exactly what needs to be fixed. These fixes would transform the system from "freezes immediately" to "actually usable."
