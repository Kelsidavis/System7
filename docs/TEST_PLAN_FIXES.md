# Test Plan: Validating the Fixes

This document explains how to test the hardware interrupt fixes and report results.

## Quick Test (QEMU) - 5 minutes

```bash
make clean && make run
```

**Expected Output:**
```
KERNEL: Enabling hardware interrupts...
KERNEL: Interrupts ENABLED (IRQ-driven mode)
Platform: PS/2 IRQs unmasked (IRQ1, IRQ12)
IRQ0: 1000 ticks received
IRQ0: 2000 ticks received
IRQ0: 3000 ticks received
```

**If you see the above:** ✅ Fix #1 is working
- Timer interrupt is firing
- System is getting timer ticks
- IRQ0 handler is executing

**Next step:** Type on keyboard in QEMU window

**Expected Output on Serial:**
```
PS/2 IRQ1: 50 events received
PS/2 IRQ1: 100 events received
```

**If you see the above:** ✅ Fix #2 is working
- Keyboard interrupt is firing
- System is getting input events
- IRQ1 handler is executing

---

## Real Hardware Test - 10 minutes

### Prerequisites
- USB bootable System 7 ISO (download from GitHub releases)
- USB-to-Serial adapter (FTDI or similar)
- Real x86 machine (Pentium 3 or newer)
- Terminal program (minicom, screen, putty)
- 115200 baud serial connection

### Step 1: Prepare Serial Cable
1. Connect USB-to-Serial adapter to USB port
2. Connect serial cable to motherboard COM port
3. Connect to terminal at 115200 baud
4. Power on machine

### Step 2: Boot System 7
1. Insert USB with System 7 ISO
2. Boot from USB (F12, then select USB device)
3. Should see GRUB menu
4. Press Enter to boot

### Step 3: Watch Serial Output
```
GRUB 2.06
GNU GRUB version 2.06
...
KERNEL: Enabling hardware interrupts...
KERNEL: Interrupts ENABLED (IRQ-driven mode)
Platform: PS/2 IRQs unmasked (IRQ1, IRQ12)
IRQ0: 1000 ticks received
IRQ0: 2000 ticks received
```

**If you see this:** ✅ Timer interrupt is working on real hardware

### Step 4: Test Keyboard Input
1. Press any key on keyboard
2. Watch serial output for:
```
PS/2 IRQ1: 50 events received
```

**If you see this:** ✅ Keyboard interrupt is working

### Step 5: Observe System
- Does desktop stay visible and responsive?
- Does mouse move? (might not work yet, that's OK)
- Does system stay running or freeze?

---

## Test Results Template

Use this to report your findings:

```
## Test Results - [Your Name/Hardware]

### Hardware
- CPU: [Model, e.g., Pentium 3, Core 2 Duo]
- RAM: [Amount]
- Machine: [Model, e.g., Dell Dimension, ThinkPad]
- Motherboard: [If known]

### QEMU Test
- [ ] Compiled successfully
- [ ] Boots in QEMU
- [ ] Timer messages appear ("IRQ0: XXXX ticks")
- [ ] Keyboard input detected ("PS/2 IRQ1: XX events")

### Real Hardware Test (if attempted)
- [ ] USB boots successfully
- [ ] GRUB menu appears
- [ ] System 7 starts loading
- [ ] Timer messages appear on serial
- [ ] System stays responsive (doesn't freeze)
- [ ] Keyboard input detected on serial
- [ ] Desktop appears responsive

### Issues Encountered
[Describe any problems]

### Serial Output Log
[Paste relevant output]

### Recommendations
[Suggest next steps]
```

---

## What Success Looks Like

### Fix #1 (Timer Interrupt)
**Problem:** System freezes immediately after boot
**Success Indicator:** System stays responsive, timer messages appear every ~1 second
**How to verify:** Boot in QEMU, watch serial for "IRQ0: XXXX ticks"

### Fix #2 (PS/2 Input)
**Problem:** No keyboard/mouse response
**Success Indicator:** Serial shows "PS/2 IRQ1: XX events" when you type
**How to verify:** Press key in QEMU, watch serial for event messages

### Overall Success
If both fixes work:
- System boots and stays responsive (doesn't freeze)
- Keyboard input generates events
- System might still have issues but it's no longer completely broken

---

## Common Issues & Fixes

### "No timer messages after boot"
- Timer interrupt might not be working
- Check: Is PIT initialized? (src/Platform/x86/pit.c)
- Check: Is IDT installed correctly? (src/Platform/x86/idt.c)
- Check: Are interrupts actually enabled? (look for "sti" in log)

### "System still freezes"
- Timer interrupt might be firing but not enough
- Check: Are messages appearing spaced 1 second apart?
- If not: Timing might be wrong

### "No PS/2 events even when typing"
- PS/2 interrupts might still be masked
- Check: Do you see "PS/2 IRQs unmasked" message?
- Try: Type different keys, move mouse

### "System boots but nothing visible"
- Video output might be broken (separate issue)
- Check: Do you see serial messages?
- That means system is running, just video is broken

---

## Success Stories Welcome!

If you successfully test these fixes:

1. Report your hardware configuration
2. Paste your serial output
3. Document what worked/didn't work
4. Create a GitHub issue with your results
5. Help debug any remaining issues

Your real hardware testing is invaluable for improving System 7.

---

## Next Phase After These Fixes

Once timer and input work, we can tackle:

1. **68K Interpreter Wiring** (Issue #3)
   - Enable running actual Mac applications
   - Estimated: 3-4 weeks

2. **Storage I/O**
   - Read from real hard drives
   - Write functionality (risky!)
   - Estimated: 2-3 weeks

3. **Additional Drivers**
   - USB support (currently stubbed)
   - Network support (currently stubbed)
   - Estimated: 1-2 months each

---

## Contributing Improvements

If you find issues:

1. Create a GitHub issue
2. Include your test results
3. Describe exact problems
4. Paste serial output
5. Suggest fixes if possible

Every hardware configuration tested helps improve the project.

---

**Your testing helps make System 7 actually run on real hardware.** 🚀
