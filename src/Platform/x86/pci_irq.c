/*
 * pci_irq.c - minimal PCI IRQ helper
 */

#include "pci_irq.h"
#include "pic.h"

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint8_t pin;
    uint8_t line;
} pci_irq_entry_t;

static pci_irq_entry_t g_irq_table[32];
static int g_irq_table_count = 0;

static void pci_irq_record(const pci_device_t *dev) {
    if (!dev || g_irq_table_count >= (int)(sizeof(g_irq_table) / sizeof(g_irq_table[0]))) {
        return;
    }
    g_irq_table[g_irq_table_count].bus = dev->bus;
    g_irq_table[g_irq_table_count].slot = dev->slot;
    g_irq_table[g_irq_table_count].func = dev->func;
    g_irq_table[g_irq_table_count].pin = dev->irq_pin;
    g_irq_table[g_irq_table_count].line = dev->irq_line;
    g_irq_table_count++;
}

uint8_t pci_irq_line(const pci_device_t *dev) {
    if (!dev) {
        return 0xFF;
    }
    return dev->irq_line;
}

bool pci_irq_register_handler(const pci_device_t *dev, irq_handler_t handler) {
    if (!dev || !handler) {
        return false;
    }
    if (dev->irq_line == 0 || dev->irq_line == 0xFF) {
        return false;
    }
    pci_irq_record(dev);
    irq_register_handler(dev->irq_line, handler);
    pic_unmask_irq(dev->irq_line);
    return true;
}
