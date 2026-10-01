/*
 * network.c - ARM64 network HAL stub
 *
 * No network controller driver yet on ARM64; these satisfy the
 * platform_network_init/poll interface, as on 32-bit ARM.
 */

#include "Platform/include/network.h"

int platform_network_init(void) {
    return -1;
}

void platform_network_poll(void) {
}
