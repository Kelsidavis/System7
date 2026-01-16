/*
 * ARM64 GIC (Generic Interrupt Controller) Driver
 * GICv2 implementation for Raspberry Pi 3/4/5
 */

#include <stdint.h>
#include <stdbool.h>
#include "mmio.h"

/* GIC Distributor (GICD) registers
 * Base address varies by model:
 *   Pi 3: 0x40041000
 *   Pi 4/5: 0xFF841000
 */

/* GIC CPU Interface (GICC) registers
 * Base address varies by model:
 *   Pi 3: 0x40042000
 *   Pi 4/5: 0xFF842000
 */

/* GICD register offsets */
#define GICD_CTLR           0x000  /* Distributor Control Register */
#define GICD_TYPER          0x004  /* Interrupt Controller Type Register */
#define GICD_ISENABLER      0x100  /* Interrupt Set-Enable Registers */
#define GICD_ICENABLER      0x180  /* Interrupt Clear-Enable Registers */
#define GICD_ISPENDR        0x200  /* Interrupt Set-Pending Registers */
#define GICD_ICPENDR        0x280  /* Interrupt Clear-Pending Registers */
#define GICD_IPRIORITYR     0x400  /* Interrupt Priority Registers */
#define GICD_ITARGETSR      0x800  /* Interrupt Processor Targets Registers */
#define GICD_ICFGR          0xC00  /* Interrupt Configuration Registers */

/* GICC register offsets */
#define GICC_CTLR           0x000  /* CPU Interface Control Register */
#define GICC_PMR            0x004  /* Interrupt Priority Mask Register */
#define GICC_IAR            0x00C  /* Interrupt Acknowledge Register */
#define GICC_EOIR           0x010  /* End of Interrupt Register */

/* Control register bits */
#define GICD_CTLR_ENABLE    (1 << 0)
#define GICC_CTLR_ENABLE    (1 << 0)

/* GIC base addresses */
static uint64_t gicd_base = 0;
static uint64_t gicc_base = 0;
static uint32_t gic_num_irqs = 0;
static bool gic_initialized = false;

/*
 * Detect GIC base addresses
 */
static bool gic_detect_base(void) {
    /* Try Raspberry Pi 4/5 addresses first */
    uint64_t test_gicd = 0xFF841000;
    uint64_t test_gicc = 0xFF842000;

    /* Read GICD_TYPER to check if GIC is present */
    uint32_t typer = mmio_read32(test_gicd + GICD_TYPER);

    /* ITLinesNumber field should be non-zero if GIC is present */
    if ((typer & 0x1F) != 0) {
        gicd_base = test_gicd;
        gicc_base = test_gicc;
        return true;
    }

    /* Try Raspberry Pi 3 addresses */
    test_gicd = 0x40041000;
    test_gicc = 0x40042000;

    typer = mmio_read32(test_gicd + GICD_TYPER);
    if ((typer & 0x1F) != 0) {
        gicd_base = test_gicd;
        gicc_base = test_gicc;
        return true;
    }

    return false;
}

/*
 * Initialize GIC
 */
bool gic_init(void) {
    if (gic_initialized) return true;

    /* Detect GIC base addresses */
    if (!gic_detect_base()) {
        return false;
    }

    /* Disable distributor during configuration */
    mmio_write32(gicd_base + GICD_CTLR, 0);

    /* Get number of interrupt lines */
    uint32_t typer = mmio_read32(gicd_base + GICD_TYPER);
    uint32_t num_lines = ((typer & 0x1F) + 1) * 32;
    gic_num_irqs = num_lines;

    /* Disable all interrupts */
    for (uint32_t i = 0; i < num_lines; i += 32) {
        mmio_write32(gicd_base + GICD_ICENABLER + (i / 8), 0xFFFFFFFF);
    }

    /* Clear all pending interrupts */
    for (uint32_t i = 0; i < num_lines; i += 32) {
        mmio_write32(gicd_base + GICD_ICPENDR + (i / 8), 0xFFFFFFFF);
    }

    /* Set all interrupts to lowest priority */
    for (uint32_t i = 0; i < num_lines; i += 4) {
        mmio_write32(gicd_base + GICD_IPRIORITYR + i, 0xA0A0A0A0);
    }

    /* Route all interrupts to CPU 0 */
    for (uint32_t i = 32; i < num_lines; i += 4) {
        mmio_write32(gicd_base + GICD_ITARGETSR + i, 0x01010101);
    }

    /* Enable distributor */
    mmio_write32(gicd_base + GICD_CTLR, GICD_CTLR_ENABLE);

    /* Set priority mask to lowest priority (accept all interrupts) */
    mmio_write32(gicc_base + GICC_PMR, 0xFF);

    /* Enable CPU interface */
    mmio_write32(gicc_base + GICC_CTLR, GICC_CTLR_ENABLE);

    gic_initialized = true;
    return true;
}

/*
 * Enable an interrupt
 */
void gic_enable_interrupt(uint32_t irq) {
    if (!gic_initialized) return;
    if (irq >= gic_num_irqs) return;

    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;

    mmio_write32(gicd_base + GICD_ISENABLER + (reg * 4), (1 << bit));
}

/*
 * Disable an interrupt
 */
void gic_disable_interrupt(uint32_t irq) {
    if (!gic_initialized) return;
    if (irq >= gic_num_irqs) return;

    uint32_t reg = irq / 32;
    uint32_t bit = irq % 32;

    mmio_write32(gicd_base + GICD_ICENABLER + (reg * 4), (1 << bit));
}

/*
 * Set interrupt priority (0 = highest, 255 = lowest)
 */
void gic_set_priority(uint32_t irq, uint8_t priority) {
    if (!gic_initialized) return;
    if (irq >= gic_num_irqs) return;

    uint32_t reg = irq / 4;
    uint32_t offset = (irq % 4) * 8;

    uint32_t value = mmio_read32(gicd_base + GICD_IPRIORITYR + (reg * 4));
    value &= ~(0xFF << offset);
    value |= (priority << offset);
    mmio_write32(gicd_base + GICD_IPRIORITYR + (reg * 4), value);
}

/*
 * Acknowledge interrupt and return IRQ number
 * Returns 1023 if no interrupt pending
 */
uint32_t gic_acknowledge_interrupt(void) {
    if (!gic_initialized) return 1023;

    uint32_t irq = mmio_read32(gicc_base + GICC_IAR) & 0x3FF;
    return irq;
}

/*
 * Signal end of interrupt
 */
void gic_end_interrupt(uint32_t irq) {
    if (!gic_initialized) return;
    if (irq >= gic_num_irqs && irq != 1023) return;  /* 1023 = spurious */

    mmio_write32(gicc_base + GICC_EOIR, irq);
}

/*
 * Set interrupt as edge-triggered (1) or level-sensitive (0)
 */
void gic_set_config(uint32_t irq, bool edge_triggered) {
    if (!gic_initialized) return;
    if (irq >= gic_num_irqs) return;

    uint32_t reg = irq / 16;
    uint32_t offset = (irq % 16) * 2;

    uint32_t value = mmio_read32(gicd_base + GICD_ICFGR + (reg * 4));

    if (edge_triggered) {
        value |= (2 << offset);  /* Set bit 1 for edge-triggered */
    } else {
        value &= ~(2 << offset); /* Clear bit 1 for level-sensitive */
    }

    mmio_write32(gicd_base + GICD_ICFGR + (reg * 4), value);
}

/*
 * Check if GIC is initialized
 */
bool gic_is_initialized(void) {
    return gic_initialized;
}
