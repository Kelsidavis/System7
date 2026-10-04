#include "EventManager/EventManagerInternal.h"
#include "EventManager/AppSwitcher.h"
#include "MemoryMgr/MemoryManager.h"
#include "Platform/PS2Input.h"
#include "Platform/x86/xhci.h"
#include "Platform/arm64/display.h"
#include "check.h"

/* Emulate DMA completions without exposing the driver's private virtqueue layout. */
#include "Platform/arm64/virtio_input.c"

static UInt16 producerIndex;
static EventRecord posted[8];
static unsigned postedCount;

uint32_t display_get_width(void) { return 640; }
uint32_t display_get_height(void) { return 480; }
void uart_puts(const char* text) { (void)text; }
void uart_putc(char character) { (void)character; }
void serial_puts(const char* text) { (void)text; }
void xhci_poll_hid_x86(void) {}
void dcache_invalidate_range(void* start, size_t length) { (void)start; (void)length; }
void DisposePtr(void* pointer) { (void)pointer; }
void AppSwitcher_CycleForward(void) {}
void AppSwitcher_CycleBackward(void) {}
void AppSwitcher_HandleKeyUp(void) {}
Boolean AppSwitcher_IsActive(void) { return false; }
UInt32 TickCount(void) { return 100; }

bool virtio_pci_find_device_from(uint16_t id, virtio_pci_device_t* device, uint8_t start)
{
    if (id != VIRTIO_DEV_INPUT || start != 0) return false;
    device->device = 1;
    return true;
}

bool virtio_pci_init_device(virtio_pci_device_t* device, uint64_t features)
{
    (void)device;
    (void)features;
    return true;
}

bool virtio_pci_setup_queue(virtio_pci_device_t* device, uint16_t queue,
                           struct virtq_desc* desc, struct virtq_avail* avail,
                           struct virtq_used* used, uint16_t size)
{
    (void)device;
    (void)queue;
    (void)desc;
    (void)avail;
    (void)used;
    return size == INPUT_QUEUE_SIZE;
}

void virtio_pci_device_ready(virtio_pci_device_t* device) { (void)device; }
void virtio_pci_notify_queue(virtio_pci_device_t* device, uint16_t queue)
{
    (void)device;
    (void)queue;
}

void serial_logf(SystemLogModule module, SystemLogLevel level, const char* format, ...)
{
    (void)module;
    (void)level;
    (void)format;
}

OSErr PostEventWithModifiers(EventMask type, UInt32 message, UInt16 modifiers)
{
    if (postedCount < sizeof(posted) / sizeof(posted[0])) {
        posted[postedCount].what = type;
        posted[postedCount].message = message;
        posted[postedCount].modifiers = modifiers;
    }
    ++postedCount;
    return noErr;
}

OSErr PostEvent(EventMask type, UInt32 message)
{
    return PostEventWithModifiers(type, message, GetModifierState());
}

static void InjectKey(UInt16 code, UInt32 value)
{
    struct input_device* device = &devices[0];
    UInt16 slot = producerIndex % INPUT_QUEUE_SIZE;
    device->event_buffers[slot] = (struct virtio_input_event){EV_KEY, code, value};
    device->eventq.used.ring[slot].id = slot;
    device->eventq.used.idx = ++producerIndex;
}

static int TestKeyMaps(void)
{
    _Alignas(UInt32) UInt8 storage[sizeof(KeyMap) + 2];
    UInt8* map = storage + 1;
    memset(storage, 0xa5, sizeof(storage));
    memset(map, 0, sizeof(KeyMap));
    for (UInt16 code = 0; code < 128; ++code) {
        KeyMapSetKey(map, code, true);
        for (UInt16 other = 0; other < 128; ++other) {
            CHECK(KeyMapHasKey(map, other) == (other == code), 1);
        }
        KeyMapSetKey(map, code, false);
    }
    KeyMapSetKey(map, 128, true);
    CHECK(!KeyMapHasKey(map, 128) && !KeyMapHasKey(map, UINT16_MAX), 2);
    CHECK(storage[0] == 0xa5 && storage[sizeof(storage) - 1] == 0xa5, 3);

    CHECK(InitKeyboardEvents() == noErr, 4);
    for (UInt16 code = 0; code < 128; ++code) {
        ResetKeyboardState();
        postedCount = 0;
        ProcessRawKeyboardEvent(code, true, 0, TickCount());
        CHECK(IsKeyDown(code), 5);
        GetKeys(map);
        CHECK(KeyMapHasKey(map, code), 6);
        CHECK(GetKeyboardState()->keyMap == GetKeyboardState()->currentKeyMap, 7);
        ProcessRawKeyboardEvent(code, false, 0, TickCount());
        CHECK(!IsKeyDown(code), 8);
    }
    ResetKeyboardState();
    ShutdownKeyboardEvents();
    return 0;
}

static int TestHALBitmap(void)
{
    _Alignas(UInt32) UInt8 storage[sizeof(KeyMap) + 2];
    memset(storage, 0xa5, sizeof(storage));
    CHECK(GetPS2KeyboardState(storage + 1), 1);
    for (unsigned i = 1; i <= sizeof(KeyMap); ++i) CHECK(storage[i] == 0, 2);
    CHECK(storage[0] == 0xa5 && storage[sizeof(storage) - 1] == 0xa5, 3);
    CHECK(!GetPS2KeyboardState(NULL), 4);
    return 0;
}

static int TestChord(void)
{
    postedCount = 0;
    InjectKey(KEY_LEFTMETA, 1);
    InjectKey(KEY_N, 1);
    InjectKey(KEY_N, 0);
    InjectKey(KEY_LEFTMETA, 0);
    ProcessModernInput();
    CHECK(postedCount == 2, 1);
    CHECK(posted[0].what == keyDown && posted[1].what == keyUp, 2);
    CHECK((posted[0].modifiers & cmdKey) && (posted[1].modifiers & cmdKey), 3);
    CHECK((posted[0].message & keyCodeMask) == 0x2d00, 4);
    CHECK(GetModifierState() == 0 && GetPS2Modifiers() == 0, 5);
    return 0;
}

static int TestModifiers(void)
{
    UInt8 code;
    Boolean pressed;
    InjectKey(KEY_LEFTSHIFT, 1);
    InjectKey(KEY_RIGHTSHIFT, 1);
    InjectKey(KEY_LEFTSHIFT, 0);
    PollPS2Input();
    CHECK(GetPS2Modifiers() == (shiftKey | rightShiftKey), 1);
    CHECK(!PS2_DequeueKeyTransition(NULL, &pressed), 2);
    CHECK(!PS2_DequeueKeyTransition(&code, NULL), 3);
    ProcessModernInput();
    CHECK(GetModifierState() == (shiftKey | rightShiftKey), 4);
    CHECK(GetKeyboardState()->modifiers == GetModifierState(), 5);
    InjectKey(KEY_RIGHTSHIFT, 0);
    InjectKey(KEY_RIGHTMETA, 1);
    InjectKey(KEY_RIGHTCTRL, 1);
    InjectKey(KEY_RIGHTALT, 1);
    ProcessModernInput();
    UInt16 expected = cmdKey | controlKey | rightControlKey | optionKey | rightOptionKey;
    CHECK(GetPS2Modifiers() == expected && GetModifierState() == expected, 6);
    InjectKey(KEY_RIGHTMETA, 0);
    InjectKey(KEY_RIGHTCTRL, 0);
    InjectKey(KEY_RIGHTALT, 0);
    ProcessModernInput();
    CHECK(GetModifierState() == 0 && GetPS2Modifiers() == 0, 7);
    return 0;
}

static int TestCapsLockAndWrap(void)
{
    InjectKey(KEY_CAPSLOCK, 1);
    InjectKey(KEY_CAPSLOCK, 2);
    InjectKey(KEY_CAPSLOCK, 1);
    InjectKey(KEY_CAPSLOCK, 0);
    ProcessModernInput();
    CHECK(GetModifierState() == alphaLock, 1);
    postedCount = 0;
    InjectKey(KEY_A, 1);
    InjectKey(KEY_A, 0);
    ProcessModernInput();
    CHECK(postedCount == 2 && (posted[0].message & charCodeMask) == 'A', 2);
    CHECK(posted[0].modifiers == alphaLock && posted[1].modifiers == alphaLock, 3);
    InjectKey(KEY_CAPSLOCK, 1);
    InjectKey(KEY_CAPSLOCK, 0);
    ProcessModernInput();
    CHECK(GetModifierState() == 0, 4);

    for (unsigned i = 0; i < 100; ++i) {
        postedCount = 0;
        InjectKey(KEY_A, 1);
        InjectKey(KEY_A, 2);
        InjectKey(KEY_A, 0);
        InjectKey(UINT16_MAX, 1);
        ProcessModernInput();
        CHECK(postedCount == 2 && (posted[0].message & charCodeMask) == 'a', 5);
        CHECK(!IsKeyDown(0), 6);
    }
    return 0;
}

int main(void)
{
    int result = TestKeyMaps();
    if (result) return result;
    CHECK(InitPS2Controller() && PS2_IsInitialized(), 1);
    result = TestHALBitmap();
    if (result) return result;
    CHECK(InitModernInput("PS2") == noErr, 2);
    result = TestChord();
    result |= TestModifiers();
    result |= TestCapsLockAndWrap();
    ShutdownModernInput();
    return result;
}
