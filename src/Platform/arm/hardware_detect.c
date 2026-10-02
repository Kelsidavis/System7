/*
 * Raspberry Pi Hardware Detection
 * Detects Pi model (3/4/5) using multiple methods
 *
 * Detection Order:
 * 1. Device Tree Model String
 * 2. Hardware Revision Register
 * 3. USB Controller Heuristic
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "System71StdLib.h"
#include "hardware_detect.h"
#include "mmio.h"
#include "device_tree.h"

/* Hardware revision register address (Pi 3/4/5 compatible) */
#define HW_REVISION_REG     0x3F00001C

/* Cached detection results */
static rpi_model_t detected_model = PI_MODEL_UNKNOWN;
static char detected_model_string[256] = {0};
static int detection_done = 0;

/*
 * Parse hardware model from revision register
 * Returns pi_model_t based on revision code
 */
static rpi_model_t parse_hw_revision(uint32_t revision) {
    /* Raspberry Pi hardware revision format:
     * Bits 23-20: Type (0=A, 1=B, 2=A+, 3=B+, 4=2B, 5=Alpha, 6=CM1, 8=3B, 9=Zero, 10=CM3, etc.)
     * Bits 19-16: Processor (0=BCM2835, 1=BCM2836, 2=BCM2837, 3=BCM2711, 4=BCM2837B0)
     * Bits 15-12: Manufacturer (0=Sony, 1=Egoman, 2=Embest, 3=Sony, 4=Embest)
     * Bits 11-4:  Memory size (0=256M, 1=512M, 2=1G, 3=2G, 4=4G, 5=8G)
     * Bits 3-0:   Revision (0-15)
     */

    uint8_t processor = (revision >> 16) & 0xF;
    uint8_t model_type = (revision >> 20) & 0xF;

    /* Match by processor type */
    switch (processor) {
        case 0:
        case 1:
            /* BCM2835/BCM2836 (Pi 1, Pi Zero, Pi 2) */
            return PI_MODEL_UNKNOWN;

        case 2:
            /* BCM2837 (Pi 3) */
            return PI_MODEL_3;

        case 3:
            /* BCM2711 (Pi 4) */
            return PI_MODEL_4;

        case 4:
            /* BCM2837B0 or BCM2837 with later revision */
            /* Check model type to distinguish Pi 3 from Pi 3+ */
            if (model_type >= 0xE) {
                return PI_MODEL_3;
            }
            return PI_MODEL_3;

        default:
            /* Unknown - assume newer Pi 5 or later */
            return PI_MODEL_5;
    }
}

/*
 * Detect Raspberry Pi model
 * Returns pi_model_t and optionally fills model_string
 */
rpi_model_t hardware_detect_model(char *model_string, uint32_t string_len) {
    if (detection_done) {
        if (model_string && string_len > 0) {
            strncpy(model_string, detected_model_string, string_len - 1);
            model_string[string_len - 1] = '\0';
        }
        return detected_model;
    }

    detected_model = PI_MODEL_UNKNOWN;
    memset(detected_model_string, 0, sizeof(detected_model_string));

    /* Method 1: Try Device Tree model string */
    const char *dtb_model = device_tree_get_model();
    if (dtb_model && dtb_model[0] != '\0') {
        strncpy(detected_model_string, dtb_model, sizeof(detected_model_string) - 1);

        /* Parse model string to determine type */
        if (strstr(dtb_model, "virt")) {
            detected_model = PI_MODEL_UNKNOWN;
            Serial_Printf("[HW] Detected QEMU virt platform via DTB: %s\n", dtb_model);
            goto detection_complete;
        } else if (strstr(dtb_model, "Pi 3") || strstr(dtb_model, "3B") || strstr(dtb_model, "3A")) {
            detected_model = PI_MODEL_3;
            Serial_Printf("[HW] Detected via DTB: %s (Pi 3)\n", dtb_model);
            goto detection_complete;
        } else if (strstr(dtb_model, "Pi 4") || strstr(dtb_model, "4B")) {
            detected_model = PI_MODEL_4;
            Serial_Printf("[HW] Detected via DTB: %s (Pi 4)\n", dtb_model);
            goto detection_complete;
        } else if (strstr(dtb_model, "Pi 5") || strstr(dtb_model, "5B")) {
            detected_model = PI_MODEL_5;
            Serial_Printf("[HW] Detected via DTB: %s (Pi 5)\n", dtb_model);
            goto detection_complete;
        }
    }

    /* Method 2: Try Hardware Revision Register */
    if (detected_model == PI_MODEL_UNKNOWN) {
        uint32_t hw_revision = 0;

        /* Read hardware revision register
         * Note: May not work on all models, but works on Pi 3/4
         * ARM32-specific instruction, skip on ARM64
         */
#if !defined(__aarch64__)
        __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 5" : "=r"(hw_revision));
#endif

        if (hw_revision != 0 && hw_revision != 0xFFFFFFFF) {
            rpi_model_t model = parse_hw_revision(hw_revision);
            if (model != PI_MODEL_UNKNOWN) {
                detected_model = model;

                if (model == PI_MODEL_3) {
                    strncpy(detected_model_string, "Raspberry Pi 3 (via HW revision)",
                            sizeof(detected_model_string) - 1);
                    Serial_Printf("[HW] Detected via HW revision: Pi 3 (rev=0x%08lx)\n",
                                  (unsigned long)hw_revision);
                } else if (model == PI_MODEL_4) {
                    strncpy(detected_model_string, "Raspberry Pi 4 (via HW revision)",
                            sizeof(detected_model_string) - 1);
                    Serial_Printf("[HW] Detected via HW revision: Pi 4 (rev=0x%08lx)\n",
                                  (unsigned long)hw_revision);
                }

                goto detection_complete;
            }
        }
    }

    /* Method 3: Heuristic - Check for USB controller presence */
    if (detected_model == PI_MODEL_UNKNOWN) {
        /* Try reading DWCOTG registers (Pi 3) */
        volatile uint32_t *dwcotg_hwcfg = (volatile uint32_t *)0x20980000;
        uint32_t dwcotg_val = *dwcotg_hwcfg;

        if (dwcotg_val != 0 && dwcotg_val != 0xFFFFFFFF) {
            detected_model = PI_MODEL_3;
            strncpy(detected_model_string, "Raspberry Pi 3 (via DWCOTG detection)",
                    sizeof(detected_model_string) - 1);
            Serial_WriteString("[HW] Detected via DWCOTG: Pi 3\n");
            goto detection_complete;
        }

        /* Default to Pi 4 if unclear */
        detected_model = PI_MODEL_4;
        strncpy(detected_model_string, "Raspberry Pi 4 or 5 (default)",
                sizeof(detected_model_string) - 1);
        Serial_WriteString("[HW] Defaulting to Pi 4/5 (XHCI expected)\n");
    }

detection_complete:
    detection_done = 1;

    if (model_string && string_len > 0) {
        strncpy(model_string, detected_model_string, string_len - 1);
        model_string[string_len - 1] = '\0';
    }

    return detected_model;
}

/*
 * Get cached model
 */
rpi_model_t hardware_get_model(void) {
    if (!detection_done) {
        hardware_detect_model(NULL, 0);
    }
    return detected_model;
}

/*
 * Get cached model string
 */
const char* hardware_get_model_string(void) {
    if (!detection_done) {
        hardware_detect_model(NULL, 0);
    }
    return detected_model_string[0] != '\0' ? detected_model_string : NULL;
}

/*
 * Report hardware info for debugging
 */
void hardware_report_info(void) {
    Serial_WriteString("════════════════════════════════════════════════════════════\n");
    Serial_WriteString("[HW] Hardware Detection Report\n");
    Serial_WriteString("════════════════════════════════════════════════════════════\n");

    rpi_model_t model = hardware_get_model();
    const char *model_str = hardware_get_model_string();

    if (model_str) {
        Serial_Printf("[HW] Model: %s\n", model_str);
    }

    switch (model) {
        case PI_MODEL_3:
            Serial_WriteString("[HW] Configuration: Raspberry Pi 3\n");
            Serial_WriteString("[HW]   CPU: ARM Cortex-A53 (4 cores @ 1.2 GHz)\n");
            Serial_WriteString("[HW]   RAM: 1GB LPDDR2\n");
            Serial_WriteString("[HW]   USB: DWCOTG (USB 2.0)\n");
            Serial_WriteString("[HW]   Timer: 19.2 MHz\n");
            break;

        case PI_MODEL_4:
            Serial_WriteString("[HW] Configuration: Raspberry Pi 4\n");
            Serial_WriteString("[HW]   CPU: ARM Cortex-A72 (4 cores @ 1.5 GHz)\n");
            Serial_WriteString("[HW]   RAM: 1-8GB LPDDR4\n");
            Serial_WriteString("[HW]   USB: XHCI (USB 3.0)\n");
            Serial_WriteString("[HW]   Timer: 54 MHz\n");
            break;

        case PI_MODEL_5:
            Serial_WriteString("[HW] Configuration: Raspberry Pi 5\n");
            Serial_WriteString("[HW]   CPU: ARM Cortex-A76 (4 cores @ 2.4 GHz)\n");
            Serial_WriteString("[HW]   RAM: 4-8GB LPDDR5\n");
            Serial_WriteString("[HW]   USB: XHCI (USB 3.0)\n");
            Serial_WriteString("[HW]   Timer: 54 MHz\n");
            break;

        default:
            Serial_WriteString("[HW] Configuration: Unknown model\n");
            break;
    }

    Serial_WriteString("════════════════════════════════════════════════════════════\n");
}
