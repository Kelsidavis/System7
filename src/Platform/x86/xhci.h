#ifndef X86_XHCI_H
#define X86_XHCI_H

#include <stdbool.h>

bool xhci_init_x86(void);
void xhci_poll_hid_x86(void);

#endif /* X86_XHCI_H */
