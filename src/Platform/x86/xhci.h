#ifndef X86_XHCI_H
#define X86_XHCI_H

#include <stdbool.h>
#include "SystemTypes.h"

bool xhci_init_x86(void);
void xhci_poll_hid_x86(void);
bool xhci_msc_available(void);
OSErr xhci_msc_get_info(uint32_t *block_size, uint64_t *block_count, bool *read_only);
OSErr xhci_msc_read_blocks(uint64_t start_block, uint32_t block_count, void *buffer);
OSErr xhci_msc_write_blocks(uint64_t start_block, uint32_t block_count, const void *buffer);

#endif /* X86_XHCI_H */
