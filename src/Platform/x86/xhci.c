/*
 * xhci.c - Minimal x86 xHCI controller bring-up (QEMU)
 *
 * Note: This is a minimal Phase-1 implementation that detects an xHCI
 * controller, enables MMIO + bus mastering, performs a controller reset,
 * and logs port connect status. HID keyboard/mouse polling is supported.
 */

#include "xhci.h"
#include "pci.h"
#include "usb_core.h"
#include "Platform/include/serial.h"
#include "PS2Controller.h"
#include "EventManager/KeyboardEvents.h"
#include "EventManager/MouseEvents.h"
#include "EventManager/EventTypes.h"
#include "EventManager/EventManager.h"
#include "OSUtils/OSUtils.h"
#include "SystemTypes.h"
#include "FileManagerTypes.h"
#include "Platform/include/storage.h"
#include "EventManager/SystemEvents.h"
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
#define XHCI_TRB_TYPE_CONFIGURE_ENDPOINT 12
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
static xhci_trb_t __attribute__((aligned(64))) g_hid_kbd_ring[32];
static xhci_trb_t __attribute__((aligned(64))) g_hid_mouse_ring[32];
static uint32_t __attribute__((aligned(64))) g_hid_kbd_buf[64];
static uint32_t __attribute__((aligned(64))) g_hid_mouse_buf[64];
static xhci_trb_t __attribute__((aligned(64))) g_msc_in_ring[32];
static xhci_trb_t __attribute__((aligned(64))) g_msc_out_ring[32];
static uint32_t __attribute__((aligned(64))) g_msc_data_buf[512];
static uint8_t __attribute__((aligned(64))) g_msc_cbw[32];
static uint8_t __attribute__((aligned(64))) g_msc_csw[16];
static xhci_trb_t __attribute__((aligned(64))) g_uasp_cmd_ring[32];
static xhci_trb_t __attribute__((aligned(64))) g_uasp_data_in_ring[32];
static xhci_trb_t __attribute__((aligned(64))) g_uasp_data_out_ring[32];
static xhci_trb_t __attribute__((aligned(64))) g_uasp_status_ring[32];
static uint8_t __attribute__((aligned(64))) g_uasp_cmd_iu[32];
static uint8_t __attribute__((aligned(64))) g_uasp_status_iu[32];
#define MAX_MSC_LUNS 8
static uint32_t g_msc_lun_block_size[MAX_MSC_LUNS];
static uint64_t g_msc_lun_block_count[MAX_MSC_LUNS];
static bool g_msc_lun_valid[MAX_MSC_LUNS];
static uint32_t g_ctx_size = 32;

typedef struct {
    uint8_t slot;
    uint8_t ep_addr;
    uint16_t mps;
    uint8_t interval;
    uint8_t ep_id;
    uint32_t ring_index;
    uint32_t cycle;
    bool pending;
    uint32_t pending_tick;
    uint32_t last_submit_tick;
    uint8_t last_report[8];
    xhci_trb_t *ring;
    uint32_t *buf;
    bool configured;
} xhci_hid_dev_t;

static xhci_hid_dev_t g_hid_kbd;
static xhci_hid_dev_t g_hid_mouse;

typedef struct {
    uint8_t slot;
    uint8_t bulk_in_ep;
    uint8_t bulk_out_ep;
    uint16_t bulk_in_mps;
    uint16_t bulk_out_mps;
    uint8_t bulk_in_ep_id;
    uint8_t bulk_out_ep_id;
    bool bot_present;
    bool bot_configured;
    uint8_t bot_in_ep;
    uint8_t bot_out_ep;
    uint16_t bot_in_mps;
    uint16_t bot_out_mps;
    uint8_t bot_in_ep_id;
    uint8_t bot_out_ep_id;
    bool uasp;
    uint8_t uasp_cmd_out_ep;
    uint8_t uasp_data_out_ep;
    uint8_t uasp_status_in_ep;
    uint8_t uasp_data_in_ep;
    uint16_t uasp_cmd_out_mps;
    uint16_t uasp_data_out_mps;
    uint16_t uasp_status_in_mps;
    uint16_t uasp_data_in_mps;
    uint8_t uasp_cmd_out_ep_id;
    uint8_t uasp_data_out_ep_id;
    uint8_t uasp_status_in_ep_id;
    uint8_t uasp_data_in_ep_id;
    uint32_t uasp_cmd_ring_index;
    uint32_t uasp_data_in_ring_index;
    uint32_t uasp_data_out_ring_index;
    uint32_t uasp_status_ring_index;
    uint32_t uasp_cmd_cycle;
    uint32_t uasp_data_in_cycle;
    uint32_t uasp_data_out_cycle;
    uint32_t uasp_status_cycle;
    uint32_t in_ring_index;
    uint32_t out_ring_index;
    uint32_t in_cycle;
    uint32_t out_cycle;
    bool configured;
    uint32_t tag;
    uint32_t block_size;
    uint64_t block_count;
    bool readonly;
    uint8_t last_sense_key;
    uint8_t last_asc;
    uint8_t last_ascq;
    uint8_t last_status;
    bool uasp_failed;
    uint8_t max_lun;
    uint8_t interface_num;
} xhci_msc_dev_t;

static xhci_msc_dev_t g_msc;

static uint32_t g_cmd_ring_index = 0;
static uint32_t g_cmd_cycle = 1;
static uint32_t g_evt_cycle = 1;
static uint32_t g_evt_ring_index = 0;
static uint32_t g_ep0_ring_index = 0;
static uint32_t g_ep0_cycle = 1;
static uintptr_t g_xhci_base = 0;
static uint32_t g_xhci_dboff = 0;
static uintptr_t g_xhci_rt_base = 0;
static uint8_t g_xhci_cap_len = 0;
static uint8_t g_xhci_ports = 0;
static uint8_t g_xhci_enum_port = 0;
static uint8_t g_msc_port = 0;
static bool g_msc_present = false;
static uint8_t g_msc_last_luns = 0;
static int g_msc_drive_base = -1;

static inline uint32_t mmio_read32(uintptr_t base, uint32_t off);
static inline void mmio_write32(uintptr_t base, uint32_t off, uint32_t value);
static void xhci_msc_smoke_test(uintptr_t base, uint32_t dboff, uintptr_t rt_base);
static bool xhci_uasp_exec_scsi(uint8_t lun, uint8_t *cdb, uint8_t cdb_len,
                                const void *data, uint32_t data_len, bool data_in);
static void xhci_msc_delay_ticks(uint32_t ticks);
static bool xhci_msc_test_unit_ready(uintptr_t base, uint32_t dboff, uintptr_t rt_base, uint8_t lun);
static bool xhci_msc_configure_bot(uintptr_t base, uint32_t dboff, uintptr_t rt_base);
static bool xhci_msc_get_max_lun(uintptr_t base, uint32_t dboff, uintptr_t rt_base, uint8_t slot_id);
static bool xhci_msc_ensure_lun_info(uint8_t lun);
static void xhci_poll_hotplug(void);
static void xhci_msc_fire_events(bool inserted);
static bool xhci_port_reset(uintptr_t op_base, uint8_t port);

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
    memset(g_hid_kbd_ring, 0, sizeof(g_hid_kbd_ring));
    memset(g_hid_mouse_ring, 0, sizeof(g_hid_mouse_ring));
    memset(g_hid_kbd_buf, 0, sizeof(g_hid_kbd_buf));
    memset(g_hid_mouse_buf, 0, sizeof(g_hid_mouse_buf));
    memset(g_msc_in_ring, 0, sizeof(g_msc_in_ring));
    memset(g_msc_out_ring, 0, sizeof(g_msc_out_ring));
    memset(g_msc_data_buf, 0, sizeof(g_msc_data_buf));
    memset(g_msc_cbw, 0, sizeof(g_msc_cbw));
    memset(g_msc_csw, 0, sizeof(g_msc_csw));
    memset(g_uasp_cmd_ring, 0, sizeof(g_uasp_cmd_ring));
    memset(g_uasp_data_in_ring, 0, sizeof(g_uasp_data_in_ring));
    memset(g_uasp_data_out_ring, 0, sizeof(g_uasp_data_out_ring));
    memset(g_uasp_status_ring, 0, sizeof(g_uasp_status_ring));
    memset(g_uasp_cmd_iu, 0, sizeof(g_uasp_cmd_iu));
    memset(g_uasp_status_iu, 0, sizeof(g_uasp_status_iu));
    memset(g_msc_lun_block_size, 0, sizeof(g_msc_lun_block_size));
    memset(g_msc_lun_block_count, 0, sizeof(g_msc_lun_block_count));
    memset(g_msc_lun_valid, 0, sizeof(g_msc_lun_valid));
    memset(&g_hid_kbd, 0, sizeof(g_hid_kbd));
    memset(&g_hid_mouse, 0, sizeof(g_hid_mouse));
    memset(&g_msc, 0, sizeof(g_msc));
    g_cmd_ring_index = 0;
    g_cmd_cycle = 1;
    g_evt_cycle = 1;
    g_evt_ring_index = 0;
    g_ep0_ring_index = 0;
    g_ep0_cycle = 1;

    g_hid_kbd.ring = g_hid_kbd_ring;
    g_hid_kbd.buf = g_hid_kbd_buf;
    g_hid_kbd.ring_index = 0;
    g_hid_kbd.cycle = 1;

    g_hid_mouse.ring = g_hid_mouse_ring;
    g_hid_mouse.buf = g_hid_mouse_buf;
    g_hid_mouse.ring_index = 0;
    g_hid_mouse.cycle = 1;

    g_msc.in_ring_index = 0;
    g_msc.out_ring_index = 0;
    g_msc.in_cycle = 1;
    g_msc.out_cycle = 1;
    g_msc.tag = 1;
    g_msc.bot_present = false;
    g_msc.bot_configured = false;
    g_msc.uasp_cmd_ring_index = 0;
    g_msc.uasp_data_in_ring_index = 0;
    g_msc.uasp_data_out_ring_index = 0;
    g_msc.uasp_status_ring_index = 0;
    g_msc.uasp_cmd_cycle = 1;
    g_msc.uasp_data_in_cycle = 1;
    g_msc.uasp_data_out_cycle = 1;
    g_msc.uasp_status_cycle = 1;
    g_msc.max_lun = 1;
    g_msc.interface_num = 0;
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
                if (evt_slot == slot_id) {
                    return true;
                }
            } else {
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

static void xhci_build_hid_endpoint_context(uint8_t ep_id, uint16_t mps, uint8_t interval,
                                            xhci_trb_t *ring) {
    uint32_t ctx_dwords = g_ctx_size / 4;
    uint32_t *ictl = &g_input_ctx[0];
    uint32_t *ep = &g_input_ctx[ctx_dwords * (1 + ep_id)];

    ictl[0] |= (1u << ep_id);

    /* EP context: interrupt IN (type 7). */
    ep[0] = (7u << 3) | ((uint32_t)interval << 16);
    ep[1] = ((uint32_t)mps << 16);
    ep[2] = (uint32_t)((uintptr_t)ring & 0xFFFFFFFFu);
    ep[3] = 0;
}

static void xhci_build_bulk_endpoint_context(uint8_t ep_id, uint16_t mps, uint8_t type,
                                             xhci_trb_t *ring) {
    uint32_t ctx_dwords = g_ctx_size / 4;
    uint32_t *ictl = &g_input_ctx[0];
    uint32_t *ep = &g_input_ctx[ctx_dwords * (1 + ep_id)];

    ictl[0] |= (1u << ep_id);

    ep[0] = ((uint32_t)type << 3);
    ep[1] = ((uint32_t)mps << 16);
    ep[2] = (uint32_t)((uintptr_t)ring & 0xFFFFFFFFu);
    ep[3] = 0;
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

static bool xhci_configure_endpoint(uintptr_t base, uint32_t dboff, uintptr_t rt_base, uint8_t slot_id) {
    xhci_trb_t *trb = &g_cmd_ring[g_cmd_ring_index++];
    uintptr_t ictx = (uintptr_t)&g_input_ctx[0];
    trb->dword0 = (uint32_t)(ictx & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = 0;
    trb->dword3 = (XHCI_TRB_TYPE_CONFIGURE_ENDPOINT << XHCI_TRB_TYPE_SHIFT) |
                  ((uint32_t)slot_id << 24) |
                  (g_cmd_cycle ? XHCI_TRB_CYCLE : 0);

    xhci_ring_doorbell(base, dboff, 0);
    if (xhci_poll_cmd_complete(rt_base, NULL)) {
        serial_printf("[XHCI] Configure Endpoint complete for slot %u\n", slot_id);
        return true;
    }
    serial_printf("[XHCI] Configure Endpoint timeout for slot %u\n", slot_id);
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
    uint8_t current_proto = 0;
    uint8_t current_class = 0;
    uint8_t current_sub = 0;
    uint8_t uasp_in_eps[2] = {0};
    uint8_t uasp_out_eps[2] = {0};
    uint16_t uasp_in_mps[2] = {0};
    uint16_t uasp_out_mps[2] = {0};
    uint8_t uasp_in_count = 0;
    uint8_t uasp_out_count = 0;
    if (g_hid_kbd.slot == slot_id) {
        g_hid_kbd.ep_addr = 0;
        g_hid_kbd.ep_id = 0;
        g_hid_kbd.mps = 0;
        g_hid_kbd.interval = 0;
        g_hid_kbd.pending = false;
        g_hid_kbd.configured = false;
        memset(g_hid_kbd.last_report, 0, sizeof(g_hid_kbd.last_report));
    }
    if (g_hid_mouse.slot == slot_id) {
        g_hid_mouse.ep_addr = 0;
        g_hid_mouse.ep_id = 0;
        g_hid_mouse.mps = 0;
        g_hid_mouse.interval = 0;
        g_hid_mouse.pending = false;
        g_hid_mouse.configured = false;
    }
    if (g_msc.slot == slot_id) {
        g_msc.bulk_in_ep = 0;
        g_msc.bulk_out_ep = 0;
        g_msc.bulk_in_ep_id = 0;
        g_msc.bulk_out_ep_id = 0;
        g_msc.bulk_in_mps = 0;
        g_msc.bulk_out_mps = 0;
        g_msc.configured = false;
        g_msc.bot_present = false;
        g_msc.bot_configured = false;
        g_msc.bot_in_ep = 0;
        g_msc.bot_out_ep = 0;
        g_msc.bot_in_ep_id = 0;
        g_msc.bot_out_ep_id = 0;
        g_msc.bot_in_mps = 0;
        g_msc.bot_out_mps = 0;
        g_msc.uasp = false;
        g_msc.uasp_cmd_out_ep = 0;
        g_msc.uasp_data_out_ep = 0;
        g_msc.uasp_status_in_ep = 0;
        g_msc.uasp_data_in_ep = 0;
        g_msc.uasp_cmd_out_ep_id = 0;
        g_msc.uasp_data_out_ep_id = 0;
        g_msc.uasp_status_in_ep_id = 0;
        g_msc.uasp_data_in_ep_id = 0;
        g_msc.uasp_cmd_out_mps = 0;
        g_msc.uasp_data_out_mps = 0;
        g_msc.uasp_status_in_mps = 0;
        g_msc.uasp_data_in_mps = 0;
    }
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
            uint8_t if_num = buf[off + 2];
            serial_printf("[XHCI] IF cls=0x%02x sub=0x%02x proto=0x%02x\n", cls, sub, proto);
            current_class = cls;
            current_sub = sub;
            current_proto = proto;
            if (cls == 0x08 && (proto == 0x50 || proto == 0x62)) {
                g_msc.slot = slot_id;
                g_msc.uasp = (proto == 0x62);
                g_msc.interface_num = if_num;
                if (g_xhci_enum_port != 0) {
                    g_msc_port = g_xhci_enum_port;
                }
            }
        }
        if (type == 5 && len >= 7) {
            uint8_t ep_addr = buf[off + 2];
            uint8_t attrs = buf[off + 3];
            uint16_t mps = (uint16_t)(buf[off + 4] | (buf[off + 5] << 8));
            uint8_t interval = buf[off + 6];
            uint8_t ep_type = attrs & 0x3;
            if ((ep_addr & 0x80) && ep_type == 3 && (current_proto == 1 || current_proto == 2)) {
                xhci_hid_dev_t *dev = (current_proto == 1) ? &g_hid_kbd : &g_hid_mouse;
                if (dev->ep_addr == 0 || dev->slot == slot_id) {
                    dev->slot = slot_id;
                    dev->ep_addr = ep_addr;
                    dev->mps = mps;
                    dev->interval = interval;
                    dev->ep_id = (uint8_t)(2 * (ep_addr & 0x0F) + 1);
                    dev->pending = false;
                    if (current_proto == 1) {
                        memset(dev->last_report, 0, sizeof(dev->last_report));
                    }
                    serial_printf("[XHCI] HID %s INT IN ep=0x%02x mps=%u interval=%u\n",
                                  (current_proto == 1) ? "kbd" : "mouse",
                                  ep_addr, mps, interval);
                }
            }
            if (current_class == 0x08 && current_sub == 0x06 && ep_type == 2) {
                if (ep_addr & 0x80) {
                    if (g_msc.uasp && uasp_in_count < 2) {
                        uasp_in_eps[uasp_in_count] = ep_addr;
                        uasp_in_mps[uasp_in_count] = mps;
                        uasp_in_count++;
                    } else if (!g_msc.uasp) {
                        g_msc.bulk_in_ep = ep_addr;
                        g_msc.bulk_in_mps = mps;
                        g_msc.bulk_in_ep_id = (uint8_t)(2 * (ep_addr & 0x0F) + 1);
                    }
                    if (current_proto == 0x50) {
                        g_msc.bot_present = true;
                        g_msc.bot_in_ep = ep_addr;
                        g_msc.bot_in_mps = mps;
                        g_msc.bot_in_ep_id = (uint8_t)(2 * (ep_addr & 0x0F) + 1);
                    }
                } else {
                    if (g_msc.uasp && uasp_out_count < 2) {
                        uasp_out_eps[uasp_out_count] = ep_addr;
                        uasp_out_mps[uasp_out_count] = mps;
                        uasp_out_count++;
                    } else if (!g_msc.uasp) {
                        g_msc.bulk_out_ep = ep_addr;
                        g_msc.bulk_out_mps = mps;
                        g_msc.bulk_out_ep_id = (uint8_t)(2 * (ep_addr & 0x0F) + 0);
                    }
                    if (current_proto == 0x50) {
                        g_msc.bot_present = true;
                        g_msc.bot_out_ep = ep_addr;
                        g_msc.bot_out_mps = mps;
                        g_msc.bot_out_ep_id = (uint8_t)(2 * (ep_addr & 0x0F) + 0);
                    }
                }
                serial_printf("[XHCI] MSC bulk %s ep=0x%02x mps=%u\n",
                              (ep_addr & 0x80) ? "IN" : "OUT", ep_addr, mps);
            }
        }
        off += len;
    }

    if (g_msc.uasp && uasp_in_count >= 2 && uasp_out_count >= 2) {
        uint8_t in0 = uasp_in_eps[0];
        uint8_t in1 = uasp_in_eps[1];
        uint16_t in0_mps = uasp_in_mps[0];
        uint16_t in1_mps = uasp_in_mps[1];
        uint8_t out0 = uasp_out_eps[0];
        uint8_t out1 = uasp_out_eps[1];
        uint16_t out0_mps = uasp_out_mps[0];
        uint16_t out1_mps = uasp_out_mps[1];

        if ((in1 & 0x0F) < (in0 & 0x0F)) {
            uint8_t t = in0; in0 = in1; in1 = t;
            uint16_t tm = in0_mps; in0_mps = in1_mps; in1_mps = tm;
        }
        if ((out1 & 0x0F) < (out0 & 0x0F)) {
            uint8_t t = out0; out0 = out1; out1 = t;
            uint16_t tm = out0_mps; out0_mps = out1_mps; out1_mps = tm;
        }

        g_msc.uasp_status_in_ep = in0;
        g_msc.uasp_status_in_mps = in0_mps;
        g_msc.uasp_data_in_ep = in1;
        g_msc.uasp_data_in_mps = in1_mps;
        g_msc.uasp_cmd_out_ep = out0;
        g_msc.uasp_cmd_out_mps = out0_mps;
        g_msc.uasp_data_out_ep = out1;
        g_msc.uasp_data_out_mps = out1_mps;

        g_msc.uasp_status_in_ep_id = (uint8_t)(2 * (g_msc.uasp_status_in_ep & 0x0F) + 1);
        g_msc.uasp_data_in_ep_id = (uint8_t)(2 * (g_msc.uasp_data_in_ep & 0x0F) + 1);
        g_msc.uasp_cmd_out_ep_id = (uint8_t)(2 * (g_msc.uasp_cmd_out_ep & 0x0F) + 0);
        g_msc.uasp_data_out_ep_id = (uint8_t)(2 * (g_msc.uasp_data_out_ep & 0x0F) + 0);

        serial_printf("[XHCI] UASP cmd-out=0x%02x data-out=0x%02x status-in=0x%02x data-in=0x%02x\n",
                      g_msc.uasp_cmd_out_ep, g_msc.uasp_data_out_ep,
                      g_msc.uasp_status_in_ep, g_msc.uasp_data_in_ep);
    }

    if (config_value != 0) {
        xhci_ep0_set_configuration(base, dboff, rt_base, slot_id, config_value);
    }

    if (g_hid_kbd.ep_id != 0 && g_hid_kbd.slot == slot_id) {
        memset(g_input_ctx, 0, sizeof(g_input_ctx));
        xhci_build_hid_endpoint_context(g_hid_kbd.ep_id, g_hid_kbd.mps,
                                        g_hid_kbd.interval, g_hid_kbd.ring);
        g_hid_kbd.configured = xhci_configure_endpoint(base, dboff, rt_base, slot_id);
    }

    if (g_hid_mouse.ep_id != 0 && g_hid_mouse.slot == slot_id) {
        memset(g_input_ctx, 0, sizeof(g_input_ctx));
        xhci_build_hid_endpoint_context(g_hid_mouse.ep_id, g_hid_mouse.mps,
                                        g_hid_mouse.interval, g_hid_mouse.ring);
        g_hid_mouse.configured = xhci_configure_endpoint(base, dboff, rt_base, slot_id);
    }

    if (g_msc.uasp && g_msc.uasp_cmd_out_ep_id && g_msc.uasp_data_out_ep_id &&
        g_msc.uasp_status_in_ep_id && g_msc.uasp_data_in_ep_id && g_msc.slot == slot_id) {
        memset(g_input_ctx, 0, sizeof(g_input_ctx));
        xhci_build_bulk_endpoint_context(g_msc.uasp_cmd_out_ep_id, g_msc.uasp_cmd_out_mps, 2,
                                         g_uasp_cmd_ring);
        xhci_build_bulk_endpoint_context(g_msc.uasp_data_out_ep_id, g_msc.uasp_data_out_mps, 2,
                                         g_uasp_data_out_ring);
        xhci_build_bulk_endpoint_context(g_msc.uasp_status_in_ep_id, g_msc.uasp_status_in_mps, 6,
                                         g_uasp_status_ring);
        xhci_build_bulk_endpoint_context(g_msc.uasp_data_in_ep_id, g_msc.uasp_data_in_mps, 6,
                                         g_uasp_data_in_ring);
        g_msc.configured = xhci_configure_endpoint(base, dboff, rt_base, slot_id);
        if (g_msc.configured) {
            serial_puts("[XHCI] UASP endpoints configured\n");
            g_msc.max_lun = 1;
            xhci_msc_smoke_test(base, dboff, rt_base);
            g_msc_present = true;
            g_msc_last_luns = xhci_msc_get_lun_count();
            g_msc_drive_base = hal_storage_get_drive_count() - g_msc_last_luns;
            if (g_msc_drive_base < 0) {
                g_msc_drive_base = 0;
            }
            xhci_msc_fire_events(true);
        }
    } else if (!g_msc.uasp && g_msc.bulk_in_ep_id != 0 && g_msc.bulk_out_ep_id != 0 &&
               g_msc.slot == slot_id) {
        memset(g_input_ctx, 0, sizeof(g_input_ctx));
        xhci_build_bulk_endpoint_context(g_msc.bulk_out_ep_id, g_msc.bulk_out_mps, 2,
                                         g_msc_out_ring);
        xhci_build_bulk_endpoint_context(g_msc.bulk_in_ep_id, g_msc.bulk_in_mps, 6,
                                         g_msc_in_ring);
        g_msc.configured = xhci_configure_endpoint(base, dboff, rt_base, slot_id);
        if (g_msc.configured) {
            serial_puts("[XHCI] MSC endpoints configured\n");
            g_msc.max_lun = 1;
            xhci_msc_get_max_lun(base, dboff, rt_base, slot_id);
            xhci_msc_smoke_test(base, dboff, rt_base);
            g_msc_present = true;
            g_msc_last_luns = xhci_msc_get_lun_count();
            g_msc_drive_base = hal_storage_get_drive_count() - g_msc_last_luns;
            if (g_msc_drive_base < 0) {
                g_msc_drive_base = 0;
            }
            xhci_msc_fire_events(true);
        }
    } else if (g_msc.bot_present && g_msc.slot == slot_id) {
        if (xhci_msc_configure_bot(base, dboff, rt_base)) {
            g_msc.max_lun = 1;
            xhci_msc_get_max_lun(base, dboff, rt_base, slot_id);
            xhci_msc_smoke_test(base, dboff, rt_base);
            g_msc_present = true;
            g_msc_last_luns = xhci_msc_get_lun_count();
            g_msc_drive_base = hal_storage_get_drive_count() - g_msc_last_luns;
            if (g_msc_drive_base < 0) {
                g_msc_drive_base = 0;
            }
            xhci_msc_fire_events(true);
        }
    }

    return true;
}

static void xhci_hid_enqueue_interrupt_in(xhci_hid_dev_t *dev, uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &dev->ring[dev->ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (dev->cycle ? XHCI_TRB_CYCLE : 0);

    if (dev->ring_index >= 31) {
        xhci_trb_t *link = &dev->ring[dev->ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&dev->ring[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        dev->ring_index = 0;
        dev->cycle ^= 1;
    }
}

static void xhci_msc_enqueue_out(uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &g_msc_out_ring[g_msc.out_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (g_msc.out_cycle ? XHCI_TRB_CYCLE : 0);

    if (g_msc.out_ring_index >= 31) {
        xhci_trb_t *link = &g_msc_out_ring[g_msc.out_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&g_msc_out_ring[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        g_msc.out_ring_index = 0;
        g_msc.out_cycle ^= 1;
    }
}

static void xhci_msc_enqueue_in(uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &g_msc_in_ring[g_msc.in_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (g_msc.in_cycle ? XHCI_TRB_CYCLE : 0);

    if (g_msc.in_ring_index >= 31) {
        xhci_trb_t *link = &g_msc_in_ring[g_msc.in_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&g_msc_in_ring[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        g_msc.in_ring_index = 0;
        g_msc.in_cycle ^= 1;
    }
}

static void xhci_uasp_enqueue_cmd(uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &g_uasp_cmd_ring[g_msc.uasp_cmd_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (g_msc.uasp_cmd_cycle ? XHCI_TRB_CYCLE : 0);

    if (g_msc.uasp_cmd_ring_index >= 31) {
        xhci_trb_t *link = &g_uasp_cmd_ring[g_msc.uasp_cmd_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&g_uasp_cmd_ring[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        g_msc.uasp_cmd_ring_index = 0;
        g_msc.uasp_cmd_cycle ^= 1;
    }
}

static void xhci_uasp_enqueue_data_in(uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &g_uasp_data_in_ring[g_msc.uasp_data_in_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (g_msc.uasp_data_in_cycle ? XHCI_TRB_CYCLE : 0);

    if (g_msc.uasp_data_in_ring_index >= 31) {
        xhci_trb_t *link = &g_uasp_data_in_ring[g_msc.uasp_data_in_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&g_uasp_data_in_ring[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        g_msc.uasp_data_in_ring_index = 0;
        g_msc.uasp_data_in_cycle ^= 1;
    }
}

static void xhci_uasp_enqueue_data_out(uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &g_uasp_data_out_ring[g_msc.uasp_data_out_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (g_msc.uasp_data_out_cycle ? XHCI_TRB_CYCLE : 0);

    if (g_msc.uasp_data_out_ring_index >= 31) {
        xhci_trb_t *link = &g_uasp_data_out_ring[g_msc.uasp_data_out_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&g_uasp_data_out_ring[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        g_msc.uasp_data_out_ring_index = 0;
        g_msc.uasp_data_out_cycle ^= 1;
    }
}

static void xhci_uasp_enqueue_status(uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &g_uasp_status_ring[g_msc.uasp_status_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (g_msc.uasp_status_cycle ? XHCI_TRB_CYCLE : 0);

    if (g_msc.uasp_status_ring_index >= 31) {
        xhci_trb_t *link = &g_uasp_status_ring[g_msc.uasp_status_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&g_uasp_status_ring[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        g_msc.uasp_status_ring_index = 0;
        g_msc.uasp_status_cycle ^= 1;
    }
}

static int xhci_poll_transfer_event(uintptr_t rt_base, uint8_t *out_slot) {
    xhci_trb_t *evt = &g_evt_ring[g_evt_ring_index];
    uint32_t cycle = evt->dword3 & XHCI_TRB_CYCLE;
    if (cycle != (g_evt_cycle ? XHCI_TRB_CYCLE : 0)) {
        return 0;
    }

    uint32_t type = (evt->dword3 & XHCI_TRB_TYPE_MASK) >> XHCI_TRB_TYPE_SHIFT;
    if (type == XHCI_TRB_TYPE_EVT_TRANSFER) {
        uint8_t evt_slot = (uint8_t)((evt->dword3 >> 24) & 0xFF);
        uint32_t comp = (evt->dword2 >> 24) & 0xFF;
        uint32_t len = evt->dword2 & 0xFFFFFF;
        if (comp != 1) {
            serial_printf("[XHCI] Transfer complete slot=%u code=%u len=%u\n",
                          evt_slot, comp, len);
        }
        if (out_slot) {
            *out_slot = evt_slot;
        }
    }

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
    return (type == XHCI_TRB_TYPE_EVT_TRANSFER) ? 1 : -1;
}

static void xhci_handle_hid_keyboard(xhci_hid_dev_t *dev, const uint8_t *report);
static void xhci_handle_hid_mouse(const uint8_t *report, uint32_t len) {
    if (len < 3) {
        return;
    }
    int16_t dx = (int8_t)report[1];
    int16_t dy = (int8_t)report[2];
    uint8_t buttons = report[0] & 0x1F;
    UpdateMouseStateDelta(dx, -dy, buttons);

    if (len >= 4) {
        int8_t wheel = (int8_t)report[3];
        if (wheel != 0) {
            UInt16 mods = GetModifierState();
            ProcessScrollWheelEvent(0, (SInt16)-wheel, mods, TickCount());
        }
    }
    if (len >= 5) {
        int8_t pan = (int8_t)report[4];
        if (pan != 0) {
            UInt16 mods = GetModifierState();
            ProcessScrollWheelEvent((SInt16)pan, 0, mods, TickCount());
        }
    }
}

static void xhci_hid_submit(uintptr_t base, uint32_t dboff, xhci_hid_dev_t *dev) {
    if (!dev->configured || dev->pending || dev->slot == 0 || dev->ep_id == 0 || dev->mps == 0) {
        return;
    }

    uint32_t now = TickCount();
    if (dev->last_submit_tick == now) {
        return;
    }

    xhci_hid_enqueue_interrupt_in(dev, (uintptr_t)dev->buf, dev->mps);
    mmio_write32(base + dboff, XHCI_DB0 + dev->slot, dev->ep_id);
    dev->pending = true;
    dev->pending_tick = now;
    dev->last_submit_tick = now;
}

static void xhci_hid_poll_events(uintptr_t rt_base) {
    uint32_t now = TickCount();
    if (g_hid_kbd.pending && (now - g_hid_kbd.pending_tick) > 10) {
        g_hid_kbd.pending = false;
    }
    if (g_hid_mouse.pending && (now - g_hid_mouse.pending_tick) > 10) {
        g_hid_mouse.pending = false;
    }

    for (int i = 0; i < 4; i++) {
        uint8_t slot = 0;
        int res = xhci_poll_transfer_event(rt_base, &slot);
        if (res == 0) {
            break;
        }
        if (res < 0) {
            continue;
        }
        if (slot == g_hid_kbd.slot && g_hid_kbd.pending) {
            g_hid_kbd.pending = false;
            xhci_handle_hid_keyboard(&g_hid_kbd, (const uint8_t *)g_hid_kbd.buf);
        } else if (slot == g_hid_mouse.slot && g_hid_mouse.pending) {
            g_hid_mouse.pending = false;
            xhci_handle_hid_mouse((const uint8_t *)g_hid_mouse.buf, g_hid_mouse.mps);
        }
    }
}

static bool xhci_msc_bulk_transfer(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                   bool in, const void *buf, uint32_t len) {
    if (!g_msc.configured) {
        return false;
    }
    if (in) {
        xhci_msc_enqueue_in((uintptr_t)buf, len);
        mmio_write32(base + dboff, XHCI_DB0 + g_msc.slot, g_msc.bulk_in_ep_id);
    } else {
        xhci_msc_enqueue_out((uintptr_t)buf, len);
        mmio_write32(base + dboff, XHCI_DB0 + g_msc.slot, g_msc.bulk_out_ep_id);
    }
    return xhci_poll_transfer_complete(rt_base, g_msc.slot);
}

static bool xhci_uasp_transfer_cmd(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                  const void *buf, uint32_t len) {
    if (!g_msc.configured || !g_msc.uasp_cmd_out_ep_id) {
        return false;
    }
    xhci_uasp_enqueue_cmd((uintptr_t)buf, len);
    mmio_write32(base + dboff, XHCI_DB0 + g_msc.slot, g_msc.uasp_cmd_out_ep_id);
    return xhci_poll_transfer_complete(rt_base, g_msc.slot);
}

static bool xhci_uasp_transfer_data(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                   bool in, const void *buf, uint32_t len) {
    if (!g_msc.configured) {
        return false;
    }
    if (in) {
        if (!g_msc.uasp_data_in_ep_id) {
            return false;
        }
        xhci_uasp_enqueue_data_in((uintptr_t)buf, len);
        mmio_write32(base + dboff, XHCI_DB0 + g_msc.slot, g_msc.uasp_data_in_ep_id);
    } else {
        if (!g_msc.uasp_data_out_ep_id) {
            return false;
        }
        xhci_uasp_enqueue_data_out((uintptr_t)buf, len);
        mmio_write32(base + dboff, XHCI_DB0 + g_msc.slot, g_msc.uasp_data_out_ep_id);
    }
    return xhci_poll_transfer_complete(rt_base, g_msc.slot);
}

static bool xhci_uasp_transfer_status(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                     void *buf, uint32_t len) {
    if (!g_msc.configured || !g_msc.uasp_status_in_ep_id) {
        return false;
    }
    xhci_uasp_enqueue_status((uintptr_t)buf, len);
    mmio_write32(base + dboff, XHCI_DB0 + g_msc.slot, g_msc.uasp_status_in_ep_id);
    return xhci_poll_transfer_complete(rt_base, g_msc.slot);
}

static bool xhci_msc_send_cbw(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                              uint8_t lun, uint8_t *cb, uint8_t cb_len,
                              uint32_t data_len, bool data_in) {
    memset(g_msc_cbw, 0, sizeof(g_msc_cbw));
    g_msc_cbw[0] = 'U';
    g_msc_cbw[1] = 'S';
    g_msc_cbw[2] = 'B';
    g_msc_cbw[3] = 'C';
    uint32_t tag = g_msc.tag++;
    g_msc_cbw[4] = (uint8_t)(tag & 0xFF);
    g_msc_cbw[5] = (uint8_t)((tag >> 8) & 0xFF);
    g_msc_cbw[6] = (uint8_t)((tag >> 16) & 0xFF);
    g_msc_cbw[7] = (uint8_t)((tag >> 24) & 0xFF);
    g_msc_cbw[8] = (uint8_t)(data_len & 0xFF);
    g_msc_cbw[9] = (uint8_t)((data_len >> 8) & 0xFF);
    g_msc_cbw[10] = (uint8_t)((data_len >> 16) & 0xFF);
    g_msc_cbw[11] = (uint8_t)((data_len >> 24) & 0xFF);
    g_msc_cbw[12] = data_in ? 0x80 : 0x00;
    g_msc_cbw[13] = lun;
    g_msc_cbw[14] = cb_len;
    memcpy(&g_msc_cbw[15], cb, cb_len);

    return xhci_msc_bulk_transfer(base, dboff, rt_base, false, g_msc_cbw, 31);
}

static bool xhci_msc_recv_csw(uintptr_t base, uint32_t dboff, uintptr_t rt_base) {
    if (!xhci_msc_bulk_transfer(base, dboff, rt_base, true, g_msc_csw, 13)) {
        return false;
    }
    if (g_msc_csw[0] != 'U' || g_msc_csw[1] != 'S' || g_msc_csw[2] != 'B' || g_msc_csw[3] != 'S') {
        serial_puts("[XHCI] MSC CSW bad signature\n");
        return false;
    }
    g_msc.last_status = g_msc_csw[12];
    if (g_msc.last_status != 0) {
        serial_printf("[XHCI] MSC CSW status=%u\n", g_msc.last_status);
        return false;
    }
    return true;
}

static void xhci_msc_parse_sense(const uint8_t *buf, uint32_t len) {
    if (!buf || len < 14) {
        return;
    }
    if ((buf[0] & 0x7F) != 0x70 && (buf[0] & 0x7F) != 0x71) {
        return;
    }
    g_msc.last_sense_key = buf[2] & 0x0F;
    g_msc.last_asc = buf[12];
    g_msc.last_ascq = buf[13];
}

static void xhci_msc_delay_ticks(uint32_t ticks) {
    uint32_t start = TickCount();
    while ((TickCount() - start) < ticks) {
        /* busy wait */
    }
}

static bool xhci_msc_request_sense_bot(uintptr_t base, uint32_t dboff, uintptr_t rt_base, uint8_t lun) {
    uint8_t cb[16];
    memset(cb, 0, sizeof(cb));
    cb[0] = 0x03; /* REQUEST SENSE */
    cb[4] = 18;
    if (!xhci_msc_send_cbw(base, dboff, rt_base, lun, cb, 6, 18, true)) {
        return false;
    }
    if (!xhci_msc_bulk_transfer(base, dboff, rt_base, true, g_msc_data_buf, 18)) {
        return false;
    }
    if (!xhci_msc_recv_csw(base, dboff, rt_base)) {
        return false;
    }
    xhci_msc_parse_sense((const uint8_t *)g_msc_data_buf, 18);
    return true;
}

static bool xhci_msc_request_sense_uasp(uint8_t lun) {
    uint8_t cb[16];
    memset(cb, 0, sizeof(cb));
    cb[0] = 0x03; /* REQUEST SENSE */
    cb[4] = 18;
    if (!xhci_uasp_exec_scsi(lun, cb, 6, g_msc_data_buf, 18, true)) {
        return false;
    }
    xhci_msc_parse_sense((const uint8_t *)g_msc_data_buf, 18);
    return true;
}

static void xhci_msc_log_sense(const char *prefix) {
    if (!prefix) {
        prefix = "MSC";
    }
    serial_printf("[XHCI] %s sense key=0x%02x asc=0x%02x ascq=0x%02x\n",
                  prefix, g_msc.last_sense_key, g_msc.last_asc, g_msc.last_ascq);
}

static bool xhci_msc_retry_not_ready(bool uasp, uint8_t lun) {
    if (g_msc.last_sense_key != 0x02) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        xhci_msc_delay_ticks(5);
        if (xhci_msc_test_unit_ready(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, lun)) {
            return true;
        }
    }
    return false;
}

static bool xhci_uasp_exec_scsi(uint8_t lun, uint8_t *cdb, uint8_t cdb_len,
                                const void *data, uint32_t data_len, bool data_in) {
    if (!g_msc.uasp) {
        return false;
    }
    memset(g_uasp_cmd_iu, 0, sizeof(g_uasp_cmd_iu));
    g_uasp_cmd_iu[0] = 0x01; /* Command IU */
    g_uasp_cmd_iu[2] = (uint8_t)(g_msc.tag & 0xFF);
    g_uasp_cmd_iu[3] = (uint8_t)((g_msc.tag >> 8) & 0xFF);
    g_uasp_cmd_iu[4] = (uint8_t)(data_len & 0xFF);
    g_uasp_cmd_iu[5] = (uint8_t)((data_len >> 8) & 0xFF);
    g_uasp_cmd_iu[6] = (uint8_t)((data_len >> 16) & 0xFF);
    g_uasp_cmd_iu[7] = (uint8_t)((data_len >> 24) & 0xFF);
    g_uasp_cmd_iu[8] = lun;
    memset(&g_uasp_cmd_iu[9], 0, 7);
    memcpy(&g_uasp_cmd_iu[16], cdb, cdb_len);

    g_msc.tag++;

    if (!xhci_uasp_transfer_cmd(g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                g_uasp_cmd_iu, sizeof(g_uasp_cmd_iu))) {
        return false;
    }

    if (data_len > 0) {
        if (!xhci_uasp_transfer_data(g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                     data_in, data, data_len)) {
            return false;
        }
    }

    memset(g_uasp_status_iu, 0, sizeof(g_uasp_status_iu));
    if (!xhci_uasp_transfer_status(g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                   g_uasp_status_iu, 16)) {
        return false;
    }
    if (g_uasp_status_iu[0] != 0x03) {
        serial_printf("[XHCI] UASP status IU id=0x%02x\n", g_uasp_status_iu[0]);
        return false;
    }
    g_msc.last_status = g_uasp_status_iu[4];
    if (g_msc.last_status != 0) {
        serial_printf("[XHCI] UASP status=0x%02x\n", g_msc.last_status);
        return false;
    }
    return true;
}

static bool xhci_msc_read_capacity(uintptr_t base, uint32_t dboff, uintptr_t rt_base, uint8_t lun) {
    uint8_t cb[16];

    memset(cb, 0, sizeof(cb));
    cb[0] = 0x25; /* READ CAPACITY(10) */
    if (g_msc.uasp) {
        if (!xhci_uasp_exec_scsi(lun, cb, 10, g_msc_data_buf, 8, true)) {
            serial_puts("[XHCI] UASP READ CAPACITY failed\n");
            xhci_msc_request_sense_uasp(lun);
            xhci_msc_log_sense("UASP");
            return false;
        }
    } else {
        if (!xhci_msc_send_cbw(base, dboff, rt_base, lun, cb, 10, 8, true)) {
            serial_puts("[XHCI] MSC READ CAPACITY CBW failed\n");
            return false;
        }
        if (!xhci_msc_bulk_transfer(base, dboff, rt_base, true, g_msc_data_buf, 8)) {
            serial_puts("[XHCI] MSC READ CAPACITY data failed\n");
            xhci_msc_request_sense_bot(base, dboff, rt_base, lun);
            xhci_msc_log_sense("MSC");
            return false;
        }
        if (!xhci_msc_recv_csw(base, dboff, rt_base)) {
            serial_puts("[XHCI] MSC READ CAPACITY CSW failed\n");
            xhci_msc_request_sense_bot(base, dboff, rt_base, lun);
            xhci_msc_log_sense("MSC");
            return false;
        }
    }

    uint32_t last_lba = (g_msc_data_buf[0] << 24) | (g_msc_data_buf[1] << 16) |
                        (g_msc_data_buf[2] << 8) | g_msc_data_buf[3];
    uint32_t blk_size = (g_msc_data_buf[4] << 24) | (g_msc_data_buf[5] << 16) |
                        (g_msc_data_buf[6] << 8) | g_msc_data_buf[7];
    if (lun < MAX_MSC_LUNS) {
        g_msc_lun_block_size[lun] = blk_size;
        g_msc_lun_block_count[lun] = (uint64_t)last_lba + 1u;
        g_msc_lun_valid[lun] = true;
    }
    if (lun == 0) {
        g_msc.block_size = blk_size;
        g_msc.block_count = (uint64_t)last_lba + 1u;
    }
    serial_printf("[XHCI] MSC capacity last_lba=%u block_size=%u\n", last_lba, blk_size);
    return (blk_size != 0);
}

static bool xhci_msc_configure_bot(uintptr_t base, uint32_t dboff, uintptr_t rt_base) {
    if (!g_msc.bot_present || g_msc.bot_in_ep_id == 0 || g_msc.bot_out_ep_id == 0) {
        return false;
    }
    memset(g_input_ctx, 0, sizeof(g_input_ctx));
    xhci_build_bulk_endpoint_context(g_msc.bot_out_ep_id, g_msc.bot_out_mps, 2, g_msc_out_ring);
    xhci_build_bulk_endpoint_context(g_msc.bot_in_ep_id, g_msc.bot_in_mps, 6, g_msc_in_ring);
    if (!xhci_configure_endpoint(base, dboff, rt_base, g_msc.slot)) {
        return false;
    }
    g_msc.configured = true;
    g_msc.bot_configured = true;
    g_msc.uasp = false;
    serial_puts("[XHCI] BOT endpoints configured\n");
    return true;
}

static bool xhci_msc_get_max_lun(uintptr_t base, uint32_t dboff, uintptr_t rt_base, uint8_t slot_id) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0xA1, /* IN | Class | Interface */
        .bRequest = 0xFE,      /* GET_MAX_LUN */
        .wValue = 0,
        .wIndex = g_msc.interface_num,
        .wLength = 1
    };

    memset(g_ep0_buf, 0, sizeof(g_ep0_buf));
    xhci_ep0_enqueue_setup(&setup);
    xhci_ep0_enqueue_data_in((uintptr_t)&g_ep0_buf[0], 1);
    xhci_ep0_enqueue_status();
    xhci_ep0_ring_doorbell(base, dboff, slot_id);

    if (!xhci_poll_transfer_complete(rt_base, slot_id)) {
        return false;
    }
    uint8_t max_lun = ((uint8_t *)g_ep0_buf)[0];
    g_msc.max_lun = (uint8_t)(max_lun + 1);
    if (g_msc.max_lun == 0) {
        g_msc.max_lun = 1;
    }
    if (g_msc.max_lun > MAX_MSC_LUNS) {
        g_msc.max_lun = MAX_MSC_LUNS;
    }
    serial_printf("[XHCI] MSC max LUN=%u\n", (unsigned)g_msc.max_lun - 1);
    return true;
}

static bool xhci_msc_try_fallback_bot(void) {
    if (!g_xhci_base || !g_xhci_rt_base) {
        return false;
    }
    if (!g_msc.bot_present || g_msc.bot_configured) {
        return false;
    }
    if (!xhci_msc_configure_bot(g_xhci_base, g_xhci_dboff, g_xhci_rt_base)) {
        return false;
    }
    return true;
}

static void xhci_msc_fire_events(bool inserted) {
    if (g_msc_last_luns == 0) {
        return;
    }
    int base = g_msc_drive_base;
    if (base < 0) {
        base = 0;
    }
    for (uint8_t i = 0; i < g_msc_last_luns; i++) {
        if (inserted) {
            ProcessDiskInsertion((SInt16)(base + i), "USB");
        } else {
            ProcessDiskEjection((SInt16)(base + i));
        }
    }
}

static void xhci_poll_hotplug(void) {
    if (!g_xhci_base || !g_xhci_cap_len || g_msc_port == 0) {
        return;
    }
    uintptr_t op_base = g_xhci_base + g_xhci_cap_len;
    uint32_t portsc = mmio_read32(op_base, XHCI_PORTSC_BASE + (g_msc_port - 1) * XHCI_PORTSC_STRIDE);
    bool connected = (portsc & XHCI_PORTSC_CCS) != 0;

    if (!connected && g_msc_present) {
        g_msc_present = false;
        g_msc.configured = false;
        g_msc.bot_configured = false;
        g_msc.uasp_failed = false;
        g_msc.slot = 0;
        g_msc.block_size = 0;
        g_msc.block_count = 0;
        memset(g_msc_lun_valid, 0, sizeof(g_msc_lun_valid));
        xhci_msc_fire_events(false);
        g_msc_last_luns = 0;
        g_msc_drive_base = -1;
        return;
    }

    if (connected && !g_msc_present && !g_msc.configured) {
        if (!xhci_port_reset(op_base, (uint8_t)(g_msc_port - 1))) {
            return;
        }
        xhci_cmd_ring_enqueue_enable_slot();
        xhci_ring_doorbell(g_xhci_base, g_xhci_dboff, 0);
        uint8_t slot_id = 0;
        if (!xhci_poll_cmd_complete(g_xhci_rt_base, &slot_id)) {
            return;
        }
        if (!xhci_address_device(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, slot_id, g_msc_port, portsc)) {
            return;
        }
        if (!xhci_ep0_get_device_descriptor(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, slot_id)) {
            return;
        }
        if (!xhci_ep0_get_config_descriptor(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, slot_id)) {
            return;
        }
        if (g_msc.configured) {
            g_msc_present = true;
            g_msc_last_luns = xhci_msc_get_lun_count();
            g_msc_drive_base = hal_storage_get_drive_count() - g_msc_last_luns;
            if (g_msc_drive_base < 0) {
                g_msc_drive_base = 0;
            }
            xhci_msc_fire_events(true);
        }
    }
}

static bool xhci_msc_inquiry(uintptr_t base, uint32_t dboff, uintptr_t rt_base, uint8_t lun) {
    uint8_t cb[16];
    memset(cb, 0, sizeof(cb));
    cb[0] = 0x12; /* INQUIRY */
    cb[4] = 36;
    if (g_msc.uasp) {
        if (!xhci_uasp_exec_scsi(lun, cb, 6, g_msc_data_buf, 36, true)) {
            serial_puts("[XHCI] UASP INQUIRY failed\n");
            xhci_msc_request_sense_uasp(lun);
            xhci_msc_log_sense("UASP");
            return false;
        }
    } else {
        if (!xhci_msc_send_cbw(base, dboff, rt_base, lun, cb, 6, 36, true)) {
            serial_puts("[XHCI] MSC INQUIRY CBW failed\n");
            return false;
        }
        if (!xhci_msc_bulk_transfer(base, dboff, rt_base, true, g_msc_data_buf, 36)) {
            serial_puts("[XHCI] MSC INQUIRY data failed\n");
            xhci_msc_request_sense_bot(base, dboff, rt_base, lun);
            xhci_msc_log_sense("MSC");
            return false;
        }
        if (!xhci_msc_recv_csw(base, dboff, rt_base)) {
            serial_puts("[XHCI] MSC INQUIRY CSW failed\n");
            xhci_msc_request_sense_bot(base, dboff, rt_base, lun);
            xhci_msc_log_sense("MSC");
            return false;
        }
    }

    serial_printf("[XHCI] MSC INQUIRY vendor=%.8s product=%.16s\n",
                  (char *)&g_msc_data_buf[2], (char *)&g_msc_data_buf[6]);
    return true;
}

static bool xhci_msc_test_unit_ready(uintptr_t base, uint32_t dboff, uintptr_t rt_base, uint8_t lun) {
    uint8_t cb[16];
    memset(cb, 0, sizeof(cb));
    cb[0] = 0x00; /* TEST UNIT READY */
    if (g_msc.uasp) {
        if (!xhci_uasp_exec_scsi(lun, cb, 6, NULL, 0, true)) {
            xhci_msc_request_sense_uasp(lun);
            xhci_msc_log_sense("UASP");
            return xhci_msc_retry_not_ready(true, lun);
        }
        return true;
    }
    if (!xhci_msc_send_cbw(base, dboff, rt_base, lun, cb, 6, 0, false)) {
        return false;
    }
    if (!xhci_msc_recv_csw(base, dboff, rt_base)) {
        xhci_msc_request_sense_bot(base, dboff, rt_base, lun);
        xhci_msc_log_sense("MSC");
        return xhci_msc_retry_not_ready(false, lun);
    }
    return true;
}

static void xhci_msc_smoke_test(uintptr_t base, uint32_t dboff, uintptr_t rt_base) {
    uint8_t cb[16];
    if (!xhci_msc_inquiry(base, dboff, rt_base, 0)) {
        return;
    }
    if (!xhci_msc_read_capacity(base, dboff, rt_base, 0)) {
        return;
    }

    if (g_msc.block_size == 0 || g_msc.block_size > sizeof(g_msc_data_buf)) {
        serial_puts("[XHCI] MSC block size unsupported for smoke test\n");
        return;
    }

    memset(cb, 0, sizeof(cb));
    cb[0] = 0x28; /* READ(10) */
    cb[7] = 0;
    cb[8] = 1; /* 1 block */
    if (!xhci_msc_send_cbw(base, dboff, rt_base, 0, cb, 10, g_msc.block_size, true)) {
        serial_puts("[XHCI] MSC READ10 CBW failed\n");
        return;
    }
    if (!xhci_msc_bulk_transfer(base, dboff, rt_base, true, g_msc_data_buf, g_msc.block_size)) {
        serial_puts("[XHCI] MSC READ10 data failed\n");
        return;
    }
    if (!xhci_msc_recv_csw(base, dboff, rt_base)) {
        serial_puts("[XHCI] MSC READ10 CSW failed\n");
        return;
    }

    serial_printf("[XHCI] MSC LBA0: %02x %02x %02x %02x\n",
                  ((uint8_t *)g_msc_data_buf)[0], ((uint8_t *)g_msc_data_buf)[1],
                  ((uint8_t *)g_msc_data_buf)[2], ((uint8_t *)g_msc_data_buf)[3]);
}

static bool xhci_msc_init_if_needed(void) {
    if (!g_msc.configured || !g_xhci_base || !g_xhci_rt_base) {
        return false;
    }
    if (g_msc.block_size != 0 && g_msc.block_count != 0) {
        return true;
    }

    if (!xhci_msc_test_unit_ready(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0)) {
        serial_puts("[XHCI] MSC not ready\n");
    }

    if (!xhci_msc_inquiry(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0)) {
        if (g_msc.uasp) {
            g_msc.uasp_failed = true;
            if (xhci_msc_try_fallback_bot()) {
                return xhci_msc_inquiry(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0) &&
                       xhci_msc_read_capacity(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0);
            }
        }
        return false;
    }
    if (!xhci_msc_read_capacity(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0)) {
        if (g_msc.uasp) {
            g_msc.uasp_failed = true;
            if (xhci_msc_try_fallback_bot()) {
                return xhci_msc_read_capacity(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0);
            }
        }
        return false;
    }
    return true;
}

bool xhci_msc_available(void) {
    return xhci_msc_init_if_needed();
}

OSErr xhci_msc_get_info(uint32_t *block_size, uint64_t *block_count, bool *read_only) {
    if (!xhci_msc_available()) {
        return paramErr;
    }
    if (g_msc.uasp_failed) {
        serial_puts("[XHCI] UASP failed, device may require BOT fallback\n");
    }
    if (block_size) {
        *block_size = g_msc.block_size;
    }
    if (block_count) {
        *block_count = g_msc.block_count;
    }
    if (read_only) {
        *read_only = g_msc.readonly;
    }
    return noErr;
}

OSErr xhci_msc_get_info_lun(uint8_t lun, uint32_t *block_size, uint64_t *block_count, bool *read_only) {
    if (!xhci_msc_ensure_lun_info(lun)) {
        return paramErr;
    }
    if (block_size) {
        *block_size = g_msc_lun_block_size[lun];
    }
    if (block_count) {
        *block_count = g_msc_lun_block_count[lun];
    }
    if (read_only) {
        *read_only = g_msc.readonly;
    }
    return noErr;
}

static bool xhci_msc_ensure_lun_info(uint8_t lun) {
    if (!xhci_msc_available()) {
        return false;
    }
    if (lun >= g_msc.max_lun || lun >= MAX_MSC_LUNS) {
        return false;
    }
    if (g_msc_lun_valid[lun]) {
        return true;
    }
    return xhci_msc_read_capacity(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, lun);
}

static OSErr xhci_msc_read_blocks_internal(uint8_t lun, uint64_t start_block,
                                           uint32_t block_count, void *buffer) {
    if (!xhci_msc_available() || !buffer) {
        return paramErr;
    }
    if (!xhci_msc_ensure_lun_info(lun)) {
        return ioErr;
    }
    uint32_t blk_size = g_msc_lun_block_size[lun];
    uint64_t blk_count = g_msc_lun_block_count[lun];
    if (blk_size == 0 || blk_count == 0) {
        return ioErr;
    }
    if (start_block >= blk_count) {
        return paramErr;
    }
    if (block_count == 0) {
        return noErr;
    }
    if (start_block + block_count > blk_count) {
        return paramErr;
    }
    if (blk_size > sizeof(g_msc_data_buf)) {
        return paramErr;
    }

    uint8_t *buf = (uint8_t *)buffer;
    uint32_t max_blocks = (uint32_t)(sizeof(g_msc_data_buf) / blk_size);
    if (max_blocks == 0) {
        return paramErr;
    }

    uint64_t lba = start_block;
    uint32_t remaining = block_count;

    bool tried_fallback = false;
    while (remaining > 0) {
        uint32_t chunk = remaining;
        if (chunk > max_blocks) {
            chunk = max_blocks;
        }
        if (chunk > 0xFFFFu) {
            chunk = 0xFFFFu;
        }
        if (lba > 0xFFFFFFFFu) {
            return paramErr;
        }

        uint8_t cb[16];
        memset(cb, 0, sizeof(cb));
        cb[0] = 0x28; /* READ(10) */
        cb[2] = (uint8_t)((lba >> 24) & 0xFF);
        cb[3] = (uint8_t)((lba >> 16) & 0xFF);
        cb[4] = (uint8_t)((lba >> 8) & 0xFF);
        cb[5] = (uint8_t)(lba & 0xFF);
        cb[7] = (uint8_t)((chunk >> 8) & 0xFF);
        cb[8] = (uint8_t)(chunk & 0xFF);

        uint32_t data_len = chunk * blk_size;
        if (g_msc.uasp) {
            if (!xhci_uasp_exec_scsi(lun, cb, 10, buf, data_len, true)) {
                xhci_msc_request_sense_uasp(lun);
                xhci_msc_log_sense("UASP");
                if (xhci_msc_retry_not_ready(true, lun)) {
                    continue;
                }
                g_msc.uasp_failed = true;
                if (!tried_fallback && xhci_msc_try_fallback_bot()) {
                    tried_fallback = true;
                    continue;
                }
                return ioErr;
            }
        } else {
            if (!xhci_msc_send_cbw(g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                   lun, cb, 10, data_len, true)) {
                return ioErr;
            }

            if (!xhci_msc_bulk_transfer(g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                        true, buf, data_len)) {
                xhci_msc_request_sense_bot(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, lun);
                xhci_msc_log_sense("MSC");
                if (xhci_msc_retry_not_ready(false, lun)) {
                    continue;
                }
                return ioErr;
            }

            if (!xhci_msc_recv_csw(g_xhci_base, g_xhci_dboff, g_xhci_rt_base)) {
                xhci_msc_request_sense_bot(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, lun);
                xhci_msc_log_sense("MSC");
                if (xhci_msc_retry_not_ready(false, lun)) {
                    continue;
                }
                return ioErr;
            }
        }

        remaining -= chunk;
        lba += chunk;
        buf += data_len;
    }

    return noErr;
}

static OSErr xhci_msc_write_blocks_internal(uint8_t lun, uint64_t start_block,
                                            uint32_t block_count, const void *buffer) {
    if (!xhci_msc_available() || !buffer) {
        return paramErr;
    }
    if (!xhci_msc_ensure_lun_info(lun)) {
        return ioErr;
    }
    uint32_t blk_size = g_msc_lun_block_size[lun];
    uint64_t blk_count = g_msc_lun_block_count[lun];
    if (blk_size == 0 || blk_count == 0) {
        return ioErr;
    }
    if (start_block >= blk_count) {
        return paramErr;
    }
    if (block_count == 0) {
        return noErr;
    }
    if (start_block + block_count > blk_count) {
        return paramErr;
    }
    if (g_msc.readonly) {
        return wPrErr;
    }
    if (blk_size > sizeof(g_msc_data_buf)) {
        return paramErr;
    }

    const uint8_t *buf = (const uint8_t *)buffer;
    uint32_t max_blocks = (uint32_t)(sizeof(g_msc_data_buf) / blk_size);
    if (max_blocks == 0) {
        return paramErr;
    }

    uint64_t lba = start_block;
    uint32_t remaining = block_count;

    bool tried_fallback = false;
    while (remaining > 0) {
        uint32_t chunk = remaining;
        if (chunk > max_blocks) {
            chunk = max_blocks;
        }
        if (chunk > 0xFFFFu) {
            chunk = 0xFFFFu;
        }
        if (lba > 0xFFFFFFFFu) {
            return paramErr;
        }

        uint8_t cb[16];
        memset(cb, 0, sizeof(cb));
        cb[0] = 0x2A; /* WRITE(10) */
        cb[2] = (uint8_t)((lba >> 24) & 0xFF);
        cb[3] = (uint8_t)((lba >> 16) & 0xFF);
        cb[4] = (uint8_t)((lba >> 8) & 0xFF);
        cb[5] = (uint8_t)(lba & 0xFF);
        cb[7] = (uint8_t)((chunk >> 8) & 0xFF);
        cb[8] = (uint8_t)(chunk & 0xFF);

        uint32_t data_len = chunk * blk_size;
        if (g_msc.uasp) {
            if (!xhci_uasp_exec_scsi(lun, cb, 10, buf, data_len, false)) {
                xhci_msc_request_sense_uasp(lun);
                xhci_msc_log_sense("UASP");
                if (xhci_msc_retry_not_ready(true, lun)) {
                    continue;
                }
                g_msc.uasp_failed = true;
                if (!tried_fallback && xhci_msc_try_fallback_bot()) {
                    tried_fallback = true;
                    continue;
                }
                return ioErr;
            }
        } else {
            if (!xhci_msc_send_cbw(g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                   lun, cb, 10, data_len, false)) {
                return ioErr;
            }
            if (!xhci_msc_bulk_transfer(g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                        false, buf, data_len)) {
                xhci_msc_request_sense_bot(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, lun);
                xhci_msc_log_sense("MSC");
                if (xhci_msc_retry_not_ready(false, lun)) {
                    continue;
                }
                return ioErr;
            }
            if (!xhci_msc_recv_csw(g_xhci_base, g_xhci_dboff, g_xhci_rt_base)) {
                xhci_msc_request_sense_bot(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, lun);
                xhci_msc_log_sense("MSC");
                if (xhci_msc_retry_not_ready(false, lun)) {
                    continue;
                }
                return ioErr;
            }
        }

        remaining -= chunk;
        lba += chunk;
        buf += data_len;
    }

    return noErr;
}

OSErr xhci_msc_read_blocks(uint64_t start_block, uint32_t block_count, void *buffer) {
    return xhci_msc_read_blocks_internal(0, start_block, block_count, buffer);
}

OSErr xhci_msc_write_blocks(uint64_t start_block, uint32_t block_count, const void *buffer) {
    return xhci_msc_write_blocks_internal(0, start_block, block_count, buffer);
}

uint8_t xhci_msc_get_lun_count(void) {
    if (!xhci_msc_available()) {
        return 0;
    }
    if (g_msc.max_lun == 0) {
        return 1;
    }
    if (g_msc.max_lun > MAX_MSC_LUNS) {
        return MAX_MSC_LUNS;
    }
    return g_msc.max_lun;
}

OSErr xhci_msc_read_blocks_lun(uint8_t lun, uint64_t start_block, uint32_t block_count, void *buffer) {
    return xhci_msc_read_blocks_internal(lun, start_block, block_count, buffer);
}

OSErr xhci_msc_write_blocks_lun(uint8_t lun, uint64_t start_block, uint32_t block_count, const void *buffer) {
    return xhci_msc_write_blocks_internal(lun, start_block, block_count, buffer);
}

static UInt16 xhci_hid_modifiers(uint8_t mods) {
    UInt16 out = 0;
    if (mods & 0x01) out |= controlKey;
    if (mods & 0x02) out |= shiftKey;
    if (mods & 0x04) out |= optionKey;
    if (mods & 0x08) out |= cmdKey;
    if (mods & 0x10) out |= controlKey;
    if (mods & 0x20) out |= shiftKey;
    if (mods & 0x40) out |= optionKey;
    if (mods & 0x80) out |= cmdKey;
    return out;
}

static UInt16 xhci_hid_to_mac_scan(uint8_t hid) {
    switch (hid) {
        case 0x04: return 0x00; /* A */
        case 0x05: return 0x0B; /* B */
        case 0x06: return 0x08; /* C */
        case 0x07: return 0x02; /* D */
        case 0x08: return 0x0E; /* E */
        case 0x09: return 0x03; /* F */
        case 0x0A: return 0x05; /* G */
        case 0x0B: return 0x04; /* H */
        case 0x0C: return 0x22; /* I */
        case 0x0D: return 0x26; /* J */
        case 0x0E: return 0x28; /* K */
        case 0x0F: return 0x25; /* L */
        case 0x10: return 0x2E; /* M */
        case 0x11: return 0x2D; /* N */
        case 0x12: return 0x1F; /* O */
        case 0x13: return 0x23; /* P */
        case 0x14: return 0x0C; /* Q */
        case 0x15: return 0x0F; /* R */
        case 0x16: return 0x01; /* S */
        case 0x17: return 0x11; /* T */
        case 0x18: return 0x20; /* U */
        case 0x19: return 0x09; /* V */
        case 0x1A: return 0x0D; /* W */
        case 0x1B: return 0x07; /* X */
        case 0x1C: return 0x10; /* Y */
        case 0x1D: return 0x06; /* Z */
        case 0x1E: return 0x12; /* 1 */
        case 0x1F: return 0x13; /* 2 */
        case 0x20: return 0x14; /* 3 */
        case 0x21: return 0x15; /* 4 */
        case 0x22: return 0x17; /* 5 */
        case 0x23: return 0x16; /* 6 */
        case 0x24: return 0x1A; /* 7 */
        case 0x25: return 0x1C; /* 8 */
        case 0x26: return 0x19; /* 9 */
        case 0x27: return 0x1D; /* 0 */
        case 0x28: return kScanReturn;
        case 0x2A: return kScanDelete;
        case 0x2B: return kScanTab;
        case 0x2C: return kScanSpace;
        case 0x29: return kScanEscape;
        default: return 0xFFFF;
    }
}

static bool xhci_hid_has_key(const uint8_t *report, uint8_t key) {
    for (int i = 2; i < 8; i++) {
        if (report[i] == key) {
            return true;
        }
    }
    return false;
}

static void xhci_hid_handle_modifiers(uint8_t current, uint8_t previous) {
    struct {
        uint8_t mask;
        UInt16 scancode;
    } mods[] = {
        {0x01, kScanControl}, {0x02, kScanShift}, {0x04, kScanOption}, {0x08, kScanCommand},
        {0x10, kScanRightControl}, {0x20, kScanRightShift}, {0x40, kScanRightOption}, {0x80, kScanCommand}
    };

    UInt16 modifiers = xhci_hid_modifiers(current);
    UInt32 ts = TickCount();

    for (unsigned i = 0; i < sizeof(mods)/sizeof(mods[0]); i++) {
        bool now = (current & mods[i].mask) != 0;
        bool prev = (previous & mods[i].mask) != 0;
        if (now != prev) {
            ProcessRawKeyboardEvent(mods[i].scancode, now, modifiers, ts);
        }
    }
}

static void xhci_handle_hid_keyboard(xhci_hid_dev_t *dev, const uint8_t *report) {
    uint8_t prev_mods = dev->last_report[0];
    uint8_t cur_mods = report[0];
    xhci_hid_handle_modifiers(cur_mods, prev_mods);

    UInt16 modifiers = xhci_hid_modifiers(cur_mods);
    UInt32 ts = TickCount();

    for (int i = 2; i < 8; i++) {
        uint8_t key = dev->last_report[i];
        if (key != 0 && !xhci_hid_has_key(report, key)) {
            UInt16 sc = xhci_hid_to_mac_scan(key);
            if (sc != 0xFFFF) {
                ProcessRawKeyboardEvent(sc, false, modifiers, ts);
            }
        }
    }

    for (int i = 2; i < 8; i++) {
        uint8_t key = report[i];
        if (key != 0 && !xhci_hid_has_key(dev->last_report, key)) {
            UInt16 sc = xhci_hid_to_mac_scan(key);
            if (sc != 0xFFFF) {
                ProcessRawKeyboardEvent(sc, true, modifiers, ts);
            }
        }
    }

    memcpy(dev->last_report, report, sizeof(dev->last_report));
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
            g_xhci_cap_len = cap_len;
            g_xhci_ports = ports;

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
                g_xhci_enum_port = (uint8_t)(p + 1);
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

            serial_puts("[XHCI] Phase-1 init complete (HID keyboard/mouse enabled)\n");
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
    xhci_hid_submit(g_xhci_base, g_xhci_dboff, &g_hid_kbd);
    xhci_hid_submit(g_xhci_base, g_xhci_dboff, &g_hid_mouse);
    xhci_hid_poll_events(g_xhci_rt_base);
    xhci_poll_hotplug();
}
