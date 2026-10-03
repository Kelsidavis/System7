# Serial Logging

The System 7.1 portable kernel provides a hierarchical module-based logging framework in `src/System71StdLib.c`. Logs written through `serial_logf()` are grouped by module and filtered by verbosity level; helper macros are available for some subsystems.

## Levels

Levels follow the familiar severity ordering:
- `kLogLevelError`
- `kLogLevelWarn`
- `kLogLevelInfo`
- `kLogLevelDebug`
- `kLogLevelTrace`

A message is emitted when its severity is no more verbose than both the global threshold and the threshold for its module (`Error` is least verbose; `Trace` is most verbose).

## Modules

`System71StdLib.h` defines the `SystemLogModule` identifiers (`kLogModuleWindow`, `kLogModuleControl`, `kLogModuleEvent`, etc.). Some subsystems provide logging headers under `include/`, such as `WindowManager/WMLogging.h` and `FontManager/FontLogging.h`; there is no `ControlManager/CtrlLogging.h`. The Window Manager macros currently compile to no-ops, so they do not emit messages. For modules without an active helper, call `serial_logf()` with the appropriate module and level:

```c
// Direct API; use a subsystem helper header when it provides an active macro.
serial_logf(kLogModuleControl, kLogLevelDebug,
            "[CTRL] tracking button id=%d state=%d\n", controlID, state);
```

Legacy `serial_printf()` calls are auto-classified via bracket tag parsing (`[CTRL]`, `[WM]`, etc.) and default to `kLogModuleGeneral`/`kLogLevelDebug` if no tag is found.

## Runtime control

```c
#include "System71StdLib.h"

// Drop everything more verbose than WARN globally.
SysLogSetGlobalLevel(kLogLevelWarn);

// Allow verbose messages passed to serial_logf() for the Window Manager module.
SysLogSetModuleLevel(kLogModuleWindow, kLogLevelDebug);
```

`SysLogGetGlobalLevel`, `SysLogGetModuleLevel`, and `SysLogModuleName` are available for status dumps.

## Emitting logs

Use an active subsystem helper macro when one exists. For example, `FontManager/FontLogging.h` defines `FONT_LOG_DEBUG`. Otherwise use `serial_logf()` directly, supplying the module and level:

```c
#include "FontManager/FontLogging.h"

FONT_LOG_DEBUG("loaded font id=%d\n", fontID);
```

Legacy `serial_printf()` calls are still supported. A recognized bracket tag (`[WM]`, `[CTRL]`, etc.) selects a module and optional level; untagged messages default to `kLogLevelDebug` under `kLogModuleGeneral`.

## Recognized bracket tags

`serial_printf()` recognizes tags from the table in `System71StdLib.c`; use a listed tag with an optional level suffix (`[TAG:LEVEL]`). Unknown tags fall back to General/Debug. For example:

```
serial_printf("[DM:TRACE] focus advanced to item %d\n", item);
```

sets the module to `kLogModuleDialog` and the level to `Trace` explicitly.
