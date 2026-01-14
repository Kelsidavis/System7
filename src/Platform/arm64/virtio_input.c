/*
 * VirtIO Input Driver for ARM64
 * Implements virtio-input device interface for keyboard/mouse in QEMU
 * Supports both PCI and MMIO transports
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uart.h"
#include "virtio_input.h"
#include "virtio_pci.h"
#include "SystemTypes.h"

/* VirtIO MMIO registers */
#define VIRTIO_MMIO_MAGIC           0x000
#define VIRTIO_MMIO_VERSION         0x004
#define VIRTIO_MMIO_DEVICE_ID       0x008
#define VIRTIO_MMIO_VENDOR_ID       0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_QUEUE_SEL       0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034
#define VIRTIO_MMIO_QUEUE_NUM       0x038
#define VIRTIO_MMIO_QUEUE_READY     0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK   0x064
#define VIRTIO_MMIO_STATUS          0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW  0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW  0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4

/* VirtIO MMIO device region (for QEMU virt machine) */
#define VIRTIO_MMIO_BASE_START      0x0a000000
#define VIRTIO_MMIO_SLOT_SIZE       0x00000200

/* VirtIO device IDs */
#define VIRTIO_ID_INPUT     18

/* VirtIO status bits */
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_FAILED        128

/* VirtIO descriptor flags */
#define VIRTQ_DESC_F_NEXT       1
#define VIRTQ_DESC_F_WRITE      2

/* Linux input event types (evdev) */
#define EV_SYN      0x00
#define EV_KEY      0x01
#define EV_REL      0x02
#define EV_ABS      0x03

/* Relative axes */
#define REL_X       0x00
#define REL_Y       0x01

/* Absolute axes */
#define ABS_X       0x00
#define ABS_Y       0x01

/* Key codes (subset) */
#define KEY_ESC         1
#define KEY_1           2
#define KEY_2           3
#define KEY_3           4
#define KEY_4           5
#define KEY_5           6
#define KEY_6           7
#define KEY_7           8
#define KEY_8           9
#define KEY_9           10
#define KEY_0           11
#define KEY_MINUS       12
#define KEY_EQUAL       13
#define KEY_BACKSPACE   14
#define KEY_TAB         15
#define KEY_Q           16
#define KEY_W           17
#define KEY_E           18
#define KEY_R           19
#define KEY_T           20
#define KEY_Y           21
#define KEY_U           22
#define KEY_I           23
#define KEY_O           24
#define KEY_P           25
#define KEY_LEFTBRACE   26
#define KEY_RIGHTBRACE  27
#define KEY_ENTER       28
#define KEY_LEFTCTRL    29
#define KEY_A           30
#define KEY_S           31
#define KEY_D           32
#define KEY_F           33
#define KEY_G           34
#define KEY_H           35
#define KEY_J           36
#define KEY_K           37
#define KEY_L           38
#define KEY_SEMICOLON   39
#define KEY_APOSTROPHE  40
#define KEY_GRAVE       41
#define KEY_LEFTSHIFT   42
#define KEY_BACKSLASH   43
#define KEY_Z           44
#define KEY_X           45
#define KEY_C           46
#define KEY_V           47
#define KEY_B           48
#define KEY_N           49
#define KEY_M           50
#define KEY_COMMA       51
#define KEY_DOT         52
#define KEY_SLASH       53
#define KEY_RIGHTSHIFT  54
#define KEY_LEFTALT     56
#define KEY_SPACE       57
#define KEY_CAPSLOCK    58
#define KEY_F1          59
#define KEY_F2          60
#define KEY_F3          61
#define KEY_F4          62
#define KEY_F5          63
#define KEY_F6          64
#define KEY_F7          65
#define KEY_F8          66
#define KEY_F9          67
#define KEY_F10         68
#define KEY_F11         87
#define KEY_F12         88
#define KEY_UP          103
#define KEY_LEFT        105
#define KEY_RIGHT       106
#define KEY_DOWN        108
#define KEY_LEFTMETA    125  /* Command/Windows key */
#define KEY_RIGHTMETA   126
#define KEY_RIGHTCTRL   97
#define KEY_RIGHTALT    100

/* Mouse button codes */
#define BTN_LEFT        0x110
#define BTN_RIGHT       0x111
#define BTN_MIDDLE      0x112

/* VirtIO input event structure (8 bytes) */
struct virtio_input_event {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} __attribute__((packed));

/* Input-specific virtqueue structures (sized for 64 entries) */
#define INPUT_QUEUE_SIZE 64

struct input_virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[INPUT_QUEUE_SIZE];
    uint16_t used_event;
} __attribute__((packed));

struct input_virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[INPUT_QUEUE_SIZE];
    uint16_t avail_event;
} __attribute__((packed));

/* Virtqueue structure for input */
struct input_virtqueue {
    struct virtq_desc desc[INPUT_QUEUE_SIZE];
    struct input_virtq_avail avail;
    uint8_t padding[4096 - sizeof(struct virtq_desc) * INPUT_QUEUE_SIZE - sizeof(struct input_virtq_avail)];
    struct input_virtq_used used;
} __attribute__((aligned(4096)));

/* Driver state */
static bool use_pci = false;
static virtio_pci_device_t pci_dev;
static volatile uint32_t *virtio_input_base = NULL;
static struct input_virtqueue eventq __attribute__((aligned(4096)));
static struct virtio_input_event event_buffers[64] __attribute__((aligned(16)));
static bool input_initialized = false;
static uint16_t avail_idx = 0;
static uint16_t used_idx = 0;

/* Input state - exported for hal_input.c */
extern Point g_mousePos;
extern uint8_t g_mouseState;

/* Mouse bounds */
#define MOUSE_MAX_X     319
#define MOUSE_MAX_Y     239

/* Modifier keys state */
static uint16_t modifier_state = 0;

/* Modifier key bits */
#define MOD_SHIFT   0x0001
#define MOD_CTRL    0x0002
#define MOD_ALT     0x0004
#define MOD_META    0x0008

/* Key event queue for keyboard input */
#define KEY_QUEUE_SIZE  32
static struct {
    uint8_t keycode;
    uint8_t modifiers;
    bool pressed;
} key_queue[KEY_QUEUE_SIZE];
static volatile int key_queue_head = 0;
static volatile int key_queue_tail = 0;

/* Helper to read MMIO register */
static inline uint32_t virtio_read32(uint32_t offset) {
    return *(volatile uint32_t *)((uintptr_t)virtio_input_base + offset);
}

/* Helper to write MMIO register */
static inline void virtio_write32(uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)((uintptr_t)virtio_input_base + offset) = value;
}

/* Notify the device about queue updates */
static void notify_queue(uint16_t queue_idx) {
    if (use_pci) {
        virtio_pci_notify_queue(&pci_dev, queue_idx);
    } else {
        virtio_write32(VIRTIO_MMIO_QUEUE_NOTIFY, queue_idx);
    }
}

/* Add buffer to available ring */
static void virtio_input_add_buffer(uint16_t desc_idx) {
    eventq.avail.ring[avail_idx % 64] = desc_idx;
    __sync_synchronize();
    eventq.avail.idx = ++avail_idx;
    __sync_synchronize();
    notify_queue(0);
}

/* Initialize all event buffers */
static void virtio_input_setup_buffers(void) {
    for (int i = 0; i < 64; i++) {
        eventq.desc[i].addr = (uint64_t)(uintptr_t)&event_buffers[i];
        eventq.desc[i].len = sizeof(struct virtio_input_event);
        eventq.desc[i].flags = VIRTQ_DESC_F_WRITE;
        eventq.desc[i].next = 0;
        virtio_input_add_buffer(i);
    }
}

/* Convert Linux keycode to Mac keycode */
static uint8_t linux_to_mac_keycode(uint16_t linux_code) {
    /* Basic mapping - can be extended */
    switch (linux_code) {
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
        case KEY_LEFT: return 0x7B;
        case KEY_RIGHT: return 0x7C;
        case KEY_DOWN: return 0x7D;
        case KEY_UP: return 0x7E;
        default: return 0xFF;
    }
}

/* Process a single input event */
static void virtio_input_process_event(struct virtio_input_event *evt) {
    switch (evt->type) {
        case EV_REL:
            /* Relative mouse movement */
            if (evt->code == REL_X) {
                int32_t new_x = g_mousePos.h + (int32_t)evt->value;
                if (new_x < 0) new_x = 0;
                if (new_x > MOUSE_MAX_X) new_x = MOUSE_MAX_X;
                g_mousePos.h = (int16_t)new_x;
            } else if (evt->code == REL_Y) {
                int32_t new_y = g_mousePos.v + (int32_t)evt->value;
                if (new_y < 0) new_y = 0;
                if (new_y > MOUSE_MAX_Y) new_y = MOUSE_MAX_Y;
                g_mousePos.v = (int16_t)new_y;
            }
            break;

        case EV_ABS:
            /* Absolute positioning (tablet) */
            if (evt->code == ABS_X) {
                /* Scale from tablet range to screen range */
                int32_t x = (evt->value * MOUSE_MAX_X) / 32767;
                if (x < 0) x = 0;
                if (x > MOUSE_MAX_X) x = MOUSE_MAX_X;
                g_mousePos.h = (int16_t)x;
            } else if (evt->code == ABS_Y) {
                int32_t y = (evt->value * MOUSE_MAX_Y) / 32767;
                if (y < 0) y = 0;
                if (y > MOUSE_MAX_Y) y = MOUSE_MAX_Y;
                g_mousePos.v = (int16_t)y;
            }
            break;

        case EV_KEY:
            /* Button or key event */
            if (evt->code == BTN_LEFT) {
                if (evt->value) {
                    g_mouseState |= 0x01;
                } else {
                    g_mouseState &= ~0x01;
                }
            } else if (evt->code == BTN_RIGHT) {
                if (evt->value) {
                    g_mouseState |= 0x02;
                } else {
                    g_mouseState &= ~0x02;
                }
            } else if (evt->code == BTN_MIDDLE) {
                if (evt->value) {
                    g_mouseState |= 0x04;
                } else {
                    g_mouseState &= ~0x04;
                }
            } else {
                /* Keyboard key */
                /* Update modifier state */
                bool pressed = (evt->value != 0);
                switch (evt->code) {
                    case KEY_LEFTSHIFT:
                    case KEY_RIGHTSHIFT:
                        if (pressed) modifier_state |= MOD_SHIFT;
                        else modifier_state &= ~MOD_SHIFT;
                        break;
                    case KEY_LEFTCTRL:
                    case KEY_RIGHTCTRL:
                        if (pressed) modifier_state |= MOD_CTRL;
                        else modifier_state &= ~MOD_CTRL;
                        break;
                    case KEY_LEFTALT:
                    case KEY_RIGHTALT:
                        if (pressed) modifier_state |= MOD_ALT;
                        else modifier_state &= ~MOD_ALT;
                        break;
                    case KEY_LEFTMETA:
                    case KEY_RIGHTMETA:
                        if (pressed) modifier_state |= MOD_META;
                        else modifier_state &= ~MOD_META;
                        break;
                    default: {
                        /* Regular key - queue it */
                        uint8_t mac_key = linux_to_mac_keycode(evt->code);
                        if (mac_key != 0xFF) {
                            int next_head = (key_queue_head + 1) % KEY_QUEUE_SIZE;
                            if (next_head != key_queue_tail) {
                                key_queue[key_queue_head].keycode = mac_key;
                                key_queue[key_queue_head].modifiers = modifier_state;
                                key_queue[key_queue_head].pressed = pressed;
                                key_queue_head = next_head;
                            }
                        }
                        break;
                    }
                }
            }
            break;

        case EV_SYN:
            /* Sync event - input state is now consistent */
            break;
    }
}

/* Initialize using PCI transport */
static bool init_pci_transport(void) {
    uart_puts("[VIRTIO-INPUT] Trying PCI transport...\n");

    /* Scan for VirtIO Input on PCI */
    if (!virtio_pci_find_device(VIRTIO_DEV_INPUT, &pci_dev)) {
        uart_puts("[VIRTIO-INPUT] No PCI input device found\n");
        return false;
    }

    uart_puts("[VIRTIO-INPUT] Found PCI input device\n");

    /* Initialize device with no special features */
    if (!virtio_pci_init_device(&pci_dev, 0)) {
        uart_puts("[VIRTIO-INPUT] PCI device init failed\n");
        return false;
    }

    /* Setup event queue (queue 0) */
    if (!virtio_pci_setup_queue(&pci_dev, 0,
                                eventq.desc,
                                (struct virtq_avail *)&eventq.avail,
                                (struct virtq_used *)&eventq.used,
                                64)) {
        uart_puts("[VIRTIO-INPUT] Queue setup failed\n");
        return false;
    }

    /* Mark device ready */
    virtio_pci_device_ready(&pci_dev);

    use_pci = true;
    return true;
}

/* Initialize using MMIO transport */
static bool init_mmio_transport(void) {
    uint32_t magic, version, device_id;

    uart_puts("[VIRTIO-INPUT] Trying MMIO transport...\n");

    /* Scan all 32 MMIO slots for input device */
    for (int slot = 0; slot < 32; slot++) {
        uintptr_t base = VIRTIO_MMIO_BASE_START + (slot * VIRTIO_MMIO_SLOT_SIZE);
        virtio_input_base = (volatile uint32_t *)base;

        magic = virtio_read32(VIRTIO_MMIO_MAGIC);
        if (magic != 0x74726976) continue;

        device_id = virtio_read32(VIRTIO_MMIO_DEVICE_ID);

        if (device_id == VIRTIO_ID_INPUT) {
            uart_puts("[VIRTIO-INPUT] Found MMIO input device at slot ");
            if (slot < 10) {
                uart_putc('0' + slot);
            } else {
                uart_putc('0' + (slot / 10));
                uart_putc('0' + (slot % 10));
            }
            uart_puts("\n");
            break;
        }
    }

    if (virtio_read32(VIRTIO_MMIO_DEVICE_ID) != VIRTIO_ID_INPUT) {
        uart_puts("[VIRTIO-INPUT] No MMIO input device found\n");
        virtio_input_base = NULL;
        return false;
    }

    /* Check version */
    version = virtio_read32(VIRTIO_MMIO_VERSION);
    if (version != 1 && version != 2) {
        uart_puts("[VIRTIO-INPUT] Unsupported version\n");
        return false;
    }

    /* Reset device */
    virtio_write32(VIRTIO_MMIO_STATUS, 0);

    /* Acknowledge device */
    virtio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);

    /* Set driver status */
    virtio_write32(VIRTIO_MMIO_STATUS,
                   VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    /* Read and accept features (none needed for basic input) */
    virtio_read32(VIRTIO_MMIO_DEVICE_FEATURES);
    virtio_write32(VIRTIO_MMIO_DRIVER_FEATURES, 0);

    /* Features OK */
    virtio_write32(VIRTIO_MMIO_STATUS,
                   VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                   VIRTIO_STATUS_FEATURES_OK);

    /* Setup event queue (queue 0) */
    virtio_write32(VIRTIO_MMIO_QUEUE_SEL, 0);

    uint32_t max_queue_size = virtio_read32(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_queue_size < 64) {
        uart_puts("[VIRTIO-INPUT] Queue too small\n");
        return false;
    }

    virtio_write32(VIRTIO_MMIO_QUEUE_NUM, 64);

    /* Set queue addresses */
    uint64_t desc_addr = (uint64_t)(uintptr_t)&eventq.desc;
    uint64_t avail_addr = (uint64_t)(uintptr_t)&eventq.avail;
    uint64_t used_addr = (uint64_t)(uintptr_t)&eventq.used;

    virtio_write32(VIRTIO_MMIO_QUEUE_DESC_LOW, desc_addr & 0xFFFFFFFF);
    virtio_write32(VIRTIO_MMIO_QUEUE_DESC_HIGH, desc_addr >> 32);
    virtio_write32(VIRTIO_MMIO_QUEUE_AVAIL_LOW, avail_addr & 0xFFFFFFFF);
    virtio_write32(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, avail_addr >> 32);
    virtio_write32(VIRTIO_MMIO_QUEUE_USED_LOW, used_addr & 0xFFFFFFFF);
    virtio_write32(VIRTIO_MMIO_QUEUE_USED_HIGH, used_addr >> 32);

    virtio_write32(VIRTIO_MMIO_QUEUE_READY, 1);

    /* Driver OK */
    virtio_write32(VIRTIO_MMIO_STATUS,
                   VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                   VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

    use_pci = false;
    return true;
}

/*
 * Initialize virtio-input device
 */
bool virtio_input_init(void) {
    uart_puts("[VIRTIO-INPUT] Initializing...\n");

    /* Try PCI first, then MMIO */
    if (!init_pci_transport() && !init_mmio_transport()) {
        uart_puts("[VIRTIO-INPUT] No input device found on PCI or MMIO\n");
        return false;
    }

    /* Setup event buffers */
    virtio_input_setup_buffers();

    input_initialized = true;
    uart_puts("[VIRTIO-INPUT] Initialized successfully\n");
    return true;
}

/*
 * Poll for input events
 */
void virtio_input_poll(void) {
    if (!input_initialized) return;

    /* Process all pending events */
    while (eventq.used.idx != used_idx) {
        uint16_t idx = used_idx % 64;
        uint32_t desc_idx = eventq.used.ring[idx].id;

        /* Process the event */
        virtio_input_process_event(&event_buffers[desc_idx]);

        /* Re-add buffer to available ring */
        virtio_input_add_buffer(desc_idx);

        used_idx++;
    }
}

/*
 * Get current modifier state
 */
uint16_t virtio_input_get_modifiers(void) {
    return modifier_state;
}

/*
 * Check if a key event is available
 */
bool virtio_input_key_available(void) {
    return key_queue_head != key_queue_tail;
}

/*
 * Get next key event
 * Returns true if event was available
 */
bool virtio_input_get_key(uint8_t *keycode, uint8_t *modifiers, bool *pressed) {
    if (key_queue_head == key_queue_tail) {
        return false;
    }

    *keycode = key_queue[key_queue_tail].keycode;
    *modifiers = key_queue[key_queue_tail].modifiers;
    *pressed = key_queue[key_queue_tail].pressed;

    key_queue_tail = (key_queue_tail + 1) % KEY_QUEUE_SIZE;
    return true;
}

/*
 * Check if input is initialized
 */
bool virtio_input_is_initialized(void) {
    return input_initialized;
}
