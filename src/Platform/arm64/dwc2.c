/*
 * DWC2 (DesignWare Core 2) USB Host Controller Driver
 * For Raspberry Pi 3/4 ARM64
 */

#include "dwc2.h"
#include "mmio.h"
#include "timer.h"
#include "uart.h"
#include "cache.h"

/* Global state */
static uint64_t dwc2_base = 0;
static bool dwc2_initialized = false;
static dwc2_channel_t channels[DWC2_MAX_CHANNELS];

/* Forward declarations */
static bool dwc2_detect_base(void);
static bool dwc2_core_reset(void);
static void dwc2_flush_fifos(void);
static void dwc2_configure_fifos(void);

/*
 * Detect DWC2 controller base address
 */
static bool dwc2_detect_base(void) {
    uint64_t bases[] = { DWC2_BASE_PI4, DWC2_BASE_PI3 };

    for (int i = 0; i < 2; i++) {
        uint32_t snpsid = mmio_read32(bases[i] + DWC2_GSNPSID);

        /* Valid Synopsys ID patterns:
         * 0x4F542xxx = OTG2.x
         * 0x4F543xxx = OTG3.x
         */
        uint32_t id_prefix = snpsid & 0xFFFFF000;
        if (id_prefix == 0x4F542000 || id_prefix == 0x4F543000) {
            dwc2_base = bases[i];
            return true;
        }
    }

    return false;
}

/*
 * Core soft reset
 */
static bool dwc2_core_reset(void) {
    uint32_t timeout;

    /* Wait for AHB master idle */
    timeout = 100000;
    while (!(mmio_read32(dwc2_base + DWC2_GRSTCTL) & GRSTCTL_AHBIDLE)) {
        if (--timeout == 0) {
            uart_puts("[DWC2] AHB idle timeout\n");
            return false;
        }
    }

    /* Issue core soft reset */
    mmio_write32(dwc2_base + DWC2_GRSTCTL, GRSTCTL_CSFTRST);

    /* Wait for reset to complete */
    timeout = 100000;
    while (mmio_read32(dwc2_base + DWC2_GRSTCTL) & GRSTCTL_CSFTRST) {
        if (--timeout == 0) {
            uart_puts("[DWC2] Core reset timeout\n");
            return false;
        }
    }

    /* Wait for AHB master idle again */
    timeout = 100000;
    while (!(mmio_read32(dwc2_base + DWC2_GRSTCTL) & GRSTCTL_AHBIDLE)) {
        if (--timeout == 0) {
            uart_puts("[DWC2] AHB idle timeout after reset\n");
            return false;
        }
    }

    /* PHY clocks stabilization delay */
    timer_usleep(100000);  /* 100ms */

    return true;
}

/*
 * Flush TX and RX FIFOs
 */
static void dwc2_flush_fifos(void) {
    uint32_t timeout;

    /* Flush all TX FIFOs */
    mmio_write32(dwc2_base + DWC2_GRSTCTL, GRSTCTL_TXFFLSH | GRSTCTL_TXFNUM_ALL);
    timeout = 10000;
    while (mmio_read32(dwc2_base + DWC2_GRSTCTL) & GRSTCTL_TXFFLSH) {
        if (--timeout == 0) break;
    }
    timer_usleep(1000);

    /* Flush RX FIFO */
    mmio_write32(dwc2_base + DWC2_GRSTCTL, GRSTCTL_RXFFLSH);
    timeout = 10000;
    while (mmio_read32(dwc2_base + DWC2_GRSTCTL) & GRSTCTL_RXFFLSH) {
        if (--timeout == 0) break;
    }
    timer_usleep(1000);
}

/*
 * Configure FIFO sizes
 * Total FIFO RAM is typically 4096 words (16KB)
 */
static void dwc2_configure_fifos(void) {
    /* RX FIFO: 512 words starting at 0 */
    mmio_write32(dwc2_base + DWC2_GRXFSIZ, 512);

    /* Non-periodic TX FIFO: 256 words starting at 512 */
    mmio_write32(dwc2_base + DWC2_GNPTXFSIZ, (256 << 16) | 512);

    /* Periodic TX FIFO: 256 words starting at 768 */
    mmio_write32(dwc2_base + DWC2_HPTXFSIZ, (256 << 16) | 768);
}

/*
 * Initialize DWC2 controller
 */
bool dwc2_init(void) {
    uint32_t gusbcfg, gahbcfg, hprt;

    if (dwc2_initialized) {
        return true;
    }

    /* Initialize channel state */
    for (int i = 0; i < DWC2_MAX_CHANNELS; i++) {
        channels[i].in_use = false;
        channels[i].complete = false;
        channels[i].error = 0;
    }

    /* Detect controller */
    if (!dwc2_detect_base()) {
        uart_puts("[DWC2] Controller not found\n");
        return false;
    }

    uart_puts("[DWC2] Controller detected\n");

    /* Core soft reset */
    if (!dwc2_core_reset()) {
        uart_puts("[DWC2] Core reset failed\n");
        return false;
    }

    /* Configure PHY - Internal UTMI+ 16-bit */
    gusbcfg = mmio_read32(dwc2_base + DWC2_GUSBCFG);
    gusbcfg &= ~(GUSBCFG_ULPI_UTMI_SEL | GUSBCFG_PHYSEL | GUSBCFG_FORCEDEVMODE);
    gusbcfg |= GUSBCFG_PHYIF_16BIT;
    gusbcfg |= GUSBCFG_USBTRDTIM(9);  /* USB turnaround time for 16-bit PHY */
    gusbcfg |= GUSBCFG_HNPCAP | GUSBCFG_SRPCAP;
    mmio_write32(dwc2_base + DWC2_GUSBCFG, gusbcfg);

    /* Force host mode */
    gusbcfg = mmio_read32(dwc2_base + DWC2_GUSBCFG);
    gusbcfg &= ~GUSBCFG_FORCEDEVMODE;
    gusbcfg |= GUSBCFG_FORCEHSTMODE;
    mmio_write32(dwc2_base + DWC2_GUSBCFG, gusbcfg);

    /* Wait for host mode */
    timer_usleep(50000);  /* 50ms */

    /* Configure AHB - Enable DMA, set burst length */
    gahbcfg = GAHBCFG_DMA_EN | GAHBCFG_HBST_LEN_INCR4;
    mmio_write32(dwc2_base + DWC2_GAHBCFG, gahbcfg);

    /* Configure FIFOs */
    dwc2_configure_fifos();

    /* Flush FIFOs */
    dwc2_flush_fifos();

    /* Configure host mode - Full speed PHY clock */
    mmio_write32(dwc2_base + DWC2_HCFG, 1);  /* FS/LS PHY clock = 48MHz */

    /* Power on port */
    hprt = mmio_read32(dwc2_base + DWC2_HPRT);
    hprt &= ~HPRT_W1C_MASK;  /* Don't clear W1C bits */
    if (!(hprt & HPRT_PRTPWR)) {
        hprt |= HPRT_PRTPWR;
        mmio_write32(dwc2_base + DWC2_HPRT, hprt);
    }

    /* Power stabilization delay */
    timer_usleep(100000);  /* 100ms */

    /* Enable core interrupts */
    mmio_write32(dwc2_base + DWC2_GINTMSK,
                 GINTSTS_PRTINT | GINTSTS_HCHINT | GINTSTS_DISCONNINT);

    /* Enable global interrupts */
    gahbcfg = mmio_read32(dwc2_base + DWC2_GAHBCFG);
    gahbcfg |= GAHBCFG_GLBL_INTR_EN;
    mmio_write32(dwc2_base + DWC2_GAHBCFG, gahbcfg);

    dwc2_initialized = true;
    uart_puts("[DWC2] Controller initialized\n");

    return true;
}

/*
 * Check if controller is initialized
 */
bool dwc2_is_initialized(void) {
    return dwc2_initialized;
}

/*
 * Get controller base address
 */
uint64_t dwc2_get_base(void) {
    return dwc2_base;
}

/*
 * Check if device is connected to port
 */
bool dwc2_port_connected(void) {
    if (!dwc2_initialized) return false;

    uint32_t hprt = mmio_read32(dwc2_base + DWC2_HPRT);
    return (hprt & HPRT_PRTCONNSTS) != 0;
}

/*
 * Reset USB port
 */
bool dwc2_port_reset(void) {
    if (!dwc2_initialized) return false;

    uint32_t hprt;

    /* Read HPRT, preserve non-W1C bits */
    hprt = mmio_read32(dwc2_base + DWC2_HPRT);
    hprt &= ~HPRT_W1C_MASK;

    /* Assert reset */
    hprt |= HPRT_PRTRST;
    mmio_write32(dwc2_base + DWC2_HPRT, hprt);

    /* USB spec requires reset for at least 10ms, we use 50ms */
    timer_usleep(50000);

    /* Deassert reset */
    hprt = mmio_read32(dwc2_base + DWC2_HPRT);
    hprt &= ~HPRT_W1C_MASK;
    hprt &= ~HPRT_PRTRST;
    mmio_write32(dwc2_base + DWC2_HPRT, hprt);

    /* Wait for port to become enabled */
    timer_usleep(20000);  /* 20ms */

    /* Check if port is enabled */
    hprt = mmio_read32(dwc2_base + DWC2_HPRT);
    if (hprt & HPRT_PRTENA) {
        uart_puts("[DWC2] Port enabled\n");
        return true;
    }

    uart_puts("[DWC2] Port enable failed\n");
    return false;
}

/*
 * Get port speed
 */
uint8_t dwc2_port_speed(void) {
    if (!dwc2_initialized) return USB_SPEED_FULL;

    uint32_t hprt = mmio_read32(dwc2_base + DWC2_HPRT);
    return (hprt & HPRT_PRTSPD_MASK) >> 17;
}

/*
 * Allocate a channel
 */
int dwc2_channel_alloc(void) {
    for (int i = 0; i < DWC2_MAX_CHANNELS; i++) {
        if (!channels[i].in_use) {
            channels[i].in_use = true;
            channels[i].complete = false;
            channels[i].error = 0;
            channels[i].transferred = 0;
            return i;
        }
    }
    return -1;
}

/*
 * Free a channel
 */
void dwc2_channel_free(int ch) {
    if (ch >= 0 && ch < DWC2_MAX_CHANNELS) {
        /* Disable channel first */
        uint32_t hcchar = mmio_read32(dwc2_base + DWC2_HCCHAR(ch));
        if (hcchar & HCCHAR_CHEN) {
            hcchar |= HCCHAR_CHDIS;
            hcchar &= ~HCCHAR_CHEN;
            mmio_write32(dwc2_base + DWC2_HCCHAR(ch), hcchar);
            timer_usleep(100);
        }

        /* Clear interrupts */
        mmio_write32(dwc2_base + DWC2_HCINT(ch), 0xFFFFFFFF);

        channels[ch].in_use = false;
    }
}

/*
 * Get channel state
 */
dwc2_channel_t *dwc2_channel_get(int ch) {
    if (ch >= 0 && ch < DWC2_MAX_CHANNELS) {
        return &channels[ch];
    }
    return NULL;
}

/*
 * Start a transfer on a channel
 */
int dwc2_transfer_start(int ch, uint8_t dev_addr, uint8_t ep_num,
                        uint8_t ep_type, bool is_in, uint16_t max_pkt,
                        uint8_t *buffer, uint32_t length, uint32_t pid) {
    if (ch < 0 || ch >= DWC2_MAX_CHANNELS || !channels[ch].in_use) {
        return -1;
    }

    dwc2_channel_t *chan = &channels[ch];

    /* Store transfer info */
    chan->dev_addr = dev_addr;
    chan->ep_num = ep_num;
    chan->ep_type = ep_type;
    chan->ep_in = is_in;
    chan->max_packet_size = max_pkt ? max_pkt : 8;
    chan->buffer = buffer;
    chan->buffer_len = length;
    chan->transferred = 0;
    chan->complete = false;
    chan->error = 0;

    /* Clean cache before OUT transfer */
    if (!is_in && buffer && length > 0) {
        dcache_clean_range(buffer, length);
    }

    /* Calculate packet count */
    uint32_t pkt_count;
    if (length == 0) {
        pkt_count = 1;  /* Zero-length packet still needs one packet */
    } else {
        pkt_count = (length + chan->max_packet_size - 1) / chan->max_packet_size;
    }

    /* Clear pending interrupts */
    mmio_write32(dwc2_base + DWC2_HCINT(ch), 0xFFFFFFFF);

    /* Enable channel interrupts */
    mmio_write32(dwc2_base + DWC2_HCINTMSK(ch),
                 HCINT_XFERCOMP | HCINT_CHHLTD | HCINT_STALL |
                 HCINT_NAK | HCINT_ACK | HCINT_XACTERR |
                 HCINT_BBLERR | HCINT_DATATGLERR);

    /* Enable this channel's interrupt in HAINTMSK */
    uint32_t haintmsk = mmio_read32(dwc2_base + DWC2_HAINTMSK);
    haintmsk |= (1 << ch);
    mmio_write32(dwc2_base + DWC2_HAINTMSK, haintmsk);

    /* Configure HCTSIZ */
    uint32_t hctsiz = HCTSIZ_XFERSIZE(length) | HCTSIZ_PKTCNT(pkt_count) | pid;
    mmio_write32(dwc2_base + DWC2_HCTSIZ(ch), hctsiz);

    /* Configure DMA address */
    mmio_write32(dwc2_base + DWC2_HCDMA(ch), (uint32_t)(uintptr_t)buffer);

    /* Configure HCCHAR */
    uint32_t hcchar = HCCHAR_MPS(chan->max_packet_size) |
                      HCCHAR_EPNUM(ep_num) |
                      (is_in ? HCCHAR_EPDIR_IN : HCCHAR_EPDIR_OUT) |
                      HCCHAR_EPTYPE(ep_type) |
                      HCCHAR_DEVADDR(dev_addr);

    /* Low-speed device handling */
    uint8_t speed = dwc2_port_speed();
    if (speed == USB_SPEED_LOW) {
        hcchar |= HCCHAR_LSDEV;
    }

    /* Use odd frame for interrupt endpoints */
    if (ep_type == EP_TYPE_INTERRUPT) {
        uint32_t hfnum = mmio_read32(dwc2_base + DWC2_HFNUM);
        if (hfnum & 1) {
            hcchar |= HCCHAR_ODDFRM;
        }
    }

    /* Enable channel - this starts the transfer */
    hcchar |= HCCHAR_CHEN;
    mmio_write32(dwc2_base + DWC2_HCCHAR(ch), hcchar);

    return 0;
}

/*
 * Wait for transfer completion (polling)
 */
int dwc2_transfer_wait(int ch, uint32_t timeout_ms) {
    if (ch < 0 || ch >= DWC2_MAX_CHANNELS || !channels[ch].in_use) {
        return -1;
    }

    dwc2_channel_t *chan = &channels[ch];
    uint64_t deadline = timer_get_ticks() + (timeout_ms * 1000);
    uint32_t retry_count = 0;
    const uint32_t max_retries = 3;

    while (!chan->complete) {
        /* Check channel interrupt status */
        uint32_t hcint = mmio_read32(dwc2_base + DWC2_HCINT(ch));

        if (hcint & HCINT_XFERCOMP) {
            /* Transfer complete */
            chan->complete = true;

            /* Invalidate cache after IN transfer */
            if (chan->ep_in && chan->buffer && chan->buffer_len > 0) {
                dcache_invalidate_range(chan->buffer, chan->buffer_len);
            }

            /* Calculate transferred bytes */
            uint32_t hctsiz = mmio_read32(dwc2_base + DWC2_HCTSIZ(ch));
            uint32_t remaining = hctsiz & HCTSIZ_XFERSIZE_MASK;
            chan->transferred = chan->buffer_len - remaining;

            /* Clear interrupt */
            mmio_write32(dwc2_base + DWC2_HCINT(ch), hcint);
            return 0;
        }

        if (hcint & HCINT_STALL) {
            /* Endpoint stalled */
            chan->complete = true;
            chan->error = HCINT_STALL;
            mmio_write32(dwc2_base + DWC2_HCINT(ch), hcint);
            return -2;
        }

        if (hcint & (HCINT_XACTERR | HCINT_BBLERR | HCINT_DATATGLERR | HCINT_AHBERR)) {
            /* Transaction error - retry */
            retry_count++;
            if (retry_count >= max_retries) {
                chan->complete = true;
                chan->error = hcint & HCINT_ERROR_MASK;
                mmio_write32(dwc2_base + DWC2_HCINT(ch), hcint);
                return -3;
            }

            /* Clear error and retry */
            mmio_write32(dwc2_base + DWC2_HCINT(ch), hcint);

            /* Re-enable channel */
            uint32_t hcchar = mmio_read32(dwc2_base + DWC2_HCCHAR(ch));
            hcchar |= HCCHAR_CHEN;
            mmio_write32(dwc2_base + DWC2_HCCHAR(ch), hcchar);
            continue;
        }

        if (hcint & HCINT_NAK) {
            /* NAK - device busy, clear and retry */
            mmio_write32(dwc2_base + DWC2_HCINT(ch), HCINT_NAK);

            /* Re-enable channel */
            uint32_t hcchar = mmio_read32(dwc2_base + DWC2_HCCHAR(ch));
            hcchar |= HCCHAR_CHEN;
            mmio_write32(dwc2_base + DWC2_HCCHAR(ch), hcchar);
        }

        if (hcint & HCINT_ACK) {
            /* ACK received - clear it */
            mmio_write32(dwc2_base + DWC2_HCINT(ch), HCINT_ACK);
        }

        /* Check timeout */
        if (timer_get_ticks() > deadline) {
            /* Timeout - disable channel */
            uint32_t hcchar = mmio_read32(dwc2_base + DWC2_HCCHAR(ch));
            hcchar |= HCCHAR_CHDIS;
            mmio_write32(dwc2_base + DWC2_HCCHAR(ch), hcchar);

            chan->complete = true;
            chan->error = 0xFFFFFFFF;  /* Timeout */
            return -4;
        }

        /* Small delay to avoid hammering the registers */
        timer_usleep(10);
    }

    return chan->error ? -1 : 0;
}
