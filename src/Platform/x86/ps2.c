/*
 * PS2Controller.c - PS/2 Keyboard and Mouse Driver
 *
 * Provides input support for QEMU's emulated PS/2 devices
 * Integrates with the Mac OS System 7.1 Event Manager
 */

#include "SystemTypes.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventManager.h"
#include "Platform/PS2Input.h"
#include "PS2Controller.h"
#include "Platform/include/input.h"
#include <stdint.h>
#include "Platform/PlatformLogging.h"
#include <string.h>

/* Event modifier key constants */
#define shiftKey    0x0200
#define alphaLock   0x0400
#define optionKey   0x0800
#define controlKey  0x1000

/* PS/2 Controller ports */
#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

/* PIC ports for IRQ unmasking */
#define PIC1_COMMAND     0x20
#define PIC1_DATA        0x21
#define PIC2_COMMAND     0xA0
#define PIC2_DATA        0xA1

/* PS/2 Controller status bits */
#define PS2_STATUS_OUTPUT_FULL  0x01
#define PS2_STATUS_INPUT_FULL   0x02
#define PS2_STATUS_SYSTEM_FLAG  0x04
#define PS2_STATUS_COMMAND      0x08
#define PS2_STATUS_AUX          0x20  /* data from mouse (AUX) when set */
#define PS2_STATUS_TIMEOUT_ERR  0x40
#define PS2_STATUS_PARITY_ERR   0x80

/* PS/2 Controller commands */
#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_DISABLE_PORT2   0xA7
#define PS2_CMD_ENABLE_PORT2    0xA8
#define PS2_CMD_TEST_PORT2      0xA9
#define PS2_CMD_TEST_CONTROLLER 0xAA
#define PS2_CMD_TEST_PORT1      0xAB
#define PS2_CMD_DISABLE_PORT1   0xAD
#define PS2_CMD_ENABLE_PORT1    0xAE
#define PS2_CMD_WRITE_PORT2     0xD4

/* PS/2 Device commands */
#define PS2_DEV_RESET           0xFF
#define PS2_DEV_ENABLE_SCAN     0xF4
#define PS2_DEV_DISABLE_SCAN    0xF5
#define PS2_DEV_SET_DEFAULTS    0xF6
#define PS2_DEV_IDENTIFY        0xF2

/* Mouse commands */
#define PS2_MOUSE_SET_SAMPLE    0xF3
#define PS2_MOUSE_ENABLE_DATA   0xF4
#define PS2_MOUSE_SET_DEFAULTS  0xF6
#define PS2_MOUSE_SET_SCALING   0xE6
#define PS2_MOUSE_SET_RESOLUTION 0xE8

/* Keyboard scan code sets */
#define SCAN_CODE_SET_1         1
#define SCAN_CODE_SET_2         2

/* External functions */
/* GetNextEvent and PostEvent declared in EventManager.h */
extern UInt32 TickCount(void);

/* External framebuffer dimensions from main.c */
extern uint32_t fb_width;
extern uint32_t fb_height;

/* I/O port functions */
#include "Platform/include/io.h"

#define inb(port) hal_inb(port)
#define outb(port, value) hal_outb(port, value)

/* Global state */
static Boolean g_ps2Initialized = false;
static Boolean g_mouseEnabled = false;
static Boolean g_keyboardEnabled = false;
static Boolean g_irqDriven = false;

/* Global mouse position - shared with Event Manager
 * Note: Initialized to center of default 800x600 screen, will be adjusted
 * to actual screen center once framebuffer dimensions are known */
Point g_mousePos = {400, 300};

/* Mouse state - exported for cursor drawing */
struct {
    int16_t x;
    int16_t y;
    uint8_t buttons;
    uint8_t packet[3];
    uint8_t packet_index;
} g_mouseState = {400, 300, 0, {0, 0, 0}, 0};

/* Get raw mouse button state - platform-independent interface */
uint8_t GetMouseButtons(void) {
    return g_mouseState.buttons;
}

void UpdateMouseStateDelta(SInt16 dx, SInt16 dy, UInt8 buttons) {
    g_mouseState.x += dx;
    g_mouseState.y += dy;

    if (g_mouseState.x < 0) g_mouseState.x = 0;
    if (g_mouseState.y < 0) g_mouseState.y = 0;
    if (fb_width > 0 && g_mouseState.x >= (int16_t)fb_width) g_mouseState.x = fb_width - 1;
    if (fb_height > 0 && g_mouseState.y >= (int16_t)fb_height) g_mouseState.y = fb_height - 1;

    g_mouseState.buttons = buttons;
    g_mousePos.h = g_mouseState.x;
    g_mousePos.v = g_mouseState.y;
}

void UpdateMouseStateAbsolute(SInt16 x, SInt16 y, UInt8 buttons) {
    g_mouseState.x = x;
    g_mouseState.y = y;

    if (g_mouseState.x < 0) g_mouseState.x = 0;
    if (g_mouseState.y < 0) g_mouseState.y = 0;
    if (fb_width > 0 && g_mouseState.x >= (int16_t)fb_width) g_mouseState.x = fb_width - 1;
    if (fb_height > 0 && g_mouseState.y >= (int16_t)fb_height) g_mouseState.y = fb_height - 1;

    g_mouseState.buttons = buttons;
    g_mousePos.h = g_mouseState.x;
    g_mousePos.v = g_mouseState.y;
}

/* Keyboard state */
typedef struct {
    UInt32 keyMap[4];
    Boolean e0Prefix;
    Boolean e1Prefix;
    UInt8 e1BytesRemaining;
    Boolean leftShift;
    Boolean rightShift;
    Boolean leftOption;
    Boolean rightOption;
    Boolean leftControl;
    Boolean rightControl;
    Boolean leftCommand;
    Boolean rightCommand;
    Boolean capsLockLatched;
} Ps2KeyboardState;

static Ps2KeyboardState g_keyboardState = {
    {0, 0, 0, 0},
    false,
    false,
    0,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false,
    false
};

static void ResetKeyboardState(void)
{
    memset(&g_keyboardState, 0, sizeof(g_keyboardState));
}


/* Wait for PS/2 controller ready for input */
static Boolean ps2_wait_input(void) {
    int timeout = 100000;
    while (timeout--) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0) {
            return true;
        }
    }
    return false;
}

/* Wait for PS/2 controller output available */
static Boolean ps2_wait_output(void) {
    int timeout = 100000;
    while (timeout--) {
        if (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
            return true;
        }
    }
    return false;
}

/* Send command to PS/2 controller */
static void ps2_send_command(uint8_t cmd) {
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, cmd);
}

/* Send data to PS/2 device */
static void ps2_send_data(uint8_t data) {
    ps2_wait_input();
    outb(PS2_DATA_PORT, data);
}

/* Read data from PS/2 device */
static uint8_t ps2_read_data(void) {
    ps2_wait_output();
    return inb(PS2_DATA_PORT);
}

/* Send command to mouse (via second PS/2 port) */
static Boolean ps2_mouse_command(uint8_t cmd) {
    /* CRITICAL FIX: Use 0xD4 prefix for ALL mouse commands */
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, 0xD4);  /* Write to aux port command */
    ps2_wait_input();
    outb(PS2_DATA_PORT, cmd);      /* The actual mouse command */

    /* Wait for ACK */
    if (ps2_wait_output()) {
        uint8_t response = ps2_read_data();
        /* PLATFORM_LOG_DEBUG("MOUSE CMD 0x%02x response: 0x%02x\n", cmd, response); */
        return (response == 0xFA); /* ACK */
    }
    /* PLATFORM_LOG_DEBUG("MOUSE CMD 0x%02x: NO RESPONSE\n", cmd); */
    return false;
}

static const uint8_t kUnmappedKey = 0xFF;

typedef struct {
    UInt8 scan;
    UInt8 mac;
} ScanMapEntry;

static const ScanMapEntry g_set1BaseMap[] = {
    {0x01, kScanEscape}, {0x02, 0x12}, {0x03, 0x13}, {0x04, 0x14}, {0x05, 0x15},
    {0x06, 0x17}, {0x07, 0x16}, {0x08, 0x1A}, {0x09, 0x1C}, {0x0A, 0x19},
    {0x0B, 0x1D}, {0x0C, 0x1B}, {0x0D, 0x18}, {0x0E, kScanDelete}, {0x0F, kScanTab},
    {0x10, 0x0C}, {0x11, 0x0D}, {0x12, 0x0E}, {0x13, 0x0F}, {0x14, 0x11},
    {0x15, 0x10}, {0x16, 0x20}, {0x17, 0x22}, {0x18, 0x1F}, {0x19, 0x23},
    {0x1A, 0x21}, {0x1B, 0x1E}, {0x1C, kScanReturn}, {0x1D, kScanControl},
    {0x1E, 0x00}, {0x1F, 0x01}, {0x20, 0x02}, {0x21, 0x03}, {0x22, 0x05},
    {0x23, 0x04}, {0x24, 0x26}, {0x25, 0x28}, {0x26, 0x25}, {0x27, 0x29},
    {0x28, 0x27}, {0x29, 0x32}, {0x2A, kScanShift}, {0x2B, 0x2A}, {0x2C, 0x06},
    {0x2D, 0x07}, {0x2E, 0x08}, {0x2F, 0x09}, {0x30, 0x0B}, {0x31, 0x2D},
    {0x32, 0x2E}, {0x33, 0x2B}, {0x34, 0x2F}, {0x35, 0x2C}, {0x36, kScanRightShift},
    {0x37, 0x43}, {0x38, kScanOption}, {0x39, kScanSpace}, {0x3A, kScanCapsLock},
    {0x3B, 0x7A}, {0x3C, 0x78}, {0x3D, 0x63}, {0x3E, 0x76}, {0x3F, 0x60},
    {0x40, 0x61}, {0x41, 0x62}, {0x42, 0x64}, {0x43, 0x65}, {0x44, 0x6D},
    {0x45, 0x47}, {0x46, 0x71}, {0x47, 0x59}, {0x48, 0x5B}, {0x49, 0x5C},
    {0x4A, 0x4E}, {0x4B, 0x56}, {0x4C, 0x57}, {0x4D, 0x58}, {0x4E, 0x45},
    {0x4F, 0x53}, {0x50, 0x54}, {0x51, 0x55}, {0x52, 0x52}, {0x53, 0x41},
    {0x57, 0x67}, {0x58, 0x6F}, {0x76, kScanEscape},
    {0xFF, kUnmappedKey}
};

static const ScanMapEntry g_set1ExtendedMap[] = {
    {0x11, kScanRightOption}, {0x14, kScanRightControl}, {0x1C, 0x4C},
    {0x35, 0x4B}, {0x37, 0x69}, {0x38, kScanRightOption}, {0x47, 0x73},
    {0x48, kScanUpArrow}, {0x49, 0x74}, {0x4B, kScanLeftArrow},
    {0x4D, kScanRightArrow}, {0x4F, 0x77}, {0x50, kScanDownArrow},
    {0x51, 0x79}, {0x52, 0x72}, {0x53, 0x75}, {0x5B, 0x37},
    {0x5C, 0x36}, {0x5D, 0x6E}, {0x5E, 0x6D}, {0x5F, 0x6F},
    {0xFF, kUnmappedKey}
};

static UInt8 map_set1_scancode_to_mac(uint8_t scanCode, Boolean extended)
{
    const ScanMapEntry* table = extended ? g_set1ExtendedMap : g_set1BaseMap;
    for (size_t i = 0; table[i].scan != 0xFF; ++i) {
        if (table[i].scan == scanCode) {
            return table[i].mac;
        }
    }
    return kUnmappedKey;
}

static void UpdateKeyMapState(uint8_t macCode, Boolean isPressed)
{
    if (macCode >= 128) {
        return;
    }

    UInt16 arrayIndex = macCode / 32;
    UInt16 bitIndex = macCode % 32;
    UInt32 mask = (1U << bitIndex);

    if (isPressed) {
        g_keyboardState.keyMap[arrayIndex] |= mask;
    } else {
        g_keyboardState.keyMap[arrayIndex] &= ~mask;
    }
}


/* Process keyboard scancode */
static void process_keyboard_scancode(uint8_t scancode)
{
    if (scancode == 0xE0) {
        g_keyboardState.e0Prefix = true;
        return;
    }

    if (scancode == 0xE1) {
        g_keyboardState.e1Prefix = true;
        g_keyboardState.e1BytesRemaining = 5; /* Pause key sequence */
        return;
    }

    if (g_keyboardState.e1Prefix) {
        if (g_keyboardState.e1BytesRemaining > 0) {
            g_keyboardState.e1BytesRemaining--;
        }
        if (g_keyboardState.e1BytesRemaining == 0) {
            g_keyboardState.e1Prefix = false;
        }
        return;
    }

    Boolean extended = g_keyboardState.e0Prefix;
    g_keyboardState.e0Prefix = false;

    Boolean isRelease = (scancode & 0x80) != 0;
    uint8_t baseCode = scancode & 0x7F;

    uint8_t macCode = map_set1_scancode_to_mac(baseCode, extended);
    if (macCode == kUnmappedKey) {
        return;
    }

    Boolean isPressed = !isRelease;
    UpdateKeyMapState(macCode, isPressed);

    switch (macCode) {
        case kScanShift:
            g_keyboardState.leftShift = isPressed;
            break;
        case kScanRightShift:
            g_keyboardState.rightShift = isPressed;
            break;
        case kScanOption:
            g_keyboardState.leftOption = isPressed;
            break;
        case kScanRightOption:
            g_keyboardState.rightOption = isPressed;
            break;
        case kScanControl:
            g_keyboardState.leftControl = isPressed;
            break;
        case kScanRightControl:
            g_keyboardState.rightControl = isPressed;
            break;
        case kScanCommand:
            g_keyboardState.leftCommand = isPressed;
            break;
        case 0x36: /* Right Command */
            g_keyboardState.rightCommand = isPressed;
            break;
        case kScanCapsLock:
            if (isPressed) {
                g_keyboardState.capsLockLatched = !g_keyboardState.capsLockLatched;
            }
            break;
        default:
            break;
    }
}

/* Process mouse packet */
static void process_mouse_packet(void) {
    uint8_t status = g_mouseState.packet[0];

    /* Check sync bit (bit 3 must be 1) for proper packet alignment */
    if (!(status & 0x08)) {
        /* Lost sync - discard packet and resync */
        g_mouseState.packet_index = 0;
        /* PLATFORM_LOG_DEBUG("Mouse packet lost sync, resetting\n"); */
        return;
    }

    int16_t dx = (int8_t)g_mouseState.packet[1];
    int16_t dy = (int8_t)g_mouseState.packet[2];

    /* Check for overflow */
    if (status & 0x40 || status & 0x80) {
        g_mouseState.packet_index = 0;
        /* PLATFORM_LOG_DEBUG("MOUSE OVERFLOW detected\n"); */
        return;
    }

    uint8_t new_buttons = status & 0x07;
    uint8_t old_buttons = g_mouseState.buttons;

    UpdateMouseStateDelta(dx, -dy, new_buttons); /* PS/2 Y is inverted */

    /* Check button state changes */
    if (new_buttons != old_buttons) {
        /* Update button state only - let ModernInput handle event posting */
        g_mouseState.buttons = new_buttons;
    }

    /* Reset packet index */
    g_mouseState.packet_index = 0;
}

/* Initialize PS/2 keyboard */
static Boolean init_keyboard(void) {
    PLATFORM_LOG_DEBUG("Initializing PS/2 keyboard...\n");

    /* Reset keyboard */
    ps2_send_data(PS2_DEV_RESET);
    if (ps2_wait_output()) {
        uint8_t response = ps2_read_data();
        if (response != 0xFA) {
            /* PLATFORM_LOG_DEBUG("Keyboard reset failed: 0x%02x\n", response); */
            return false;
        }
    }

    /* Wait for self-test result */
    if (ps2_wait_output()) {
        uint8_t result = ps2_read_data();
        if (result != 0xAA) {
            /* PLATFORM_LOG_DEBUG("Keyboard self-test failed: 0x%02x\n", result); */
            return false;
        }
    }

    /* Enable scanning */
    ps2_send_data(PS2_DEV_ENABLE_SCAN);
    if (ps2_wait_output()) {
        uint8_t response = ps2_read_data();
        if (response != 0xFA) {
            /* PLATFORM_LOG_DEBUG("Failed to enable keyboard scanning: 0x%02x\n", response); */
            return false;
        }
    }

    ResetKeyboardState();

    g_keyboardEnabled = true;
    PLATFORM_LOG_DEBUG("PS/2 keyboard initialized\n");
    return true;
}

/* Initialize PS/2 mouse */
static Boolean init_mouse(void) {
    /* PLATFORM_LOG_DEBUG("Initializing PS/2 mouse...\n"); */

    /* Enable second PS/2 port for mouse */
    /* PLATFORM_LOG_DEBUG("MOUSE: Enabling port 2...\n"); */
    ps2_send_command(PS2_CMD_ENABLE_PORT2);

    /* Reset mouse */
    /* PLATFORM_LOG_DEBUG("MOUSE: Sending reset command (0xFF)...\n"); */
    if (!ps2_mouse_command(PS2_DEV_RESET)) {
        /* PLATFORM_LOG_DEBUG("MOUSE: Reset failed - no ACK\n"); */
        return false;
    }
    /* PLATFORM_LOG_DEBUG("MOUSE: Reset ACK received\n"); */

    /* Wait for self-test result */
    /* PLATFORM_LOG_DEBUG("MOUSE: Waiting for self-test result...\n"); */
    if (ps2_wait_output()) {
        uint8_t result = ps2_read_data();
        /* PLATFORM_LOG_DEBUG("MOUSE: Self-test result: 0x%02x\n", result); */
        if (result != 0xAA) {
            /* PLATFORM_LOG_DEBUG("MOUSE: Self-test FAILED (expected 0xAA)\n"); */
            return false;
        }
        /* PLATFORM_LOG_DEBUG("MOUSE: Self-test PASSED\n"); */
    } else {
        /* PLATFORM_LOG_DEBUG("MOUSE: Self-test timeout\n"); */
        return false;
    }

    /* Read and discard mouse ID */
    /* PLATFORM_LOG_DEBUG("MOUSE: Reading mouse ID...\n"); */

    if (ps2_wait_output()) {
        uint8_t id = ps2_read_data();
        (void)id;
        /* PLATFORM_LOG_DEBUG("MOUSE: Mouse ID: 0x%02x\n", id); */
    } else {
        /* PLATFORM_LOG_DEBUG("MOUSE: No mouse ID received\n"); */
    }

    /* Set defaults */
    /* PLATFORM_LOG_DEBUG("MOUSE: Setting defaults (0xF6)...\n"); */
    if (!ps2_mouse_command(PS2_MOUSE_SET_DEFAULTS)) {
        /* PLATFORM_LOG_DEBUG("MOUSE: Failed to set defaults - no ACK\n"); */
        return false;
    }
    /* PLATFORM_LOG_DEBUG("MOUSE: Defaults set\n"); */

    /* Enable data reporting */
    /* PLATFORM_LOG_DEBUG("MOUSE: Enabling data reporting (0xF4)...\n"); */
    if (!ps2_mouse_command(PS2_MOUSE_ENABLE_DATA)) {
        /* PLATFORM_LOG_DEBUG("MOUSE: Failed to enable data reporting - no ACK\n"); */
        return false;
    }
    /* PLATFORM_LOG_DEBUG("MOUSE: Data reporting enabled\n"); */

    /* Verify port 2 is enabled in controller configuration */
    ps2_send_command(PS2_CMD_READ_CONFIG);
    uint8_t final_config = ps2_read_data();
    /* PLATFORM_LOG_DEBUG("MOUSE: Final controller config: 0x%02x\n", final_config); */
    /* PLATFORM_LOG_DEBUG("MOUSE: Port 2 enabled: %s\n", (final_config & 0x20) ? "NO (disabled!)" : "YES"); */
    /* PLATFORM_LOG_DEBUG("MOUSE: Port 2 interrupt (bit 1): %s\n", (final_config & 0x02) ? "YES" : "NO (PROBLEM!)"); */

    /* If bit 1 is not set, try to set it again */
    if (!(final_config & 0x02)) {
        /* PLATFORM_LOG_DEBUG("MOUSE: WARNING - IRQ12 not enabled! Attempting to fix...\n"); */
        final_config |= 0x02;
        ps2_send_command(PS2_CMD_WRITE_CONFIG);
        ps2_send_data(final_config);

        /* Verify again */
        ps2_send_command(PS2_CMD_READ_CONFIG);
        final_config = ps2_read_data();
        /* PLATFORM_LOG_DEBUG("MOUSE: Config after fix attempt: 0x%02x\n", final_config); */
    }

    g_mouseEnabled = true;
    /* PLATFORM_LOG_DEBUG("PS/2 mouse initialized successfully\n"); */
    return true;
}

/* Initialize PS/2 controller */
Boolean InitPS2Controller(void) {
    if (g_ps2Initialized) return true;

    /* PLATFORM_LOG_DEBUG("Initializing PS/2 controller...\n"); */

    /* CRITICAL FIX #1: Unmask IRQ12 and IRQ2 in PIC */
    /* PLATFORM_LOG_DEBUG("PS2: Unmasking IRQ12 and IRQ2 in PIC...\n"); */
    uint8_t pic1_mask = inb(PIC1_DATA);
    uint8_t pic2_mask = inb(PIC2_DATA);
    /* PLATFORM_LOG_DEBUG("PS2: PIC1 mask before: 0x%02x, PIC2 mask before: 0x%02x\n", pic1_mask, pic2_mask); */

    pic1_mask &= ~0x04;  /* Unmask IRQ2 (cascade) */
    pic2_mask &= ~0x10;  /* Unmask IRQ12 (mouse) - bit 4 for IRQ12 */

    outb(PIC1_DATA, pic1_mask);
    outb(PIC2_DATA, pic2_mask);

    pic1_mask = inb(PIC1_DATA);
    pic2_mask = inb(PIC2_DATA);
    /* PLATFORM_LOG_DEBUG("PS2: PIC1 mask after: 0x%02x, PIC2 mask after: 0x%02x\n", pic1_mask, pic2_mask); */

    /* Disable both PS/2 ports during initialization */
    ps2_send_command(PS2_CMD_DISABLE_PORT1);
    ps2_send_command(PS2_CMD_DISABLE_PORT2);

    /* Flush output buffer */
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
        inb(PS2_DATA_PORT);
    }

    /* Get controller configuration */
    ps2_send_command(PS2_CMD_READ_CONFIG);
    uint8_t config = ps2_read_data();
    /* PLATFORM_LOG_DEBUG("PS2: Initial config byte: 0x%02x\n", config); */

    /* CRITICAL FIX #2: Set bit 1 to enable IRQ12 for mouse */
    config |= 0x02;  /* Enable mouse IRQ (bit 1) */
    config |= 0x01;  /* Enable keyboard IRQ (bit 0) */
    config &= ~0x20; /* Enable AUX port (clear bit 5) */
    config |= 0x40;  /* Enable set 1 translation */

    /* PLATFORM_LOG_DEBUG("PS2: New config byte: 0x%02x\n", config); */

    /* Write configuration back */
    ps2_send_command(PS2_CMD_WRITE_CONFIG);
    ps2_send_data(config);

    /* Perform controller self-test */
    ps2_send_command(PS2_CMD_TEST_CONTROLLER);
    if (ps2_wait_output()) {
        uint8_t result = ps2_read_data();
        if (result != 0x55) {
            /* PLATFORM_LOG_DEBUG("PS/2 controller self-test failed: 0x%02x\n", result); */
            return false;
        }
    }

    /* Test first PS/2 port */
    ps2_send_command(PS2_CMD_TEST_PORT1);
    if (ps2_wait_output()) {
        uint8_t result = ps2_read_data();
        if (result != 0x00) {
            /* PLATFORM_LOG_DEBUG("PS/2 port 1 test failed: 0x%02x\n", result); */
            return false;
        }
    }

    /* Test second PS/2 port */
    ps2_send_command(PS2_CMD_TEST_PORT2);
    if (ps2_wait_output()) {
        uint8_t result = ps2_read_data();
        if (result != 0x00) {
            /* PLATFORM_LOG_DEBUG("PS/2 port 2 test failed: 0x%02x\n", result); */
            /* Mouse is optional, continue anyway */
        }
    }

    /* Enable first PS/2 port */
    ps2_send_command(PS2_CMD_ENABLE_PORT1);

    /* Initialize keyboard */
    if (!init_keyboard()) {
        PLATFORM_LOG_DEBUG("Warning: Keyboard initialization failed\n");
    }

    /* Enable second PS/2 port for mouse */
    ps2_send_command(PS2_CMD_ENABLE_PORT2);

    /* Initialize mouse */
    if (!init_mouse()) {
        /* PLATFORM_LOG_DEBUG("Warning: Mouse initialization failed\n"); */
    }

    /* Center mouse cursor at actual screen center (not hardcoded) */
    if (fb_width > 0 && fb_height > 0) {
        g_mouseState.x = fb_width / 2;
        g_mouseState.y = fb_height / 2;
        g_mousePos.h = g_mouseState.x;
        g_mousePos.v = g_mouseState.y;
    }

    g_ps2Initialized = true;
    /* PLATFORM_LOG_DEBUG("PS/2 controller initialized\n"); */
    return true;
}

/* Poll for PS/2 input (call this regularly) */
void PollPS2Input(void) {
    extern void serial_puts(const char* str);
    if (!g_ps2Initialized) return;

    static int mouse_byte_count = 0;
    static int call_count = 0;
    static int packet_count = 0;

    /* First call notification */
    if (call_count == 0) {
        PLATFORM_LOG_DEBUG("PS2: PollPS2Input first call!\n");
    }

    /* Disabled polling debug output to reduce serial log noise */
    call_count++;

    /* Drain the controller completely this tick */
    for (;;) {
        uint8_t status = inb(PS2_STATUS_PORT);
        if (!(status & PS2_STATUS_OUTPUT_FULL)) {
            break; /* nothing left to read */
        }

        uint8_t data = inb(PS2_DATA_PORT);

        if (status & PS2_STATUS_AUX) {
            if (!hal_input_ps2_mouse_active()) {
                continue;
            }
            /* --- Mouse byte --- */
            mouse_byte_count++;
            /* PLATFORM_LOG_DEBUG("POLL: Got mouse byte 0x%02x (status=0x%02x) idx=%d enabled=%d count=%d\n",
                          data, status, g_mouseState.packet_index, g_mouseEnabled, mouse_byte_count); */

            if (!g_mouseEnabled) {
                continue; /* ignore until fully enabled */
            }

            /* Enforce sync only when starting a new packet; don't exit polling */
            if (g_mouseState.packet_index == 0 && !(data & 0x08)) {
                /* Not a valid first byte; wait for one that has sync bit set */
                continue; /* (was 'return' before; that aborted the drain) */
            }

            g_mouseState.packet[g_mouseState.packet_index++] = data;

            if (g_mouseState.packet_index >= 3) {
                packet_count++;
                process_mouse_packet(); /* resets packet_index to 0 */
            }
        } else {
            /* --- Keyboard byte --- */
            if (g_keyboardEnabled) {
                process_keyboard_scancode(data);
            }
        }
    }
}

/* Get current mouse position - returns in global (screen) coordinates */
void GetMouse(Point* mouseLoc) {
    if (mouseLoc) {
        /* g_mouseState is in global coordinates */
        mouseLoc->h = g_mouseState.x;
        mouseLoc->v = g_mouseState.y;
    }
}

/* Button() moved to MouseEvents.c - reads gCurrentButtons instead of hardware */


/* Get current keyboard modifiers as Event Manager modifier flags */
UInt16 GetPS2Modifiers(void)
{
    UInt16 modifiers = 0;

    if (g_keyboardState.leftCommand || g_keyboardState.rightCommand) {
        modifiers |= cmdKey;
    }
    if (g_keyboardState.leftShift || g_keyboardState.rightShift) {
        modifiers |= shiftKey;
    }
    if (g_keyboardState.rightShift) {
        modifiers |= rightShiftKey;
    }
    if (g_keyboardState.leftOption || g_keyboardState.rightOption) {
        modifiers |= optionKey;
    }
    if (g_keyboardState.rightOption) {
        modifiers |= rightOptionKey;
    }
    if (g_keyboardState.leftControl || g_keyboardState.rightControl) {
        modifiers |= controlKey;
    }
    if (g_keyboardState.rightControl) {
        modifiers |= rightControlKey;
    }
    if (g_keyboardState.capsLockLatched) {
        modifiers |= alphaLock;
    }

    return modifiers;
}


/* Get keyboard state for Event Manager */
Boolean GetPS2KeyboardState(KeyMap keyMap)
{
    if (!g_keyboardEnabled || keyMap == NULL) {
        return false;
    }

    memcpy(keyMap, g_keyboardState.keyMap, sizeof(KeyMap));
    return true;
}

void PS2_SetIRQDriven(Boolean enabled)
{
    g_irqDriven = enabled;
}

Boolean PS2_IsIRQDriven(void)
{
    return g_irqDriven;
}

Boolean PS2_IsInitialized(void)
{
    return g_ps2Initialized;
}
