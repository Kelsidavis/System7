# Event Flow Mystery - Investigation Notes

## The Problem

The System 7.1 reimplementation has a critical event flow issue:

1. **Events ARE being processed** - We see HandleMouseDown debug output
2. **Events ARE being posted** - PostEvent logs show events added to queue
3. **But GetNextEvent is NEVER called** - No debug logs from GetNextEvent
4. **The main event loop NEVER starts** - No "MAIN: Entering main event loop NOW!" message

## Evidence

### What We See in Output:
```
  Finder initialized
PostEvent: Posting mouseDown (type=1), msg=0x02c60037 at (710,55)
PostEvent: Successfully posted mouseDown at position 4, queue now has 1 events
HandleMouseDown: event=%p, where={v=1392468,h=55}, modifiers=0x%04x
HandleMouseDown: part=0, window=%p at (0,710)
```

### What We DON'T See:
```
MAIN: About to call WM_Update
MAIN: Entering main event loop NOW!
GetNextEvent: Call #1 with mask=0x...
MAIN: Got event type=1 at (710,55)
```

## Code Flow Analysis

The expected flow should be:
1. `kmain()` → `init_system71()` → returns
2. `kmain()` → `create_system71_windows()` → returns
3. `kmain()` → enters main loop at line 1890
4. Main loop calls `GetNextEvent()` → `DispatchEvent()` → `HandleMouseDown()`

But what's actually happening:
1. System initializes successfully
2. Finder initializes and returns
3. **Code never reaches the main loop**
4. **Yet HandleMouseDown is still being called somehow**

## Theories

### Theory 1: Hidden Event Loop
There might be another event loop running somewhere that's calling DispatchEvent directly.

Checked:
- Finder's MainEventLoop is disabled (#if 0)
- No other while(true) loops found that would process events

### Theory 2: Interrupt Handler
Maybe PS2 interrupts are directly dispatching events?

Evidence against:
- PS2Controller only calls PostEvent, not DispatchEvent
- No direct connection found between PostEvent and HandleMouseDown

### Theory 3: Code Stuck Before Main Loop
The system might be stuck somewhere between Finder initialization and the main loop.

Evidence:
- We see "Finder initialized"
- We don't see "MAIN: About to call WM_Update"
- Something is blocking execution from reaching line 1810

### Theory 4: Phantom Event Processor
There's some unknown code path that's processing events without going through GetNextEvent.

This would explain:
- Why events are handled (HandleMouseDown runs)
- Why GetNextEvent is never called
- Why the main loop never starts

## Next Steps to Investigate

1. **Add more debug points** between Finder init and main loop
2. **Search for ANY code calling DispatchEvent** besides main loop
3. **Check if there's a timer/interrupt that processes events**
4. **Trace backwards from HandleMouseDown** to find who's calling it
5. **Check if FlushEvents or another function has side effects**

## Critical Questions

1. How is HandleMouseDown being called if GetNextEvent is never called?
2. Why doesn't execution reach the main event loop?
3. What code is running between "Finder initialized" and the main loop?
4. Is there a parallel execution path we're not aware of?

## Files to Check

- [ ] PS2Controller.c - interrupt handlers?
- [ ] EventDispatcher.c - who calls DispatchEvent?
- [ ] ModernInput.c - ProcessModernInput side effects?
- [ ] Finder/finder_main.c - hidden loops or callbacks?
- [ ] SystemInit.c - timer interrupts?

## Resolution

**TO BE DETERMINED** - This mystery requires further investigation to understand the phantom event processing path.