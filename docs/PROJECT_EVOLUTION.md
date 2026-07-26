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

❌ **The discipline relaxed**
- Evidence ledgers no longer maintained for every function
- Some "reasonable implementations" added without binary proof
- Code added faster than provenance tracking could keep up
- Partial implementations left in-place hoping to complete later
- "TODO" comments accumulated faster than fixes

✅ **But the scope exploded**
- M68K interpreter with 84 opcode handlers (not in original scope)
- Segment loader for running actual Mac applications
- Sound Manager with MIDI synthesis
- Dialog Manager with keyboard navigation
- Text editing with clipboard support
- SimpleText application (working MDI editor)
- HFS file system with virtual folder windows
- Multi-language support (37 languages!)
- Time Manager with microsecond precision
- Device Manager with driver support
- Control Manager with scrollbars

### Why It Got "Messy"

1. **Research mindset → Development mindset**
   - Research: "Prove one thing rigorously"
   - Development: "Make it work end-to-end"
   - These are different games with different rules

2. **Speed over documentation**
   - Early research had extensive evidence comments
   - Now: Just make the next feature work
   - Comments often say WHAT, not WHY

3. **Ambition outpaced rigor**
   - Research: ~6,000 LOC, fully traced
   - Now: ~57,500 LOC, many paths partially explored
   - Some subsystems "stubbed but functional"

4. **"Good enough" became acceptable**
   - Rendering at 800×600 @ 32-bit instead of faithful era recreation
   - PC speaker synthesis instead of multi-channel audio mixing
   - Simplified HFS instead of full read-write support
   - M68K interpreter instead of full JIT

5. **Real-world constraints**
   - No team, just one person's free time
   - Trade-off: "Ship it working" vs. "Make it perfect"
   - Ship won, and that's OK

---

## The Current State: A Productive Mess

What you have now is **not** the research prototype anymore. It's:

### ✅ What Actually Works Well
- **94% core system functionality** (by our best estimate, not rigorous audit)
- Desktop rendering, menus, windows, dialogs
- File browsing and SimpleText editor
- Bootable, runnable, *actually usable* OS
- Multi-language support that genuinely works
- Real applications can load and theoretically run

### ⚠️ What's Partially Done
- M68K execution framework exists, needs testing on real apps
- Sound Manager has command processing but limited mixing
- Speech Manager API-complete but synthesis stubbed
- Many controls/dialogs exist but edge cases untested
- HFS file system works but read-only

### ❌ What's Intentionally Missing
- Printing (never started)
- Networking (out of scope)
- TrueType fonts (bitmap-only)
- Stability guarantees (crashes happen)
- Complete test coverage

### 🤔 The Honest Assessment
- **Proof of concept?** ✅ Yes—proves AI-assisted reverse engineering works
- **Production-ready?** ❌ No—explicitly not stable
- **Educational?** ✅✅ Absolutely—most complete System 7 source ever assembled
- **Usable?** Surprisingly, yes—SimpleText works, desktop is responsive, visually impressive
- **Maintainable?** Getting harder as complexity grows without documentation

---

## Why This Evolution Matters

### For Research
The initial discipline showed **how** to reverse-engineer systematically with AI. That methodology is published. The current state shows **what happens next**—what does a reconstructed OS look like when you keep building?

### For Preservation
We now have:
- 225+ source files documenting System 7 internals
- Extracted resources (fonts, patterns, icons) in modern formats
- A bootable, interactive System 7 that runs code
- A living reference implementation

This isn't just a paper artifact anymore. It's a working system that teaches through exploration.

### For Learning
- Study the architecture by reading real code
- Understand constraints of the 1990s through working implementations
- See how classic Mac OS solved problems without modern abstractions
- Experiment with modifications and see results in QEMU

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

| Phase | Goal | Method | Status | Focus |
|-------|------|--------|--------|-------|
| **Research** (3 days) | Prove AI-assisted RE works | Disciplined, evidence-first | ✅ Complete | Methodology |
| **Experiment** (ongoing) | Build a working System 7 | Rapid development, feature-driven | 🔧 In progress | Capability |
| **Legacy** (future?) | Preserve Mac OS history | Community-driven? | ❓ TBD | Impact |

We're in Phase 2. We have no idea when Phase 3 starts. But we're still learning what System 7 *can be* in code.

That's the honest story. That's why it got messy. And that's why it still matters.
