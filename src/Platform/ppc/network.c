/*
 * network.c - PowerPC network HAL stub
 *
 * PowerPC platforms do not currently have a network controller driver.
 * These stubs satisfy the platform_network_init/poll interface.
 */

#include "Platform/include/network.h"

int platform_network_init(void) {
    return -1;
}

void platform_network_poll(void) {
}
