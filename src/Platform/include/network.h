#ifndef HAL_NETWORK_H
#define HAL_NETWORK_H

#include <stdint.h>

typedef struct {
    uint8_t mac[6];
    uint32_t ip;
} netif_t;

int platform_network_init(void);
void platform_network_poll(void);

#endif /* HAL_NETWORK_H */
