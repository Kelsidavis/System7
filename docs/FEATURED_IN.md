# Featured In

System 7 has been featured in major tech media, showcasing the project's unique approach to OS reverse engineering and its real-world capabilities.

## Action Retro — "The World's Most Cursed Operating System"

**Video:** https://www.youtube.com/watch?v=rJRlHKQqX2M

Action Retro tested System 7 on real hardware and documented the results.

> *"It's more cursed than ReactOS. It's more cursed than Hannah Montana Linux."*

> *"This is literally the AI sloperating system."*

> *"No freaking way. This abomination is booting."*

### Video Highlights

**What Worked:**
- System 7 boots from USB on real hardware
- Successfully boots on:
  - Dell Pentium 3 system
  - ThinkPad X1 Carbon (Lenovo)
  - MacBook Air 11-inch (Intel)
- Desktop graphics render correctly
- Menu bar appears as expected
- Grub bootloader loads the kernel

**What Didn't Work (Yet):**
- System freezes after boot on most hardware
- Mouse input not responding
- Keyboard input not responding
- Applications crash when launched
- Only partially functional even when it does boot

### Key Takeaways from Testing

The YouTuber's testing revealed exactly what our documentation states:

> "This is legitimately so hard to use... It works at all is just absolutely insane... It boots on real hardware and it was mostly AI's hallucination of Mac OS 7.1."

**The Testing Confirmed:**
1. **QEMU-only testing was masking real problems** - System looked good in emulation but fails on real hardware
2. **Input handling is broken** - No keyboard/mouse response
3. **Timing/interrupt issues** - System freezes, suggesting timer interrupt problems
4. **Bare metal hardware detection missing** - Serial I/O and other subsystems QEMU-specific

### Impact

This feature brought significant attention to the project and validated our honest assessment:
- ✅ The project IS sloppy and experimental
- ✅ QEMU-only testing IS insufficient
- ✅ Bare metal support IS the critical blocker
- ✅ Documentation IS necessary (which is why we have it)

### What This Means

**For New Users:** You're now aware the project works partially on real hardware but needs significant bare metal improvements.

**For Contributors:** Real hardware testing data is now available showing exactly where work is needed. See [BARE_METAL_IMPROVEMENTS.md](BARE_METAL_IMPROVEMENTS.md) and [BARE_METAL_TODO.md](BARE_METAL_TODO.md).

**For the Project:** This visibility created an opportunity to be honest about limitations and transparent about the roadmap, which is exactly what our documentation provides.

## Media Coverage

- **Format:** YouTube feature/review
- **Audience:** Technical enthusiasts, OS hobbyists, retro computing enthusiasts
- **Tone:** "Cursed but fascinating" - impressed by what works, honest about what doesn't
- **Focus:** Real hardware testing, comparison to other OS reimplementations (ReactOS, Hannah Montana Linux)

## Related Documentation

- [PROJECT_EVOLUTION.md](PROJECT_EVOLUTION.md) - Why the project is "sloppy"
- [BARE_METAL_IMPROVEMENTS.md](BARE_METAL_IMPROVEMENTS.md) - Roadmap to fix the issues
- [BARE_METAL_TODO.md](BARE_METAL_TODO.md) - Specific tasks for bare metal support
- [KNOWN_ISSUES.md](KNOWN_ISSUES.md) - Detailed list of known problems

## Contributing Based on YouTube Testing

If you watched the YouTube feature and want to help fix the bare metal issues:

1. **Start here:** [BARE_METAL_IMPROVEMENTS.md](BARE_METAL_IMPROVEMENTS.md)
2. **Pick a task:** [BARE_METAL_TODO.md](BARE_METAL_TODO.md)
3. **Test safely:** See [CLAUDE.md](../CLAUDE.md) for development setup
4. **Report results:** Document what hardware you test and what works/breaks

The real hardware testing data from the YouTube feature is incredibly valuable for fixing these issues.

---

**This project is now in a unique position:** It's honest about its limitations, has clear documentation of problems, and has real-world testing data showing exactly what needs to be fixed.

That's something most experimental OS projects don't have.
