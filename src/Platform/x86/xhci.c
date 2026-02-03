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

extern uint32_t fb_width;
extern uint32_t fb_height;

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
#define XHCI_PORTSC_PP    (1u << 9)
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

#define MAX_XHCI_PORTS 32
#define MAX_XHCI_SLOTS 32

static xhci_trb_t __attribute__((aligned(64))) g_cmd_ring[32];
static xhci_trb_t __attribute__((aligned(64))) g_evt_ring[32];
static xhci_erst_entry_t __attribute__((aligned(64))) g_erst[1];
static uint64_t __attribute__((aligned(64))) g_dcbaa[256];
static uint32_t __attribute__((aligned(64))) g_input_ctx[48];
static uint32_t __attribute__((aligned(64))) g_dev_ctx[MAX_XHCI_SLOTS][32];
static xhci_trb_t __attribute__((aligned(64))) g_ep0_ring[MAX_XHCI_SLOTS][32];
static uint32_t __attribute__((aligned(64))) g_ep0_buf[MAX_XHCI_SLOTS][64];

typedef struct {
    xhci_trb_t ring[32] __attribute__((aligned(64)));
    uint32_t buf[64] __attribute__((aligned(64)));
} xhci_hid_io_t;

typedef struct {
    xhci_trb_t in_ring[32] __attribute__((aligned(64)));
    xhci_trb_t out_ring[32] __attribute__((aligned(64)));
    uint32_t data_buf[512] __attribute__((aligned(64)));
    uint8_t cbw[32] __attribute__((aligned(64)));
    uint8_t csw[16] __attribute__((aligned(64)));
    xhci_trb_t uasp_cmd_ring[32] __attribute__((aligned(64)));
    xhci_trb_t uasp_data_in_ring[32] __attribute__((aligned(64)));
    xhci_trb_t uasp_data_out_ring[32] __attribute__((aligned(64)));
    xhci_trb_t uasp_status_ring[32] __attribute__((aligned(64)));
    uint8_t uasp_cmd_iu[32] __attribute__((aligned(64)));
    uint8_t uasp_status_iu[32] __attribute__((aligned(64)));
} xhci_msc_io_t;

static xhci_hid_io_t g_hid_kbd_io[MAX_XHCI_PORTS];
static xhci_hid_io_t g_hid_mouse_io[MAX_XHCI_PORTS];
static xhci_msc_io_t g_msc_io[MAX_XHCI_PORTS];
#define MAX_MSC_LUNS 8
static bool g_port_connected[MAX_XHCI_PORTS];
static uint32_t g_ctx_size = 32;

typedef struct {
    uint8_t slot;
    uint8_t ep_addr;
    uint16_t mps;
    uint8_t interval;
    uint8_t ep_id;
    uint8_t interface_num;
    uint8_t alt_setting;
    uint32_t ring_index;
    uint32_t cycle;
    bool pending;
    uint32_t pending_tick;
    uint32_t last_submit_tick;
    uint8_t last_report[8];
    xhci_trb_t *ring;
    uint32_t *buf;
    bool configured;
    uint8_t port;
    xhci_hid_io_t *io;
    bool present;
    bool absolute_pointer;
} xhci_hid_dev_t;

static xhci_hid_dev_t g_hid_kbd[MAX_XHCI_PORTS];
static xhci_hid_dev_t g_hid_mouse[MAX_XHCI_PORTS];

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
    uint8_t port;
    uint32_t lun_block_size[MAX_MSC_LUNS];
    uint64_t lun_block_count[MAX_MSC_LUNS];
    bool lun_valid[MAX_MSC_LUNS];
    xhci_msc_io_t *io;
    bool present;
} xhci_msc_dev_t;

static xhci_msc_dev_t g_msc[MAX_XHCI_PORTS];

#define MSC_IO(dev) ((dev)->io)
#define MSC_DATA(dev) ((dev)->io->data_buf)
#define MSC_CBW(dev) ((dev)->io->cbw)
#define MSC_CSW(dev) ((dev)->io->csw)
#define MSC_UASP_CMD_IU(dev) ((dev)->io->uasp_cmd_iu)
#define MSC_UASP_STATUS_IU(dev) ((dev)->io->uasp_status_iu)
#define MSC_IN_RING(dev) ((dev)->io->in_ring)
#define MSC_OUT_RING(dev) ((dev)->io->out_ring)
#define MSC_UASP_CMD_RING(dev) ((dev)->io->uasp_cmd_ring)
#define MSC_UASP_DATA_IN_RING(dev) ((dev)->io->uasp_data_in_ring)
#define MSC_UASP_DATA_OUT_RING(dev) ((dev)->io->uasp_data_out_ring)
#define MSC_UASP_STATUS_RING(dev) ((dev)->io->uasp_status_ring)

static uint32_t g_cmd_ring_index = 0;
static uint32_t g_cmd_cycle = 1;
static uint32_t g_evt_cycle = 1;
static uint32_t g_evt_ring_index = 0;
static uint32_t g_ep0_ring_index[MAX_XHCI_SLOTS];
static uint32_t g_ep0_cycle[MAX_XHCI_SLOTS];
static uintptr_t g_xhci_base = 0;
static uint32_t g_xhci_dboff = 0;
static uintptr_t g_xhci_rt_base = 0;
static uint8_t g_xhci_cap_len = 0;
static uint8_t g_xhci_ports = 0;
static uint8_t g_xhci_max_slots = 0;
static uint8_t g_xhci_enum_port = 0;
static uint8_t g_msc_last_luns[MAX_XHCI_PORTS];
static uint8_t g_slot_num_configs[MAX_XHCI_SLOTS];
static uint32_t g_ep0_tmp_buf[(1024 + 3) / 4] __attribute__((aligned(64)));

static inline uint32_t mmio_read32(uintptr_t base, uint32_t off);
static inline void mmio_write32(uintptr_t base, uint32_t off, uint32_t value);
static xhci_hid_dev_t *xhci_hid_find_by_slot(xhci_hid_dev_t *devs, uint8_t slot_id);
static xhci_msc_dev_t *xhci_msc_get_present_by_index(uint8_t index, uint8_t *out_port_index);
static void xhci_msc_smoke_test(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff, uintptr_t rt_base);
static bool xhci_uasp_exec_scsi(xhci_msc_dev_t *dev, uint8_t lun, uint8_t *cdb, uint8_t cdb_len,
                                const void *data, uint32_t data_len, bool data_in);
static void xhci_msc_delay_ticks(uint32_t ticks);
static bool xhci_msc_test_unit_ready(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                     uintptr_t rt_base, uint8_t lun);
static bool xhci_msc_configure_bot(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                   uintptr_t rt_base);
static bool xhci_msc_get_max_lun(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                 uintptr_t rt_base, uint8_t slot_id);
static bool xhci_msc_ensure_lun_info(xhci_msc_dev_t *dev, uint8_t lun);
static void xhci_msc_fire_events(uint8_t port_index, bool inserted);
static uint8_t xhci_msc_get_lun_count_dev(xhci_msc_dev_t *dev);
static bool xhci_port_reset(uintptr_t op_base, uint8_t port);
static void xhci_poll_ports_hotplug(void);
static void xhci_enumerate_port(uint8_t port_index);
static const char *xhci_speed_name(uint32_t portsc);

static void xhci_ring_doorbell(uintptr_t base, uint32_t db_off, uint32_t target) {
    mmio_write32(base + db_off, XHCI_DB0 + target, 0);
}

static void xhci_ring_init(void) {
    memset(g_cmd_ring, 0, sizeof(g_cmd_ring));
    memset(g_evt_ring, 0, sizeof(g_evt_ring));
    memset(g_erst, 0, sizeof(g_erst));
    memset(g_dcbaa, 0, sizeof(g_dcbaa));
    memset(g_dev_ctx, 0, sizeof(g_dev_ctx));
    memset(g_ep0_ring, 0, sizeof(g_ep0_ring));
    memset(g_ep0_buf, 0, sizeof(g_ep0_buf));
    memset(g_port_connected, 0, sizeof(g_port_connected));
    memset(g_hid_kbd_io, 0, sizeof(g_hid_kbd_io));
    memset(g_hid_mouse_io, 0, sizeof(g_hid_mouse_io));
    memset(g_msc_io, 0, sizeof(g_msc_io));
    memset(g_hid_kbd, 0, sizeof(g_hid_kbd));
    memset(g_hid_mouse, 0, sizeof(g_hid_mouse));
    memset(g_msc, 0, sizeof(g_msc));
    memset(g_msc_last_luns, 0, sizeof(g_msc_last_luns));
    g_cmd_ring_index = 0;
    g_cmd_cycle = 1;
    g_evt_cycle = 1;
    g_evt_ring_index = 0;
    for (int i = 0; i < MAX_XHCI_SLOTS; i++) {
        g_ep0_ring_index[i] = 0;
        g_ep0_cycle[i] = 1;
    }

    for (int i = 0; i < MAX_XHCI_PORTS; i++) {
        g_hid_kbd[i].io = &g_hid_kbd_io[i];
        g_hid_kbd[i].ring = g_hid_kbd_io[i].ring;
        g_hid_kbd[i].buf = g_hid_kbd_io[i].buf;
        g_hid_kbd[i].ring_index = 0;
        g_hid_kbd[i].cycle = 1;
        g_hid_kbd[i].port = (uint8_t)(i + 1);
        g_hid_kbd[i].interface_num = 0xFF;
        g_hid_kbd[i].alt_setting = 0xFF;

        g_hid_mouse[i].io = &g_hid_mouse_io[i];
        g_hid_mouse[i].ring = g_hid_mouse_io[i].ring;
        g_hid_mouse[i].buf = g_hid_mouse_io[i].buf;
        g_hid_mouse[i].ring_index = 0;
        g_hid_mouse[i].cycle = 1;
        g_hid_mouse[i].port = (uint8_t)(i + 1);
        g_hid_mouse[i].interface_num = 0xFF;
        g_hid_mouse[i].alt_setting = 0xFF;

        g_msc[i].io = &g_msc_io[i];
        g_msc[i].in_ring_index = 0;
        g_msc[i].out_ring_index = 0;
        g_msc[i].in_cycle = 1;
        g_msc[i].out_cycle = 1;
        g_msc[i].tag = 1;
        g_msc[i].bot_present = false;
        g_msc[i].bot_configured = false;
        g_msc[i].uasp_cmd_ring_index = 0;
        g_msc[i].uasp_data_in_ring_index = 0;
        g_msc[i].uasp_data_out_ring_index = 0;
        g_msc[i].uasp_status_ring_index = 0;
        g_msc[i].uasp_cmd_cycle = 1;
        g_msc[i].uasp_data_in_cycle = 1;
        g_msc[i].uasp_data_out_cycle = 1;
        g_msc[i].uasp_status_cycle = 1;
        g_msc[i].max_lun = 1;
        g_msc[i].interface_num = 0;
        g_msc[i].port = (uint8_t)(i + 1);
    }
}

static xhci_hid_dev_t *xhci_hid_find_by_slot(xhci_hid_dev_t *devs, uint8_t slot_id) {
    if (!devs || slot_id == 0) {
        return NULL;
    }
    uint8_t ports = g_xhci_ports ? g_xhci_ports : MAX_XHCI_PORTS;
    if (ports > MAX_XHCI_PORTS) {
        ports = MAX_XHCI_PORTS;
    }
    for (uint8_t i = 0; i < ports; i++) {
        if (devs[i].slot == slot_id) {
            return &devs[i];
        }
    }
    return NULL;
}

static xhci_msc_dev_t *xhci_msc_get_present_by_index(uint8_t index, uint8_t *out_port_index) {
    uint8_t count = 0;
    uint8_t ports = g_xhci_ports ? g_xhci_ports : MAX_XHCI_PORTS;
    if (ports > MAX_XHCI_PORTS) {
        ports = MAX_XHCI_PORTS;
    }
    for (uint8_t i = 0; i < ports; i++) {
        if (g_msc[i].present) {
            if (count == index) {
                if (out_port_index) {
                    *out_port_index = i;
                }
                return &g_msc[i];
            }
            count++;
        }
    }
    return NULL;
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
    for (uint32_t i = 0; i < 2000000; i++) {
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
            } else {
                /* Advance past unrelated events (e.g., port status change) */
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

static bool xhci_poll_transfer_complete(uintptr_t rt_base, uint8_t slot_id) {
    for (uint32_t i = 0; i < 2000000; i++) {
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
    if (slot_id == 0 || slot_id > MAX_XHCI_SLOTS) {
        return;
    }
    uint8_t slot_index = (uint8_t)(slot_id - 1);
    memset(g_dev_ctx[slot_index], 0, sizeof(g_dev_ctx[slot_index]));

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
    ep0[2] = (uint32_t)((uintptr_t)&g_ep0_ring[slot_index][0] & 0xFFFFFFFFu);
    ep0[3] = 0;

    /* Device context placeholder */
    g_dcbaa[slot_id] = (uint64_t)(uintptr_t)&g_dev_ctx[slot_index][0];
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

static void xhci_ep0_enqueue_setup(uint8_t slot_id, const usb_setup_packet_t *setup) {
    if (slot_id == 0 || slot_id > MAX_XHCI_SLOTS) {
        return;
    }
    uint8_t slot_index = (uint8_t)(slot_id - 1);
    xhci_trb_t *trb = &g_ep0_ring[slot_index][g_ep0_ring_index[slot_index]++];
    trb->dword0 = *(const uint32_t *)setup;
    trb->dword1 = *((const uint32_t *)setup + 1);
    trb->dword2 = 8;
    trb->dword3 = (XHCI_TRB_TYPE_SETUP_STAGE << XHCI_TRB_TYPE_SHIFT) |
                  (2u << 16) | /* transfer type: IN data stage */
                  XHCI_TRB_IDT |
                  (g_ep0_cycle[slot_index] ? XHCI_TRB_CYCLE : 0);
}

static void xhci_ep0_enqueue_data_in(uint8_t slot_id, uintptr_t buf, uint32_t len) {
    if (slot_id == 0 || slot_id > MAX_XHCI_SLOTS) {
        return;
    }
    uint8_t slot_index = (uint8_t)(slot_id - 1);
    xhci_trb_t *trb = &g_ep0_ring[slot_index][g_ep0_ring_index[slot_index]++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_DATA_STAGE << XHCI_TRB_TYPE_SHIFT) |
                  (1u << 16) | /* IN */
                  XHCI_TRB_IOC |
                  (g_ep0_cycle[slot_index] ? XHCI_TRB_CYCLE : 0);
}

static void xhci_ep0_enqueue_status(uint8_t slot_id) {
    if (slot_id == 0 || slot_id > MAX_XHCI_SLOTS) {
        return;
    }
    uint8_t slot_index = (uint8_t)(slot_id - 1);
    xhci_trb_t *trb = &g_ep0_ring[slot_index][g_ep0_ring_index[slot_index]++];
    trb->dword0 = 0;
    trb->dword1 = 0;
    trb->dword2 = 0;
    trb->dword3 = (XHCI_TRB_TYPE_STATUS_STAGE << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (g_ep0_cycle[slot_index] ? XHCI_TRB_CYCLE : 0);
}

static void xhci_ep0_ring_doorbell(uintptr_t base, uint32_t dboff, uint8_t slot_id) {
    mmio_write32(base + dboff, XHCI_DB0 + slot_id, 1);
}

static void xhci_ep0_reset_ring(uint8_t slot_id) {
    if (slot_id == 0 || slot_id > MAX_XHCI_SLOTS) {
        return;
    }
    uint8_t slot_index = (uint8_t)(slot_id - 1);
    g_ep0_ring_index[slot_index] = 0;
    g_ep0_cycle[slot_index] = 1;
}

static bool xhci_ep0_control_no_data(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                     uint8_t slot_id, const usb_setup_packet_t *setup) {
    if (!setup) {
        return false;
    }
    xhci_ep0_reset_ring(slot_id);
    xhci_ep0_enqueue_setup(slot_id, setup);
    xhci_ep0_enqueue_status(slot_id);
    xhci_ep0_ring_doorbell(base, dboff, slot_id);
    return xhci_poll_transfer_complete(rt_base, slot_id);
}

static bool xhci_ep0_clear_halt(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                uint8_t slot_id, uint8_t ep_addr) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x02, /* Host->Device | Standard | Endpoint */
        .bRequest = 1,         /* CLEAR_FEATURE */
        .wValue = 0,
        .wIndex = ep_addr,
        .wLength = 0
    };
    return xhci_ep0_control_no_data(base, dboff, rt_base, slot_id, &setup);
}

static bool xhci_ep0_mass_storage_reset(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                        uintptr_t rt_base) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x21, /* Host->Device | Class | Interface */
        .bRequest = 0xFF,      /* Mass Storage Reset */
        .wValue = 0,
        .wIndex = dev ? dev->interface_num : 0,
        .wLength = 0
    };
    if (!dev || dev->slot == 0) {
        return false;
    }
    return xhci_ep0_control_no_data(base, dboff, rt_base, dev->slot, &setup);
}

static bool xhci_uasp_recover(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                              uintptr_t rt_base) {
    if (!dev || dev->slot == 0) {
        return false;
    }
    if (!xhci_ep0_mass_storage_reset(dev, base, dboff, rt_base)) {
        return false;
    }
    if (dev->uasp_cmd_out_ep) {
        xhci_ep0_clear_halt(base, dboff, rt_base, dev->slot, dev->uasp_cmd_out_ep);
    }
    if (dev->uasp_data_out_ep) {
        xhci_ep0_clear_halt(base, dboff, rt_base, dev->slot, dev->uasp_data_out_ep);
    }
    if (dev->uasp_status_in_ep) {
        xhci_ep0_clear_halt(base, dboff, rt_base, dev->slot, dev->uasp_status_in_ep);
    }
    if (dev->uasp_data_in_ep) {
        xhci_ep0_clear_halt(base, dboff, rt_base, dev->slot, dev->uasp_data_in_ep);
    }
    dev->uasp_cmd_ring_index = 0;
    dev->uasp_data_in_ring_index = 0;
    dev->uasp_data_out_ring_index = 0;
    dev->uasp_status_ring_index = 0;
    dev->uasp_cmd_cycle = 1;
    dev->uasp_data_in_cycle = 1;
    dev->uasp_data_out_cycle = 1;
    dev->uasp_status_cycle = 1;
    return true;
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

    xhci_ep0_reset_ring(slot_id);
    xhci_ep0_enqueue_setup(slot_id, &setup);
    xhci_ep0_enqueue_status(slot_id);
    xhci_ep0_ring_doorbell(base, dboff, slot_id);

    serial_printf("[XHCI] SET_CONFIGURATION %u for slot %u\n", config_value, slot_id);
    return xhci_poll_transfer_complete(rt_base, slot_id);
}

static bool xhci_ep0_set_interface(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                   uint8_t slot_id, uint8_t interface_num, uint8_t alt_setting) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x01, /* Host->Device | Standard | Interface */
        .bRequest = 11,        /* SET_INTERFACE */
        .wValue = alt_setting,
        .wIndex = interface_num,
        .wLength = 0
    };

    xhci_ep0_reset_ring(slot_id);
    xhci_ep0_enqueue_setup(slot_id, &setup);
    xhci_ep0_enqueue_status(slot_id);
    xhci_ep0_ring_doorbell(base, dboff, slot_id);
    return xhci_poll_transfer_complete(rt_base, slot_id);
}

static bool xhci_ep0_set_protocol(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                  uint8_t slot_id, uint8_t interface_num, uint8_t protocol) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0x21, /* Host->Device | Class | Interface */
        .bRequest = 0x0B,      /* SET_PROTOCOL */
        .wValue = protocol,
        .wIndex = interface_num,
        .wLength = 0
    };

    xhci_ep0_reset_ring(slot_id);
    xhci_ep0_enqueue_setup(slot_id, &setup);
    xhci_ep0_enqueue_status(slot_id);
    xhci_ep0_ring_doorbell(base, dboff, slot_id);
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

    xhci_ep0_reset_ring(slot_id);
    xhci_ep0_enqueue_setup(slot_id, &setup);
    xhci_ep0_enqueue_data_in(slot_id, (uintptr_t)&g_ep0_buf[slot_id - 1][0], 18);
    xhci_ep0_enqueue_status(slot_id);
    xhci_ep0_ring_doorbell(base, dboff, slot_id);

    serial_printf("[XHCI] GET_DESCRIPTOR issued for slot %u\n", slot_id);
    if (!xhci_poll_transfer_complete(rt_base, slot_id)) {
        return false;
    }

    uint8_t *desc = (uint8_t *)&g_ep0_buf[slot_id - 1][0];
    uint16_t vid = (uint16_t)(desc[8] | (desc[9] << 8));
    uint16_t pid = (uint16_t)(desc[10] | (desc[11] << 8));
    g_slot_num_configs[slot_id - 1] = desc[17];
    serial_printf("[XHCI] Device descriptor: VID=0x%04x PID=0x%04x class=0x%02x\n",
                  vid, pid, desc[4]);
    return true;
}

static bool xhci_ep0_get_config_descriptor(uintptr_t base, uint32_t dboff, uintptr_t rt_base,
                                           uint8_t slot_id, uint8_t config_index) {
    const uint16_t max_len = (uint16_t)sizeof(g_ep0_tmp_buf);
    usb_setup_packet_t setup = {
        .bmRequestType = 0x80,
        .bRequest = 6,
        .wValue = (uint16_t)(0x0200 | config_index),
        .wIndex = 0,
        .wLength = 9
    };

    memset(g_ep0_tmp_buf, 0, sizeof(g_ep0_tmp_buf));
    xhci_ep0_reset_ring(slot_id);
    xhci_ep0_enqueue_setup(slot_id, &setup);
    xhci_ep0_enqueue_data_in(slot_id, (uintptr_t)&g_ep0_tmp_buf[0], 9);
    xhci_ep0_enqueue_status(slot_id);
    xhci_ep0_ring_doorbell(base, dboff, slot_id);

    serial_printf("[XHCI] GET_CONFIG header for slot %u\n", slot_id);
    if (!xhci_poll_transfer_complete(rt_base, slot_id)) {
        return false;
    }

    uint8_t *buf = (uint8_t *)&g_ep0_tmp_buf[0];
    uint16_t total = (uint16_t)(buf[2] | (buf[3] << 8));
    uint8_t config_value = buf[5];
    if (total > max_len) {
        total = max_len;
    }

    if (total > 9) {
        setup.wLength = total;
        memset(g_ep0_tmp_buf, 0, sizeof(g_ep0_tmp_buf));
        xhci_ep0_reset_ring(slot_id);
        xhci_ep0_enqueue_setup(slot_id, &setup);
        xhci_ep0_enqueue_data_in(slot_id, (uintptr_t)&g_ep0_tmp_buf[0], total);
        xhci_ep0_enqueue_status(slot_id);
        xhci_ep0_ring_doorbell(base, dboff, slot_id);

        serial_printf("[XHCI] GET_CONFIG full len=%u for slot %u\n", total, slot_id);
        if (!xhci_poll_transfer_complete(rt_base, slot_id)) {
            return false;
        }
        buf = (uint8_t *)&g_ep0_tmp_buf[0];
    }

    serial_printf("[XHCI] Config total length=%u\n", total);

    if (g_xhci_enum_port == 0) {
        return false;
    }

    uint8_t port_index = (uint8_t)(g_xhci_enum_port - 1);
    xhci_hid_dev_t *kbd = &g_hid_kbd[port_index];
    xhci_hid_dev_t *mouse = &g_hid_mouse[port_index];
    xhci_msc_dev_t *msc = &g_msc[port_index];

    uint32_t off = 0;
    uint8_t current_proto = 0;
    uint8_t current_class = 0;
    uint8_t current_sub = 0;
    uint8_t current_if_num = 0;
    uint8_t current_alt = 0;
    uint8_t current_hid_role = 0; /* 1=keyboard, 2=pointer */
    uint8_t uasp_in_eps[2] = {0};
    uint8_t uasp_out_eps[2] = {0};
    uint16_t uasp_in_mps[2] = {0};
    uint16_t uasp_out_mps[2] = {0};
    uint8_t uasp_in_count = 0;
    uint8_t uasp_out_count = 0;
    kbd->slot = 0;
    kbd->ep_addr = 0;
    kbd->ep_id = 0;
    kbd->mps = 0;
    kbd->interval = 0;
    kbd->pending = false;
    kbd->configured = false;
    kbd->present = false;
    kbd->ring_index = 0;
    kbd->cycle = 1;
    kbd->interface_num = 0xFF;
    kbd->alt_setting = 0xFF;
    kbd->interface_num = 0xFF;
    kbd->absolute_pointer = false;
    memset(kbd->last_report, 0, sizeof(kbd->last_report));

    mouse->slot = 0;
    mouse->ep_addr = 0;
    mouse->ep_id = 0;
    mouse->mps = 0;
    mouse->interval = 0;
    mouse->pending = false;
    mouse->configured = false;
    mouse->present = false;
    mouse->ring_index = 0;
    mouse->cycle = 1;
    mouse->interface_num = 0xFF;
    mouse->alt_setting = 0xFF;
    mouse->interface_num = 0xFF;
    mouse->absolute_pointer = false;

    msc->slot = 0;
    msc->bulk_in_ep = 0;
    msc->bulk_out_ep = 0;
    msc->bulk_in_ep_id = 0;
    msc->bulk_out_ep_id = 0;
    msc->bulk_in_mps = 0;
    msc->bulk_out_mps = 0;
    msc->configured = false;
    msc->bot_present = false;
    msc->bot_configured = false;
    msc->bot_in_ep = 0;
    msc->bot_out_ep = 0;
    msc->bot_in_ep_id = 0;
    msc->bot_out_ep_id = 0;
    msc->bot_in_mps = 0;
    msc->bot_out_mps = 0;
    msc->uasp = false;
    msc->uasp_cmd_out_ep = 0;
    msc->uasp_data_out_ep = 0;
    msc->uasp_status_in_ep = 0;
    msc->uasp_data_in_ep = 0;
    msc->uasp_cmd_out_ep_id = 0;
    msc->uasp_data_out_ep_id = 0;
    msc->uasp_status_in_ep_id = 0;
    msc->uasp_data_in_ep_id = 0;
    msc->uasp_cmd_out_mps = 0;
    msc->uasp_data_out_mps = 0;
    msc->uasp_status_in_mps = 0;
    msc->uasp_data_in_mps = 0;
    msc->tag = 1;
    msc->in_ring_index = 0;
    msc->out_ring_index = 0;
    msc->in_cycle = 1;
    msc->out_cycle = 1;
    msc->uasp_cmd_ring_index = 0;
    msc->uasp_data_in_ring_index = 0;
    msc->uasp_data_out_ring_index = 0;
    msc->uasp_status_ring_index = 0;
    msc->uasp_cmd_cycle = 1;
    msc->uasp_data_in_cycle = 1;
    msc->uasp_data_out_cycle = 1;
    msc->uasp_status_cycle = 1;
    msc->max_lun = 1;
    msc->interface_num = 0;
    msc->block_size = 0;
    msc->block_count = 0;
    msc->readonly = false;
    msc->uasp_failed = false;
    msc->present = false;
    memset(msc->lun_valid, 0, sizeof(msc->lun_valid));

    bool found_supported = false;
    while (off + 1 < total) {
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
            uint8_t alt = buf[off + 3];
            serial_printf("[XHCI] IF cls=0x%02x sub=0x%02x proto=0x%02x\n", cls, sub, proto);
            current_class = cls;
            current_sub = sub;
            current_proto = proto;
            current_if_num = if_num;
            current_alt = alt;
            current_hid_role = 0;
            if (cls == 0x08 && (proto == 0x50 || proto == 0x62)) {
                msc->slot = slot_id;
                msc->uasp = (proto == 0x62);
                msc->interface_num = if_num;
                found_supported = true;
            }
            if (cls == 0x03 && ((sub == 0x01 && (proto == 0x01 || proto == 0x02)) || proto == 0x00)) {
                bool is_keyboard = (sub == 0x01 && proto == 0x01);
                xhci_hid_dev_t *dev = is_keyboard ? kbd : mouse;
                if (dev->ep_addr == 0 && dev->interface_num == 0xFF) {
                    dev->slot = slot_id;
                    dev->interface_num = if_num;
                    dev->alt_setting = alt;
                    dev->absolute_pointer = !is_keyboard && (proto == 0x00);
                    /* Boot protocol HID: force alt setting 0 and boot protocol when supported. */
                    xhci_ep0_set_interface(base, dboff, rt_base, slot_id, if_num, 0);
                    if (sub == 0x01) {
                        xhci_ep0_set_protocol(base, dboff, rt_base, slot_id, if_num, 0);
                    }
                    current_hid_role = is_keyboard ? 1 : 2;
                    found_supported = true;
                }
            }
        }
        if (type == 5 && len >= 7) {
            uint8_t ep_addr = buf[off + 2];
            uint8_t attrs = buf[off + 3];
            uint16_t mps = (uint16_t)(buf[off + 4] | (buf[off + 5] << 8));
            uint8_t interval = buf[off + 6];
            uint8_t ep_type = attrs & 0x3;
            if ((ep_addr & 0x80) && ep_type == 3 && current_hid_role != 0) {
                xhci_hid_dev_t *dev = (current_hid_role == 1) ? kbd : mouse;
                if (dev->ep_addr == 0 && dev->slot == slot_id &&
                    dev->interface_num == current_if_num &&
                    dev->alt_setting == current_alt) {
                    dev->slot = slot_id;
                    dev->ep_addr = ep_addr;
                    if (current_hid_role == 1 && mps > sizeof(dev->last_report)) {
                        mps = (uint16_t)sizeof(dev->last_report);
                    }
                    dev->mps = mps;
                    dev->interval = interval;
                    dev->ep_id = (uint8_t)(2 * (ep_addr & 0x0F) + 1);
                    dev->pending = false;
                    if (current_hid_role == 1) {
                        memset(dev->last_report, 0, sizeof(dev->last_report));
                    }
                    serial_printf("[XHCI] HID %s%s INT IN ep=0x%02x mps=%u interval=%u\n",
                                  (current_hid_role == 1) ? "kbd" : "mouse",
                                  (dev->absolute_pointer ? "(abs)" : ""),
                                  ep_addr, mps, interval);
                    found_supported = true;
                }
            }
            if (current_class == 0x08 && current_sub == 0x06 && ep_type == 2) {
                found_supported = true;
                if (ep_addr & 0x80) {
                    if (msc->uasp && uasp_in_count < 2) {
                        uasp_in_eps[uasp_in_count] = ep_addr;
                        uasp_in_mps[uasp_in_count] = mps;
                        uasp_in_count++;
                    } else if (!msc->uasp) {
                        msc->bulk_in_ep = ep_addr;
                        msc->bulk_in_mps = mps;
                        msc->bulk_in_ep_id = (uint8_t)(2 * (ep_addr & 0x0F) + 1);
                    }
                    if (current_proto == 0x50) {
                        msc->bot_present = true;
                        msc->bot_in_ep = ep_addr;
                        msc->bot_in_mps = mps;
                        msc->bot_in_ep_id = (uint8_t)(2 * (ep_addr & 0x0F) + 1);
                    }
                } else {
                    if (msc->uasp && uasp_out_count < 2) {
                        uasp_out_eps[uasp_out_count] = ep_addr;
                        uasp_out_mps[uasp_out_count] = mps;
                        uasp_out_count++;
                    } else if (!msc->uasp) {
                        msc->bulk_out_ep = ep_addr;
                        msc->bulk_out_mps = mps;
                        msc->bulk_out_ep_id = (uint8_t)(2 * (ep_addr & 0x0F) + 0);
                    }
                    if (current_proto == 0x50) {
                        msc->bot_present = true;
                        msc->bot_out_ep = ep_addr;
                        msc->bot_out_mps = mps;
                        msc->bot_out_ep_id = (uint8_t)(2 * (ep_addr & 0x0F) + 0);
                    }
                }
                serial_printf("[XHCI] MSC bulk %s ep=0x%02x mps=%u\n",
                              (ep_addr & 0x80) ? "IN" : "OUT", ep_addr, mps);
            }
        }
        off += len;
    }

    if (msc->uasp && uasp_in_count >= 2 && uasp_out_count >= 2) {
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

        msc->uasp_status_in_ep = in0;
        msc->uasp_status_in_mps = in0_mps;
        msc->uasp_data_in_ep = in1;
        msc->uasp_data_in_mps = in1_mps;
        msc->uasp_cmd_out_ep = out0;
        msc->uasp_cmd_out_mps = out0_mps;
        msc->uasp_data_out_ep = out1;
        msc->uasp_data_out_mps = out1_mps;

        msc->uasp_status_in_ep_id = (uint8_t)(2 * (msc->uasp_status_in_ep & 0x0F) + 1);
        msc->uasp_data_in_ep_id = (uint8_t)(2 * (msc->uasp_data_in_ep & 0x0F) + 1);
        msc->uasp_cmd_out_ep_id = (uint8_t)(2 * (msc->uasp_cmd_out_ep & 0x0F) + 0);
        msc->uasp_data_out_ep_id = (uint8_t)(2 * (msc->uasp_data_out_ep & 0x0F) + 0);

        serial_printf("[XHCI] UASP cmd-out=0x%02x data-out=0x%02x status-in=0x%02x data-in=0x%02x\n",
                      msc->uasp_cmd_out_ep, msc->uasp_data_out_ep,
                      msc->uasp_status_in_ep, msc->uasp_data_in_ep);
    }

    if (!found_supported) {
        return false;
    }

    if (config_value != 0) {
        xhci_ep0_set_configuration(base, dboff, rt_base, slot_id, config_value);
    }

    if (kbd->interface_num != 0xFF && kbd->alt_setting != 0xFF) {
        xhci_ep0_set_interface(base, dboff, rt_base, slot_id, kbd->interface_num, kbd->alt_setting);
    }
    if (mouse->interface_num != 0xFF && mouse->alt_setting != 0xFF) {
        xhci_ep0_set_interface(base, dboff, rt_base, slot_id, mouse->interface_num, mouse->alt_setting);
    }

    if (kbd->ep_id != 0 && kbd->slot == slot_id) {
        memset(g_input_ctx, 0, sizeof(g_input_ctx));
        xhci_build_hid_endpoint_context(kbd->ep_id, kbd->mps,
                                        kbd->interval, kbd->ring);
        kbd->configured = xhci_configure_endpoint(base, dboff, rt_base, slot_id);
        if (kbd->configured) {
            kbd->present = true;
            serial_printf("[XHCI] HID keyboard connected (port %u)\n", kbd->port);
        }
    }

    if (mouse->ep_id != 0 && mouse->slot == slot_id) {
        memset(g_input_ctx, 0, sizeof(g_input_ctx));
        xhci_build_hid_endpoint_context(mouse->ep_id, mouse->mps,
                                        mouse->interval, mouse->ring);
        mouse->configured = xhci_configure_endpoint(base, dboff, rt_base, slot_id);
        if (mouse->configured) {
            mouse->present = true;
            serial_printf("[XHCI] HID mouse connected (port %u)\n", mouse->port);
        }
    }

    if (msc->uasp && msc->uasp_cmd_out_ep_id && msc->uasp_data_out_ep_id &&
        msc->uasp_status_in_ep_id && msc->uasp_data_in_ep_id && msc->slot == slot_id) {
        memset(g_input_ctx, 0, sizeof(g_input_ctx));
        xhci_build_bulk_endpoint_context(msc->uasp_cmd_out_ep_id, msc->uasp_cmd_out_mps, 2,
                                         MSC_UASP_CMD_RING(msc));
        xhci_build_bulk_endpoint_context(msc->uasp_data_out_ep_id, msc->uasp_data_out_mps, 2,
                                         MSC_UASP_DATA_OUT_RING(msc));
        xhci_build_bulk_endpoint_context(msc->uasp_status_in_ep_id, msc->uasp_status_in_mps, 6,
                                         MSC_UASP_STATUS_RING(msc));
        xhci_build_bulk_endpoint_context(msc->uasp_data_in_ep_id, msc->uasp_data_in_mps, 6,
                                         MSC_UASP_DATA_IN_RING(msc));
        msc->configured = xhci_configure_endpoint(base, dboff, rt_base, slot_id);
        if (msc->configured) {
            serial_puts("[XHCI] UASP endpoints configured\n");
            msc->max_lun = 1;
            msc->present = true;
            xhci_msc_smoke_test(msc, base, dboff, rt_base);
            g_msc_last_luns[port_index] = xhci_msc_get_lun_count_dev(msc);
            xhci_msc_fire_events(port_index, true);
        }
    } else if (!msc->uasp && msc->bulk_in_ep_id != 0 && msc->bulk_out_ep_id != 0 &&
               msc->slot == slot_id) {
        memset(g_input_ctx, 0, sizeof(g_input_ctx));
        xhci_build_bulk_endpoint_context(msc->bulk_out_ep_id, msc->bulk_out_mps, 2,
                                         MSC_OUT_RING(msc));
        xhci_build_bulk_endpoint_context(msc->bulk_in_ep_id, msc->bulk_in_mps, 6,
                                         MSC_IN_RING(msc));
        msc->configured = xhci_configure_endpoint(base, dboff, rt_base, slot_id);
        if (msc->configured) {
            serial_puts("[XHCI] MSC endpoints configured\n");
            msc->max_lun = 1;
            msc->present = true;
            xhci_msc_get_max_lun(msc, base, dboff, rt_base, slot_id);
            xhci_msc_smoke_test(msc, base, dboff, rt_base);
            g_msc_last_luns[port_index] = xhci_msc_get_lun_count_dev(msc);
            xhci_msc_fire_events(port_index, true);
        }
    } else if (msc->bot_present && msc->slot == slot_id) {
        if (xhci_msc_configure_bot(msc, base, dboff, rt_base)) {
            msc->max_lun = 1;
            msc->present = true;
            xhci_msc_get_max_lun(msc, base, dboff, rt_base, slot_id);
            xhci_msc_smoke_test(msc, base, dboff, rt_base);
            g_msc_last_luns[port_index] = xhci_msc_get_lun_count_dev(msc);
            xhci_msc_fire_events(port_index, true);
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

static void xhci_msc_enqueue_out(xhci_msc_dev_t *dev, uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &MSC_OUT_RING(dev)[dev->out_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (dev->out_cycle ? XHCI_TRB_CYCLE : 0);

    if (dev->out_ring_index >= 31) {
        xhci_trb_t *link = &MSC_OUT_RING(dev)[dev->out_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&MSC_OUT_RING(dev)[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        dev->out_ring_index = 0;
        dev->out_cycle ^= 1;
    }
}

static void xhci_msc_enqueue_in(xhci_msc_dev_t *dev, uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &MSC_IN_RING(dev)[dev->in_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (dev->in_cycle ? XHCI_TRB_CYCLE : 0);

    if (dev->in_ring_index >= 31) {
        xhci_trb_t *link = &MSC_IN_RING(dev)[dev->in_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&MSC_IN_RING(dev)[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        dev->in_ring_index = 0;
        dev->in_cycle ^= 1;
    }
}

static void xhci_uasp_enqueue_cmd(xhci_msc_dev_t *dev, uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &MSC_UASP_CMD_RING(dev)[dev->uasp_cmd_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (dev->uasp_cmd_cycle ? XHCI_TRB_CYCLE : 0);

    if (dev->uasp_cmd_ring_index >= 31) {
        xhci_trb_t *link = &MSC_UASP_CMD_RING(dev)[dev->uasp_cmd_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&MSC_UASP_CMD_RING(dev)[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        dev->uasp_cmd_ring_index = 0;
        dev->uasp_cmd_cycle ^= 1;
    }
}

static void xhci_uasp_enqueue_data_in(xhci_msc_dev_t *dev, uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &MSC_UASP_DATA_IN_RING(dev)[dev->uasp_data_in_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (dev->uasp_data_in_cycle ? XHCI_TRB_CYCLE : 0);

    if (dev->uasp_data_in_ring_index >= 31) {
        xhci_trb_t *link = &MSC_UASP_DATA_IN_RING(dev)[dev->uasp_data_in_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&MSC_UASP_DATA_IN_RING(dev)[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        dev->uasp_data_in_ring_index = 0;
        dev->uasp_data_in_cycle ^= 1;
    }
}

static void xhci_uasp_enqueue_data_out(xhci_msc_dev_t *dev, uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &MSC_UASP_DATA_OUT_RING(dev)[dev->uasp_data_out_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (dev->uasp_data_out_cycle ? XHCI_TRB_CYCLE : 0);

    if (dev->uasp_data_out_ring_index >= 31) {
        xhci_trb_t *link = &MSC_UASP_DATA_OUT_RING(dev)[dev->uasp_data_out_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&MSC_UASP_DATA_OUT_RING(dev)[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        dev->uasp_data_out_ring_index = 0;
        dev->uasp_data_out_cycle ^= 1;
    }
}

static void xhci_uasp_enqueue_status(xhci_msc_dev_t *dev, uintptr_t buf, uint32_t len) {
    xhci_trb_t *trb = &MSC_UASP_STATUS_RING(dev)[dev->uasp_status_ring_index++];
    trb->dword0 = (uint32_t)(buf & 0xFFFFFFFFu);
    trb->dword1 = 0;
    trb->dword2 = len;
    trb->dword3 = (XHCI_TRB_TYPE_NORMAL << XHCI_TRB_TYPE_SHIFT) |
                  XHCI_TRB_IOC |
                  (dev->uasp_status_cycle ? XHCI_TRB_CYCLE : 0);

    if (dev->uasp_status_ring_index >= 31) {
        xhci_trb_t *link = &MSC_UASP_STATUS_RING(dev)[dev->uasp_status_ring_index++];
        link->dword0 = (uint32_t)((uintptr_t)&MSC_UASP_STATUS_RING(dev)[0] & 0xFFFFFFFFu);
        link->dword1 = 0;
        link->dword2 = 0;
        link->dword3 = (6u << XHCI_TRB_TYPE_SHIFT) | XHCI_TRB_CYCLE | (1u << 1);
        dev->uasp_status_ring_index = 0;
        dev->uasp_status_cycle ^= 1;
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
static void xhci_handle_hid_mouse(xhci_hid_dev_t *dev, const uint8_t *report, uint32_t len) {
    if (len < 3) {
        return;
    }
    uint32_t offset = 0;
    if (len >= 4) {
        uint8_t rid = report[0];
        uint8_t btn1 = report[1] & 0x1F;
        bool looks_like_id = (rid != 0 && rid <= 8 &&
                              (report[1] & 0xE0) == 0 &&
                              (btn1 != 0 || report[2] != 0 || report[3] != 0));
        if (looks_like_id) {
            offset = 1;
        }
    }

    if (len < 3 + offset) {
        return;
    }

    uint8_t buttons = report[offset + 0] & 0x1F;
    if (dev && dev->absolute_pointer && len >= 5 + offset && fb_width > 0 && fb_height > 0) {
        uint16_t x_raw = (uint16_t)(report[offset + 1] | ((uint16_t)report[offset + 2] << 8));
        uint16_t y_raw = (uint16_t)(report[offset + 3] | ((uint16_t)report[offset + 4] << 8));
        uint32_t max_coord = (x_raw > 0x7FFF || y_raw > 0x7FFF) ? 0xFFFFu : 0x7FFFu;
        SInt16 x = (SInt16)((uint32_t)x_raw * (fb_width - 1) / max_coord);
        SInt16 y = (SInt16)((uint32_t)y_raw * (fb_height - 1) / max_coord);
        UpdateMouseStateAbsolute(x, y, buttons);
    } else {
        int16_t dx = (int8_t)report[offset + 1];
        int16_t dy = (int8_t)report[offset + 2];
        UpdateMouseStateDelta(dx, -dy, buttons);
    }

    if (len >= 4 + offset) {
        int8_t wheel = (int8_t)report[3 + offset];
        if (wheel != 0) {
            UInt16 mods = GetModifierState();
            ProcessScrollWheelEvent(0, (SInt16)-wheel, mods, TickCount());
        }
    }
    if (len >= 5 + offset) {
        int8_t pan = (int8_t)report[4 + offset];
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
    uint8_t ports = g_xhci_ports ? g_xhci_ports : MAX_XHCI_PORTS;
    if (ports > MAX_XHCI_PORTS) {
        ports = MAX_XHCI_PORTS;
    }
    for (uint8_t i = 0; i < ports; i++) {
        if (g_hid_kbd[i].pending && (now - g_hid_kbd[i].pending_tick) > 10) {
            g_hid_kbd[i].pending = false;
        }
        if (g_hid_mouse[i].pending && (now - g_hid_mouse[i].pending_tick) > 10) {
            g_hid_mouse[i].pending = false;
        }
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
        xhci_hid_dev_t *kbd = xhci_hid_find_by_slot(g_hid_kbd, slot);
        if (kbd && kbd->pending) {
            kbd->pending = false;
            xhci_handle_hid_keyboard(kbd, (const uint8_t *)kbd->buf);
            continue;
        }
        xhci_hid_dev_t *mouse = xhci_hid_find_by_slot(g_hid_mouse, slot);
        if (mouse && mouse->pending) {
            mouse->pending = false;
            xhci_handle_hid_mouse(mouse, (const uint8_t *)mouse->buf, mouse->mps);
        }
    }
}

static bool xhci_msc_bulk_transfer(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                   uintptr_t rt_base, bool in, const void *buf, uint32_t len) {
    if (!dev || !dev->configured) {
        return false;
    }
    if (in) {
        xhci_msc_enqueue_in(dev, (uintptr_t)buf, len);
        mmio_write32(base + dboff, XHCI_DB0 + dev->slot, dev->bulk_in_ep_id);
    } else {
        xhci_msc_enqueue_out(dev, (uintptr_t)buf, len);
        mmio_write32(base + dboff, XHCI_DB0 + dev->slot, dev->bulk_out_ep_id);
    }
    return xhci_poll_transfer_complete(rt_base, dev->slot);
}

static bool xhci_uasp_transfer_cmd(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                   uintptr_t rt_base, const void *buf, uint32_t len) {
    if (!dev || !dev->configured || !dev->uasp_cmd_out_ep_id) {
        return false;
    }
    xhci_uasp_enqueue_cmd(dev, (uintptr_t)buf, len);
    mmio_write32(base + dboff, XHCI_DB0 + dev->slot, dev->uasp_cmd_out_ep_id);
    return xhci_poll_transfer_complete(rt_base, dev->slot);
}

static bool xhci_uasp_transfer_data(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                    uintptr_t rt_base, bool in, const void *buf, uint32_t len) {
    if (!dev || !dev->configured) {
        return false;
    }
    if (in) {
        if (!dev->uasp_data_in_ep_id) {
            return false;
        }
        xhci_uasp_enqueue_data_in(dev, (uintptr_t)buf, len);
        mmio_write32(base + dboff, XHCI_DB0 + dev->slot, dev->uasp_data_in_ep_id);
    } else {
        if (!dev->uasp_data_out_ep_id) {
            return false;
        }
        xhci_uasp_enqueue_data_out(dev, (uintptr_t)buf, len);
        mmio_write32(base + dboff, XHCI_DB0 + dev->slot, dev->uasp_data_out_ep_id);
    }
    return xhci_poll_transfer_complete(rt_base, dev->slot);
}

static bool xhci_uasp_transfer_status(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                      uintptr_t rt_base, void *buf, uint32_t len) {
    if (!dev || !dev->configured || !dev->uasp_status_in_ep_id) {
        return false;
    }
    xhci_uasp_enqueue_status(dev, (uintptr_t)buf, len);
    mmio_write32(base + dboff, XHCI_DB0 + dev->slot, dev->uasp_status_in_ep_id);
    return xhci_poll_transfer_complete(rt_base, dev->slot);
}

static bool xhci_msc_send_cbw(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                              uintptr_t rt_base, uint8_t lun, uint8_t *cb, uint8_t cb_len,
                              uint32_t data_len, bool data_in) {
    uint8_t *cbw = MSC_CBW(dev);
    memset(cbw, 0, 31);
    cbw[0] = 'U';
    cbw[1] = 'S';
    cbw[2] = 'B';
    cbw[3] = 'C';
    uint32_t tag = dev->tag++;
    cbw[4] = (uint8_t)(tag & 0xFF);
    cbw[5] = (uint8_t)((tag >> 8) & 0xFF);
    cbw[6] = (uint8_t)((tag >> 16) & 0xFF);
    cbw[7] = (uint8_t)((tag >> 24) & 0xFF);
    cbw[8] = (uint8_t)(data_len & 0xFF);
    cbw[9] = (uint8_t)((data_len >> 8) & 0xFF);
    cbw[10] = (uint8_t)((data_len >> 16) & 0xFF);
    cbw[11] = (uint8_t)((data_len >> 24) & 0xFF);
    cbw[12] = data_in ? 0x80 : 0x00;
    cbw[13] = lun;
    cbw[14] = cb_len;
    memcpy(&cbw[15], cb, cb_len);

    return xhci_msc_bulk_transfer(dev, base, dboff, rt_base, false, cbw, 31);
}

static bool xhci_msc_recv_csw(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                              uintptr_t rt_base) {
    uint8_t *csw = MSC_CSW(dev);
    if (!xhci_msc_bulk_transfer(dev, base, dboff, rt_base, true, csw, 13)) {
        return false;
    }
    if (csw[0] != 'U' || csw[1] != 'S' || csw[2] != 'B' || csw[3] != 'S') {
        serial_puts("[XHCI] MSC CSW bad signature\n");
        return false;
    }
    dev->last_status = csw[12];
    if (dev->last_status != 0) {
        serial_printf("[XHCI] MSC CSW status=%u\n", dev->last_status);
        return false;
    }
    return true;
}

static void xhci_msc_parse_sense(xhci_msc_dev_t *dev, const uint8_t *buf, uint32_t len) {
    if (!buf || len < 14) {
        return;
    }
    if ((buf[0] & 0x7F) != 0x70 && (buf[0] & 0x7F) != 0x71) {
        return;
    }
    dev->last_sense_key = buf[2] & 0x0F;
    dev->last_asc = buf[12];
    dev->last_ascq = buf[13];
}

static void xhci_msc_delay_ticks(uint32_t ticks) {
    uint32_t start = TickCount();
    while ((TickCount() - start) < ticks) {
        /* busy wait */
    }
}

static bool xhci_msc_request_sense_bot(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                       uintptr_t rt_base, uint8_t lun) {
    uint8_t cb[16];
    memset(cb, 0, sizeof(cb));
    cb[0] = 0x03; /* REQUEST SENSE */
    cb[4] = 18;
    if (!xhci_msc_send_cbw(dev, base, dboff, rt_base, lun, cb, 6, 18, true)) {
        return false;
    }
    if (!xhci_msc_bulk_transfer(dev, base, dboff, rt_base, true, MSC_DATA(dev), 18)) {
        return false;
    }
    if (!xhci_msc_recv_csw(dev, base, dboff, rt_base)) {
        return false;
    }
    xhci_msc_parse_sense(dev, (const uint8_t *)MSC_DATA(dev), 18);
    return true;
}

static bool xhci_msc_recover_bot(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                 uintptr_t rt_base) {
    if (!dev || dev->slot == 0) {
        return false;
    }
    if (!xhci_ep0_mass_storage_reset(dev, base, dboff, rt_base)) {
        return false;
    }
    if (dev->bulk_in_ep) {
        xhci_ep0_clear_halt(base, dboff, rt_base, dev->slot, dev->bulk_in_ep);
    }
    if (dev->bulk_out_ep) {
        xhci_ep0_clear_halt(base, dboff, rt_base, dev->slot, dev->bulk_out_ep);
    }
    if (dev->bot_in_ep) {
        xhci_ep0_clear_halt(base, dboff, rt_base, dev->slot, dev->bot_in_ep);
    }
    if (dev->bot_out_ep) {
        xhci_ep0_clear_halt(base, dboff, rt_base, dev->slot, dev->bot_out_ep);
    }
    dev->in_ring_index = 0;
    dev->out_ring_index = 0;
    dev->in_cycle = 1;
    dev->out_cycle = 1;
    return true;
}

static bool xhci_msc_request_sense_uasp(xhci_msc_dev_t *dev, uint8_t lun) {
    uint8_t cb[16];
    memset(cb, 0, sizeof(cb));
    cb[0] = 0x03; /* REQUEST SENSE */
    cb[4] = 18;
    if (!xhci_uasp_exec_scsi(dev, lun, cb, 6, MSC_DATA(dev), 18, true)) {
        return false;
    }
    xhci_msc_parse_sense(dev, (const uint8_t *)MSC_DATA(dev), 18);
    return true;
}

static void xhci_msc_log_sense(xhci_msc_dev_t *dev, const char *prefix) {
    if (!prefix) {
        prefix = "MSC";
    }
    serial_printf("[XHCI] %s sense key=0x%02x asc=0x%02x ascq=0x%02x\n",
                  prefix, dev->last_sense_key, dev->last_asc, dev->last_ascq);
}

static bool xhci_msc_retry_not_ready(xhci_msc_dev_t *dev, bool uasp, uint8_t lun) {
    if (dev->last_sense_key != 0x02) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        xhci_msc_delay_ticks(5);
        if (xhci_msc_test_unit_ready(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base, lun)) {
            return true;
        }
    }
    return false;
}

static bool xhci_uasp_exec_scsi(xhci_msc_dev_t *dev, uint8_t lun, uint8_t *cdb, uint8_t cdb_len,
                                const void *data, uint32_t data_len, bool data_in) {
    if (!dev || !dev->uasp) {
        return false;
    }
    bool recovered = false;
    for (;;) {
        uint8_t *cmd_iu = MSC_UASP_CMD_IU(dev);
        memset(cmd_iu, 0, 32);
        cmd_iu[0] = 0x01; /* Command IU */
        cmd_iu[2] = (uint8_t)(dev->tag & 0xFF);
        cmd_iu[3] = (uint8_t)((dev->tag >> 8) & 0xFF);
        cmd_iu[4] = (uint8_t)(data_len & 0xFF);
        cmd_iu[5] = (uint8_t)((data_len >> 8) & 0xFF);
        cmd_iu[6] = (uint8_t)((data_len >> 16) & 0xFF);
        cmd_iu[7] = (uint8_t)((data_len >> 24) & 0xFF);
        cmd_iu[8] = lun;
        memset(&cmd_iu[9], 0, 7);
        memcpy(&cmd_iu[16], cdb, cdb_len);

        dev->tag++;

        if (!xhci_uasp_transfer_cmd(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                    cmd_iu, 32)) {
            /* fall through to recovery */
        } else if (data_len > 0 &&
                   !xhci_uasp_transfer_data(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                            data_in, data, data_len)) {
            /* fall through to recovery */
        } else {
            uint8_t *status_iu = MSC_UASP_STATUS_IU(dev);
            memset(status_iu, 0, 16);
            if (!xhci_uasp_transfer_status(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                           status_iu, 16)) {
                /* fall through to recovery */
            } else if (status_iu[0] != 0x03) {
                serial_printf("[XHCI] UASP status IU id=0x%02x\n", status_iu[0]);
                /* fall through to recovery */
            } else {
                dev->last_status = status_iu[4];
                if (dev->last_status != 0) {
                    serial_printf("[XHCI] UASP status=0x%02x\n", dev->last_status);
                    /* fall through to recovery */
                } else {
                    return true;
                }
            }
        }

        if (!recovered && xhci_uasp_recover(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base)) {
            recovered = true;
            continue;
        }
        return false;
    }
}

static bool xhci_msc_read_capacity(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                   uintptr_t rt_base, uint8_t lun) {
    uint8_t cb[16];
    bool recovered = false;

    for (;;) {
        memset(cb, 0, sizeof(cb));
        cb[0] = 0x25; /* READ CAPACITY(10) */
        if (dev->uasp) {
            if (!xhci_uasp_exec_scsi(dev, lun, cb, 10, MSC_DATA(dev), 8, true)) {
                serial_puts("[XHCI] UASP READ CAPACITY failed\n");
                xhci_msc_request_sense_uasp(dev, lun);
                xhci_msc_log_sense(dev, "UASP");
                return false;
            }
        } else {
            if (!xhci_msc_send_cbw(dev, base, dboff, rt_base, lun, cb, 10, 8, true)) {
                serial_puts("[XHCI] MSC READ CAPACITY CBW failed\n");
            } else if (!xhci_msc_bulk_transfer(dev, base, dboff, rt_base, true, MSC_DATA(dev), 8)) {
                serial_puts("[XHCI] MSC READ CAPACITY data failed\n");
                xhci_msc_request_sense_bot(dev, base, dboff, rt_base, lun);
                xhci_msc_log_sense(dev, "MSC");
            } else if (!xhci_msc_recv_csw(dev, base, dboff, rt_base)) {
                serial_puts("[XHCI] MSC READ CAPACITY CSW failed\n");
                xhci_msc_request_sense_bot(dev, base, dboff, rt_base, lun);
                xhci_msc_log_sense(dev, "MSC");
            } else {
                break;
            }

            if (!recovered && xhci_msc_recover_bot(dev, base, dboff, rt_base)) {
                recovered = true;
                continue;
            }
            return false;
        }
        break;
    }

    uint8_t *data_buf = (uint8_t *)MSC_DATA(dev);
    uint32_t last_lba = (data_buf[0] << 24) | (data_buf[1] << 16) |
                        (data_buf[2] << 8) | data_buf[3];
    uint32_t blk_size = (data_buf[4] << 24) | (data_buf[5] << 16) |
                        (data_buf[6] << 8) | data_buf[7];

    if (last_lba == 0xFFFFFFFFu) {
        memset(cb, 0, sizeof(cb));
        cb[0] = 0x9E; /* READ CAPACITY(16) */
        cb[1] = 0x10; /* service action */
        cb[13] = 16;  /* allocation length */
        if (dev->uasp) {
            if (!xhci_uasp_exec_scsi(dev, lun, cb, 16, MSC_DATA(dev), 16, true)) {
                serial_puts("[XHCI] UASP READ CAPACITY(16) failed\n");
                xhci_msc_request_sense_uasp(dev, lun);
                xhci_msc_log_sense(dev, "UASP");
                return false;
            }
        } else {
            if (!xhci_msc_send_cbw(dev, base, dboff, rt_base, lun, cb, 16, 16, true)) {
                serial_puts("[XHCI] MSC READ CAPACITY(16) CBW failed\n");
                return false;
            }
            if (!xhci_msc_bulk_transfer(dev, base, dboff, rt_base, true, MSC_DATA(dev), 16)) {
                serial_puts("[XHCI] MSC READ CAPACITY(16) data failed\n");
                xhci_msc_request_sense_bot(dev, base, dboff, rt_base, lun);
                xhci_msc_log_sense(dev, "MSC");
                return false;
            }
            if (!xhci_msc_recv_csw(dev, base, dboff, rt_base)) {
                serial_puts("[XHCI] MSC READ CAPACITY(16) CSW failed\n");
                xhci_msc_request_sense_bot(dev, base, dboff, rt_base, lun);
                xhci_msc_log_sense(dev, "MSC");
                return false;
            }
        }

        uint64_t last_lba64 = ((uint64_t)data_buf[0] << 56) |
                              ((uint64_t)data_buf[1] << 48) |
                              ((uint64_t)data_buf[2] << 40) |
                              ((uint64_t)data_buf[3] << 32) |
                              ((uint64_t)data_buf[4] << 24) |
                              ((uint64_t)data_buf[5] << 16) |
                              ((uint64_t)data_buf[6] << 8) |
                              (uint64_t)data_buf[7];
        blk_size = (data_buf[8] << 24) | (data_buf[9] << 16) |
                   (data_buf[10] << 8) | data_buf[11];
        if (lun < MAX_MSC_LUNS) {
            dev->lun_block_size[lun] = blk_size;
            dev->lun_block_count[lun] = last_lba64 + 1u;
            dev->lun_valid[lun] = true;
        }
        if (lun == 0) {
            dev->block_size = blk_size;
            dev->block_count = last_lba64 + 1u;
        }
        serial_printf("[XHCI] MSC capacity last_lba=%llu block_size=%u\n",
                      (unsigned long long)last_lba64, blk_size);
        return (blk_size != 0);
    }
    if (lun < MAX_MSC_LUNS) {
        dev->lun_block_size[lun] = blk_size;
        dev->lun_block_count[lun] = (uint64_t)last_lba + 1u;
        dev->lun_valid[lun] = true;
    }
    if (lun == 0) {
        dev->block_size = blk_size;
        dev->block_count = (uint64_t)last_lba + 1u;
    }
    serial_printf("[XHCI] MSC capacity last_lba=%u block_size=%u\n", last_lba, blk_size);
    return (blk_size != 0);
}

static bool xhci_msc_configure_bot(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                   uintptr_t rt_base) {
    if (!dev || !dev->bot_present || dev->bot_in_ep_id == 0 || dev->bot_out_ep_id == 0) {
        return false;
    }
    memset(g_input_ctx, 0, sizeof(g_input_ctx));
    xhci_build_bulk_endpoint_context(dev->bot_out_ep_id, dev->bot_out_mps, 2, MSC_OUT_RING(dev));
    xhci_build_bulk_endpoint_context(dev->bot_in_ep_id, dev->bot_in_mps, 6, MSC_IN_RING(dev));
    if (!xhci_configure_endpoint(base, dboff, rt_base, dev->slot)) {
        return false;
    }
    dev->configured = true;
    dev->bot_configured = true;
    dev->uasp = false;
    serial_puts("[XHCI] BOT endpoints configured\n");
    return true;
}

static bool xhci_msc_get_max_lun(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                 uintptr_t rt_base, uint8_t slot_id) {
    usb_setup_packet_t setup = {
        .bmRequestType = 0xA1, /* IN | Class | Interface */
        .bRequest = 0xFE,      /* GET_MAX_LUN */
        .wValue = 0,
        .wIndex = dev->interface_num,
        .wLength = 1
    };

    memset(g_ep0_buf[slot_id - 1], 0, sizeof(g_ep0_buf[slot_id - 1]));
    xhci_ep0_reset_ring(slot_id);
    xhci_ep0_enqueue_setup(slot_id, &setup);
    xhci_ep0_enqueue_data_in(slot_id, (uintptr_t)&g_ep0_buf[slot_id - 1][0], 1);
    xhci_ep0_enqueue_status(slot_id);
    xhci_ep0_ring_doorbell(base, dboff, slot_id);

    if (!xhci_poll_transfer_complete(rt_base, slot_id)) {
        return false;
    }
    uint8_t max_lun = ((uint8_t *)&g_ep0_buf[slot_id - 1][0])[0];
    dev->max_lun = (uint8_t)(max_lun + 1);
    if (dev->max_lun == 0) {
        dev->max_lun = 1;
    }
    if (dev->max_lun > MAX_MSC_LUNS) {
        dev->max_lun = MAX_MSC_LUNS;
    }
    serial_printf("[XHCI] MSC max LUN=%u\n", (unsigned)dev->max_lun - 1);
    return true;
}

static bool xhci_msc_try_fallback_bot(xhci_msc_dev_t *dev) {
    if (!g_xhci_base || !g_xhci_rt_base) {
        return false;
    }
    if (!dev || !dev->bot_present || dev->bot_configured) {
        return false;
    }
    if (!xhci_msc_configure_bot(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base)) {
        return false;
    }
    return true;
}

static int xhci_msc_compute_drive_base(uint8_t port_index) {
    int base = hal_storage_get_ata_count();
    uint8_t ports = g_xhci_ports ? g_xhci_ports : MAX_XHCI_PORTS;
    if (ports > MAX_XHCI_PORTS) {
        ports = MAX_XHCI_PORTS;
    }
    for (uint8_t i = 0; i < ports && i < port_index; i++) {
        if (g_msc[i].present && g_msc_last_luns[i] > 0) {
            base += g_msc_last_luns[i];
        }
    }
    return base;
}

static void xhci_msc_fire_events(uint8_t port_index, bool inserted) {
    if (port_index >= MAX_XHCI_PORTS || g_msc_last_luns[port_index] == 0) {
        return;
    }
    int base = xhci_msc_compute_drive_base(port_index);
    uint8_t luns = g_msc_last_luns[port_index];
    serial_printf("[XHCI] MSC port %u drive base=%d luns=%u\n",
                  (unsigned)(port_index + 1), base, luns);
    for (uint8_t i = 0; i < luns; i++) {
        if (inserted) {
            ProcessDiskInsertion((SInt16)(base + i), "USB");
        } else {
            ProcessDiskEjection((SInt16)(base + i));
        }
    }
}

static void xhci_poll_ports_hotplug(void) {
    if (!g_xhci_base || !g_xhci_cap_len) {
        return;
    }
    uintptr_t op_base = g_xhci_base + g_xhci_cap_len;
    uint8_t ports = g_xhci_ports ? g_xhci_ports : MAX_XHCI_PORTS;
    if (ports > MAX_XHCI_PORTS) {
        ports = MAX_XHCI_PORTS;
    }
    static bool logged_ports = false;
    static uint8_t retry_budget[MAX_XHCI_PORTS];
    static uint16_t retry_cooldown[MAX_XHCI_PORTS];

    for (uint8_t p = 0; p < ports; p++) {
        uint32_t off = XHCI_PORTSC_BASE + p * XHCI_PORTSC_STRIDE;
        uint32_t portsc = mmio_read32(op_base, off);
        if ((portsc & XHCI_PORTSC_PP) == 0) {
            mmio_write32(op_base, off, portsc | XHCI_PORTSC_PP);
            portsc = mmio_read32(op_base, off);
        }
        bool connected = (portsc & XHCI_PORTSC_CCS) != 0;
        if (!logged_ports) {
            serial_printf("[XHCI] port %u PORTSC=0x%08x CCS=%u\n",
                          (unsigned)(p + 1), portsc, connected ? 1u : 0u);
        }

        if (!connected && g_port_connected[p]) {
            g_port_connected[p] = false;
            retry_budget[p] = 0;
            retry_cooldown[p] = 0;
            xhci_msc_dev_t *msc = &g_msc[p];
            if (msc->present) {
                msc->present = false;
                msc->configured = false;
                msc->bot_configured = false;
                msc->uasp_failed = false;
                msc->slot = 0;
                msc->block_size = 0;
                msc->block_count = 0;
                msc->bulk_in_ep = 0;
                msc->bulk_out_ep = 0;
                msc->bulk_in_ep_id = 0;
                msc->bulk_out_ep_id = 0;
                msc->bot_present = false;
                msc->bot_in_ep = 0;
                msc->bot_out_ep = 0;
                msc->bot_in_ep_id = 0;
                msc->bot_out_ep_id = 0;
                memset(msc->lun_valid, 0, sizeof(msc->lun_valid));
                xhci_msc_fire_events(p, false);
                g_msc_last_luns[p] = 0;
            }
            xhci_hid_dev_t *kbd = &g_hid_kbd[p];
            if (kbd->present) {
                kbd->present = false;
                kbd->configured = false;
                kbd->slot = 0;
                kbd->ep_addr = 0;
                kbd->ep_id = 0;
                kbd->pending = false;
                kbd->last_submit_tick = 0;
                serial_puts("[XHCI] HID keyboard disconnected\n");
            }
            xhci_hid_dev_t *mouse = &g_hid_mouse[p];
            if (mouse->present) {
                mouse->present = false;
                mouse->configured = false;
                mouse->slot = 0;
                mouse->ep_addr = 0;
                mouse->ep_id = 0;
                mouse->pending = false;
                mouse->last_submit_tick = 0;
                mouse->absolute_pointer = false;
                serial_puts("[XHCI] HID mouse disconnected\n");
            }
            continue;
        }

        if (connected && !g_port_connected[p]) {
            g_port_connected[p] = true;
            retry_budget[p] = 5;
            retry_cooldown[p] = 0;
            xhci_enumerate_port(p);
        }
        if (connected && g_port_connected[p] &&
            !g_hid_mouse[p].configured && !g_hid_kbd[p].configured && !g_msc[p].configured) {
            if (retry_cooldown[p] > 0) {
                retry_cooldown[p]--;
            } else if (retry_budget[p] > 0) {
                retry_budget[p]--;
                retry_cooldown[p] = 300;
                xhci_enumerate_port(p);
            }
        }
    }
    logged_ports = true;
}

static bool xhci_msc_inquiry(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                             uintptr_t rt_base, uint8_t lun) {
    uint8_t cb[16];
    bool recovered = false;
    for (;;) {
        memset(cb, 0, sizeof(cb));
        cb[0] = 0x12; /* INQUIRY */
        cb[4] = 36;
        if (dev->uasp) {
            if (!xhci_uasp_exec_scsi(dev, lun, cb, 6, MSC_DATA(dev), 36, true)) {
                serial_puts("[XHCI] UASP INQUIRY failed\n");
                xhci_msc_request_sense_uasp(dev, lun);
                xhci_msc_log_sense(dev, "UASP");
                return false;
            }
        } else {
            if (!xhci_msc_send_cbw(dev, base, dboff, rt_base, lun, cb, 6, 36, true)) {
                serial_puts("[XHCI] MSC INQUIRY CBW failed\n");
            } else if (!xhci_msc_bulk_transfer(dev, base, dboff, rt_base, true, MSC_DATA(dev), 36)) {
                serial_puts("[XHCI] MSC INQUIRY data failed\n");
                xhci_msc_request_sense_bot(dev, base, dboff, rt_base, lun);
                xhci_msc_log_sense(dev, "MSC");
            } else if (!xhci_msc_recv_csw(dev, base, dboff, rt_base)) {
                serial_puts("[XHCI] MSC INQUIRY CSW failed\n");
                xhci_msc_request_sense_bot(dev, base, dboff, rt_base, lun);
                xhci_msc_log_sense(dev, "MSC");
            } else {
                break;
            }

            if (!recovered && xhci_msc_recover_bot(dev, base, dboff, rt_base)) {
                recovered = true;
                continue;
            }
            return false;
        }
        break;
    }

    serial_printf("[XHCI] MSC INQUIRY vendor=%.8s product=%.16s\n",
                  (char *)&MSC_DATA(dev)[2], (char *)&MSC_DATA(dev)[6]);
    return true;
}

static void xhci_enumerate_port(uint8_t port_index) {
    if (!g_xhci_base || !g_xhci_rt_base || !g_xhci_cap_len) {
        return;
    }
    uint8_t ports = g_xhci_ports ? g_xhci_ports : MAX_XHCI_PORTS;
    if (ports > MAX_XHCI_PORTS) {
        ports = MAX_XHCI_PORTS;
    }
    if (port_index >= ports) {
        return;
    }
    uintptr_t op_base = g_xhci_base + g_xhci_cap_len;
    uint32_t portsc = mmio_read32(op_base, XHCI_PORTSC_BASE + port_index * XHCI_PORTSC_STRIDE);
    if ((portsc & XHCI_PORTSC_CCS) == 0) {
        return;
    }
    g_xhci_enum_port = (uint8_t)(port_index + 1);
    serial_printf("[XHCI] port %u connected (%s) PORTSC=0x%08x\n",
                  (unsigned)g_xhci_enum_port, xhci_speed_name(portsc), portsc);

    if (!xhci_port_reset(op_base, port_index)) {
        serial_printf("[XHCI] port %u reset failed\n", (unsigned)g_xhci_enum_port);
        return;
    }

    serial_printf("[XHCI] port %u reset ok\n", (unsigned)g_xhci_enum_port);
    xhci_cmd_ring_enqueue_enable_slot();
    xhci_ring_doorbell(g_xhci_base, g_xhci_dboff, 0);
    uint8_t slot_id = 0;
    if (!xhci_poll_cmd_complete(g_xhci_rt_base, &slot_id)) {
        serial_puts("[XHCI] Enable Slot timeout\n");
        return;
    }
    if (slot_id == 0 || slot_id > MAX_XHCI_SLOTS) {
        serial_printf("[XHCI] Enable Slot returned invalid slot %u\n", slot_id);
        return;
    }
    serial_printf("[XHCI] Enable Slot complete, slot=%u\n", slot_id);
    if (!xhci_address_device(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, slot_id,
                             g_xhci_enum_port, portsc)) {
        return;
    }
    if (!xhci_ep0_get_device_descriptor(g_xhci_base, g_xhci_dboff, g_xhci_rt_base, slot_id)) {
        return;
    }
    uint8_t configs = g_slot_num_configs[slot_id - 1];
    if (configs == 0) {
        configs = 1;
    }
    bool configured = false;
    for (uint8_t cfg = 0; cfg < configs; cfg++) {
        if (xhci_ep0_get_config_descriptor(g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                           slot_id, cfg)) {
            configured = true;
            break;
        }
    }
    if (!configured) {
        serial_printf("[XHCI] No supported configuration found for slot %u\n", slot_id);
        return;
    }
}

static bool xhci_msc_test_unit_ready(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                     uintptr_t rt_base, uint8_t lun) {
    uint8_t cb[16];
    memset(cb, 0, sizeof(cb));
    cb[0] = 0x00; /* TEST UNIT READY */
    if (dev->uasp) {
        if (!xhci_uasp_exec_scsi(dev, lun, cb, 6, NULL, 0, true)) {
            xhci_msc_request_sense_uasp(dev, lun);
            xhci_msc_log_sense(dev, "UASP");
            return xhci_msc_retry_not_ready(dev, true, lun);
        }
        return true;
    }
    bool recovered = false;
    for (;;) {
        if (!xhci_msc_send_cbw(dev, base, dboff, rt_base, lun, cb, 6, 0, false)) {
            /* fall through */
        } else if (!xhci_msc_recv_csw(dev, base, dboff, rt_base)) {
            xhci_msc_request_sense_bot(dev, base, dboff, rt_base, lun);
            xhci_msc_log_sense(dev, "MSC");
            if (xhci_msc_retry_not_ready(dev, false, lun)) {
                return true;
            }
        } else {
            return true;
        }

        if (!recovered && xhci_msc_recover_bot(dev, base, dboff, rt_base)) {
            recovered = true;
            continue;
        }
        return false;
    }
}

static void xhci_msc_smoke_test(xhci_msc_dev_t *dev, uintptr_t base, uint32_t dboff,
                                uintptr_t rt_base) {
    uint8_t cb[16];
    if (!xhci_msc_inquiry(dev, base, dboff, rt_base, 0)) {
        return;
    }
    if (!xhci_msc_read_capacity(dev, base, dboff, rt_base, 0)) {
        return;
    }

    if (dev->block_size == 0 || dev->block_size > sizeof(dev->io->data_buf)) {
        serial_puts("[XHCI] MSC block size unsupported for smoke test\n");
        return;
    }

    memset(cb, 0, sizeof(cb));
    cb[0] = 0x28; /* READ(10) */
    cb[7] = 0;
    cb[8] = 1; /* 1 block */
    if (!xhci_msc_send_cbw(dev, base, dboff, rt_base, 0, cb, 10, dev->block_size, true)) {
        serial_puts("[XHCI] MSC READ10 CBW failed\n");
        return;
    }
    if (!xhci_msc_bulk_transfer(dev, base, dboff, rt_base, true, MSC_DATA(dev), dev->block_size)) {
        serial_puts("[XHCI] MSC READ10 data failed\n");
        return;
    }
    if (!xhci_msc_recv_csw(dev, base, dboff, rt_base)) {
        serial_puts("[XHCI] MSC READ10 CSW failed\n");
        return;
    }

    serial_printf("[XHCI] MSC LBA0: %02x %02x %02x %02x\n",
                  ((uint8_t *)MSC_DATA(dev))[0], ((uint8_t *)MSC_DATA(dev))[1],
                  ((uint8_t *)MSC_DATA(dev))[2], ((uint8_t *)MSC_DATA(dev))[3]);
}

static bool xhci_msc_init_if_needed(xhci_msc_dev_t *dev) {
    if (!dev || !dev->configured || !g_xhci_base || !g_xhci_rt_base) {
        return false;
    }
    if (dev->block_size != 0 && dev->block_count != 0) {
        return true;
    }

    if (!xhci_msc_test_unit_ready(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0)) {
        serial_puts("[XHCI] MSC not ready\n");
    }

    if (!xhci_msc_inquiry(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0)) {
        if (dev->uasp) {
            dev->uasp_failed = true;
            if (xhci_msc_try_fallback_bot(dev)) {
                return xhci_msc_inquiry(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0) &&
                       xhci_msc_read_capacity(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0);
            }
        }
        return false;
    }
    if (!xhci_msc_read_capacity(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0)) {
        if (dev->uasp) {
            dev->uasp_failed = true;
            if (xhci_msc_try_fallback_bot(dev)) {
                return xhci_msc_read_capacity(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base, 0);
            }
        }
        return false;
    }
    return true;
}

bool xhci_msc_available(void) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(0, NULL);
    return dev ? xhci_msc_init_if_needed(dev) : false;
}

bool xhci_hid_available(void) {
    uint8_t ports = g_xhci_ports ? g_xhci_ports : MAX_XHCI_PORTS;
    if (ports > MAX_XHCI_PORTS) {
        ports = MAX_XHCI_PORTS;
    }

    for (uint8_t i = 0; i < ports; i++) {
        if (g_hid_kbd[i].present || g_hid_mouse[i].present) {
            return true;
        }
    }
    return false;
}

static uint8_t xhci_msc_get_lun_count_dev(xhci_msc_dev_t *dev) {
    if (!dev || !xhci_msc_init_if_needed(dev)) {
        return 0;
    }
    if (dev->max_lun == 0) {
        return 1;
    }
    if (dev->max_lun > MAX_MSC_LUNS) {
        return MAX_MSC_LUNS;
    }
    return dev->max_lun;
}

OSErr xhci_msc_get_info(uint32_t *block_size, uint64_t *block_count, bool *read_only) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(0, NULL);
    if (!dev || !xhci_msc_init_if_needed(dev)) {
        return paramErr;
    }
    if (dev->uasp_failed) {
        serial_puts("[XHCI] UASP failed, device may require BOT fallback\n");
    }
    if (block_size) {
        *block_size = dev->block_size;
    }
    if (block_count) {
        *block_count = dev->block_count;
    }
    if (read_only) {
        *read_only = dev->readonly;
    }
    return noErr;
}

OSErr xhci_msc_get_info_lun(uint8_t lun, uint32_t *block_size, uint64_t *block_count, bool *read_only) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(0, NULL);
    if (!dev || !xhci_msc_ensure_lun_info(dev, lun)) {
        return paramErr;
    }
    if (block_size) {
        *block_size = dev->lun_block_size[lun];
    }
    if (block_count) {
        *block_count = dev->lun_block_count[lun];
    }
    if (read_only) {
        *read_only = dev->readonly;
    }
    return noErr;
}

static bool xhci_msc_ensure_lun_info(xhci_msc_dev_t *dev, uint8_t lun) {
    if (!dev || !xhci_msc_init_if_needed(dev)) {
        return false;
    }
    if (lun >= dev->max_lun || lun >= MAX_MSC_LUNS) {
        return false;
    }
    if (dev->lun_valid[lun]) {
        return true;
    }
    return xhci_msc_read_capacity(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base, lun);
}

static OSErr xhci_msc_read_blocks_internal(xhci_msc_dev_t *dev, uint8_t lun, uint64_t start_block,
                                           uint32_t block_count, void *buffer) {
    if (!dev || !xhci_msc_init_if_needed(dev) || !buffer) {
        return paramErr;
    }
    if (!xhci_msc_ensure_lun_info(dev, lun)) {
        return ioErr;
    }
    uint32_t blk_size = dev->lun_block_size[lun];
    uint64_t blk_count = dev->lun_block_count[lun];
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
    if (blk_size > sizeof(dev->io->data_buf)) {
        return paramErr;
    }

    uint8_t *buf = (uint8_t *)buffer;
    uint32_t max_blocks = (uint32_t)(sizeof(dev->io->data_buf) / blk_size);
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
        if (dev->uasp) {
            if (!xhci_uasp_exec_scsi(dev, lun, cb, 10, buf, data_len, true)) {
                xhci_msc_request_sense_uasp(dev, lun);
                xhci_msc_log_sense(dev, "UASP");
                if (xhci_msc_retry_not_ready(dev, true, lun)) {
                    continue;
                }
                dev->uasp_failed = true;
                if (!tried_fallback && xhci_msc_try_fallback_bot(dev)) {
                    tried_fallback = true;
                    continue;
                }
                return ioErr;
            }
        } else {
            bool bot_ok = false;
            if (xhci_msc_send_cbw(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                  lun, cb, 10, data_len, true) &&
                xhci_msc_bulk_transfer(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                       true, buf, data_len) &&
                xhci_msc_recv_csw(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base)) {
                bot_ok = true;
            } else {
                xhci_msc_request_sense_bot(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base, lun);
                xhci_msc_log_sense(dev, "MSC");
                if (xhci_msc_retry_not_ready(dev, false, lun)) {
                    continue;
                }
            }
            if (!bot_ok) {
                if (!tried_fallback && xhci_msc_recover_bot(dev, g_xhci_base,
                                                           g_xhci_dboff, g_xhci_rt_base)) {
                    tried_fallback = true;
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

static OSErr xhci_msc_write_blocks_internal(xhci_msc_dev_t *dev, uint8_t lun,
                                            uint64_t start_block,
                                            uint32_t block_count, const void *buffer) {
    if (!dev || !xhci_msc_init_if_needed(dev) || !buffer) {
        return paramErr;
    }
    if (!xhci_msc_ensure_lun_info(dev, lun)) {
        return ioErr;
    }
    uint32_t blk_size = dev->lun_block_size[lun];
    uint64_t blk_count = dev->lun_block_count[lun];
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
    if (dev->readonly) {
        return wPrErr;
    }
    if (blk_size > sizeof(dev->io->data_buf)) {
        return paramErr;
    }

    const uint8_t *buf = (const uint8_t *)buffer;
    uint32_t max_blocks = (uint32_t)(sizeof(dev->io->data_buf) / blk_size);
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
        if (dev->uasp) {
            if (!xhci_uasp_exec_scsi(dev, lun, cb, 10, buf, data_len, false)) {
                xhci_msc_request_sense_uasp(dev, lun);
                xhci_msc_log_sense(dev, "UASP");
                if (xhci_msc_retry_not_ready(dev, true, lun)) {
                    continue;
                }
                dev->uasp_failed = true;
                if (!tried_fallback && xhci_msc_try_fallback_bot(dev)) {
                    tried_fallback = true;
                    continue;
                }
                return ioErr;
            }
        } else {
            bool bot_ok = false;
            if (xhci_msc_send_cbw(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                  lun, cb, 10, data_len, false) &&
                xhci_msc_bulk_transfer(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base,
                                       false, buf, data_len) &&
                xhci_msc_recv_csw(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base)) {
                bot_ok = true;
            } else {
                xhci_msc_request_sense_bot(dev, g_xhci_base, g_xhci_dboff, g_xhci_rt_base, lun);
                xhci_msc_log_sense(dev, "MSC");
                if (xhci_msc_retry_not_ready(dev, false, lun)) {
                    continue;
                }
            }
            if (!bot_ok) {
                if (!tried_fallback && xhci_msc_recover_bot(dev, g_xhci_base,
                                                           g_xhci_dboff, g_xhci_rt_base)) {
                    tried_fallback = true;
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
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(0, NULL);
    return dev ? xhci_msc_read_blocks_internal(dev, 0, start_block, block_count, buffer) : paramErr;
}

OSErr xhci_msc_write_blocks(uint64_t start_block, uint32_t block_count, const void *buffer) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(0, NULL);
    return dev ? xhci_msc_write_blocks_internal(dev, 0, start_block, block_count, buffer) : paramErr;
}

uint8_t xhci_msc_get_lun_count(void) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(0, NULL);
    return dev ? xhci_msc_get_lun_count_dev(dev) : 0;
}

OSErr xhci_msc_read_blocks_lun(uint8_t lun, uint64_t start_block, uint32_t block_count, void *buffer) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(0, NULL);
    return dev ? xhci_msc_read_blocks_internal(dev, lun, start_block, block_count, buffer) : paramErr;
}

OSErr xhci_msc_write_blocks_lun(uint8_t lun, uint64_t start_block, uint32_t block_count, const void *buffer) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(0, NULL);
    return dev ? xhci_msc_write_blocks_internal(dev, lun, start_block, block_count, buffer) : paramErr;
}

uint8_t xhci_msc_get_device_count(void) {
    uint8_t count = 0;
    uint8_t ports = g_xhci_ports ? g_xhci_ports : MAX_XHCI_PORTS;
    if (ports > MAX_XHCI_PORTS) {
        ports = MAX_XHCI_PORTS;
    }
    for (uint8_t i = 0; i < ports; i++) {
        if (g_msc[i].present) {
            count++;
        }
    }
    return count;
}

uint8_t xhci_msc_get_lun_count_device(uint8_t dev_index) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(dev_index, NULL);
    return dev ? xhci_msc_get_lun_count_dev(dev) : 0;
}

OSErr xhci_msc_get_info_device_lun(uint8_t dev_index, uint8_t lun,
                                   uint32_t *block_size, uint64_t *block_count,
                                   bool *read_only) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(dev_index, NULL);
    if (!dev || !xhci_msc_ensure_lun_info(dev, lun)) {
        return paramErr;
    }
    if (block_size) {
        *block_size = dev->lun_block_size[lun];
    }
    if (block_count) {
        *block_count = dev->lun_block_count[lun];
    }
    if (read_only) {
        *read_only = dev->readonly;
    }
    return noErr;
}

OSErr xhci_msc_read_blocks_device_lun(uint8_t dev_index, uint8_t lun, uint64_t start_block,
                                      uint32_t block_count, void *buffer) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(dev_index, NULL);
    return dev ? xhci_msc_read_blocks_internal(dev, lun, start_block, block_count, buffer) : paramErr;
}

OSErr xhci_msc_write_blocks_device_lun(uint8_t dev_index, uint8_t lun, uint64_t start_block,
                                       uint32_t block_count, const void *buffer) {
    xhci_msc_dev_t *dev = xhci_msc_get_present_by_index(dev_index, NULL);
    return dev ? xhci_msc_write_blocks_internal(dev, lun, start_block, block_count, buffer) : paramErr;
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

static bool xhci_hid_has_key(const uint8_t *report, uint8_t key, uint8_t end) {
    for (uint8_t i = 2; i < end; i++) {
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
    uint8_t end = dev->mps;
    if (end > sizeof(dev->last_report)) {
        end = (uint8_t)sizeof(dev->last_report);
    }
    if (end < 8) {
        end = 8;
    }

    for (uint8_t i = 2; i < end; i++) {
        uint8_t key = dev->last_report[i];
        if (key != 0 && !xhci_hid_has_key(report, key, end)) {
            UInt16 sc = xhci_hid_to_mac_scan(key);
            if (sc != 0xFFFF) {
                ProcessRawKeyboardEvent(sc, false, modifiers, ts);
            }
        }
    }

    for (uint8_t i = 2; i < end; i++) {
        uint8_t key = report[i];
        if (key != 0 && !xhci_hid_has_key(dev->last_report, key, end)) {
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

    uint32_t timeout = 500000;
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
    cmd |= XHCI_CMD_RUN | XHCI_CMD_INTE;
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
            uint8_t max_slots = (uint8_t)(params1 & 0xFF);
            uint8_t ports = (uint8_t)((params1 >> 24) & 0xFF);
            if (ports == 0) {
                ports = MAX_XHCI_PORTS;
            }
            g_ctx_size = (hccparams1 & (1u << 2)) ? 64 : 32;
            g_xhci_cap_len = cap_len;
            g_xhci_ports = ports;
            g_xhci_max_slots = max_slots;

            serial_printf("[XHCI] caplen=%u version=%x ports=%u slots=%u\n",
                          cap_len, version, ports, max_slots);
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

            /* Enable interrupter (IE bit) */
            mmio_write32(rt_base, XHCI_RT_IMAN, 0x2);
            mmio_write32(rt_base, XHCI_RT_IMOD, 0);

            /* Set max device slots enabled */
            uint8_t slots_enabled = g_xhci_max_slots;
            if (slots_enabled == 0) {
                slots_enabled = 1;
            }
            if (slots_enabled > MAX_XHCI_SLOTS) {
                slots_enabled = MAX_XHCI_SLOTS;
            }
            mmio_write32(op_base, XHCI_CONFIG, slots_enabled);

            if (!xhci_start(base, cap_len)) {
                serial_puts("[XHCI] start timeout\n");
                return false;
            }

            /* Power on ports if needed */
            for (uint8_t p = 0; p < ports; p++) {
                uint32_t portsc = mmio_read32(op_base, XHCI_PORTSC_BASE + p * XHCI_PORTSC_STRIDE);
                if ((portsc & XHCI_PORTSC_PP) == 0) {
                    mmio_write32(op_base, XHCI_PORTSC_BASE + p * XHCI_PORTSC_STRIDE,
                                 portsc | XHCI_PORTSC_PP);
                }
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
                xhci_enumerate_port(p);
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
    uint8_t ports = g_xhci_ports ? g_xhci_ports : MAX_XHCI_PORTS;
    if (ports > MAX_XHCI_PORTS) {
        ports = MAX_XHCI_PORTS;
    }
    for (uint8_t i = 0; i < ports; i++) {
        xhci_hid_submit(g_xhci_base, g_xhci_dboff, &g_hid_kbd[i]);
        xhci_hid_submit(g_xhci_base, g_xhci_dboff, &g_hid_mouse[i]);
    }
    xhci_hid_poll_events(g_xhci_rt_base);
    xhci_poll_ports_hotplug();
}
