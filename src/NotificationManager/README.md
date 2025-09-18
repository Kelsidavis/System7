# System 7.1 Notification Manager - Portable Implementation

## Overview

This is a complete, portable implementation of the Apple Macintosh System 7.1 Notification Manager, converted from the original assembly source code to modern C. The Notification Manager provides background task notifications and system alerts, allowing applications to communicate with users even when running in the background.

## Architecture

The Notification Manager has been modularized into several key components:

### Core Components

1. **NotificationManagerCore.c** - Main notification management and API entry points
   - Implements the original `NMInstall` and `NMRemove` trap functions
   - Provides extended notification API with modern features
   - Manages notification lifecycle and processing

2. **NotificationQueue.c** - Queue management and priority handling
   - Maintains pending and active notification queues
   - Supports priority-based ordering and FIFO processing
   - Provides queue statistics and management functions

3. **SystemAlerts.c** - System alert display and user interaction
   - Manages modal and non-modal alert dialogs
   - Handles user interaction and button responses
   - Supports custom alert configurations and positioning

4. **ResponseHandling.c** - Notification response processing and callbacks
   - Executes original Mac OS callback procedures safely
   - Supports modern callback functions
   - Provides asynchronous response processing

5. **BackgroundNotifications.c** - Background task notification processing
   - Monitors system resources (memory, disk, battery)
   - Manages background task registration and state
   - Provides system event notifications

6. **NotificationScheduler.c** - Priority-based notification scheduling
   - Schedules notifications based on priority and availability
   - Manages concurrent notification limits
   - Supports round-robin and priority-based scheduling

7. **AppRegistration.c** - Application notification registration and management
   - Tracks registered applications and their preferences
   - Manages per-application notification settings
   - Provides default icons and sounds per application

8. **ResourceManager.c** - Notification resource loading and management
   - Loads and caches notification icons, sounds, and strings
   - Manages resource files and memory usage
   - Provides default system resources

9. **ModernNotifications.c** - Modern notification system abstraction
   - Bridges classic Mac OS notifications with modern platform APIs
   - Supports macOS Notification Center, Windows notifications, Linux desktop notifications
   - Provides rich content, actions, and modern features

## API Compatibility

### Original Mac OS API

The implementation maintains 100% compatibility with the original Mac OS Notification Manager API:

```c
// Classic Mac OS functions
OSErr NMInstall(NMRecPtr nmReqPtr);
OSErr NMRemove(NMRecPtr nmReqPtr);

// Notification record structure (exact match)
struct NMRec {
    QElemPtr    qLink;      // Next queue entry
    short       qType;      // Queue type - ORD(nmType) = 8
    short       nmFlags;    // Notification flags
    long        nmPrivate;  // Reserved for system use
    short       nmReserved; // Reserved
    short       nmMark;     // Item to mark in Apple menu
    Handle      nmIcon;     // Handle to small icon
    Handle      nmSound;    // Handle to sound record
    StringPtr   nmStr;      // String to appear in alert
    NMProcPtr   nmResp;     // Pointer to response routine
    long        nmRefCon;   // For application use
};
```

### Extended API

Enhanced functionality is provided through extended API functions:

```c
// Extended notification management
OSErr NMInstallExtended(NMExtendedRecPtr nmExtPtr);
OSErr NMRemoveExtended(NMExtendedRecPtr nmExtPtr);
OSErr NMSetPriority(NMRecPtr nmReqPtr, NMPriority priority);
OSErr NMSetTimeout(NMRecPtr nmReqPtr, UInt32 timeout);

// Queue management
OSErr NMGetQueueStatus(short *count, short *maxSize);
OSErr NMFlushQueue(void);
OSErr NMFlushCategory(StringPtr category);

// Configuration
OSErr NMSetEnabled(Boolean enabled);
OSErr NMSetSoundsEnabled(Boolean enabled);
OSErr NMSetAlertsEnabled(Boolean enabled);
```

### Modern Integration

The implementation provides seamless integration with modern notification systems:

```c
// Modern notification API
OSErr NMPostModernNotification(ModernNotificationPtr notification);
OSErr NMRequestNotificationPermission(Boolean *granted);
OSErr NMSetAppBadge(short count);

// Rich content support
OSErr NMCreateRichContent(RichNotificationPtr *content);
OSErr NMSetContentTitle(RichNotificationPtr content, StringPtr title);
OSErr NMSetContentBody(RichNotificationPtr content, StringPtr body);

// Action support
OSErr NMCreateNotificationAction(NotificationActionPtr *action,
                                StringPtr title, StringPtr identifier);
```

## Usage Examples

### Basic Notification

```c
// Original Mac OS style
NMRec notification;
notification.qLink = NULL;
notification.qType = nmType;
notification.nmFlags = 0;
notification.nmPrivate = 0;
notification.nmReserved = 0;
notification.nmMark = 0;
notification.nmIcon = NULL;
notification.nmSound = NULL;
notification.nmStr = "\pHello, World!";
notification.nmResp = NULL;
notification.nmRefCon = 0;

OSErr err = NMInstall(&notification);
```

### Extended Notification with Priority

```c
NMExtendedRec extNotification;
// ... set up base notification ...
extNotification.priority = nmPriorityHigh;
extNotification.timeout = 300; // 5 seconds
extNotification.persistent = true;
extNotification.modal = false;

OSErr err = NMInstallExtended(&extNotification);
```

### Modern Rich Notification

```c
ModernNotificationRequest modern;
NMCreateRichContent(&modern.content);
NMSetContentTitle(&modern.content, "\pImportant Update");
NMSetContentBody(&modern.content, "\pYour application has finished processing.");

modern.identifier = "\pcom.myapp.update.complete";
modern.critical = false;
modern.platform = platformNotificationMacOS;

OSErr err = NMPostModernNotification(&modern);
```

### Background Task Monitoring

```c
// Register background task
BackgroundTaskRegistration task;
task.appSignature = 'MyAp';
task.appName = "\pMy Application";
task.taskID = BGGenerateTaskID();
task.state = bgTaskActive;
task.notifyOnStateChange = true;
task.notifyOnCompletion = true;

OSErr err = BGRegisterTask(&task);

// Post background notification
BackgroundNotificationRequest bgNotify;
bgNotify.type = bgNotifyLowMemory;
bgNotify.appSignature = 'MyAp';
bgNotify.message = "\pLow memory warning";
bgNotify.urgent = true;

err = BGPostNotification(&bgNotify);
```

## Platform Integration

### macOS Integration

```c
// Initialize modern notifications for macOS
NMModernInit(platformNotificationMacOS);

// Request permission (required on macOS 10.14+)
Boolean granted;
NMRequestNotificationPermission(&granted);

// Notifications will appear in Notification Center
```

### Cross-Platform Support

The implementation automatically detects the platform and uses appropriate notification APIs:

- **macOS**: Notification Center (NSUserNotification/UNUserNotification)
- **Windows**: Windows 10/11 notifications (WinRT ToastNotification)
- **Linux**: Desktop notifications (libnotify)
- **Legacy**: Falls back to system alerts for unsupported platforms

## Error Handling

The implementation provides comprehensive error codes:

```c
// Core error codes
#define nmErrNotInstalled       -40900  // Notification Manager not installed
#define nmErrInvalidRecord      -40901  // Invalid notification record
#define nmErrQueueFull          -40902  // Notification queue is full
#define nmErrNotFound           -40903  // Notification not found

// Modern notification error codes
#define modernErrNotSupported   -44000  // Feature not supported
#define modernErrPermissionDenied -44001 // Permission denied
#define modernErrPlatformFailure -44003 // Platform-specific failure

// Background notification error codes
#define bgErrNotInitialized     -41000  // Background notifications not initialized
#define bgErrTaskNotFound       -41001  // Task not found
```

## Performance Considerations

### Memory Management

- Resource caching with configurable limits
- Automatic cleanup of expired notifications
- Reference counting for shared resources
- Memory pool for frequently allocated structures

### Queue Performance

- Priority-based insertion and removal
- Configurable queue size limits
- Automatic compaction of expired entries
- Statistics tracking for optimization

### Platform Efficiency

- Lazy loading of platform-specific components
- Batched operations where supported
- Minimal memory footprint in legacy mode
- Efficient callback execution with proper A5 world management

## Thread Safety

The implementation is designed to be thread-safe:

- All global state is protected by appropriate synchronization
- Queue operations are atomic
- Callback execution is serialized
- Platform integration handles threading appropriately

## Building and Integration

### Dependencies

- Standard Mac OS headers (Types.h, OSUtils.h, Memory.h, etc.)
- Platform-specific headers for modern integration
- No external libraries required for basic functionality

### Compilation

```makefile
# Basic compilation
cc -c NotificationManagerCore.c -I../include

# With modern notification support
cc -c ModernNotifications.c -I../include -DENABLE_MODERN_NOTIFICATIONS

# Platform-specific flags
# macOS: -framework Foundation -framework UserNotifications
# Windows: -lwindows.ui.notifications
# Linux: -lnotify
```

### Integration Steps

1. Include the main header: `#include "NotificationManager/NotificationManager.h"`
2. Initialize the Notification Manager: `NMInstall()` will auto-initialize
3. For modern features: `NMModernInit(platformNotificationNone)` for auto-detection
4. Register applications: `NMRegisterApplication()` for per-app settings
5. Post notifications using appropriate API level

## Testing

The implementation includes comprehensive test coverage:

- Unit tests for each component
- Integration tests for complete workflows
- Platform-specific tests for modern integration
- Memory leak detection
- Performance benchmarking

## Compatibility Notes

### Mac OS Classic Compatibility

- 100% API compatibility with System 7.1 Notification Manager
- Proper A5 world handling for classic callbacks
- Resource fork support for icons and sounds
- Pascal string handling throughout

### Modern Platform Compatibility

- macOS 10.9+ (NSUserNotification) and 10.14+ (UserNotifications framework)
- Windows 10+ (WinRT notifications)
- Linux with D-Bus and libnotify
- Graceful degradation on unsupported platforms

## License

This implementation is derived from the original Apple Macintosh System 7.1 source code and is provided for research and educational purposes. See the main project license for details.

## Contributing

When contributing to this implementation:

1. Maintain compatibility with the original Mac OS API
2. Follow the existing code style and patterns
3. Add appropriate error handling and validation
4. Include comprehensive documentation
5. Test on multiple platforms where applicable
6. Consider memory management and performance implications

## Future Enhancements

Potential areas for future development:

- WebAssembly support for browser integration
- Mobile platform support (iOS, Android)
- Network-based notification delivery
- Advanced scheduling with calendar integration
- Machine learning for notification prioritization
- Enhanced accessibility features
- Internationalization and localization support