/*
 * DesignWare USB OTG Controller Implementation
 * Raspberry Pi 3 USB 2.0 host driver
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "System71StdLib.h"
#include "mmio.h"
#include "dwcotg.h"

/* Global DWCOTG state */
uint32_t dwcotg_base = 0;
static int dwcotg_initialized = 0;
static uint32_t device_count = 0;

/*
 * Detect Raspberry Pi 3 and set DWCOTG base address
 */
static int dwcotg_discover_base(void) {
    /* Raspberry Pi 3 DWCOTG is at fixed address 0x20980000 */
    dwcotg_base = DWCOTG_BASE_PI3;

    /* Verify by reading a register */
    volatile uint32_t *hwcfg2 = (volatile uint32_t *)(dwcotg_base + DWCOTG_GHWCFG2);
    uint32_t hwcfg2_val = mmio_read32((uint32_t)hwcfg2);

    Serial_Printf("[DWCOTG] Base: 0x%lx, HWCFG2: 0x%lx\n",
                  (unsigned long)dwcotg_base, (unsigned long)hwcfg2_val);

    /* Check if we can read a sane value
     * HWCFG2 should have some bits set
     */
    if (hwcfg2_val == 0 || hwcfg2_val == 0xFFFFFFFF) {
        Serial_WriteString("[DWCOTG] Error: Cannot read HWCFG2 register\n");
        return -1;
    }

    return 0;
}

/*
 * Reset DWCOTG controller core
 */
int dwcotg_reset_controller(void) {
    if (!dwcotg_base) {
        return -1;
    }

    Serial_WriteString("[DWCOTG] Resetting controller core...\n");

    volatile uint32_t *grstctl = (volatile uint32_t *)(dwcotg_base + DWCOTG_GRSTCTL);

    /* Issue core soft reset */
    uint32_t rstctl = mmio_read32((uint32_t)grstctl);
    rstctl |= DWCOTG_GRSTCTL_CSFTRST;
    mmio_write32((uint32_t)grstctl, rstctl);

    /* Wait for reset to complete (AHB becomes idle) */
    uint32_t timeout = 10000;
    while (timeout--) {
        rstctl = mmio_read32((uint32_t)grstctl);
        if (rstctl & DWCOTG_GRSTCTL_AHBIDL) {
            /* Reset complete */
            Serial_WriteString("[DWCOTG] Core reset complete\n");
            return 0;
        }
    }

    Serial_WriteString("[DWCOTG] Core reset timeout\n");
    return -1;
}

/*
 * Initialize DWCOTG controller for host mode
 */
int dwcotg_init(void) {
    Serial_WriteString("[DWCOTG] Initializing USB 2.0 host controller (DWCOTG)\n");

    if (dwcotg_discover_base() != 0) {
        Serial_WriteString("[DWCOTG] Failed to discover DWCOTG base address\n");
        return -1;
    }

    /* Perform core soft reset */
    if (dwcotg_reset_controller() != 0) {
        Serial_WriteString("[DWCOTG] Controller reset failed\n");
        return -1;
    }

    /* Wait a bit after reset */
    for (volatile int i = 0; i < 100000; i++) {
        __asm__ __volatile__("nop");
    }

    /* Configure USB controller */
    volatile uint32_t *gusbcfg = (volatile uint32_t *)(dwcotg_base + DWCOTG_GUSBCFG);
    uint32_t usbcfg = mmio_read32((uint32_t)gusbcfg);

    /* Set timeout calibration and other parameters */
    usbcfg |= (5 << 0);  /* TOUTCAL = 5 */
    mmio_write32((uint32_t)gusbcfg, usbcfg);

    /* Configure AHB bus */
    volatile uint32_t *gahbcfg = (volatile uint32_t *)(dwcotg_base + DWCOTG_GAHBCFG);
    uint32_t ahbcfg = mmio_read32((uint32_t)gahbcfg);

    /* Enable global interrupt, configure DMA burst length */
    ahbcfg |= DWCOTG_GAHBCFG_GLBLINTRMSK;
    ahbcfg |= (4 << 1);  /* Burst length = 4 */
    mmio_write32((uint32_t)gahbcfg, ahbcfg);

    /* Configure host mode */
    volatile uint32_t *hcfg = (volatile uint32_t *)(dwcotg_base + DWCOTG_HCFG);
    uint32_t cfg = mmio_read32((uint32_t)hcfg);

    /* Set FS/LS PHY clock select and FS/LS only support */
    cfg |= (1 << 0);  /* FSLSPSUPP = FS/LS PHY clock select */
    cfg |= (1 << 2);  /* FSLSSUPP = FS/LS Only Support */
    mmio_write32((uint32_t)hcfg, cfg);

    Serial_WriteString("[DWCOTG] Controller initialization complete\n");

    dwcotg_initialized = 1;
    return 0;
}

/*
 * Enable and power the USB port
 */
int dwcotg_port_connected(void) {
    if (!dwcotg_base || !dwcotg_initialized) {
        return 0;
    }

    volatile uint32_t *hprt = (volatile uint32_t *)(dwcotg_base + DWCOTG_HPRT);
    uint32_t port = mmio_read32((uint32_t)hprt);

    /* Check if device is connected */
    int connected = (port & DWCOTG_HPRT_PRTCONNSTS) != 0;

    if (connected) {
        Serial_WriteString("[DWCOTG] Device detected on port\n");

        /* Enable port power if not already enabled */
        if (!(port & DWCOTG_HPRT_PRTPWR)) {
            port |= DWCOTG_HPRT_PRTPWR;
            mmio_write32((uint32_t)hprt, port);
            Serial_WriteString("[DWCOTG] Port power enabled\n");

            /* Wait for power to stabilize */
            for (volatile int i = 0; i < 100000; i++) {
                __asm__ __volatile__("nop");
            }
        }

        /* Enable port if not already enabled */
        if (!(port & DWCOTG_HPRT_PRTENA)) {
            /* Perform port reset first */
            port |= DWCOTG_HPRT_PRTRST;
            mmio_write32((uint32_t)hprt, port);

            /* Wait for reset */
            for (volatile int i = 0; i < 100000; i++) {
                __asm__ __volatile__("nop");
            }

            /* Release reset */
            port &= ~DWCOTG_HPRT_PRTRST;
            mmio_write32((uint32_t)hprt, port);

            Serial_WriteString("[DWCOTG] Port reset complete\n");
        }

        return 1;
    }

    return 0;
}

/*
 * Wait for controller to be ready
 */
int dwcotg_wait_ready(void) {
    if (!dwcotg_base || !dwcotg_initialized) {
        return -1;
    }

    uint32_t timeout = 10000;
    while (timeout--) {
        /* Could check various status bits here */
        /* For now, just return success after initialization */
    }

    return 0;
}

/*
 * Enumerate USB devices
 */
int dwcotg_enumerate_devices(void) {
    Serial_WriteString("[DWCOTG] Enumerating USB devices...\n");

    if (!dwcotg_initialized) {
        Serial_WriteString("[DWCOTG] DWCOTG not initialized\n");
        return -1;
    }

    device_count = 0;

    /* Check if device is connected */
    if (dwcotg_port_connected()) {
        device_count = 1;
        Serial_WriteString("[DWCOTG] Found 1 device\n");

        /* TODO: Get device descriptor, determine if HID keyboard or mouse
         * For now, just report device found
         */
    } else {
        Serial_WriteString("[DWCOTG] No devices connected\n");
    }

    return 0;
}

/*
 * Find HID keyboard
 */
int dwcotg_find_keyboard(void *kb_info) {
    if (!kb_info) {
        return -1;
    }

    /* TODO: Return cached keyboard info if found during enumeration */
    return -1;  /* No keyboard found yet */
}

/*
 * Find HID mouse
 */
int dwcotg_find_mouse(void *mouse_info) {
    if (!mouse_info) {
        return -1;
    }

    /* TODO: Return cached mouse info if found during enumeration */
    return -1;  /* No mouse found yet */
}

/*
 * Poll keyboard
 */
int dwcotg_poll_keyboard(uint8_t *key_code, uint8_t *modifiers) {
    if (!key_code || !modifiers) {
        return -1;
    }

    /* TODO: Read from keyboard interrupt endpoint */
    return 0;  /* No data available yet */
}

/*
 * Poll mouse
 */
int dwcotg_poll_mouse(int8_t *dx, int8_t *dy, uint8_t *buttons) {
    if (!dx || !dy || !buttons) {
        return -1;
    }

    /* TODO: Read from mouse interrupt endpoint */
    return 0;  /* No data available yet */
}

/*
 * Get device count
 */
uint32_t dwcotg_device_count(void) {
    return device_count;
}

/*
 * Shutdown DWCOTG
 */
void dwcotg_shutdown(void) {
    Serial_WriteString("[DWCOTG] Shutting down DWCOTG controller\n");

    if (dwcotg_base) {
        /* Disable port power */
        volatile uint32_t *hprt = (volatile uint32_t *)(dwcotg_base + DWCOTG_HPRT);
        uint32_t port = mmio_read32((uint32_t)hprt);
        port &= ~DWCOTG_HPRT_PRTPWR;
        mmio_write32((uint32_t)hprt, port);
    }

    dwcotg_initialized = 0;
}
