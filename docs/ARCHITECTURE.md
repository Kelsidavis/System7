# System7 Architecture Guide

## Overview

System7 is a layered architecture that reimplements the classic Mac OS System 7.1 components with modern portability in mind.

## Architecture Layers

```
┌─────────────────────────────────────────┐
│         Applications & Desk Accessories │
├─────────────────────────────────────────┤
│              Toolbox Managers           │
│  (Window, Menu, Dialog, Control, etc.)  │
├─────────────────────────────────────────┤
│          Core System Services           │
│  (Memory, File, Resource, Process)      │
├─────────────────────────────────────────┤
│    Hardware Abstraction Layer (HAL)     │
│  (Platform-specific implementations)    │
├─────────────────────────────────────────┤
│         Operating System (Host)         │
│      (Linux/X11, macOS, Windows)        │
└─────────────────────────────────────────┘
```

## Component Organization

### Manager Pattern

Each system component follows the Manager pattern:

```c
typedef struct {
    /* Private state */
    Handle privateData;
    Boolean initialized;
    /* Function pointers for polymorphism */
    ProcPtr customProcs[16];
} ManagerGlobals;

/* Public API */
OSErr InitManager(void);
void CloseManager(void);
OSErr ManagerOperation(Handle param);

/* HAL Layer */
void Manager_HAL_PlatformInit(void);
void Manager_HAL_PlatformOperation(void);
```

### Handle-Based Memory

Following classic Mac OS conventions:

```c
/* Handles are pointers to pointers */
typedef char** Handle;
typedef struct Point** PointHandle;

/* Handle operations */
Handle NewHandle(Size size);
void DisposeHandle(Handle h);
void SetHandleSize(Handle h, Size newSize);
void HLock(Handle h);
void HUnlock(Handle h);
```

## Key Components

### 1. Window Manager
- **Purpose**: Window creation, management, and event routing
- **Key APIs**: NewWindow, SelectWindow, DragWindow, CloseWindow
- **HAL**: Platform window creation, drawing surface management

### 2. Menu Manager
- **Purpose**: Menu bar, hierarchical menus, keyboard shortcuts
- **Key APIs**: NewMenu, AppendMenu, MenuSelect, DrawMenuBar
- **HAL**: Native menu integration where available

### 3. QuickDraw
- **Purpose**: 2D graphics primitives and image operations
- **Key APIs**: MoveTo, LineTo, FillRect, CopyBits
- **HAL**: Cairo (Linux), Core Graphics (macOS), GDI (Windows)

### 4. Event Manager
- **Purpose**: Event queue management and dispatching
- **Key APIs**: WaitNextEvent, PostEvent, FlushEvents
- **HAL**: Platform event translation

### 5. Resource Manager
- **Purpose**: Resource fork emulation and data management
- **Key APIs**: GetResource, AddResource, ReleaseResource
- **HAL**: File system abstraction

## Platform Abstraction Strategy

### HAL Design Principles

1. **Minimal Surface Area**: Keep HAL interfaces small and focused
2. **No Leaky Abstractions**: Platform details don't escape HAL
3. **Performance Critical**: HAL functions are optimized hot paths
4. **Compile-Time Selection**: Use #ifdef for platform code

### Example HAL Implementation

```c
/* WindowMgr_HAL.h */
void* WindowMgr_HAL_CreateNativeWindow(Rect* bounds);
void WindowMgr_HAL_ShowWindow(void* nativeWindow);
void WindowMgr_HAL_DrawContent(void* nativeWindow, RgnHandle clipRgn);

/* WindowMgr_HAL.c */
#ifdef __APPLE__
    /* Core Graphics implementation */
    #include <CoreGraphics/CoreGraphics.h>

    void* WindowMgr_HAL_CreateNativeWindow(Rect* bounds) {
        CGRect frame = CGRectMake(bounds->left, bounds->top,
                                 bounds->right - bounds->left,
                                 bounds->bottom - bounds->top);
        /* Create NSWindow equivalent */
    }

#elif defined(HAS_X11)
    /* X11 implementation */
    #include <X11/Xlib.h>

    void* WindowMgr_HAL_CreateNativeWindow(Rect* bounds) {
        Display* display = XOpenDisplay(NULL);
        Window window = XCreateSimpleWindow(display, /*...*/);
        return (void*)window;
    }
#endif
```

## Memory Management

### Zone-Based Allocation

System7 implements the classic Mac OS zone-based memory model:

```c
typedef struct Zone {
    Ptr     bkLim;          /* Back limit of zone */
    Ptr     purgePtr;       /* Next block to purge */
    Ptr     hFstFree;       /* First free block */
    long    zcbFree;        /* Free bytes in zone */
    ProcPtr gzProc;         /* Grow zone function */
    short   moreMast;       /* Master pointers to allocate */
    short   flags;          /* Zone flags */
    Handle  heapData;       /* Zone heap data */
} Zone, *THz;

/* Current zone operations */
THz GetZone(void);
void SetZone(THz hz);
THz HandleZone(Handle h);
```

### Handle Lifecycle

1. **Allocation**: NewHandle creates moveable memory
2. **Locking**: HLock prevents relocation during use
3. **Growing**: SetHandleSize can expand/shrink
4. **Purging**: Memory pressure triggers purgeable handle removal
5. **Disposal**: DisposeHandle frees handle and master pointer

## Event System

### Event Queue

```c
typedef struct EventQueue {
    EventRecord events[32];  /* Circular buffer */
    short       head;
    short       tail;
    short       count;
} EventQueue;

/* Event types match System 7.1 */
enum {
    nullEvent = 0,
    mouseDown = 1,
    mouseUp = 2,
    keyDown = 3,
    keyUp = 4,
    autoKey = 5,
    updateEvt = 6,
    diskEvt = 7,
    activateEvt = 8,
    osEvt = 15
};
```

### Event Routing

1. Hardware/OS events enter through HAL
2. HAL translates to EventRecord format
3. Events queued in Event Manager
4. WaitNextEvent dequeues for application
5. Standard event handlers process events

## Graphics Architecture

### Coordinate System

- Origin (0,0) at top-left
- Y increases downward
- Integer coordinates
- Local vs. Global coordinate spaces

### Graphics Port

```c
typedef struct GrafPort {
    short       device;         /* Device ID */
    BitMap      portBits;      /* Bitmap for port */
    Rect        portRect;      /* Port rectangle */
    RgnHandle   visRgn;        /* Visible region */
    RgnHandle   clipRgn;       /* Clipping region */
    Pattern     bkPat;         /* Background pattern */
    Pattern     fillPat;       /* Fill pattern */
    Point       pnLoc;         /* Pen location */
    Point       pnSize;        /* Pen size */
    short       pnMode;        /* Pen mode */
    Pattern     pnPat;         /* Pen pattern */
    short       pnVis;         /* Pen visibility */
    Style       txFont;        /* Text font */
    Style       txFace;        /* Text style */
    short       txMode;        /* Text mode */
    short       txSize;        /* Text size */
} GrafPort;
```

## File System

### HFS+ Emulation

```c
typedef struct FCB {        /* File Control Block */
    long        fcbFlNm;    /* File number */
    short       fcbFlags;   /* File flags */
    short       fcbTypByt;  /* File type */
    long        fcbCatPos;  /* Catalog position */
    long        fcbDirID;   /* Directory ID */
    Str31       fcbCName;   /* File name */
    long        fcbFType;   /* File type */
    long        fcbEOF;     /* Logical EOF */
    long        fcbPLen;    /* Physical EOF */
    Handle      fcbBfAdr;   /* Buffer address */
} FCB;
```

## Threading Model

System7 maintains single-threaded execution with cooperative multitasking:

1. **Main Thread**: All Toolbox calls execute on main thread
2. **Background Tasks**: Time Manager for deferred execution
3. **Async I/O**: Completion routines for disk/network
4. **HAL Threading**: Platform threads isolated in HAL layer

## Build System

### Modular Components

Each manager can be built independently:

```makefile
# Component library
libMenuManager.a: $(MENU_OBJS)
    $(AR) rcs $@ $^

# HAL selection
ifeq ($(PLATFORM),macos)
    CFLAGS += -DHAS_COREGRAPHICS
else ifeq ($(PLATFORM),linux)
    CFLAGS += -DHAS_X11
endif
```

### Testing Strategy

1. **Unit Tests**: Per-component function testing
2. **Integration Tests**: Manager interaction testing
3. **Compatibility Tests**: Validate against System 7.1 behavior
4. **Platform Tests**: HAL implementation verification

## Performance Considerations

### Optimization Points

1. **Region Operations**: Critical for window refresh
2. **BitBlt**: CopyBits is performance critical
3. **Event Dispatch**: Minimize latency
4. **Memory Compaction**: Balance with performance

### Caching Strategy

- Window backing stores
- Font metrics cache
- Resource cache with LRU eviction
- Icon bitmap cache

## Future Extensibility

### Planned Extensions

1. **Unicode Support**: UTF-8 text handling
2. **High DPI**: Retina/4K display support
3. **GPU Acceleration**: Metal/Vulkan rendering
4. **Network Transparency**: Remote display protocol

### Compatibility Layers

- Carbon API subset for modern apps
- POSIX wrapper for Unix tools
- Win32 subset for Windows apps

## Debugging Support

### Debug Facilities

```c
/* Debug zones for selective logging */
#define DEBUG_WINDOW_MGR    0x0001
#define DEBUG_MENU_MGR      0x0002
#define DEBUG_EVENT_MGR     0x0004
#define DEBUG_MEMORY_MGR    0x0008

/* Diagnostic functions */
void DebugStr(ConstStr255Param message);
void Debugger(void);
void SysBreak(void);
```

## Security Considerations

- No code execution from resources
- Bounds checking on handle access
- Stack guard pages
- Input validation at HAL boundary

---

For implementation details of specific components, see the source code in `src/` directory.