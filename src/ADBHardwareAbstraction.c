/*
 * ADBHardwareAbstraction.c - Hardware Abstraction Layer for ADB Manager
 *
 * Portable System 7.1 Implementation
 *
 * This file provides hardware abstraction implementations for modern systems,
 * allowing the ADB Manager to work with USB, PS/2, and other input devices
 * while maintaining compatibility with the original Mac OS ADB protocol.
 *
 * Supported platforms:
 * - Linux (evdev, uinput)
 * - Windows (Raw Input API, DirectInput)
 * - macOS (HID Manager, Carbon Events)
 * - Generic POSIX systems
 */

#define _POSIX_C_SOURCE 199309L  /* For clock_gettime and nanosleep */

#include "ADBManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __linux__
    #include <linux/input.h>
    #include <linux/uinput.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/select.h>
#endif

#ifdef _WIN32
    #include <windows.h>
    #include <winuser.h>
#endif

#ifdef __APPLE__
    #include <CoreFoundation/CoreFoundation.h>
    #include <IOKit/hid/IOHIDManager.h>
#endif

/* Platform-specific data structures */
typedef struct {
    int keyboard_fd;
    int mouse_fd;
    bool running;

#ifdef __linux__
    int uinput_fd;
    struct input_event event_buffer[64];
#endif

#ifdef _WIN32
    HWND message_window;
    RAWINPUTDEVICE rid[2];
#endif

#ifdef __APPLE__
    IOHIDManagerRef hid_manager;
#endif

    /* Key state tracking */
    uint8_t key_state[256];
    uint8_t modifier_state;

    /* Mouse state tracking */
    int16_t mouse_x, mouse_y;
    uint8_t mouse_buttons;

    /* Timing */
    struct timespec last_poll;
    uint32_t tick_counter;

} PlatformHardwareData;

/* Forward declarations */
static int initializePlatformHardware(PlatformHardwareData* hwData);
static void shutdownPlatformHardware(PlatformHardwareData* hwData);
static int pollInputDevices(PlatformHardwareData* hwData, ADBManager* mgr);
static void simulateADBTiming(void);

/* ADB Hardware Interface Implementation */
static int startADBRequest(ADBManager* mgr, uint8_t command, uint8_t* data, int dataLen, bool isImplicit);
static void pollADBDevice(ADBManager* mgr);
static void resetADBBus(ADBManager* mgr);

/* Platform-specific implementations */
#ifdef __linux__
static int initializeLinuxHardware(PlatformHardwareData* hwData);
static int pollLinuxInputDevices(PlatformHardwareData* hwData, ADBManager* mgr);
#endif

#ifdef _WIN32
static int initializeWindowsHardware(PlatformHardwareData* hwData);
static int pollWindowsInputDevices(PlatformHardwareData* hwData, ADBManager* mgr);
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

#ifdef __APPLE__
static int initializeMacOSHardware(PlatformHardwareData* hwData);
static int pollMacOSInputDevices(PlatformHardwareData* hwData, ADBManager* mgr);
static void handleHIDInput(void* context, IOReturn result, void* sender, IOHIDValueRef value);
#endif

/*
 * ADBManager_CreateHardwareInterface - Create hardware interface for current platform
 */
ADBHardwareInterface* ADBManager_CreateHardwareInterface(void) {
    ADBHardwareInterface* interface = malloc(sizeof(ADBHardwareInterface));
    if (!interface) {
        return NULL;
    }

    PlatformHardwareData* hwData = malloc(sizeof(PlatformHardwareData));
    if (!hwData) {
        free(interface);
        return NULL;
    }

    memset(hwData, 0, sizeof(PlatformHardwareData));

    /* Initialize platform-specific hardware */
    if (initializePlatformHardware(hwData) != 0) {
        free(hwData);
        free(interface);
        return NULL;
    }

    /* Set up interface function pointers */
    interface->startRequest = startADBRequest;
    interface->pollDevice = pollADBDevice;
    interface->resetBus = resetADBBus;
    interface->platformData = hwData;

    printf("Hardware interface created for platform\n");
    return interface;
}

/*
 * ADBManager_DestroyHardwareInterface - Clean up hardware interface
 */
void ADBManager_DestroyHardwareInterface(ADBHardwareInterface* interface) {
    if (!interface) return;

    if (interface->platformData) {
        PlatformHardwareData* hwData = (PlatformHardwareData*)interface->platformData;
        shutdownPlatformHardware(hwData);
        free(hwData);
    }

    free(interface);
    printf("Hardware interface destroyed\n");
}

/*
 * ADBManager_ProcessHardwareEvents - Process hardware events (call regularly)
 */
void ADBManager_ProcessHardwareEvents(ADBManager* mgr) {
    if (!mgr || !mgr->hardware.platformData) return;

    PlatformHardwareData* hwData = (PlatformHardwareData*)mgr->hardware.platformData;
    pollInputDevices(hwData, mgr);
}

/*
 * startADBRequest - Start an ADB request (hardware abstraction)
 */
static int startADBRequest(ADBManager* mgr, uint8_t command, uint8_t* data, int dataLen, bool isImplicit) {
    if (!mgr) return ADB_ERROR_INVALID_PARAM;

    PlatformHardwareData* hwData = (PlatformHardwareData*)mgr->hardware.platformData;
    if (!hwData) return ADB_ERROR_HARDWARE;

    /* Extract command components */
    uint8_t address = (command >> 4) & 0x0F;
    uint8_t cmd = command & 0x0C;
    uint8_t reg = command & 0x03;

    /* Simulate ADB timing */
    simulateADBTiming();

    /* For modern systems, we don't actually send ADB commands to hardware.
     * Instead, we simulate the behavior and generate appropriate responses
     * based on the current input state.
     */

    if (cmd == ADB_TALK_CMD) {
        /* Talk command - return current device state */
        uint8_t responseData[ADB_MAX_DATA_SIZE];
        int responseLen = 0;

        if (address == ADB_KEYBOARD_ADDR && reg == 0) {
            /* Return keyboard data if keys are pressed */
            /* For now, return no data (empty response) */
            responseLen = 0;
        } else if (address == ADB_MOUSE_ADDR && reg == 0) {
            /* Return mouse data if movement or button changes */
            if (hwData->mouse_x != 0 || hwData->mouse_y != 0 ||
                (hwData->mouse_buttons != 0)) {

                responseData[0] = (hwData->mouse_buttons ? 0x00 : 0x80) | (hwData->mouse_y & 0x7F);
                responseData[1] = hwData->mouse_x & 0x7F;
                responseLen = 2;

                /* Clear delta values after reporting */
                hwData->mouse_x = 0;
                hwData->mouse_y = 0;
            }
        } else if (reg == 3) {
            /* Talk R3 - return device information */
            if (address == ADB_KEYBOARD_ADDR || address == ADB_MOUSE_ADDR) {
                responseData[0] = address; /* Device address */
                responseData[1] = (address == ADB_KEYBOARD_ADDR) ? 1 : 2; /* Device type */
                responseLen = 2;
            }
        }

        /* Simulate completion */
        if (isImplicit) {
            ADBManager_ImplicitRequestDone(mgr, responseLen, command, responseData);
        } else {
            ADBManager_ExplicitRequestDone(mgr, responseLen, command, responseData);
        }

    } else if (cmd == ADB_LISTEN_CMD) {
        /* Listen command - configure device */
        printf("ADB Listen: addr=%d, reg=%d, len=%d\n", address, reg, dataLen);

        /* Simulate completion with no response data */
        if (isImplicit) {
            ADBManager_ImplicitRequestDone(mgr, 0, command, NULL);
        } else {
            ADBManager_ExplicitRequestDone(mgr, 0, command, NULL);
        }

    } else if (cmd == ADB_FLUSH_CMD) {
        /* Flush command - clear device buffers */
        printf("ADB Flush: addr=%d\n", address);

        /* For keyboards, this clears any pending keystrokes */
        if (address == ADB_KEYBOARD_ADDR) {
            memset(hwData->key_state, 0, sizeof(hwData->key_state));
        }

        /* Simulate completion */
        if (isImplicit) {
            ADBManager_ImplicitRequestDone(mgr, 0, command, NULL);
        } else {
            ADBManager_ExplicitRequestDone(mgr, 0, command, NULL);
        }

    } else if (cmd == ADB_RESET_CMD) {
        /* Reset command - reset all devices */
        printf("ADB Reset\n");
        memset(hwData->key_state, 0, sizeof(hwData->key_state));
        hwData->mouse_x = hwData->mouse_y = 0;
        hwData->mouse_buttons = 0;

        /* Simulate completion */
        if (isImplicit) {
            ADBManager_ImplicitRequestDone(mgr, 0, command, NULL);
        } else {
            ADBManager_ExplicitRequestDone(mgr, 0, command, NULL);
        }
    }

    return ADB_NO_ERROR;
}

/*
 * pollADBDevice - Poll ADB devices for auto-polling
 */
static void pollADBDevice(ADBManager* mgr) {
    if (!mgr) return;

    PlatformHardwareData* hwData = (PlatformHardwareData*)mgr->hardware.platformData;
    if (!hwData) return;

    /* In auto-polling mode, we cycle through active devices */
    static uint8_t currentPollAddress = ADB_KEYBOARD_ADDR;

    /* Check if device exists in device table */
    ADBDeviceEntry* entry;
    int result = ADBManager_GetDeviceInfo(mgr, currentPollAddress, entry);

    if (result == ADB_NO_ERROR) {
        /* Poll this device */
        uint8_t command = (currentPollAddress << 4) | ADB_TALK_CMD | 0;
        startADBRequest(mgr, command, NULL, 0, true);
    } else {
        /* No device at this address, just complete immediately */
        ADBManager_ImplicitRequestDone(mgr, 0, 0, NULL);
    }

    /* Move to next address for next poll */
    currentPollAddress++;
    if (currentPollAddress >= ADB_MAX_DEVICES) {
        currentPollAddress = ADB_KEYBOARD_ADDR;
    }
}

/*
 * resetADBBus - Reset the ADB bus
 */
static void resetADBBus(ADBManager* mgr) {
    if (!mgr) return;

    PlatformHardwareData* hwData = (PlatformHardwareData*)mgr->hardware.platformData;
    if (hwData) {
        memset(hwData->key_state, 0, sizeof(hwData->key_state));
        hwData->mouse_x = hwData->mouse_y = 0;
        hwData->mouse_buttons = 0;
        hwData->modifier_state = 0;
    }

    printf("ADB bus reset\n");
}

/*
 * initializePlatformHardware - Initialize platform-specific hardware
 */
static int initializePlatformHardware(PlatformHardwareData* hwData) {
    if (!hwData) return -1;

    hwData->running = true;
    clock_gettime(CLOCK_MONOTONIC, &hwData->last_poll);

#ifdef __linux__
    return initializeLinuxHardware(hwData);
#elif defined(_WIN32)
    return initializeWindowsHardware(hwData);
#elif defined(__APPLE__)
    return initializeMacOSHardware(hwData);
#else
    printf("Generic hardware initialization (no platform-specific support)\n");
    return 0;
#endif
}

/*
 * shutdownPlatformHardware - Clean up platform-specific hardware
 */
static void shutdownPlatformHardware(PlatformHardwareData* hwData) {
    if (!hwData) return;

    hwData->running = false;

#ifdef __linux__
    if (hwData->keyboard_fd >= 0) close(hwData->keyboard_fd);
    if (hwData->mouse_fd >= 0) close(hwData->mouse_fd);
    if (hwData->uinput_fd >= 0) close(hwData->uinput_fd);
#endif

#ifdef _WIN32
    if (hwData->message_window) {
        DestroyWindow(hwData->message_window);
    }
#endif

#ifdef __APPLE__
    if (hwData->hid_manager) {
        IOHIDManagerClose(hwData->hid_manager, kIOHIDOptionsTypeNone);
        CFRelease(hwData->hid_manager);
    }
#endif

    printf("Platform hardware shut down\n");
}

/*
 * pollInputDevices - Poll for input events from platform
 */
static int pollInputDevices(PlatformHardwareData* hwData, ADBManager* mgr) {
    if (!hwData || !mgr) return -1;

#ifdef __linux__
    return pollLinuxInputDevices(hwData, mgr);
#elif defined(_WIN32)
    return pollWindowsInputDevices(hwData, mgr);
#elif defined(__APPLE__)
    return pollMacOSInputDevices(hwData, mgr);
#else
    /* Generic implementation - no actual input polling */
    return 0;
#endif
}

/*
 * simulateADBTiming - Add timing delays to simulate original ADB timing
 */
static void simulateADBTiming(void) {
    /* Original ADB had specific timing requirements.
     * We add small delays to simulate this for compatibility.
     */
    struct timespec delay = {0, 100000}; /* 0.1ms */
    nanosleep(&delay, NULL);
}

#ifdef __linux__
/*
 * Linux-specific implementation using evdev
 */
static int initializeLinuxHardware(PlatformHardwareData* hwData) {
    printf("Initializing Linux hardware (evdev)...\n");

    /* In a real implementation, we would:
     * 1. Enumerate /dev/input/event* devices
     * 2. Open keyboard and mouse devices
     * 3. Set up uinput for generating events
     * 4. Configure device capabilities
     */

    hwData->keyboard_fd = -1; /* Would open actual keyboard device */
    hwData->mouse_fd = -1;    /* Would open actual mouse device */
    hwData->uinput_fd = -1;   /* Would open /dev/uinput */

    printf("Linux hardware initialized (stub implementation)\n");
    return 0;
}

static int pollLinuxInputDevices(PlatformHardwareData* hwData, ADBManager* mgr) {
    /* In a real implementation, we would:
     * 1. Use select() or poll() to check for input events
     * 2. Read struct input_event from device files
     * 3. Convert Linux keycodes to Mac keycodes
     * 4. Generate appropriate ADB events
     */

    /* Stub implementation - no actual polling */
    return 0;
}
#endif /* __linux__ */

#ifdef _WIN32
/*
 * Windows-specific implementation using Raw Input API
 */
static int initializeWindowsHardware(PlatformHardwareData* hwData) {
    printf("Initializing Windows hardware (Raw Input)...\n");

    /* In a real implementation, we would:
     * 1. Create a message-only window
     * 2. Register for raw input devices (keyboard, mouse)
     * 3. Set up message loop for input processing
     */

    printf("Windows hardware initialized (stub implementation)\n");
    return 0;
}

static int pollWindowsInputDevices(PlatformHardwareData* hwData, ADBManager* mgr) {
    /* In a real implementation, we would:
     * 1. Process Windows messages
     * 2. Handle WM_INPUT messages
     * 3. Convert Windows scan codes to Mac keycodes
     * 4. Generate appropriate ADB events
     */

    /* Stub implementation - process Windows messages without blocking */
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INPUT:
            /* Handle raw input */
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
#endif /* _WIN32 */

#ifdef __APPLE__
/*
 * macOS-specific implementation using IOKit HID Manager
 */
static int initializeMacOSHardware(PlatformHardwareData* hwData) {
    printf("Initializing macOS hardware (HID Manager)...\n");

    /* In a real implementation, we would:
     * 1. Create IOHIDManager
     * 2. Set up device matching for keyboards and mice
     * 3. Register input callbacks
     * 4. Schedule with run loop
     */

    hwData->hid_manager = NULL; /* Would create actual HID manager */

    printf("macOS hardware initialized (stub implementation)\n");
    return 0;
}

static int pollMacOSInputDevices(PlatformHardwareData* hwData, ADBManager* mgr) {
    /* In a real implementation, we would:
     * 1. Run the CFRunLoop to process HID events
     * 2. Convert HID usage codes to Mac keycodes
     * 3. Generate appropriate ADB events
     */

    /* Stub implementation - run run loop briefly */
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
    return 0;
}

static void handleHIDInput(void* context, IOReturn result, void* sender, IOHIDValueRef value) {
    /* Handle HID input events */
    if (result != kIOReturnSuccess) return;

    IOHIDElementRef element = IOHIDValueGetElement(value);
    uint32_t usage = IOHIDElementGetUsage(element);
    uint32_t usagePage = IOHIDElementGetUsagePage(element);
    int intValue = (int)IOHIDValueGetIntegerValue(value);

    /* Convert HID events to ADB events */
    /* This would involve mapping HID usage codes to Mac keycodes */
}
#endif /* __APPLE__ */

/*
 * Key code mapping utilities
 */

/* Convert platform-specific key codes to Mac ADB key codes */
static uint8_t convertToMacKeycode(int platformKeycode) {
    /* This is a simplified mapping - a full implementation would
     * have complete lookup tables for each platform.
     */

#ifdef __linux__
    /* Linux to Mac keycode mapping (simplified) */
    switch (platformKeycode) {
        case KEY_A: return 0x00;
        case KEY_S: return 0x01;
        case KEY_D: return 0x02;
        case KEY_F: return 0x03;
        case KEY_H: return 0x04;
        case KEY_G: return 0x05;
        case KEY_Z: return 0x06;
        case KEY_X: return 0x07;
        case KEY_C: return 0x08;
        case KEY_V: return 0x09;
        case KEY_B: return 0x0B;
        case KEY_Q: return 0x0C;
        case KEY_W: return 0x0D;
        case KEY_E: return 0x0E;
        case KEY_R: return 0x0F;
        case KEY_Y: return 0x10;
        case KEY_T: return 0x11;
        case KEY_1: return 0x12;
        case KEY_2: return 0x13;
        case KEY_3: return 0x14;
        case KEY_4: return 0x15;
        case KEY_6: return 0x16;
        case KEY_5: return 0x17;
        case KEY_EQUAL: return 0x18;
        case KEY_9: return 0x19;
        case KEY_7: return 0x1A;
        case KEY_MINUS: return 0x1B;
        case KEY_8: return 0x1C;
        case KEY_0: return 0x1D;
        case KEY_RIGHTBRACE: return 0x1E;
        case KEY_O: return 0x1F;
        case KEY_U: return 0x20;
        case KEY_LEFTBRACE: return 0x21;
        case KEY_I: return 0x22;
        case KEY_P: return 0x23;
        case KEY_ENTER: return 0x24;
        case KEY_L: return 0x25;
        case KEY_J: return 0x26;
        case KEY_APOSTROPHE: return 0x27;
        case KEY_K: return 0x28;
        case KEY_SEMICOLON: return 0x29;
        case KEY_BACKSLASH: return 0x2A;
        case KEY_COMMA: return 0x2B;
        case KEY_SLASH: return 0x2C;
        case KEY_N: return 0x2D;
        case KEY_M: return 0x2E;
        case KEY_DOT: return 0x2F;
        case KEY_TAB: return 0x30;
        case KEY_SPACE: return 0x31;
        case KEY_GRAVE: return 0x32;
        case KEY_BACKSPACE: return 0x33;
        case KEY_ESC: return 0x35;
        default: return 0xFF; /* Unknown key */
    }
#else
    /* For other platforms, return the keycode as-is for now */
    return (uint8_t)platformKeycode;
#endif
}

/*
 * Test/Debug Functions
 */

void ADBManager_SimulateKeypress(ADBManager* mgr, uint8_t keycode, bool isDown) {
    if (!mgr) return;

    uint8_t adbKeycode = isDown ? keycode : (keycode | 0x80);
    uint8_t data[1] = { adbKeycode };

    printf("Simulating key %s: 0x%02X\n", isDown ? "down" : "up", adbKeycode);

    /* Simulate keyboard input */
    ADBManager_ImplicitRequestDone(mgr, 1, (ADB_KEYBOARD_ADDR << 4) | ADB_TALK_CMD, data);
}

void ADBManager_SimulateMouseMove(ADBManager* mgr, int16_t deltaX, int16_t deltaY, bool buttonDown) {
    if (!mgr) return;

    uint8_t data[2];
    data[0] = (buttonDown ? 0x00 : 0x80) | (deltaY & 0x7F);
    data[1] = deltaX & 0x7F;

    printf("Simulating mouse: dx=%d, dy=%d, button=%s\n",
           deltaX, deltaY, buttonDown ? "down" : "up");

    /* Simulate mouse input */
    ADBManager_ImplicitRequestDone(mgr, 2, (ADB_MOUSE_ADDR << 4) | ADB_TALK_CMD, data);
}