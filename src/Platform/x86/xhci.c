/*
 * xhci.c - Minimal x86 xHCI controller bring-up (QEMU)
 *
 * Note: This is a minimal Phase-1 implementation that detects an xHCI
 * controller, enables MMIO + bus mastering, performs a controller reset,
 * and logs port connect status. HID transfers are not implemented yet.
 */

#include "xhci.h"
#include "pci.h"
#include "usb_core.h"
#include "Platform/include/serial.h"
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define XHCI_CLASS_CODE 0x0C
#define XHCI_SUBCLASS   0x03
#define XHCI_PROG_IF    0x30

/* Capability registers */
#define XHCI_CAPLENGTH    0x00
#define XHCI_HCSPARAMS1   0x04
#define XHCI_HCIVERSION   0x02
#define XHCI_DBOFF        0x14
#define XHCI_RTSOFF       0x18
#define XHCI_HCCPARAMS1   0x10

/* Operational registers (offset from operational base) */
#define XHCI_USBCMD       0x00
#define XHCI_USBSTS       0x04
#define XHCI_CONFIG       0x38
#define XHCI_DNCTRL       0x14
#define XHCI_DCBAAP_LO    0x30
#define XHCI_DCBAAP_HI    0x34
#define XHCI_CRCR_LO      0x18
#define XHCI_CRCR_HI      0x1C
#define XHCI_PORTSC_BASE  0x400
#define XHCI_PORTSC_STRIDE 0x10

/* USBCMD bits */
#define XHCI_CMD_RUN      (1u << 0)
#define XHCI_CMD_RESET    (1u << 1)
#define XHCI_CMD_INTE     (1u << 2)

/* USBSTS bits */
#define XHCI_STS_HCH      (1u << 0)
#define XHCI_STS_CNR      (1u << 11)
#define XHCI_STS_CE       (1u << 2)
#define XHCI_STS_EINT     (1u << 3)

#define XHCI_PORTSC_CCS   (1u << 0)
#define XHCI_PORTSC_PED   (1u << 1)
#define XHCI_PORTSC_PR    (1u << 4)
#define XHCI_PORTSC_PRC   (1u << 21)
#define XHCI_PORTSC_PEC   (1u << 19)

/* Runtime registers (offset from runtime base) */
#define XHCI_RT_IMAN      0x20
#define XHCI_RT_IMOD      0x24
#define XHCI_RT_ERSTSZ    0x28
#define XHCI_RT_ERSTBA_LO 0x30
#define XHCI_RT_ERSTBA_HI 0x34
#define XHCI_RT_ERDP_LO   0x38
#define XHCI_RT_ERDP_HI   0x3C

/* Doorbell */
#define XHCI_DB0          0x00

/* TRB */
typedef struct __attribute__((packed)) {
    uint32_t dword0;
    uint32_t dword1;
    uint32_t dword2;
    uint32_t dword3;
} xhci_trb_t;

#define XHCI_TRB_TYPE_SHIFT 10
#define XHCI_TRB_TYPE_MASK  (0x3F << XHCI_TRB_TYPE_SHIFT)
#define XHCI_TRB_TYPE_NOOP  23
#define XHCI_TRB_TYPE_EVT_CMD_COMPLETE 33
#define XHCI_TRB_TYPE_ENABLE_SLOT 9
#define XHCI_TRB_TYPE_ADDRESS_DEVICE 11
#define XHCI_TRB_TYPE_SETUP_STAGE 2
#define XHCI_TRB_TYPE_DATA_STAGE 3
#define XHCI_TRB_TYPE_STATUS_STAGE 4
#define XHCI_TRB_TYPE_NORMAL 1
#define XHCI_TRB_TYPE_EVT_TRANSFER 32
#define XHCI_TRB_CYCLE      (1u << 0)
#define XHCI_TRB_IOC        (1u << 5)
#define XHCI_TRB_IDT        (1u << 6)

typedef struct __attribute__((packed)) {
    uint64_t seg_base;
    uint32_t seg_size;
    uint32_t rsvd;
} xhci_erst_entry_t;

typedef struct __attribute__((packed)) {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

static xhci_trb_t __attribute__((aligned(64))) g_cmd_ring[32];
static xhci_trb_t __attribute__((aligned(64))) g_evt_ring[32];
static xhci_erst_entry_t __attribute__((aligned(64))) g_erst[1];
static uint64_t __attribute__((aligned(64))) g_dcbaa[256];
static uint32_t __attribute__((aligned(64))) g_input_ctx[48];
static uint32_t __attribute__((aligned(64))) g_dev_ctx[32];
static xhci_trb_t __attribute__((aligned(64))) g_ep0_ring[32];
static uint32_t __attribute__((aligned(64))) g_ep0_buf[64];
static uint8_t g_hid_ep_addr = 0;
static uint16_t g_hid_ep_mps = 0;
static uint8_t g_hid_ep_interval = 0;
static uint8_t g_hid_slot = 0;
static uint32_t g_ctx_size = 32;

static uint32_t g_cmd_ring_index = 0;
static uint32_t g_cmd_cycle = 1;
static uint32_t g_evt_cycle = 1;
static uint32_t g_evt_ring_index = 0;
static uint32_t g_ep0_ring_index = 0;
static uint32_t g_ep0_cycle = 1;
static uintptr_t g_xhci_base = 0;
static uint32_t g_xhci_dboff = 0;
static uintptr_t g_xhci_rt_base = 0;

static inline uint32_t mmio_read32(uintptr_t base, uint32_t off);
static inline void mmio_write32(uintptr_t base, uint32_t off, uint32_t value);

static void xhci_ring_doorbell(uintptr_t base, uint32_t db_off, uint32_t target) {
    mmio_write32(base + db_off, XHCI_DB0 + target, 0);
}

static void xhci_ring_init(void) {
    memset(g_cmd_ring, 0, sizeof(g_cmd_ring));
    memset(g_evt_ring, 0, sizeof(g_evt_ring));
    memset(g_erst, 0, sizeof(g_erst));
    memset(g_dcbaa, 0, sizeof(g_dcbaa));
    memset(g_ep0_ring, 0, sizeof(g_ep0_ring));
    memset(g_ep0_buf, 0, sizeof(g_ep0_buf));
    g_cmd_ring_index = 0;
    g_cmd_cycle = 1;
    g_evt_cycle = 1;
    g_evt_ring_index = 0;
    g_ep0_ring_index = 0;
    g_ep0_cycle = 1;
}

static void xhci_cmd_ring_enqueue_noop(void) {
    xhci_trb_t *trb = &g_cmd_ring[g_cmd_ring_index++];
    trb->dword0 = 0;
    trb->dword1 = 0;
    trb->dword2 = 0;
    trb->dword3 = (XHCI_TRB_TYPE_NOOP << XHCI_TRB_TYPE_SHIFT) | (g_cmd_cycle ? XHCI_TRB_CYCLE : 0);

    if (g_cmd_ring_index >= 31) {
        xhci_trb_t *link = &g_cmd_ring[g_cmd_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&g_cmd_ring[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        g_cmd_ring_index = 0;
        g_cmd_cycle ^= 1;
    }
}

static bool xhci_poll_cmd_complete(uintptr_t rt_base, uint8_t *out_slot_id) {
    for (uint32_t i = 0; i < 100000; i++) {
        xhci_trb_t *evt = &g_evt_ring[g_evt_ring_index];
        uint32_t cycle = evt->dword3 & XHCI_TRB_CYCLE;
        if (cycle == (g_evt_cycle ? XHCI_TRB_CYCLE : 0)) {
            uint32_t type = (evt->dword3 & XHCI_TRB_TYPE_MASK) >> XHCI_TRB_TYPE_SHIFT;
            if (type == XHCI_TRB_TYPE_EVT_CMD_COMPLETE) {
                if (out_slot_id) {
                    *out_slot_id = (uint8_t)((evt->dword3 >> 24) & 0xFF);
                }

                /* advance dequeue */
                g_evt_ring_index++;
                if (g_evt_ring_index >= (sizeof(g_evt_ring) / sizeof(g_evt_ring[0]))) {
                    g_evt_ring_index = 0;
                    g_evt_cycle ^= 1;
                }

                uintptr_t erdp = (uintptr_t)&g_evt_ring[g_evt_ring_index];
                mmio_write32(rt_base, XHCI_RT_ERDP_LO, (uint32_t)(erdp & 0xFFFFFFFFu));
#if UINTPTR_MAX > 0xFFFFFFFFu
                mmio_write32(rt_base, XHCI_RT_ERDP_HI, (uint32_t)(erdp >> 32));
#else
                mmio_write32(rt_base, XHCI_RT_ERDP_HI, 0);
#endif
                return true;
            }
        }
    }
    return false;
}

static bool xhci_poll_transfer_complete(uintptr_t rt_base, uint8_t slot_id) {
    for (uint32_t i = 0; i < 100000; i++) {
        xhci_trb_t *evt = &g_evt_ring[g_evt_ring_index];
        uint32_t cycle = evt->dword3 & XHCI_TRB_CYCLE;
        if (cycle == (g_evt_cycle ? XHCI_TRB_CYCLE : 0)) {
            uint32_t type = (evt->dword3 & XHCI_TRB_TYPE_MASK) >> XHCI_TRB_TYPE_SHIFT;
            if (type == XHCI_TRB_TYPE_EVT_TRANSFER) {
                uint8_t evt_slot = (uint8_t)((evt->dword3 >> 24) & 0xFF);
                if (evt_slot == slot_id) {
                    uint32_t comp = (evt->dword2 >> 24) & 0xFF;
                    uint32_t len = evt->dword2 & 0xFFFFFF;
                    serial_printf("[XHCI] Transfer complete slot=%u code=%u len=%u\n",
                                  evt_slot, comp, len);

                    g_evt_ring_index++;
                    if (g_evt_ring_index >= (sizeof(g_evt_ring) / sizeof(g_evt_ring[0]))) {
                        g_evt_ring_index = 0;
                        g_evt_cycle ^= 1;
                    }
                    uintptr_t erdp = (uintptr_t)&g_evt_ring[g_evt_ring_index];
                    mmio_write32(rt_base, XHCI_RT_ERDP_LO, (uint32_t)(erdp & 0xFFFFFFFFu));
#if UINTPTR_MAX > 0xFFFFFFFFu
                    mmio_write32(rt_base, XHCI_RT_ERDP_HI, (uint32_t)(erdp >> 32));
#else
                    mmio_write32(rt_base, XHCI_RT_ERDP_HI, 0);
#endif
                    return true;
                }
            }
        }
    }
    return false;
}

static void xhci_cmd_ring_enqueue_enable_slot(void) {
    xhci_trb_t *trb = &g_cmd_ring[g_cmd_ring_index++];
    trb->dword0 = 0;
    trb->dword1 = 0;
    trb->dword2 = 0;
    trb->dword3 = (XHCI_TRB_TYPE_ENABLE_SLOT << XHCI_TRB_TYPE_SHIFT) |
                  (g_cmd_cycle ? XHCI_TRB_CYCLE : 0);

    if (g_cmd_ring_index >= 31) {
        xhci_trb_t *link = &g_cmd_ring[g_cmd_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&g_cmd_ring[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        g_cmd_ring_index = 0;
        g_cmd_cycle ^= 1;
    }
}

static uint16_t xhci_ep0_mps(uint32_t portsc) {
    uint32_t speed = (portsc >> 10) & 0xF;
    switch (speed) {
        case 2: /* low */ return 8;
        case 1: /* full */ return 8;
        case 3: /* high */ return 64;
        case 4: /* super */ return 512;
        default: return 8;
    }
}

static void xhci_build_contexts(uint8_t slot_id, uint8_t port, uint32_t portsc) {
    memset(g_input_ctx, 0, sizeof(g_input_ctx));
    memset(g_dev_ctx, 0, sizeof(g_dev_ctx));

    uint32_t ctx_dwords = g_ctx_size / 4;
    uint32_t *ictl = &g_input_ctx[0];
    uint32_t *slot = &g_input_ctx[ctx_dwords * 1];
    uint32_t *ep0  = &g_input_ctx[ctx_dwords * 2];

    /* Input control context */
    ictl[0] = (1u << 0) | (1u << 1); /* add slot + ep0 */

    /* Slot context */
    uint32_t speed = (portsc >> 10) & 0xF;
    slot[0] = (1u << 27) | (speed << 20); /* context entries=1, speed */
    slot[1] = port; /* root hub port */

    /* EP0 context */
    uint16_t mps = xhci_ep0_mps(portsc);
    ep0[1] = ((uint32_t)mps << 16); /* Max Packet Size */
    ep0[0] = (4u << 3); /* EP type = control (4) */
    ep0[2] = (uint32_t)((uintptr_t)&g_ep0_ring[0] & 0xFFFFFFFFu);
    ep0[3] = 0;

    /* Device context placeholder */
    g_dcbaa[slot_id] = (uint64_t)(uintptr_t)&g_dev_ctx[0];
}

static bool xhci_address_device(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                uint8_t slot_id, uint8_t port, uint32_t portsc) {
    xhci_build_contexts(slot_id, port, portsc);

    xhci_trb_t *trb = &g_cmd_ring[g_cmd_ring_index++];
    uintptr_t ictx = (uintptr_t)&g_input_ctx[0];
    trb->dword0 = (uint32_t)(ictx & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = 0;
    trb->dword3 = (XHCI_TRB_TYPE_ADDRESS_DEVICE << XHCI_TRB_TYPE_SHIFT) |
                  ((uint32_t)slot_id << 24) |
                  (g_cmd_cycle ? XHCI_TRB_CYCLE : 0);

    xhci_ring_doorbell(base, dboff, 0);
    if (xhci_poll_cmd_complete(rt_base, NULL)) {
        serial_printf("[XHCI] Address Device complete for slot %u\n", slot_id);
        return true;
    }

    serial_printf("[XHCI] Address Device timeout for slot %u\n", slot_id);
    return false;
}

static void xhci_ep0_enqueue_setup(const usb_setup_packet_t *setup) {
    xhci_trb_t *trb = &g_ep0_ring[g_ep0_ring_index++];
    trb->dword0 = *(const uint32_t *)setup;
    trb->dword1 = *((const uint32_t *)setup + 1);
    trb->dword2 = 8;
    trb->dword3 = (XHCI_TRB_TYPE_SETUP_STAGE << XHCI_TRB_TYPE_SHIFT) |
                  (2u << 16) | /* transfer type: IN data stage */
                  XHCI_TRB_IDT |
                  (g_ep0_cycle ? XHCI_TRB_CYCLE : 0);
}

static void xhci_ep0_enqueue_data_in(uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &g_ep0_ring[g_ep0_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_DATA_STAGE << XHCI_TRB_TYPE_SHIFT) |
                  (1u << 16) | /* IN */
                  XHCI_TRB_IOC |
                  (g_ep0_cycle ? XHCI_TRB_CYCLE : 0);
}

static void xhci_ep0_enqueue_status(void) {
    xhci_trb_t *trb = &g_ep0_ring[g_ep0_ring_index++];
    trb->dword0 = 0;
    trb->dword1 = 0;
    trb->dword2 = 0;
    trb->dword3 = (XHCI_TRB_TYPE_STATUS_STAGE << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (g_ep0_cycle ? XHCI_TRB_CYCLE : 0);
}

static void xhci_ep0_ring_doorbell(uintptr_t base, uint32_t dboff, uint8_t slot_id) {
    mmio_write32(base + dboff, XHCI_DB0 + slot_id, 1);
}

static bool xhci_ep0_set_configuration(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                       uint8_t slot_id, uint8_t config_value) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x00,
        .bRequest = 9,
        .wValue = config_value,
        .wIndex = 0,
        .wLength = 0
    };

    xhci_ep0_enqueue_setup(&setup);
    xhci_ep0_enqueue_status();
    xhci_ep0_ring_doorbell(base, dboff, slot_id);

    serial_printf("[XHCI] SET_CONFIGURATION %u for slot %u\n", config_value, slot_id);
    return xhci_poll_transfer_complete(rt_base, slot_id);
}

static bool xhci_ep0_get_device_descriptor(uintptr_t base, uint32_t dboff, uintptr_t rt_base, uint8_t slot_id) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x80,
        .bRequest = 6,
        .wValue = 0x0100,
        .wIndex = 0,
        .wLength = 18
    };

    xhci_ep0_enqueue_setup(&setup);
    xhci_ep0_enqueue_data_in((uintptr_t)&g_ep0_buf[0], 18);
    xhci_ep0_enqueue_status();
    xhci_ep0_ring_doorbell(base, dboff, slot_id);

    serial_printf("[XHCI] GET_DESCRIPTOR issued for slot %u\n", slot_id);
    if (!xhci_poll_transfer_complete(rt_base, slot_id)) {
        return false;
    }

    uint8_t *desc = (uint8_t *)&g_ep0_buf[0];
    uint16_t vid = (uint16_t)(desc[8] | (desc[9] << 8));
    uint16_t pid = (uint16_t)(desc[10] | (desc[11] << 8));
    serial_printf("[XHCI] Device descriptor: VID=0x%04x PID=0x%04x class=0x%02x\n",
                  vid, pid, desc[4]);
    return true;
}

static bool xhci_ep0_get_config_descriptor(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                           uint8_t slot_id) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x80,
        .bRequest = 6,
        .wValue = 0x0200,
        .wIndex = 0,
        .wLength = 64
    };

    memset(g_ep0_buf, 0, sizeof(g_ep0_buf));
    xhci_ep0_enqueue_setup(&setup);
    xhci_ep0_enqueue_data_in((uintptr_t)&g_ep0_buf[0], 64);
    xhci_ep0_enqueue_status();
    xhci_ep0_ring_doorbell(base, dboff, slot_id);

    serial_printf("[XHCI] GET_CONFIG issued for slot %u\n", slot_id);
    if (!xhci_poll_transfer_complete(rt_base, slot_id)) {
        return false;
    }

    uint8_t *buf = (uint8_t *)&g_ep0_buf[0];
    uint16_t total = (uint16_t)(buf[2] | (buf[3] << 8));
    uint8_t config_value = buf[5];
    serial_printf("[XHCI] Config total length=%u\n", total);

    uint32_t off = 0;
    g_hid_ep_addr = 0;
    g_hid_ep_mps = 0;
    g_hid_ep_interval = 0;
    while (off + 1 < 64) {
        uint8_t len = buf[off];
        uint8_t type = buf[off + 1];
        if (len < 2) {
            break;
        }
        if (type == 4 && len >= 9) {
            uint8_t cls = buf[off + 5];
            uint8_t sub = buf[off + 6];
            uint8_t proto = buf[off + 7];
            serial_printf("[XHCI] IF cls=0x%02x sub=0x%02x proto=0x%02x\n", cls, sub, proto);
        }
        if (type == 5 && len >= 7) {
            uint8_t ep_addr = buf[off + 2];
            uint8_t attrs = buf[off + 3];
            uint16_t mps = (uint16_t)(buf[off + 4] | (buf[off + 5] << 8));
            uint8_t interval = buf[off + 6];
            uint8_t ep_type = attrs & 0x3;
            if ((ep_addr & 0x80) && ep_type == 3 && g_hid_ep_addr == 0) {
                g_hid_ep_addr = ep_addr;
                g_hid_ep_mps = mps;
                g_hid_ep_interval = interval;
                serial_printf("[XHCI] HID INT IN ep=0x%02x mps=%u interval=%u\n",
                              ep_addr, mps, interval);
            }
        }
        off += len;
    }

    g_hid_slot = slot_id;
    if (config_value != 0) {
        xhci_ep0_set_configuration(base, dboff, rt_base, slot_id, config_value);
    }

    return true;
}

static void xhci_hid_enqueue_interrupt_in(uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &g_ep0_ring[g_ep0_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (g_ep0_cycle ? XHCI_TRB_CYCLE : 0);
}

static void xhci_hid_poll(uintptr_t base, uint32_t dboff, uintptr_t rt_base) {
    if (g_hid_slot == 0 || g_hid_ep_addr == 0 || g_hid_ep_mps == 0) {
        return;
    }

    /* Reuse EP0 ring for now (placeholder) */
    xhci_hid_enqueue_interrupt_in((uintptr_t)&g_ep0_buf[0], g_hid_ep_mps);
    xhci_ep0_ring_doorbell(base, dboff, g_hid_slot);

    if (xhci_poll_transfer_complete(rt_base, g_hid_slot)) {
        uint8_t *data = (uint8_t *)&g_ep0_buf[0];
        serial_printf("[XHCI] HID data: %02x %02x %02x %02x\n",
                      data[0], data[1], data[2], data[3]);
    }
}

static inline uint32_t mmio_read32(uintptr_t base, uint32_t off) {
    volatile uint32_t *ptr = (volatile uint32_t *)(base + off);
    return *ptr;
}

static inline void mmio_write32(uintptr_t base, uint32_t off, uint32_t value) {
    volatile uint32_t *ptr = (volatile uint32_t *)(base + off);
    *ptr = value;
}

static bool xhci_reset(uintptr_t base, uint32_t cap_len) {
    uintptr_t op_base = base + cap_len;
    uint32_t cmd = mmio_read32(op_base, XHCI_USBCMD);
    cmd &= ~XHCI_CMD_RUN;
    mmio_write32(op_base, XHCI_USBCMD, cmd);

    uint32_t timeout = 100000;
    while (timeout-- > 0) {
        uint32_t sts = mmio_read32(op_base, XHCI_USBSTS);
        if (sts & XHCI_STS_HCH) {
            break;
        }
    }

    cmd |= XHCI_CMD_RESET;
    mmio_write32(op_base, XHCI_USBCMD, cmd);

    timeout = 100000;
    while (timeout-- > 0) {
        uint32_t sts = mmio_read32(op_base, XHCI_USBSTS);
        if ((sts & XHCI_STS_CNR) == 0) {
            return true;
        }
    }

    return false;
}

static bool xhci_start(uintptr_t base, uint32_t cap_len) {
    uintptr_t op_base = base + cap_len;
    uint32_t cmd = mmio_read32(op_base, XHCI_USBCMD);
    cmd |= XHCI_CMD_RUN;
    mmio_write32(op_base, XHCI_USBCMD, cmd);

    uint32_t timeout = 100000;
    while (timeout-- > 0) {
        uint32_t sts = mmio_read32(op_base, XHCI_USBSTS);
        if ((sts & XHCI_STS_HCH) == 0) {
            return true;
        }
    }
    return false;
}

static const char *xhci_speed_name(uint32_t portsc) {
    uint32_t speed = (portsc >> 10) & 0xF;
    switch (speed) {
        case 1: return "full";
        case 2: return "low";
        case 3: return "high";
        case 4: return "super";
        case 5: return "super+";
        default: return "unknown";
    }
}

static bool xhci_port_reset(uintptr_t op_base, uint8_t port) {
    uintptr_t off = XHCI_PORTSC_BASE + port * XHCI_PORTSC_STRIDE;
    uint32_t portsc = mmio_read32(op_base, off);
    if ((portsc & XHCI_PORTSC_CCS) == 0) {
        return false;
    }

    /* Set PR (port reset). Preserve RW1C bits by writing them as 0. */
    mmio_write32(op_base, off, portsc | XHCI_PORTSC_PR);

    uint32_t timeout = 100000;
    while (timeout-- > 0) {
        uint32_t now = mmio_read32(op_base, off);
        if ((now & XHCI_PORTSC_PR) == 0) {
            /* Clear PRC/PEC if set (RW1C) */
            if (now & (XHCI_PORTSC_PRC | XHCI_PORTSC_PEC)) {
                mmio_write32(op_base, off, now | XHCI_PORTSC_PRC | XHCI_PORTSC_PEC);
            }
            return (now & XHCI_PORTSC_PED) != 0;
        }
    }
    return false;
}

bool xhci_init_x86(void) {
    pci_device_t devices[64];
    int found = pci_scan(devices, 64);
    if (found > 64) {
        found = 64;
    }

    for (int i = 0; i < found; i++) {
        if (devices[i].class_code == XHCI_CLASS_CODE &&
            devices[i].subclass == XHCI_SUBCLASS &&
            devices[i].prog_if == XHCI_PROG_IF) {

            uint32_t cmd = pci_read_config_dword(
                devices[i].bus, devices[i].slot, devices[i].func, 0x04);
            cmd |= (1 << 1); /* Memory Space */
            cmd |= (1 << 2); /* Bus Master */
            pci_write_config_dword(
                devices[i].bus, devices[i].slot, devices[i].func, 0x04, cmd);

            uintptr_t base = (uintptr_t)devices[i].bar_addrs[0];
            serial_printf("[XHCI] MMIO base=0x%08x size=0x%08x\n",
                          (uint32_t)base, devices[i].bar_sizes[0]);
            usb_core_x86_register_controller(USB_CTRL_XHCI, base, true);

            uint8_t cap_len = (uint8_t)(mmio_read32(base, XHCI_CAPLENGTH) & 0xFF);
            uint16_t version = (uint16_t)((mmio_read32(base, XHCI_HCIVERSION)) & 0xFFFF);
            uint32_t params1 = mmio_read32(base, XHCI_HCSPARAMS1);
            uint32_t hccparams1 = mmio_read32(base, XHCI_HCCPARAMS1);
            uint32_t dboff = mmio_read32(base, XHCI_DBOFF) & ~0x3u;
            uint32_t rtsoff = mmio_read32(base, XHCI_RTSOFF) & ~0x1Fu;
            uint8_t ports = (uint8_t)(params1 & 0xFF);
            g_ctx_size = (hccparams1 & (1u << 2)) ? 64 : 32;

            serial_printf("[XHCI] caplen=%u version=%x ports=%u\n", cap_len, version, ports);
            serial_printf("[XHCI] dboff=0x%08x rtsoff=0x%08x ctx=%u\n", dboff, rtsoff, g_ctx_size);

            if (!xhci_reset(base, cap_len)) {
                serial_puts("[XHCI] reset timeout\n");
                return false;
            }

            xhci_ring_init();

            uintptr_t op_base = base + cap_len;
            g_xhci_base = base;
            g_xhci_dboff = dboff;
            g_xhci_rt_base = base + rtsoff;
            /* Program DCBAA */
            mmio_write32(op_base, XHCI_DCBAAP_LO, (uint32_t)((uintptr_t)&g_dcbaa[0] & 0xFFFFFFFFu));
            mmio_write32(op_base, XHCI_DCBAAP_HI, 0);

            /* Program command ring */
            uintptr_t cmd_ring_ptr = (uintptr_t)&g_cmd_ring[0];
            mmio_write32(op_base, XHCI_CRCR_LO, (uint32_t)(cmd_ring_ptr & 0xFFFFFFFFu) | XHCI_TRB_CYCLE);
#if UINTPTR_MAX > 0xFFFFFFFFu
            mmio_write32(op_base, XHCI_CRCR_HI, (uint32_t)(cmd_ring_ptr >> 32));
#else
            mmio_write32(op_base, XHCI_CRCR_HI, 0);
#endif

            /* Event ring (interrupter 0) */
            g_erst[0].seg_base = (uint64_t)(uintptr_t)&g_evt_ring[0];
            g_erst[0].seg_size = (uint32_t)(sizeof(g_evt_ring) / sizeof(g_evt_ring[0]));

            uintptr_t rt_base = base + rtsoff;
            mmio_write32(rt_base, XHCI_RT_ERSTSZ, 1);
            mmio_write32(rt_base, XHCI_RT_ERSTBA_LO, (uint32_t)((uintptr_t)&g_erst[0] & 0xFFFFFFFFu));
            mmio_write32(rt_base, XHCI_RT_ERSTBA_HI, 0);
            mmio_write32(rt_base, XHCI_RT_ERDP_LO, (uint32_t)((uintptr_t)&g_evt_ring[0] & 0xFFFFFFFFu));
            mmio_write32(rt_base, XHCI_RT_ERDP_HI, 0);

            /* Enable interrupter */
            mmio_write32(rt_base, XHCI_RT_IMAN, 1);
            mmio_write32(rt_base, XHCI_RT_IMOD, 0);

            /* Set max device slots enabled */
            mmio_write32(op_base, XHCI_CONFIG, 1);

            if (!xhci_start(base, cap_len)) {
                serial_puts("[XHCI] start timeout\n");
                return false;
            }

            /* Test: issue a NO-OP command */
            xhci_cmd_ring_enqueue_noop();
            xhci_ring_doorbell(base, dboff, 0);
            if (xhci_poll_cmd_complete(rt_base, NULL)) {
                serial_puts("[XHCI] NOOP command completed\n");
            } else {
                serial_puts("[XHCI] NOOP command timeout\n");
            }
            for (uint8_t p = 0; p < ports; p++) {
                uint32_t portsc = mmio_read32(op_base, XHCI_PORTSC_BASE + p * XHCI_PORTSC_STRIDE);
                if (portsc & XHCI_PORTSC_CCS) {
                    serial_printf("[XHCI] port %u connected (%s) PORTSC=0x%08x\n",
                                  (unsigned)p + 1, xhci_speed_name(portsc), portsc);
                    if (xhci_port_reset(op_base, p)) {
                        serial_printf("[XHCI] port %u reset ok\n", (unsigned)p + 1);
                        xhci_cmd_ring_enqueue_enable_slot();
                        xhci_ring_doorbell(base, dboff, 0);
                        uint8_t slot_id = 0;
                        if (xhci_poll_cmd_complete(rt_base, &slot_id)) {
                            serial_printf("[XHCI] Enable Slot complete, slot=%u\n", slot_id);
                            if (xhci_address_device(base, dboff, rt_base, slot_id, (uint8_t)(p + 1), portsc)) {
                                if (xhci_ep0_get_device_descriptor(base, dboff, rt_base, slot_id)) {
                                    xhci_ep0_get_config_descriptor(base, dboff, rt_base, slot_id);
                                }
                            }
                        } else {
                            serial_puts("[XHCI] Enable Slot timeout\n");
                        }
                    } else {
                        serial_printf("[XHCI] port %u reset failed\n", (unsigned)p + 1);
                    }
                }
            }

            serial_puts("[XHCI] Phase-1 init complete (no HID transfers yet)\n");
            return true;
        }
    }

    serial_puts("[XHCI] controller not found\n");
    return false;
}

void xhci_poll_hid_x86(void) {
    if (!g_xhci_base || !g_xhci_rt_base) {
        return;
    }
    xhci_hid_poll(g_xhci_base, g_xhci_dboff, g_xhci_rt_base);
}
