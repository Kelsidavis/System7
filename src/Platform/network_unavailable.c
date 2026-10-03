/* Shared fallback for platforms without a network controller driver. */
#include "Platform/include/network.h"

int platform_network_init(void) {
    return -1;
}

void platform_network_poll(void) {
}
