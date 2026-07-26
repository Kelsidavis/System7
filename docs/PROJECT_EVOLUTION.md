# System 7: From Research Paper to Living Experiment

## The Beginning: A Disciplined Research Sprint (2025)

This project started with a clear, bounded mission: **Can we use AI-assisted reverse engineering to reconstruct a bootable System 7 prototype in days rather than months?**

The answer was yes. In a focused 3-day sprint, we:

- Curated evidence from 68k binaries using Ghidra/radare2
- Recovered structs, resource formats (PPAT, NFNT, ICON), and font metrics
- Implemented Window/Menu/Event/QuickDraw subsystems with full provenance tracking
- Achieved a bootable desktop with rendering visible in QEMU
- Documented the methodology in a formal research paper (Zenodo)

**Key principle then:** Every line of code was traceable to binary evidence. No speculation. No "creative fill." Agent accountability with strict audits. Partial coverage was intentional—only what we could justify with binary evidence.

## The Inflection Point: "What If We Just... Kept Going?"

After the research concluded, we faced a choice:

> Archive the prototype as a research artifact? Or continue building?

We chose to continue. And everything changed.

---

## Phase 2: The Experiment Years (2025–Present)

The project shifted from **"Can we do this rigorously?"** to **"How far can we actually push System 7 compatibility?"**

### What Stayed

✅ **Foundational components built during research**
- Core boot sequence, hardware abstraction layer
- Graphics pipeline (QuickDraw, framebuffer, patterns)
- Window/Menu/Event managers (the scaffolding that works)
- Resource Manager with binary-accurate parsing
- Font rendering with Chicago bitmap support

### What Changed Dramatically

❌ **The discipline completely relaxed**
- Evidence ledgers abandoned
- "Reasonable implementations" added without any binary verification
- Code added way faster than testing or understanding
- Partial implementations left half-done, hoping to complete "later"
- "TODO: fix this" and "HACK:" comments everywhere
- Testing was: "does it boot in QEMU?" not "does it actually work?"

❓ **Scope exploded (but much of it is rough)**
- M68K interpreter with 84 opcode handlers → mostly untested
- Segment loader for Mac applications → can load, probably breaks on execution
- Sound Manager → MIDI conversion works, actual audio output is iffy
- Dialog Manager → exists, but keyboard handling is incomplete
- Text editing → SimpleText works *in QEMU*, probably breaks elsewhere
- SimpleText application → functional in emulation, edge cases unknown
- HFS file system → works in QEMU, real performance/robustness unknown
- Multi-language support → 37 languages, all tested only in emulation
- Time Manager → has microsecond precision in QEMU abstractions
- Device Manager → driver framework exists, actual drivers are skeletal
- Control Manager → scrollbars render, might not work correctly

**The honest version**: Got excited about capabilities and built a lot. Most of it works in the emulator. Most of it is untested elsewhere.

### Why It Got "Sloppy"

1. **QEMU made it too easy**
   - QEMU abstracts hardware → easy to ignore real hardware issues
   - Serial logging works → no need to debug real video output
   - Emulated interrupts → no need to handle real interrupt timing
   - Emulated mouse/keyboard → untested on real devices
   - Result: "It works in QEMU" became the only test

2. **One person, no code review**
   - No one to say "wait, this is incomplete"
   - No pressure to finish things properly
   - Can hack and move on to the next feature
   - Technical debt accumulated silently

3. **Excitement beat discipline**
   - "Hey, we can make an M68K interpreter!" → built it
   - "Hey, we can add multi-language support!" → added 37 languages
   - "Hey, we can implement Sound Manager!" → started it
   - No one saying "is this actually working or just rendering?"

4. **Testing was minimal**
   - Early research: QEMU boots and tests passed
   - Later: if it boots, ship it
   - Edge cases? "We'll fix that later"
   - Bare metal? "Untested, probably broken"

5. **The distance between QEMU and reality**
   - QEMU hides so much complexity
   - Real hardware interrupt timing? Unknown
   - Real device I/O? Not tested
   - Real memory pressure? Never happens in emulation
   - Result: Code that looks like it works but might not

---

## The Current State: A Sloppy But Functional Operating System

What you have now is **not** the research prototype anymore. It's more honest to call it:

**A proof-of-concept that works in QEMU but is rough around the edges everywhere.**

### ✅ What Actually Works (In QEMU)
- Desktop rendering, menus, windows, dialogs *in the emulator*
- File browsing and SimpleText editor *in QEMU*
- Boots to a usable desktop *under emulation*
- Multi-language support works *during QEMU sessions*
- Real 68K applications can theoretically load
- **Reality check**: ~94% sounds polished. It's not. It's *functional* in QEMU. Bare metal? Very different story.

### ⚠️ What's Partially Done & Rough
- **M68K execution**: Framework exists, but untested on real apps. Probably breaks.
- **Sound Manager**: Has MIDI conversion, but no real mixing or audio output on bare metal.
- **Speech Manager**: API skeleton, synthesis is stubbed. Does nothing.
- **Controls/Dialogs**: Exist, but edge cases crash constantly. Keyboard handling is incomplete.
- **HFS file system**: Works in QEMU, read-only, likely has bugs under stress.
- **Hardware abstraction**: Designed for QEMU. Bare metal support is minimal/missing.
- **Device drivers**: DCE framework exists but actual driver support is skeletal.
- **Exception handling**: Partially implemented. What happens when something crashes? Unpredictable.

### ❌ What's Actually Broken or Missing
- **Bare metal hardware support**: Most testing was QEMU. Real hardware? Unknown.
- **Real device I/O**: PS/2 keyboard/mouse work in QEMU. Real hardware? Untested.
- **Memory management**: Zone-based allocation, but tested only in the emulator.
- **VESA framebuffer**: Works in QEMU. Real graphics card? Might not.
- **Interrupt handling**: QEMU abstracts this. Real hardware interrupts? Unknown.
- **Printing**: Never started
- **Networking**: Out of scope
- **TrueType fonts**: Bitmap-only
- **Stability guarantees**: Crashes are frequent and expected

### 🤔 The Honest Assessment
- **Proof of concept?** ✅ Yes—proves AI-assisted reverse engineering can work
- **Production-ready?** ❌❌ Hell no. It's a QEMU toy.
- **Educational?** ✅✅ Yes—most complete System 7 source code + can study how it works in emulation
- **Actually usable for retro computing?** ❌ Not really. SimpleText works in QEMU, but that's not the same as a real system.
- **Will it run on real Mac hardware?** Unknown. Probably not without significant work.
- **Maintainable?** Getting harder. Code grew faster than understanding.

---

## Why This Evolution Matters (Even Though It's Sloppy)

### For Research
The initial discipline showed **how** to reverse-engineer systematically with AI. That methodology is published and valuable. The current state shows **what happens when you stop being disciplined**—which is also valuable as a cautionary tale.

### For Understanding System 7
We now have:
- 225+ source files documenting System 7 internals (even if incomplete/rough)
- Extracted resources (fonts, patterns, icons) that are historically accurate
- A running System 7 in QEMU showing how it actually behaves
- Proof that you can build a recognizable OS from reverse engineering

This is the closest thing to "open source System 7" that exists. It's not perfect, but it's real.

### For Education
- Read the code to understand classic Mac architecture
- See what a 1990s OS actually needed (not what we imagined)
- Study mistakes: how QEMU-only testing created blind spots
- Learn why bare metal testing is non-negotiable
- Observe how one person's project becomes unmaintainable at scale

### For Learning What NOT to Do
- Don't let emulation be your only test environment
- Don't add features faster than you understand them
- Don't trust "works in emulation" without real hardware validation
- Don't accumulate technical debt without documenting it

---

## The Messy Experiment Phase: What It Means

### Why "Messy" Isn't Shameful Here

1. **Trade-offs were intentional**
   - Chose rapid feature coverage over exhaustive documentation
   - Chose "ships with it" over "perfect reference"
   - Chose collaborative experimentation over solo research rigor

2. **The mess has value**
   - Shows real-world complexity of large legacy systems
   - Demonstrates where partial implementations start to struggle
   - Reveals which subsystems need each other
   - Honest about limitations rather than hiding them

3. **It's still better documented than the original**
   - 57K LOC with *some* structure beats a black box
   - Component guides exist even if not 100% current
   - You can read and modify the source
   - Original ROM source remains lost

### What "Messy" Means Practically

```
Research Phase: "Every line is justified. Prove it works."
Experiment Phase: "Does it work? Let's make it better."
```

You'll see:
- Functions marked with "TODO: fix this properly"
- Comments that say "HACK:" followed by a workaround
- Subsystems that work but don't handle edge cases
- Code added without matching documentation updates
- Partial implementations coexisting with complete ones

This is **not** a bug in the project—it's a feature of where it is in its journey.

---

## Looking Forward: Honest Next Steps

### If You Want to Contribute
**Know what you're signing up for:**
- Not a production OS. Crashes are normal.
- Not exhaustively documented. Read the code.
- Not fully tested. You'll find edge cases.
- Actively being modified. Expect rebasing.

**Where to start:**
- Pick a component in `docs/components/` that interests you
- Read the source in `include/` and `src/`
- Try to use the feature in QEMU
- Find the edge cases
- Document what you find
- Submit improvements

### If You Want to Understand the Design
**The honest way:**
1. Read `zenodo_paper_markdown.md` first (this is the foundation)
2. Look at the research-phase code (early commits, clean with evidence comments)
3. Trace how it diverged into the experiment phase
4. Ask: "Why did they make *that* choice?"
5. Often the answer is "AI showed us this was possible, so we kept building"

### If You Want to Study Reverse Engineering
**This project shows both paths:**
- The **methodology** that worked: careful, evidenced, agent-driven
- The **reality**: you often want to keep building beyond the proof
- The **trade-off**: speed vs. documentation, ambition vs. correctness
- The **truth**: even "messy" work at scale is valuable

---

## The Relationship to the Zenodo Paper

That paper is the **research foundation**. It documents:
- ✅ How AI agents can guide reverse engineering
- ✅ Why resource-faithful recreation matters
- ✅ How to maintain provenance and prevent drift
- ✅ Reproducible methodology

This current codebase is the **living aftermath**. It shows:
- What you can build when you let go of some rigor
- How a prototype becomes an experiment
- What really matters vs. what's nice to have
- Where the methodology breaks down at scale

Both are true. Both are valuable.

---

## For New Contributors After the YouTube Boom

**You're joining at an interesting inflection point:**

- **Before:** Carefully constrained research project
- **Now:** Active experimental system with real capabilities
- **Next:** Could become more disciplined again, or continue as experiment

What it becomes depends on **why people care** and **what they want to build**.

If you're here because:

- **"I want to understand System 7"** → Start with docs/GETTING_STARTED.md, then dig into components
- **"I want to fix bugs"** → Check docs/KNOWN_ISSUES.md, it's honest about what's broken
- **"I want to run real Mac software"** → M68K interpreter is your target; it's partially there
- **"I want to study the reverse engineering"** → Read the Zenodo paper, then compare to current code
- **"I just think it's cool"** → Fire up `make run` and explore. Crash things. Learn.

All of these are valid.

---

## TL;DR: The Story

| Phase | Goal | Method | Reality | Status |
|-------|------|--------|---------|--------|
| **Research** (3 days) | Prove AI-assisted RE works | Disciplined, audited, evidence-first | ✅ Worked perfectly | Complete |
| **Experiment** (ongoing) | See how far we can push it | Build fast, test in QEMU only | ⚠️ Works in emulation, untested bare metal | Sloppy |
| **Legacy** (future?) | Preserve/educate about Mac OS | Documentation + community? | ❓ Too early to say | TBD |

**The honest progression:**
- Phase 1: "Can we do this carefully?" → Yes, and we proved it
- Phase 2: "What if we don't have to be so careful?" → More features, less testing, more problems
- Phase 3: "Do we fix this or keep building?" → Unknown

We're solidly in Phase 2. Got excited. Built too much. Tested too little. QEMU is not hardware. All the warnings applied.

**That's the real story.** And it matters because it's honest.
