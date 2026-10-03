# Event Manager

## Overview
Provides the Event Manager event-loop API and dispatch integration for System 7 applications. `WaitNextEvent` is implemented here; the active, process-aware event queue and public `GetNextEvent`/`EventAvail` entries live in `src/ProcessMgr/EventIntegration.c`.

## Source Layout
- `src/EventManager/event_manager.c` – `InitEvents` and `WaitNextEvent`; the active queue and process-aware `GetNextEvent`/`EventAvail` entries live in `src/ProcessMgr/EventIntegration.c`
- `src/EventManager/EventGlobals.c` – global event state and initialization
- `src/EventManager/EventDispatcher.c` – routes events to Window Manager, Dialog Manager, and Process Manager
- `src/EventManager/MouseEvents.c` and `KeyboardEvents.c` – mouse tracking and keyboard event handling
- `src/EventManager/ModernInput.c` – platform input integration and translation into classic event records
- `src/EventManager/SystemEvents.c` – activate/deactivate and other system events
- `src/EventManager/AppSwitcher.c` – application-switching event behavior

## Responsibilities
- Initialize the active Process Manager event queue through `InitEvents`
- Honour event masks and coordinate input, system-event dispatch, and cooperative waiting
- Provide `WaitNextEvent` sleep semantics using Time Manager tick scheduling
- Track mouse button transitions, double-click thresholds, and cursor location (`globalMouse`)
- Coordinate application activation/deactivation and propagate activate events to windows

## Integration Points
- **PS/2 Controller** feeds scancodes and button changes via `ModernInput.c`
- **Window Manager** receives activate/deactivate, update, and mouse events through `EventDispatcher`
- **Dialog Manager** hooks into `DialogSelect` using `WaitNextEvent` output when in modal loops
- **Time Manager** supplies ticks for sleep intervals and double-click windows

## Testing & Debugging
- `make run` with serial logging enabled will surface `[EVT]` traces (toggle via `System71StdLib.c` whitelist)
- Use the serial shell commands to post synthetic events or inspect queue depth when diagnosing starvation
- Keyboard repeat, modifier handling, and mouse tracking were validated using the control smoke app and Finder window interactions

## Future Work
- Integrate Process Manager idle coalescing so background cooperative tasks get predictable slices
- Surface instrumentation counters (queue length, average sleep) via the serial console
- Add optional logging categories for high-frequency mouse move events without flooding the console
