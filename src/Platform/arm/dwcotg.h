/*
 * DesignWare USB OTG Controller Driver for Raspberry Pi 3
 * Implements USB 2.0 host controller interface
 *
 * References:
 *   - DesignWare Cores USB 2.0 OTG Controller Datasheet
 *   - Broadcom BCM2835 ARM Peripherals
 *   - Raspberry Pi 3 Device Tree
 */

#ifndef ARM_DWCOTG_H
#define ARM_DWCOTG_H

#include <stdint.h>
#include "usb_controller.h"

/* ===== DWCOTG Controller Base Address ===== */

/* Raspberry Pi 3: DWCOTG USB controller */
#define DWCOTG_BASE_PI3         0x20980000

extern uint32_t dwcotg_base;

/* ===== DWCOTG Register Map (offsets from base) ===== */

/* Core Global Registers */
#define DWCOTG_GOTGCTL          0x000   /* OTG Control */
#define DWCOTG_GOTGINT          0x004   /* OTG Interrupt */
#define DWCOTG_GAHBCFG          0x008   /* AHB Configuration */
#define DWCOTG_GUSBCFG          0x00C   /* USB Configuration */
#define DWCOTG_GRSTCTL          0x010   /* Reset Control */
#define DWCOTG_GINTSTS          0x014   /* Interrupt Status */
#define DWCOTG_GINTMSK          0x018   /* Interrupt Mask */
#define DWCOTG_GRXSTSR          0x01C   /* Receive Status Debug Read */
#define DWCOTG_GRXSTSP          0x020   /* Receive Status Debug Pop */
#define DWCOTG_GRXFSIZ          0x024   /* Receive FIFO Size */
#define DWCOTG_GNPTXFSIZ        0x028   /* Non-Periodic Transmit FIFO Size */
#define DWCOTG_GNPTXSTS         0x02C   /* Non-Periodic Transmit FIFO/Queue Status */
#define DWCOTG_GHWCFG1          0x044   /* Hardware Configuration 1 */
#define DWCOTG_GHWCFG2          0x048   /* Hardware Configuration 2 */
#define DWCOTG_GHWCFG3          0x04C   /* Hardware Configuration 3 */
#define DWCOTG_GHWCFG4          0x050   /* Hardware Configuration 4 */
#define DWCOTG_GFIFSIZ          0x054   /* Global FIFO Configuration */

/* Host Mode Registers */
#define DWCOTG_HCFG             0x400   /* Host Configuration */
#define DWCOTG_HFIR             0x404   /* Host Frame Interval */
#define DWCOTG_HFNUM            0x408   /* Host Frame Number/Time */
#define DWCOTG_HPTXSTS          0x410   /* Host Periodic Transmit FIFO/Queue Status */
#define DWCOTG_HAINT            0x414   /* Host All Channels Interrupt */
#define DWCOTG_HAINTMSK         0x418   /* Host All Channels Interrupt Mask */
#define DWCOTG_HPRT             0x440   /* Host Port Control and Status */

/* Host Channel Registers (per channel) */
#define DWCOTG_HCCHAR(n)        (0x500 + (n) * 0x20)   /* Host Channel Characteristics */
#define DWCOTG_HCSPLT(n)        (0x504 + (n) * 0x20)   /* Host Channel Split Control */
#define DWCOTG_HCINT(n)         (0x508 + (n) * 0x20)   /* Host Channel Interrupt */
#define DWCOTG_HCINTMSK(n)      (0x50C + (n) * 0x20)   /* Host Channel Interrupt Mask */
#define DWCOTG_HCTSIZ(n)        (0x510 + (n) * 0x20)   /* Host Channel Transfer Size */
#define DWCOTG_HCDMA(n)         (0x514 + (n) * 0x20)   /* Host Channel DMA Address */

/* ===== GAHBCFG Register Bits ===== */
#define DWCOTG_GAHBCFG_GLBLINTRMSK  (1 << 0)   /* Global Interrupt Mask */
#define DWCOTG_GAHBCFG_HBURSTLEN    (0xF << 1) /* Burst Length/Type */
#define DWCOTG_GAHBCFG_DMAEN        (1 << 5)   /* DMA Enable */
#define DWCOTG_GAHBCFG_NPTXFEMPLVL  (1 << 7)   /* Non-Periodic TX FIFO Empty Level */

/* ===== GUSBCFG Register Bits ===== */
#define DWCOTG_GUSBCFG_TOUTCAL      (0x7 << 0)   /* Timeout Calibration */
#define DWCOTG_GUSBCFG_PHYIF        (1 << 3)     /* PHY Interface (0=16-bit, 1=8-bit) */
#define DWCOTG_GUSBCFG_ULPI_UTMI_SEL (1 << 4)    /* ULPI vs UTMI Selection */
#define DWCOTG_GUSBCFG_FSLSPCLKSEL  (0x3 << 6)   /* FS/LS PHY Clock Select */
#define DWCOTG_GUSBCFG_SRPCAP       (1 << 12)    /* SRP Capable */
#define DWCOTG_GUSBCFG_HNPCAP       (1 << 13)    /* HNP Capable */
#define DWCOTG_GUSBCFG_USBTRDTIM    (0xF << 10)  /* USB Turnaround Time */

/* ===== GRSTCTL Register Bits ===== */
#define DWCOTG_GRSTCTL_CSFTRST      (1 << 0)   /* Core Soft Reset */
#define DWCOTG_GRSTCTL_HSFTRST      (1 << 1)   /* Host Soft Reset */
#define DWCOTG_GRSTCTL_FSFTRST      (1 << 2)   /* Frame Counter Reset */
#define DWCOTG_GRSTCTL_RXFFLSH      (1 << 4)   /* RxFIFO Flush */
#define DWCOTG_GRSTCTL_TXFFLSH      (1 << 5)   /* TxFIFO Flush */
#define DWCOTG_GRSTCTL_TXFNUM       (0x1F << 6) /* TxFIFO Number */
#define DWCOTG_GRSTCTL_AHBIDL       (UINT32_C(1) << 31)  /* AHB Master Idle */

/* ===== HCFG Register Bits ===== */
#define DWCOTG_HCFG_FSLSPSUPP       (0x3 << 0)   /* FS/LS PHY Clock Select */
#define DWCOTG_HCFG_FSLSSUPP        (1 << 2)     /* FS/LS Only Support */

/* ===== HPRT Register Bits ===== */
#define DWCOTG_HPRT_PRTCONNSTS      (1 << 0)   /* Port Connect Status */
#define DWCOTG_HPRT_PRTCONNDET      (1 << 1)   /* Port Connect Detected */
#define DWCOTG_HPRT_PRTENA          (1 << 2)   /* Port Enable */
#define DWCOTG_HPRT_PRTENCHNG       (1 << 3)   /* Port Enable/Disable Change */
#define DWCOTG_HPRT_PRTOVRCURRACT   (1 << 4)   /* Port Overcurrent Active */
#define DWCOTG_HPRT_PRTOVRCURRCHG   (1 << 5)   /* Port Overcurrent Change */
#define DWCOTG_HPRT_PRTRES          (1 << 6)   /* Port Resume */
#define DWCOTG_HPRT_PRTSUSP         (1 << 7)   /* Port Suspend */
#define DWCOTG_HPRT_PRTRST          (1 << 8)   /* Port Reset */
#define DWCOTG_HPRT_PRTPWR          (1 << 12)  /* Port Power */
#define DWCOTG_HPRT_PRTSPD          (0x3 << 17) /* Port Speed */

/* ===== HCCHAR Register Bits ===== */
#define DWCOTG_HCCHAR_MPSIZ         (0x3FF << 0)  /* Maximum Packet Size */
#define DWCOTG_HCCHAR_EPNUM         (0xF << 11)   /* Endpoint Number */
#define DWCOTG_HCCHAR_EPDIR         (1 << 15)     /* Endpoint Direction (0=OUT, 1=IN) */
#define DWCOTG_HCCHAR_LSPDDEV       (1 << 17)     /* Low-Speed Device */
#define DWCOTG_HCCHAR_EPTYPE        (0x3 << 18)   /* Endpoint Type */
#define DWCOTG_HCCHAR_EC            (0x3 << 20)   /* Error Count */
#define DWCOTG_HCCHAR_DEVADDR       (0x7F << 22)  /* Device Address */
#define DWCOTG_HCCHAR_ODDFRM        (1 << 29)     /* Odd Frame */
#define DWCOTG_HCCHAR_CHDIS         (1 << 30)     /* Channel Disable */
#define DWCOTG_HCCHAR_CHENA         (1 << 31)     /* Channel Enable */

/* ===== HCINT Register Bits ===== */
#define DWCOTG_HCINT_XFERCOMPL      (1 << 0)   /* Transfer Completed */
#define DWCOTG_HCINT_CHHLTD         (1 << 1)   /* Channel Halted */
#define DWCOTG_HCINT_AHBERR         (1 << 2)   /* AHB Error */
#define DWCOTG_HCINT_STALL          (1 << 3)   /* STALL Response Received */
#define DWCOTG_HCINT_NAK            (1 << 4)   /* NAK Response Received */
#define DWCOTG_HCINT_ACK            (1 << 5)   /* ACK Response Received */
#define DWCOTG_HCINT_XACTERR        (1 << 7)   /* Transaction Error */
#define DWCOTG_HCINT_BBLERR         (1 << 8)   /* Babble Error */
#define DWCOTG_HCINT_FRMOVRUN       (1 << 9)   /* Frame Overrun */
#define DWCOTG_HCINT_DTERR          (1 << 10)  /* Data Toggle Error */

/* ===== HCTSIZ Register Bits ===== */
#define DWCOTG_HCTSIZ_XFERSIZE      (0x7FFFF << 0)   /* Transfer Size */
#define DWCOTG_HCTSIZ_PKTCNT        (0x3FF << 19)    /* Packet Count */
#define DWCOTG_HCTSIZ_DPID          (0x3 << 29)      /* Data PID */

/* ===== Endpoint Types ===== */
#define DWCOTG_EPTYPE_CONTROL       0
#define DWCOTG_EPTYPE_ISOCHRONOUS   1
#define DWCOTG_EPTYPE_BULK          2
#define DWCOTG_EPTYPE_INTERRUPT     3

/* ===== Data PIDs ===== */
#define DWCOTG_DPID_DATA0           0
#define DWCOTG_DPID_DATA1           1
#define DWCOTG_DPID_DATA2           2
#define DWCOTG_DPID_MDATA           3

/* ===== Public API ===== */

/* Initialize DWCOTG controller */
int dwcotg_init(void);

/* Enumerate connected USB devices */
int dwcotg_enumerate_devices(void);

/* Find HID keyboard device */
int dwcotg_find_keyboard(hid_device_info_t *kb_info);

/* Find HID mouse device */
int dwcotg_find_mouse(hid_device_info_t *mouse_info);

/* Poll keyboard for input */
int dwcotg_poll_keyboard(uint8_t *key_code, uint8_t *modifiers);

/* Poll mouse for movement/buttons */
int dwcotg_poll_mouse(int8_t *dx, int8_t *dy, uint8_t *buttons);

/* Get number of connected devices */
uint32_t dwcotg_device_count(void);

/* Shutdown DWCOTG controller */
void dwcotg_shutdown(void);

/* Low-level operations */
int dwcotg_reset_controller(void);
int dwcotg_wait_ready(void);
int dwcotg_port_connected(void);

#endif /* ARM_DWCOTG_H */
