#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include "pci.h"

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E

#define RX_RING_SIZE 16
#define TX_RING_SIZE 16
#define ETH_FRAME_SIZE 1518

typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uint32_t mmio_base;
    uint8_t  mac[6];
    uint32_t ip;
} e1000_t;

int  e1000_init(e1000_t* dev, pci_device_t* pci_dev);
void e1000_poll(e1000_t* dev);

#endif /* E1000_H */
