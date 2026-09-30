/*
 * ARM64 GIC (Generic Interrupt Controller) Interface
 * GICv2 for Raspberry Pi 3/4/5
 */

#ifndef ARM64_GIC_H
#define ARM64_GIC_H

#include <stdint.h>
#include <stdbool.h>

/* Common ARM interrupt numbers */
#define IRQ_TIMER_PHYS      30  /* Physical timer interrupt (PPI) */
#define IRQ_TIMER_VIRT      27  /* Virtual timer interrupt (PPI) */

/* Initialize GIC */
bool gic_init(void);


#endif /* ARM64_GIC_H */
