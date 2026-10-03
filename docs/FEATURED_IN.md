# Featured In

System 7 was featured by Action Retro, showcasing the project's approach to OS reverse engineering and testing it on real hardware.

> Historical snapshot: the problems below describe the revision tested in the video, not the current repository state. Later changes addressed the reported boot, interrupt, and input problems. Physical verification remains limited to one UEFI ThinkPad; broader hardware compatibility and real-application compatibility are unverified. See [Known Issues](KNOWN_ISSUES.md) and [Bare Metal](BARE_METAL_IMPROVEMENTS.md) for current limitations.

## Action Retro — "The World's Most Cursed Operating System"

**Video:** https://www.youtube.com/watch?v=rJRlHKQqX2M

Action Retro tested System 7 on real hardware and documented the results.

> *"It's more cursed than ReactOS. It's more cursed than Hannah Montana Linux."*

> *"This is literally the AI sloperating system."*

> *"No freaking way. This abomination is booting."*

### Findings on the Tested Revision

**What Worked:**
- System 7 boots from USB on real hardware
- Successfully boots on:
  - Dell Pentium 3 system
  - ThinkPad X1 Carbon (Lenovo)
  - MacBook Air 11-inch (Intel)
- Desktop graphics render correctly
- Menu bar appears as expected
- Grub bootloader loads the kernel

**What Didn't Work in That Revision:**
- System freezes after boot on most hardware
- Mouse input not responding
- Keyboard input not responding
- Applications crash when launched
- Only partially functional even when it does boot

### Key Takeaways from the Historical Test

The testing revealed issues in the revision shown in the video:

> "This is legitimately so hard to use... It works at all is just absolutely insane... It boots on real hardware and it was mostly AI's hallucination of Mac OS 7.1."

**The Testing Found:**
1. **QEMU testing had missed real-hardware problems** - The tested revision behaved differently outside emulation
2. **Input handling was broken** - Keyboard and mouse input did not respond
3. **Timing/interrupt problems caused freezes** - Investigation traced the behavior to interrupt handling
4. **Hardware detection was incomplete** - Several subsystems depended on QEMU-specific behavior

## Media Coverage

- **Format:** YouTube feature/review
- **Audience:** Technical enthusiasts, OS hobbyists, retro computing enthusiasts
- **Tone:** "Cursed but fascinating" - impressed by what works, honest about what doesn't
- **Focus:** Real hardware testing, comparison to other OS reimplementations (ReactOS, Hannah Montana Linux)

## Related Documentation

- [PROJECT_EVOLUTION.md](PROJECT_EVOLUTION.md) - Historical retrospective on the project's development phases
- [BARE_METAL_IMPROVEMENTS.md](BARE_METAL_IMPROVEMENTS.md) - Current bare-metal status and remaining work
- [KNOWN_ISSUES.md](KNOWN_ISSUES.md) - Detailed list of known problems

## Contributing Based on YouTube Testing

If you watched the YouTube feature and want to contribute:

1. **Start here:** [BARE_METAL_IMPROVEMENTS.md](BARE_METAL_IMPROVEMENTS.md)
2. **Test safely:** See [CLAUDE.md](../CLAUDE.md) for development setup
3. **Report results:** Document what hardware you test and what works/breaks

The video remains useful as a historical test report. Use the current issue and compatibility documents above to identify remaining work.
