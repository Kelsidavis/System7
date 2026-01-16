/*
 * DWC2 (DesignWare Core 2) USB Host Controller Driver
 * For Raspberry Pi 3/4 ARM64
 */

#ifndef ARM64_DWC2_H
#define ARM64_DWC2_H

#include <stdint.h>
#include <stdbool.h>

/* Base addresses for different Pi models */
#define DWC2_BASE_PI3       0x3F980000
#define DWC2_BASE_PI4       0xFE980000

/* Core Global Registers (0x0000 - 0x03FF) */
#define DWC2_GOTGCTL        0x000   /* OTG Control and Status */
#define DWC2_GOTGINT        0x004   /* OTG Interrupt */
#define DWC2_GAHBCFG        0x008   /* AHB Configuration */
#define DWC2_GUSBCFG        0x00C   /* USB Configuration */
#define DWC2_GRSTCTL        0x010   /* Reset Control */
#define DWC2_GINTSTS        0x014   /* Interrupt Status */
#define DWC2_GINTMSK        0x018   /* Interrupt Mask */
#define DWC2_GRXSTSR        0x01C   /* Receive Status Read */
#define DWC2_GRXSTSP        0x020   /* Receive Status Pop */
#define DWC2_GRXFSIZ        0x024   /* Receive FIFO Size */
#define DWC2_GNPTXFSIZ      0x028   /* Non-Periodic TX FIFO Size */
#define DWC2_GNPTXSTS       0x02C   /* Non-Periodic TX FIFO Status */
#define DWC2_GSNPSID        0x040   /* Synopsys ID (verify presence) */
#define DWC2_GHWCFG1        0x044   /* Hardware Config 1 */
#define DWC2_GHWCFG2        0x048   /* Hardware Config 2 */
#define DWC2_GHWCFG3        0x04C   /* Hardware Config 3 */
#define DWC2_GHWCFG4        0x050   /* Hardware Config 4 */
#define DWC2_HPTXFSIZ       0x100   /* Host Periodic TX FIFO Size */

/* Host Mode Registers (0x0400 - 0x04FF) */
#define DWC2_HCFG           0x400   /* Host Configuration */
#define DWC2_HFIR           0x404   /* Host Frame Interval */
#define DWC2_HFNUM          0x408   /* Host Frame Number/Remaining */
#define DWC2_HPTXSTS        0x410   /* Host Periodic TX FIFO Status */
#define DWC2_HAINT          0x414   /* Host All Channels Interrupt */
#define DWC2_HAINTMSK       0x418   /* Host All Channels Interrupt Mask */
#define DWC2_HPRT           0x440   /* Host Port Control and Status */

/* Host Channel Registers (0x0500 + n*0x20) */
#define DWC2_HCCHAR(n)      (0x500 + (n) * 0x20)  /* Channel Characteristics */
#define DWC2_HCSPLT(n)      (0x504 + (n) * 0x20)  /* Channel Split Control */
#define DWC2_HCINT(n)       (0x508 + (n) * 0x20)  /* Channel Interrupt */
#define DWC2_HCINTMSK(n)    (0x50C + (n) * 0x20)  /* Channel Interrupt Mask */
#define DWC2_HCTSIZ(n)      (0x510 + (n) * 0x20)  /* Channel Transfer Size */
#define DWC2_HCDMA(n)       (0x514 + (n) * 0x20)  /* Channel DMA Address */

/* GAHBCFG bits */
#define GAHBCFG_GLBL_INTR_EN    (1 << 0)
#define GAHBCFG_HBST_LEN_SINGLE (0 << 1)
#define GAHBCFG_HBST_LEN_INCR   (1 << 1)
#define GAHBCFG_HBST_LEN_INCR4  (3 << 1)
#define GAHBCFG_HBST_LEN_INCR8  (5 << 1)
#define GAHBCFG_HBST_LEN_INCR16 (7 << 1)
#define GAHBCFG_DMA_EN          (1 << 5)
#define GAHBCFG_NPTXFEMPLVL     (1 << 7)
#define GAHBCFG_PTXFEMPLVL      (1 << 8)

/* GUSBCFG bits */
#define GUSBCFG_TOUTCAL(x)      ((x) & 0x7)
#define GUSBCFG_PHYIF_16BIT     (1 << 3)
#define GUSBCFG_ULPI_UTMI_SEL   (1 << 4)
#define GUSBCFG_FSINTF          (1 << 5)
#define GUSBCFG_PHYSEL          (1 << 6)
#define GUSBCFG_SRPCAP          (1 << 8)
#define GUSBCFG_HNPCAP          (1 << 9)
#define GUSBCFG_USBTRDTIM(x)    (((x) & 0xF) << 10)
#define GUSBCFG_PHYLPWRCLKSEL   (1 << 15)
#define GUSBCFG_FORCEHSTMODE    (1 << 29)
#define GUSBCFG_FORCEDEVMODE    (1 << 30)

/* GRSTCTL bits */
#define GRSTCTL_CSFTRST         (1 << 0)
#define GRSTCTL_HSFTRST         (1 << 1)
#define GRSTCTL_FRMCNTRRST      (1 << 2)
#define GRSTCTL_RXFFLSH         (1 << 4)
#define GRSTCTL_TXFFLSH         (1 << 5)
#define GRSTCTL_TXFNUM(x)       (((x) & 0x1F) << 6)
#define GRSTCTL_TXFNUM_ALL      GRSTCTL_TXFNUM(0x10)
#define GRSTCTL_DMAREQ          (1 << 30)
#define GRSTCTL_AHBIDLE         (1 << 31)

/* GINTSTS/GINTMSK bits */
#define GINTSTS_CURMODE_HOST    (1 << 0)
#define GINTSTS_MODEMIS         (1 << 1)
#define GINTSTS_SOF             (1 << 3)
#define GINTSTS_RXFLVL          (1 << 4)
#define GINTSTS_NPTXFEMP        (1 << 5)
#define GINTSTS_GINNAKEFF       (1 << 6)
#define GINTSTS_GOUTNAKEFF      (1 << 7)
#define GINTSTS_I2CINT          (1 << 9)
#define GINTSTS_ERLYSUSP        (1 << 10)
#define GINTSTS_USBSUSP         (1 << 11)
#define GINTSTS_USBRST          (1 << 12)
#define GINTSTS_ENUMDONE        (1 << 13)
#define GINTSTS_ISOOUTDROP      (1 << 14)
#define GINTSTS_EOPF            (1 << 15)
#define GINTSTS_INCOMPISOOUT    (1 << 21)
#define GINTSTS_INCOMPISOIN     (1 << 20)
#define GINTSTS_HCHINT          (1 << 25)
#define GINTSTS_PTXFEMP         (1 << 26)
#define GINTSTS_PRTINT          (1 << 24)
#define GINTSTS_DISCONNINT      (1 << 29)
#define GINTSTS_SESSREQINT      (1 << 30)
#define GINTSTS_WKUPINT         (1 << 31)

/* HPRT (Host Port) bits */
#define HPRT_PRTCONNSTS         (1 << 0)   /* Port connect status (RO) */
#define HPRT_PRTCONNDET         (1 << 1)   /* Port connect detected (W1C) */
#define HPRT_PRTENA             (1 << 2)   /* Port enable (W1C to disable) */
#define HPRT_PRTENCHNG          (1 << 3)   /* Port enable/disable change (W1C) */
#define HPRT_PRTOVRCURRACT      (1 << 4)   /* Port overcurrent active (RO) */
#define HPRT_PRTOVRCURRCHNG     (1 << 5)   /* Port overcurrent change (W1C) */
#define HPRT_PRTRES             (1 << 6)   /* Port resume */
#define HPRT_PRTSUSP            (1 << 7)   /* Port suspend */
#define HPRT_PRTRST             (1 << 8)   /* Port reset */
#define HPRT_PRTLNSTS_MASK      (0x3 << 10) /* Port line status */
#define HPRT_PRTPWR             (1 << 12)  /* Port power */
#define HPRT_PRTTSTCTL_MASK     (0xF << 13) /* Port test control */
#define HPRT_PRTSPD_MASK        (0x3 << 17) /* Port speed */
#define HPRT_PRTSPD_HIGH        (0x0 << 17)
#define HPRT_PRTSPD_FULL        (0x1 << 17)
#define HPRT_PRTSPD_LOW         (0x2 << 17)
/* W1C bits - mask these when modifying other bits */
#define HPRT_W1C_MASK           (HPRT_PRTCONNDET | HPRT_PRTENA | HPRT_PRTENCHNG | HPRT_PRTOVRCURRCHNG)

/* HCCHAR (Host Channel Characteristics) bits */
#define HCCHAR_MPS_MASK         0x7FF
#define HCCHAR_MPS(x)           ((x) & HCCHAR_MPS_MASK)
#define HCCHAR_EPNUM_MASK       (0xF << 11)
#define HCCHAR_EPNUM(x)         (((x) & 0xF) << 11)
#define HCCHAR_EPDIR            (1 << 15)
#define HCCHAR_EPDIR_OUT        (0 << 15)
#define HCCHAR_EPDIR_IN         (1 << 15)
#define HCCHAR_LSDEV            (1 << 17)
#define HCCHAR_EPTYPE_MASK      (0x3 << 18)
#define HCCHAR_EPTYPE(x)        (((x) & 0x3) << 18)
#define HCCHAR_EC_MASK          (0x3 << 20)
#define HCCHAR_EC(x)            (((x) & 0x3) << 20)
#define HCCHAR_DEVADDR_MASK     (0x7F << 22)
#define HCCHAR_DEVADDR(x)       (((x) & 0x7F) << 22)
#define HCCHAR_ODDFRM           (1 << 29)
#define HCCHAR_CHDIS            (1 << 30)
#define HCCHAR_CHEN             (1 << 31)

/* Endpoint types */
#define EP_TYPE_CONTROL         0
#define EP_TYPE_ISOC            1
#define EP_TYPE_BULK            2
#define EP_TYPE_INTERRUPT       3

/* HCTSIZ (Host Channel Transfer Size) bits */
#define HCTSIZ_XFERSIZE_MASK    0x7FFFF
#define HCTSIZ_XFERSIZE(x)      ((x) & HCTSIZ_XFERSIZE_MASK)
#define HCTSIZ_PKTCNT_MASK      (0x3FF << 19)
#define HCTSIZ_PKTCNT(x)        (((x) & 0x3FF) << 19)
#define HCTSIZ_PID_MASK         (0x3 << 29)
#define HCTSIZ_PID_DATA0        (0 << 29)
#define HCTSIZ_PID_DATA2        (1 << 29)
#define HCTSIZ_PID_DATA1        (2 << 29)
#define HCTSIZ_PID_MDATA        (3 << 29)
#define HCTSIZ_PID_SETUP        (3 << 29)
#define HCTSIZ_DOPNG            (1 << 31)

/* HCINT (Host Channel Interrupt) bits */
#define HCINT_XFERCOMP          (1 << 0)   /* Transfer complete */
#define HCINT_CHHLTD            (1 << 1)   /* Channel halted */
#define HCINT_AHBERR            (1 << 2)   /* AHB error */
#define HCINT_STALL             (1 << 3)   /* STALL response */
#define HCINT_NAK               (1 << 4)   /* NAK response */
#define HCINT_ACK               (1 << 5)   /* ACK response */
#define HCINT_NYET              (1 << 6)   /* NYET response */
#define HCINT_XACTERR           (1 << 7)   /* Transaction error */
#define HCINT_BBLERR            (1 << 8)   /* Babble error */
#define HCINT_FRMOVRUN          (1 << 9)   /* Frame overrun */
#define HCINT_DATATGLERR        (1 << 10)  /* Data toggle error */
#define HCINT_BNA               (1 << 11)  /* Buffer not available */
#define HCINT_XCS_XACT_ERR      (1 << 12)  /* Excessive transaction error */
#define HCINT_DESC_LST_ROLLINTR (1 << 13)  /* Descriptor list rollover */
/* Error mask for quick checking */
#define HCINT_ERROR_MASK        (HCINT_AHBERR | HCINT_STALL | HCINT_XACTERR | HCINT_BBLERR | HCINT_DATATGLERR)

/* Number of host channels available */
#define DWC2_MAX_CHANNELS       16

/* Port speed values */
#define USB_SPEED_HIGH          0
#define USB_SPEED_FULL          1
#define USB_SPEED_LOW           2

/* Channel state */
typedef struct {
    bool     in_use;
    uint8_t  dev_addr;
    uint8_t  ep_num;
    uint8_t  ep_type;
    bool     ep_in;
    uint16_t max_packet_size;
    uint8_t  data_toggle;
    volatile bool complete;
    volatile uint32_t error;
    uint8_t *buffer;
    uint32_t buffer_len;
    uint32_t transferred;
} dwc2_channel_t;

/* Public API */
bool dwc2_init(void);
bool dwc2_is_initialized(void);
uint64_t dwc2_get_base(void);

/* Port management */
bool dwc2_port_connected(void);
bool dwc2_port_reset(void);
uint8_t dwc2_port_speed(void);

/* Channel management */
int dwc2_channel_alloc(void);
void dwc2_channel_free(int ch);
dwc2_channel_t *dwc2_channel_get(int ch);

/* Transfers */
int dwc2_transfer_start(int ch, uint8_t dev_addr, uint8_t ep_num,
                        uint8_t ep_type, bool is_in, uint16_t max_pkt,
                        uint8_t *buffer, uint32_t length, uint32_t pid);
int dwc2_transfer_wait(int ch, uint32_t timeout_ms);

#endif /* ARM64_DWC2_H */
