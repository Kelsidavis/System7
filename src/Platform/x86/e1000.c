#include "e1000.h"
#include "pci.h"
#include "Platform/include/serial.h"
#include <string.h>

/* ============== REGISTERS ================ */

#define E1000_REG_CTRL   0x0000
#define E1000_REG_RCTL   0x0100
#define E1000_REG_TCTL   0x0400

#define E1000_REG_RDBAL  0x2800
#define E1000_REG_RDLEN  0x2808
#define E1000_REG_RDH    0x2810
#define E1000_REG_RDT    0x2818

#define E1000_REG_TDBAL  0x3800
#define E1000_REG_TDLEN  0x3808
#define E1000_REG_TDH    0x3810
#define E1000_REG_TDT    0x3818

#define E1000_REG_RAL    0x5400
#define E1000_REG_RAH    0x5404

#define E1000_RCTL_EN    (1 << 1)
#define E1000_RCTL_BAM   (1 << 15)
#define E1000_RCTL_SECRC (1 << 26)

#define E1000_TCTL_EN   (1 << 1)
#define E1000_TCTL_PSP  (1 << 3)
#define E1000_TCTL_CT   (0x10 << 4)
#define E1000_TCTL_COLD (0x40 << 12)

/* ============== DESCRIPTORS =============== */

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} __attribute__((packed)) rx_desc_t;

typedef struct {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} __attribute__((packed)) tx_desc_t;

/* =============== BUFFERS ================== */

static rx_desc_t rx_ring[RX_RING_SIZE];
static tx_desc_t tx_ring[TX_RING_SIZE];

static uint8_t rx_buf[RX_RING_SIZE][ETH_FRAME_SIZE];
static uint8_t tx_buf[TX_RING_SIZE][ETH_FRAME_SIZE];

static uint16_t rx_idx = 0;
static uint16_t tx_idx = 0;

/* =============== MMIO ===================== */

static inline void mmio_write(uint32_t base, uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(base + reg) = val;
}

static inline uint32_t mmio_read(uint32_t base, uint32_t reg) {
    return *(volatile uint32_t*)(base + reg);
}

/* ============== NET STRUCTS =============== */

#pragma pack(push,1)
typedef struct {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} eth_hdr_t;

typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[6];
    uint32_t spa;
    uint8_t  tha[6];
    uint32_t tpa;
} arp_pkt_t;

typedef struct {
    uint8_t  ver_ihl;
    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    uint16_t flags;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t csum;
    uint32_t src;
    uint32_t dst;
} ip_hdr_t;

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t csum;
    uint16_t id;
    uint16_t seq;
} icmp_hdr_t;
#pragma pack(pop)

/* ============== CHECKSUM ================== */

static uint16_t checksum(void* data, int len) {
    uint32_t sum = 0;
    uint16_t* p = data;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(uint8_t*)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

/* ============== TX SEND =================== */

static void e1000_send(e1000_t* dev, void* data, uint16_t len) {
    memcpy(tx_buf[tx_idx], data, len);

    tx_ring[tx_idx].addr   = (uint64_t)(uint32_t)tx_buf[tx_idx];
    tx_ring[tx_idx].length = len;
    tx_ring[tx_idx].cmd    = 0x0B; /* EOP | IFCS | RS */
    tx_ring[tx_idx].status = 0;

    mmio_write(dev->mmio_base, E1000_REG_TDT, tx_idx);
    tx_idx = (tx_idx + 1) % TX_RING_SIZE;
}

/* ============== ARP ======================= */

static void handle_arp(e1000_t* dev, uint8_t* pkt) {
    eth_hdr_t* eth = (eth_hdr_t*)pkt;
    arp_pkt_t* arp = (arp_pkt_t*)(pkt + sizeof(*eth));

    if (__builtin_bswap16(arp->oper) != 1) return;
    if (__builtin_bswap32(arp->tpa) != dev->ip) return;

    uint8_t out[64];
    eth_hdr_t* re = (eth_hdr_t*)out;
    arp_pkt_t* ra = (arp_pkt_t*)(out + sizeof(*re));

    memcpy(re->dst, eth->src, 6);
    memcpy(re->src, dev->mac, 6);
    re->type = __builtin_bswap16(0x0806);

    *ra = *arp;
    ra->oper = __builtin_bswap16(2);
    memcpy(ra->sha, dev->mac, 6);
    ra->spa = __builtin_bswap32(dev->ip);
    memcpy(ra->tha, arp->sha, 6);
    ra->tpa = arp->spa;

    e1000_send(dev, out, sizeof(*re) + sizeof(*ra));
}

/* ============== IP ======================== */

static void handle_ip(e1000_t* dev, uint8_t* pkt, uint16_t len) {
    eth_hdr_t* eth = (eth_hdr_t*)pkt;
    ip_hdr_t* ip   = (ip_hdr_t*)(pkt + sizeof(*eth));

    if (ip->proto == 1) {
        /* ICMP */
        icmp_hdr_t* ic = (icmp_hdr_t*)((uint8_t*)ip + 20);
        if (ic->type != 8) return; /* Only handle echo request */

        uint8_t out[ETH_FRAME_SIZE];
        memcpy(out, pkt, len);

        eth_hdr_t* re = (eth_hdr_t*)out;
        ip_hdr_t* rip = (ip_hdr_t*)(out + sizeof(*re));
        icmp_hdr_t* ric = (icmp_hdr_t*)((uint8_t*)rip + 20);

        memcpy(re->dst, eth->src, 6);
        memcpy(re->src, dev->mac, 6);

        uint32_t t = rip->src; rip->src = rip->dst; rip->dst = t;

        ric->type = 0; /* Echo reply */
        ric->csum = 0;
        ric->csum = checksum(ric, len - sizeof(*re) - 20);

        rip->csum = 0;
        rip->csum = checksum(rip, 20);

        e1000_send(dev, out, len);
    }
}

/* ============== INIT ====================== */

int e1000_init(e1000_t* dev, pci_device_t* pci) {
    dev->mmio_base = pci->bar0 & 0xFFFFFFF0;

    /* Device reset */
    mmio_write(dev->mmio_base, E1000_REG_CTRL, 1 << 26);

    /* Brief delay for reset to begin */
    for (volatile int i = 0; i < 10000; i++) { }

    /* Wait for reset to self-clear (bit 26) */
    {
        int timeout = 100000;
        while ((mmio_read(dev->mmio_base, E1000_REG_CTRL) & (1 << 26)) && --timeout > 0) {
            for (volatile int i = 0; i < 100; i++) { }
        }
    }

    /* Read MAC from RAL/RAH registers (populated by EEPROM auto-load after reset) */
    uint32_t ral = mmio_read(dev->mmio_base, E1000_REG_RAL);
    uint32_t rah = mmio_read(dev->mmio_base, E1000_REG_RAH);

    if (ral != 0) {
        dev->mac[0] = (uint8_t)(ral);
        dev->mac[1] = (uint8_t)(ral >> 8);
        dev->mac[2] = (uint8_t)(ral >> 16);
        dev->mac[3] = (uint8_t)(ral >> 24);
        dev->mac[4] = (uint8_t)(rah);
        dev->mac[5] = (uint8_t)(rah >> 8);
    } else {
        /* Fallback: default QEMU MAC */
        dev->mac[0]=0x52; dev->mac[1]=0x54; dev->mac[2]=0x00;
        dev->mac[3]=0x12; dev->mac[4]=0x34; dev->mac[5]=0x56;
    }

    serial_printf("[E1000] MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
        dev->mac[0], dev->mac[1], dev->mac[2],
        dev->mac[3], dev->mac[4], dev->mac[5]);

    /* Setup RX ring */
    for (int i = 0; i < RX_RING_SIZE; i++) {
        rx_ring[i].addr = (uint64_t)(uint32_t)rx_buf[i];
        rx_ring[i].status = 0;
    }

    mmio_write(dev->mmio_base, E1000_REG_RDBAL, (uint32_t)rx_ring);
    mmio_write(dev->mmio_base, E1000_REG_RDLEN, RX_RING_SIZE * sizeof(rx_desc_t));
    mmio_write(dev->mmio_base, E1000_REG_RDH, 0);
    mmio_write(dev->mmio_base, E1000_REG_RDT, RX_RING_SIZE - 1);

    /* Setup TX ring */
    mmio_write(dev->mmio_base, E1000_REG_TDBAL, (uint32_t)tx_ring);
    mmio_write(dev->mmio_base, E1000_REG_TDLEN, TX_RING_SIZE * sizeof(tx_desc_t));
    mmio_write(dev->mmio_base, E1000_REG_TDH, 0);
    mmio_write(dev->mmio_base, E1000_REG_TDT, 0);

    /* Enable RX */
    mmio_write(dev->mmio_base, E1000_REG_RCTL,
        E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SECRC);

    /* Enable TX */
    mmio_write(dev->mmio_base, E1000_REG_TCTL,
        E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT | E1000_TCTL_COLD);

    return 0;
}

/* ============== POLL ====================== */

void e1000_poll(e1000_t* dev) {
    int budget = 16;

    while (budget-- > 0) {
        rx_desc_t* d = &rx_ring[rx_idx];
        if (!(d->status & 0x01)) break;

        uint8_t* pkt = rx_buf[rx_idx];
        uint16_t len = d->length;

        eth_hdr_t* eth = (eth_hdr_t*)pkt;
        uint16_t type = __builtin_bswap16(eth->type);

        if (type == 0x0806) handle_arp(dev, pkt);
        if (type == 0x0800) handle_ip(dev, pkt, len);

        d->status = 0;
        mmio_write(dev->mmio_base, E1000_REG_RDT, rx_idx);
        rx_idx = (rx_idx + 1) % RX_RING_SIZE;
    }
}
