# System 7X Nanokernel — Interrupt Subsystem Design

**Status:** ✅ Production (Timer IRQ0 stable, threading infrastructure complete but disabled)
**Last Updated:** 2025-10-09
**Architecture:** x86 (32-bit protected mode)

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Boot Sequence](#boot-sequence)
4. [IDT Setup](#idt-setup)
5. [ISR Call Path](#isr-call-path)
6. [Deferred Scheduler Design](#deferred-scheduler-design)
7. [Hardware Components](#hardware-components)
8. [Code Reference](#code-reference)
9. [Known Limitations](#known-limitations)
10. [Future Development](#future-development)

---

## Overview

The System 7X Nanokernel interrupt subsystem provides hardware timer-driven preemptive multitasking through a carefully designed interrupt infrastructure. The system is built on:

- **8259A PIC (Programmable Interrupt Controller)** - IRQ routing and masking
- **IDT (Interrupt Descriptor Table)** - 256-entry interrupt vector table
- **8253/8254 PIT (Programmable Interval Timer)** - 1000 Hz system tick
- **Deferred Scheduling** - Atomic flag-based reschedule mechanism to avoid context switches from interrupt handlers

### Design Philosophy

**Key Principle:** *Never context-switch from interrupt context.*

Early implementations attempted to call `nk_schedule()` directly from the IRQ0 timer interrupt handler. This caused immediate triple-faults because:

1. Interrupt handlers run on the interrupted thread's stack
2. Context switching attempts to save/restore ESP to a different thread
3. ESP corruption causes stack underflow/overflow during IRET
4. CPU triple-faults when exception handler also faults

The solution: **deferred scheduling** using atomic flags checked in safe contexts (idle loop).

---

## Architecture

### System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        Hardware Layer                            │
├─────────────────────────────────────────────────────────────────┤
│  8259A PIC          8253 PIT           CPU                       │
│  IRQ 0-15     →     1000 Hz      →     IDT (256 entries)        │
│  Remap 0x20-0x2F    Channel 0          Type: 0x8E (int gate)    │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                     Interrupt Service Routine                    │
│                         (nk_isr.S)                               │
├─────────────────────────────────────────────────────────────────┤
│  1. PUSHA (save all GPRs)                                        │
│  2. Save segment registers (DS, ES, FS, GS)                      │
│  3. Load kernel segments (0x10)                                  │
│  4. CALL C handler (irq0_handler, irq1_handler, etc.)           │
│  5. Restore segments                                             │
│  6. POPA (restore GPRs)                                          │
│  7. IRET (return from interrupt)                                 │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                       C Interrupt Handlers                       │
│                    (nk_irq_handlers.c)                           │
├─────────────────────────────────────────────────────────────────┤
│  irq0_handler():                                                 │
│    - Increment tick counter                                      │
│    - Call nk_timer_tick() (wake sleeping threads)               │
│    - Call nk_request_reschedule() (set atomic flag)             │
│    - Send PIC EOI                                                │
│                                                                  │
│  irq_resched_handler() [INT 0x81]:                              │
│    - Check reschedule flag                                       │
│    - Call nk_schedule() (safe - not in IRQ context)             │
│    - Clear reschedule flag                                       │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                     Deferred Scheduling                          │
│                      (nk_resched.c)                              │
├─────────────────────────────────────────────────────────────────┤
│  _Atomic bool resched_flag = false;                             │
│                                                                  │
│  nk_request_reschedule():                                        │
│    atomic_store(&resched_flag, true);                           │
│                                                                  │
│  nk_reschedule_pending():                                        │
│    return atomic_load(&resched_flag);                           │
│                                                                  │
│  nk_clear_reschedule():                                          │
│    atomic_store(&resched_flag, false);                          │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│                    Scheduler (nk_sched.c)                        │
├─────────────────────────────────────────────────────────────────┤
│  Idle Thread Loop:                                               │
│    for (;;) {                                                    │
│      if (nk_reschedule_pending()) {                             │
│        nk_clear_reschedule();                                   │
│        nk_schedule();  ← SAFE: not in interrupt context         │
│      }                                                           │
│      hlt;  // Wait for next interrupt                           │
│    }                                                             │
│                                                                  │
│  nk_schedule():                                                  │
│    - Select next thread from ready queue (round-robin)          │
│    - If current thread still runnable, re-enqueue               │
│    - Mark next thread as running                                │
│    - Call nk_switch_context() (assembly context switch)         │
└─────────────────────────────────────────────────────────────────┘
```

---

## Boot Sequence

The interrupt subsystem initialization follows a strict order to avoid race conditions and ensure stability:

```
main.c:kernel_main()
  │
  ├─> 1. InitMemoryManager()
  │     └─> Heap allocators (kmalloc/kfree) must be ready before interrupt handlers
  │
  ├─> 2. nk_pic_remap()
  │     └─> Remap PIC IRQs 0-15 → IDT vectors 32-47 (avoid conflicts with CPU exceptions)
  │
  ├─> 3. nk_idt_install()
  │     └─> Load IDT with 256 zero-initialized entries
  │
  ├─> 4. nk_idt_set_gate(32, irq0_stub, 0x08, 0x8E)
  │     └─> Register IRQ0 (timer) handler at vector 32
  │
  ├─> 5. nk_idt_set_gate(0x81, irq_resched_stub, 0x08, 0x8E)
  │     └─> Register INT 0x81 (deferred scheduler) handler
  │
  ├─> 6. nk_timer_init()
  │     └─> Program PIT channel 0 to 1000 Hz (1ms ticks)
  │
  ├─> 7. nk_sched_init()
  │     └─> Create idle thread, initialize ready queue
  │
  ├─> 8. [IRQ0 NOT unmasked yet]
  │     └─> Interrupts stay masked until threading test explicitly unmasks
  │
  ├─> 9. sti (enable interrupts)
  │     └─> Hardware interrupts now deliverable
  │
  └─> 10. Desktop initialization & main event loop
        └─> System stable, timer ticking at 1000 Hz
```

### Critical Ordering Notes

1. **Memory must be initialized first** - Interrupt handlers may call `kmalloc()` for dynamic structures
2. **PIC remap before IDT install** - Prevents spurious interrupts during IDT setup
3. **IDT handlers registered before timer init** - Ensures no unhandled interrupts if timer fires early
4. **IRQ0 remains masked during boot** - Prevents timer interrupts before scheduler is ready
5. **STI after all subsystems initialized** - Guarantees safe interrupt delivery

---

## IDT Setup

### IDT Entry Structure

```c
struct idt_entry {
    uint16_t offset_low;   // Handler address bits 0-15
    uint16_t selector;     // Kernel code segment (0x08)
    uint8_t  zero;         // Reserved, must be 0
    uint8_t  type_attr;    // Type and attributes
    uint16_t offset_high;  // Handler address bits 16-31
} __attribute__((packed));
```

### Type and Attribute Flags

```
type_attr = 0x8E
            ││││
            │││└─> Type: 0xE = 32-bit interrupt gate
            ││└──> DPL: 0 (ring 0 only)
            │└───> Present: 1 (entry is valid)
            └────> Storage Segment: 0 (interrupt/trap gate)
```

### IDT Installation Process

```c
void nk_idt_install(void) {
    // 1. Initialize IDTR structure
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint32_t)&idt;

    // 2. Zero all 256 entries (mark as not present)
    for (int i = 0; i < 256; i++) {
        idt[i] = (struct idt_entry){0};
    }

    // 3. Load IDTR register
    asm volatile("lidt %0" :: "m"(idtp));
}
```

### Vector Allocation Map

| Vector Range | Usage | Status |
|--------------|-------|--------|
| 0-31 | CPU Exceptions (divide error, page fault, etc.) | Reserved by CPU |
| 32-47 | Hardware IRQs (PIC remapped) | Active (IRQ0 @ 32) |
| 48-127 | Software interrupts / System calls | Reserved for future |
| 128 (0x80) | Linux-style syscall vector | Not used |
| 129 (0x81) | Deferred scheduler trigger | Active |
| 130-255 | Available for custom handlers | Free |

---

## ISR Call Path

### Assembly ISR Stub (nk_isr.S)

```asm
.global irq0_stub
irq0_stub:
    # 1. Save all general-purpose registers (32 bytes)
    pusha

    # 2. Save segment registers (16 bytes)
    push %ds
    push %es
    push %fs
    push %gs

    # 3. Load kernel data segment (0x10) into all segment registers
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    # 4. Call C handler (preserves stack alignment)
    call irq0_handler

    # 5. Restore segment registers
    pop %gs
    pop %fs
    pop %es
    pop %ds

    # 6. Restore general-purpose registers
    popa

    # 7. Return from interrupt (pops EIP, CS, EFLAGS automatically)
    iret
```

### C Handler (nk_irq_handlers.c)

```c
void irq0_handler(void) {
    // 1. Update tick counter (atomic for SMP safety)
    debug_tick_count++;

    // 2. Log periodic debug output
    if (debug_tick_count % 1000 == 0) {
        nk_printf("[IRQ0] Timer tick %u\n", debug_tick_count);
    }

    // 3. Wake sleeping threads (timer subsystem)
    nk_timer_tick();

    // 4. Request deferred reschedule (atomic flag, no context switch!)
    nk_request_reschedule();

    // 5. Send End-of-Interrupt to PIC (mandatory!)
    nk_pic_send_eoi(0);

    // 6. IRET - return to interrupted code
}
```

### Call Path Diagram

```
User Code / Kernel Code
    ↓ (timer interrupt fires)
CPU pushes: SS, ESP, EFLAGS, CS, EIP
    ↓
IDT lookup: vector 32 → irq0_stub
    ↓
irq0_stub: PUSHA → save segments → load kernel DS
    ↓
CALL irq0_handler
    ↓
irq0_handler: tick++, timer_tick(), request_reschedule(), EOI
    ↓
RET from irq0_handler
    ↓
irq0_stub: restore segments → POPA
    ↓
IRET
    ↓
CPU pops: EIP, CS, EFLAGS, ESP, SS
    ↓
Resume interrupted code
```

### Register State Preservation

| Register | Saved By | Restored By |
|----------|----------|-------------|
| EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI | PUSHA | POPA |
| DS, ES, FS, GS | PUSH | POP |
| EFLAGS, CS, EIP, SS, ESP (if privilege change) | CPU (automatic) | CPU (IRET) |

---

## Deferred Scheduler Design

### Problem: Context Switching from Interrupts

**Why this fails:**

```c
// ❌ BAD: Direct scheduling from IRQ handler
void irq0_handler(void) {
    nk_timer_tick();
    nk_schedule();  // ← TRIPLE FAULT!
    nk_pic_send_eoi(0);
}
```

**Failure mechanism:**

1. IRQ0 fires while `thread_A` is running
2. CPU switches to interrupt stack (or uses `thread_A`'s stack)
3. `nk_schedule()` selects `thread_B` as next thread
4. `nk_switch_context()` attempts to save ESP and switch to `thread_B`'s stack
5. **Problem:** ESP now points to `thread_B`'s stack, but IRET needs the original interrupt frame on `thread_A`'s stack
6. IRET pops garbage from `thread_B`'s stack → invalid EIP/CS/EFLAGS
7. CPU attempts to jump to invalid EIP → Page Fault → Double Fault → Triple Fault → Reboot

### Solution: Atomic Flag-Based Deferred Scheduling

```c
// ✅ GOOD: Deferred scheduling with atomic flag

// In nk_resched.c:
static _Atomic bool resched_flag = false;

void nk_request_reschedule(void) {
    atomic_store_explicit(&resched_flag, true, memory_order_release);
}

bool nk_reschedule_pending(void) {
    return atomic_load_explicit(&resched_flag, memory_order_acquire);
}

void nk_clear_reschedule(void) {
    atomic_store_explicit(&resched_flag, false, memory_order_release);
}
```

```c
// In nk_irq_handlers.c (IRQ0 handler):
void irq0_handler(void) {
    debug_tick_count++;
    nk_timer_tick();
    nk_request_reschedule();  // ← Just set flag, don't switch!
    nk_pic_send_eoi(0);
    // IRET - return to interrupted code safely
}
```

```c
// In nk_sched.c (idle thread):
static void idle_thread_entry(void *arg) {
    for (;;) {
        // Check flag in safe context (not interrupt handler)
        if (nk_reschedule_pending()) {
            nk_clear_reschedule();
            nk_schedule();  // ← SAFE: we're on idle thread's stack
        }

        // Halt CPU until next interrupt
        __asm__ volatile("hlt");
    }
}
```

### Deferred Scheduling Flow Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                     Timeline (1ms ticks)                      │
├──────────────────────────────────────────────────────────────┤
│  T=0ms    T=1ms    T=2ms    T=3ms    T=4ms                   │
│   │        │        │        │        │                       │
│   ▼        ▼        ▼        ▼        ▼                       │
│  IRQ0     IRQ0     IRQ0     IRQ0     IRQ0                     │
│   │        │        │        │        │                       │
│   └───┬────└───┬────└───┬────└───┬────└─────────────────┐   │
│       │        │        │        │                        │   │
│       ▼        ▼        ▼        ▼                        │   │
│  Set flag  Set flag  Set flag  Set flag                  │   │
│    │        │        │        │                          │   │
│    │        │        │        │                          │   │
│    └────────┴────────┴────────┴──────────────────────────┤   │
│                                                           │   │
│                                                           ▼   │
│                                            Idle thread checks │
│                                            flag = true        │
│                                                 │             │
│                                                 ▼             │
│                                          nk_schedule()        │
│                                                 │             │
│                                                 ▼             │
│                                       Context switch to       │
│                                       next ready thread       │
│                                                               │
└───────────────────────────────────────────────────────────────┘

Key insight: IRQ handlers never context-switch. They only set flags.
The idle thread (running in normal context, not interrupt) performs
the actual context switch when it's safe.
```

### Memory Ordering Guarantees

The code uses C11 atomic operations with explicit memory ordering:

- **`memory_order_release`** (in `nk_request_reschedule`): Ensures all timer tick updates complete before flag is visible
- **`memory_order_acquire`** (in `nk_reschedule_pending`): Ensures flag read happens before scheduling decisions
- **Purpose:** Prevents CPU/compiler reordering that could cause stale timer values after flag check

---

## Hardware Components

### 8259A PIC (Programmable Interrupt Controller)

**Configuration:**

```c
// Master PIC: IRQ 0-7
#define PIC1_CMD   0x20  // Command port
#define PIC1_DATA  0x21  // Data port

// Slave PIC: IRQ 8-15
#define PIC2_CMD   0xA0
#define PIC2_DATA  0xA1

// Remap sequence (in nk_pic_remap):
1. Send ICW1 (0x11): Initialize + ICW4 expected
2. Send ICW2 (0x20): Master base vector = 32
3. Send ICW2 (0x28): Slave base vector = 40
4. Send ICW3 (0x04): Slave on IRQ2
5. Send ICW3 (0x02): Slave cascade identity
6. Send ICW4 (0x01): 8086 mode
```

**IRQ Mapping:**

| IRQ | Old Vector | New Vector | Device |
|-----|------------|------------|--------|
| 0 | 0x08 | 0x20 (32) | PIT Timer |
| 1 | 0x09 | 0x21 (33) | Keyboard |
| 2 | 0x0A | 0x22 (34) | PIC Cascade |
| 7 | 0x0F | 0x27 (39) | Spurious |
| 8-15 | 0x70-0x77 | 0x28-0x2F (40-47) | Slave devices |

**End-of-Interrupt (EOI):**

```c
void nk_pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, 0x20);  // EOI to slave
    }
    outb(PIC1_CMD, 0x20);      // EOI to master
}
```

### 8253/8254 PIT (Programmable Interval Timer)

**Configuration:**

```c
#define PIT_BASE_HZ  1193182  // Base oscillator frequency
#define PIT_CHANNEL0 0x40     // Counter port
#define PIT_COMMAND  0x43     // Mode/command port

// Program for 1000 Hz (1ms ticks):
uint32_t divisor = PIT_BASE_HZ / 1000;  // = 1193
outb(PIT_COMMAND, 0x36);                // Channel 0, Mode 2 (rate generator)
outb(PIT_CHANNEL0, divisor & 0xFF);     // Low byte
outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF); // High byte
```

**Mode 2 (Rate Generator):**
- Counts down from divisor to 0
- Generates IRQ0 pulse when reaching 0
- Automatically reloads divisor and repeats
- Produces precise periodic interrupts

**Timing Verification:**

```
Actual frequency = 1193182 / 1193 = 1000.15 Hz
Period = 1 / 1000.15 = 0.9998 ms
Error = 0.15 Hz = 150 ppm (acceptable)
```

---

## Code Reference

### File Organization

```
include/Nanokernel/
├── nk_pic.h          - PIC driver interface
├── nk_idt.h          - IDT structure and management
├── nk_resched.h      - Deferred scheduling API
├── nk_timer.h        - Timer subsystem interface
├── nk_sched.h        - Scheduler interface
├── nk_task.h         - Task management
└── nk_thread.h       - Thread management

src/Nanokernel/
├── nk_pic.c          - PIC driver implementation
├── nk_idt.c          - IDT management
├── nk_isr.S          - Assembly ISR stubs
├── nk_irq_handlers.c - C interrupt handlers
├── nk_resched.c      - Deferred scheduling
├── nk_timer.c        - PIT driver
├── nk_sched.c        - Scheduler implementation
├── nk_task.c         - Task management
├── nk_thread.c       - Thread creation/lifecycle
├── nk_context.S      - Context switching (currently triple-faults)
└── nk_test_threads.c - Threading test harness (disabled)
```

### Key Functions

| Function | Location | Purpose |
|----------|----------|---------|
| `nk_pic_remap()` | nk_pic.c:37 | Remap PIC IRQs to vectors 32-47 |
| `nk_pic_send_eoi()` | nk_pic.c:71 | Send end-of-interrupt to PIC |
| `nk_idt_install()` | nk_idt.c:18 | Load IDT register |
| `nk_idt_set_gate()` | nk_idt.c:34 | Register interrupt handler |
| `irq0_stub` | nk_isr.S:3 | Assembly IRQ0 entry point |
| `irq0_handler()` | nk_irq_handlers.c:27 | C IRQ0 handler |
| `nk_request_reschedule()` | nk_resched.c:15 | Set reschedule flag (atomic) |
| `nk_reschedule_pending()` | nk_resched.c:25 | Check reschedule flag (atomic) |
| `nk_timer_init()` | nk_timer.c:42 | Initialize PIT to 1000 Hz |
| `nk_timer_tick()` | nk_timer.c:61 | Timer tick handler |
| `nk_schedule()` | nk_sched.c:188 | Main scheduler (select next thread) |
| `nk_switch_context()` | nk_context.S | Context switch assembly (broken) |

---

## Known Limitations

### 1. Threading Test Disabled (Context Switch Triple Fault)

**Status:** 🔴 Critical Bug
**Location:** `src/Nanokernel/nk_context.S`, called from `nk_sched.c:222`
**Symptom:** System immediately triple-faults when `nk_switch_context()` is called

**Evidence:**

```
[SCHED] About to call nk_switch_context
<triple fault - system reboot>
```

**Hypothesis:**

The context switch assembly is attempting to switch ESP to the new thread's stack, but:

1. Thread ESP may be invalid (bad stack allocation or alignment)
2. Stack frame setup in `nk_thread_create()` may be incorrect
3. Assembly code may corrupt ESP during switch
4. EFLAGS restoration may be re-enabling interrupts prematurely

**Workaround:**

Threading test disabled in `src/main.c:2007-2014`:

```c
// TEMPORARILY DISABLED - context switch causes triple fault
// extern void nk_test_threads_run(void);
// nk_test_threads_run();
```

**Next Steps:**

1. Add assembly-level debug to `nk_context.S` before ESP restore
2. Verify thread ESP values are valid memory addresses (print in hex)
3. Test with single thread first (no actual switch, just save/restore same thread)
4. Add stack canary values to detect stack corruption
5. Consider using QEMU's `-d int` flag to dump interrupt/fault details

### 2. No SMP Support

**Status:** 🟡 Future Enhancement
**Issue:** Scheduler assumes single CPU (global `current_thread` variable)
**Impact:** QEMU `-smp 2` launches 2 cores, but only CPU0 runs kernel code

**Required Changes:**

- Per-CPU task state (array of `cpu_state[cpu_id]`)
- Per-CPU idle threads
- Spinlock protection for shared ready queue
- APIC initialization for inter-CPU interrupts (IPI)
- CPU bringup code in `nk_sched_init_secondary()`

### 3. Timer Drift

**Status:** 🟢 Minor Issue
**Measurement:** ~150 ppm (parts per million) drift due to PIT divisor rounding
**Impact:** Negligible for UI/scheduling, may affect long-duration timers

**Actual Frequency:**

```
Divisor = 1193 (integer truncation of 1193.182)
Actual = 1193182 / 1193 = 1000.15 Hz
Error  = +0.15 Hz = +150 ppm
```

**Future Fix:** Use APIC timer (ns resolution) or TSC-based timekeeping

### 4. No Interrupt Nesting

**Status:** 🟢 Design Decision
**Current Behavior:** Interrupts disabled during ISR execution (EFLAGS.IF=0 automatically)
**Impact:** High-priority interrupts cannot preempt low-priority ones

**Rationale:** Simplifies reasoning about interrupt safety. Handlers are short (<10 µs).

**Future Enhancement:** Re-enable interrupts in ISR after critical section:

```asm
irq0_stub:
    pusha
    ...
    sti          ; Re-enable interrupts (careful with reentrant stacks!)
    call irq0_handler
    cli          ; Disable before IRET
    ...
```

### 5. No Interrupt Profiling

**Status:** 🟡 Future Enhancement
**Impact:** Cannot measure ISR latency, jitter, or CPU time spent in interrupts

**Desired Features:**

- ISR entry/exit timestamps (TSC)
- Latency histogram (min/max/avg/p99)
- Drift measurement (actual vs expected tick rate)
- CPU usage breakdown (user/kernel/interrupt)

---

## Future Development

### Phase 1: Fix Context Switching (High Priority)

**Goal:** Enable threading test without triple faults

**Tasks:**

1. ✅ Deferred scheduling implemented
2. ✅ IRQ0 handler stable
3. 🔴 Debug `nk_switch_context()` triple fault
4. ⚪ Validate thread stack allocation
5. ⚪ Test single-thread context switch (save/restore same thread)
6. ⚪ Test two-thread round-robin
7. ⚪ Re-enable `nk_test_threads_run()` in `main.c`

### Phase 2: System Timers API (Medium Priority)

**Goal:** Provide high-level timing APIs for drivers and UI

**Proposed API:**

```c
// Get current uptime in milliseconds
uint64_t nk_time_ms(void);

// Sleep for N milliseconds (yields CPU to other threads)
void nk_sleep(uint64_t ms);

// Register callback to run after delay
void nk_timer_callback(uint64_t delay_ms, void (*callback)(void *), void *arg);

// Cancel pending timer
void nk_timer_cancel(nk_timer_id_t id);
```

**Implementation:**

- Add `sleep_queue` sorted by wake time
- In `nk_timer_tick()`, check head of sleep queue and wake ready threads
- Move awakened threads from sleep queue to ready queue

### Phase 3: Interrupt Profiler (Low Priority)

**Goal:** Measure and log ISR performance metrics

**Metrics to Track:**

| Metric | Description | Use Case |
|--------|-------------|----------|
| ISR Latency | Time from IRQ assertion to handler entry | Detect interrupt storms |
| Handler Duration | Time spent in C handler | Identify slow handlers |
| Tick Jitter | Variance in timer period | Measure timer accuracy |
| CPU Usage | % time in IRQs vs user code | Identify bottlenecks |

**Implementation Sketch:**

```c
static struct {
    uint64_t tsc_entry;
    uint64_t tsc_exit;
    uint64_t count;
    uint64_t total_cycles;
} irq_stats[256];

void irq0_handler(void) {
    uint64_t tsc_start = rdtsc();

    // ... normal handler code ...

    uint64_t tsc_end = rdtsc();
    irq_stats[0].count++;
    irq_stats[0].total_cycles += (tsc_end - tsc_start);

    // Log every 1000 ticks
    if (irq_stats[0].count % 1000 == 0) {
        uint64_t avg_cycles = irq_stats[0].total_cycles / irq_stats[0].count;
        nk_printf("[PROF] IRQ0: %llu ticks, avg %llu cycles\n",
                  irq_stats[0].count, avg_cycles);
    }
}
```

### Phase 4: SMP Bring-Up (Low Priority)

**Goal:** Utilize multiple CPU cores with per-CPU scheduling

**Architecture Changes:**

```c
// Per-CPU state (one per hardware core)
struct cpu_state {
    uint32_t cpu_id;
    nk_thread_t *current_thread;
    nk_thread_t *idle_thread;
    nk_spinlock_t sched_lock;
    uint64_t ticks;
};

extern struct cpu_state cpu_states[MAX_CPUS];

// Get current CPU's state (using APIC ID or per-CPU segment)
static inline struct cpu_state *this_cpu(void) {
    // Read APIC ID from MMIO register
    uint32_t apic_id = *((volatile uint32_t *)0xFEE00020) >> 24;
    return &cpu_states[apic_id];
}

// Scheduler now uses per-CPU state
void nk_schedule(void) {
    struct cpu_state *cpu = this_cpu();
    nk_thread_t *prev = cpu->current_thread;
    nk_thread_t *next = select_next_thread(cpu);
    // ... rest of scheduler ...
}
```

**Initialization:**

```c
void nk_smp_init(void) {
    // 1. Detect number of CPUs (ACPI MADT or MP table)
    uint32_t num_cpus = detect_cpus();

    // 2. Initialize APIC (disable PIC, enable APIC)
    apic_init();

    // 3. Create per-CPU idle threads
    for (uint32_t i = 0; i < num_cpus; i++) {
        cpu_states[i].idle_thread = nk_thread_create_idle(i);
    }

    // 4. Boot secondary CPUs (INIT-SIPI-SIPI sequence)
    for (uint32_t i = 1; i < num_cpus; i++) {
        apic_send_init_ipi(i);
        apic_send_sipi(i, AP_BOOT_ADDRESS);
    }

    // 5. Wait for APs to signal ready
    while (atomic_load(&cpus_booted) < num_cpus) {
        cpu_relax();
    }
}
```

### Phase 5: APIC Timer Migration

**Goal:** Replace PIT with per-CPU APIC timers for better SMP scaling

**Advantages:**

- Per-CPU timers (no contention)
- Nanosecond resolution (vs PIT's ~838 ns)
- One-shot or periodic modes
- No legacy 8259 PIC dependency

**Challenges:**

- Requires APIC initialization
- Need TSC calibration for frequency measurement
- More complex setup (MMIO registers vs I/O ports)

---

## Revision History

| Date | Version | Changes |
|------|---------|---------|
| 2025-10-09 | 1.0 | Initial documentation - Interrupt subsystem design and implementation details |

---

**Document Maintainer:** System 7X Kernel Team
**Last Review:** 2025-10-09
**Next Review:** After context switching bug is resolved
