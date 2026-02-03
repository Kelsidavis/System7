#ifndef PCI_IRQ_H
#define PCI_IRQ_H

#include <stdbool.h>
#include "pci.h"
#include "idt.h"

bool pci_irq_register_handler(const pci_device_t *dev, irq_handler_t handler);
uint8_t pci_irq_line(const pci_device_t *dev);

#endif /* PCI_IRQ_H */
