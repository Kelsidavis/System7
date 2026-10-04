# Event Manager

## Overview

Provides the Event Manager event-loop API and dispatch integration for System 7 applications. `WaitNextEvent` is implemented here; the active, process-aware event queue and public `GetNextEvent`/`EventAvail` entries live in `src/ProcessMgr/EventIntegration.c`.

## Source Layout

- `src/EventManager/event_manager.c` – `InitEvents` and `WaitNextEvent`; the active queue and process-aware `GetNextEvent`/`EventAvail` entries live in `src/ProcessMgr/EventIntegration.c`
- `src/EventManager/EventGlobals.c` – double-click timing, button state, and modal mouse-tracking suppression
- `src/EventManager/EventDispatcher.c` – routes events to Window Manager, Dialog Manager, and Process Manager
- `src/EventManager/MouseEvents.c` and `KeyboardEvents.c` – mouse tracking and keyboard event handling
- `src/EventManager/ModernInput.c` – platform input integration and translation into classic event records
- `src/EventManager/SystemEvents.c` – activate/deactivate and other system events
- `src/EventManager/AppSwitcher.c` – application-switching event behavior

## Responsibilities

- Initialize the active Process Manager event queue through `InitEvents`
- Honour event masks and coordinate input, system-event dispatch, and cooperative waiting
- Provide `WaitNextEvent` sleep semantics using Time Manager tick scheduling
- Track mouse button transitions and double-click thresholds; read screen coordinates through `GetMouse`
- Coordinate application activation/deactivation and propagate activate events to windows

## Integration Points

- **Platform input backends** supply mouse position, button state, and queued keyboard transitions to `ModernInput.c`
- **Window Manager** receives activate/deactivate, update, and mouse events through `EventDispatcher`
- **Dialog Manager** hooks into `DialogSelect` using `WaitNextEvent` output when in modal loops
- **Time Manager** supplies ticks for sleep intervals and double-click windows

## Testing & Debugging

- `make test-input` checks mouse motion during polling, click counting, modal drag suppression, keyboard modifier replay, platform stub coordinates, and C++ linkage. It also runs as part of `make check`.
- Use `make run` for interactive input checks. Enable event traces with `SysLogSetGlobalLevel(kLogLevelTrace)` and `SysLogSetModuleLevel(kLogModuleEvent, kLogLevelTrace)`.
- `Event_DumpQueue` logs the shared event queue when diagnosing starvation; enable the Process Manager log module at debug level to see its output.

## Future Work

- Integrate Process Manager idle coalescing so background cooperative tasks get predictable slices
- Surface instrumentation counters (queue length, average sleep) via the serial console
- Add optional logging categories for high-frequency mouse move events without flooding the console
