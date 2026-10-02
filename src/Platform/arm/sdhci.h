#ifndef ARM_SDHCI_H
#define ARM_SDHCI_H

#include <stdint.h>

int sdhci_init(void);
int sdhci_read_blocks(uint32_t address, uint32_t count, void *buffer);
int sdhci_write_blocks(uint32_t address, uint32_t count, const void *buffer);
int sdhci_get_card_info(uint32_t *block_count);
int sdhci_card_ready(void);
void sdhci_shutdown(void);
int sdhci_card_present(void);

int sdhci_send_command(uint32_t base, uint8_t command, uint32_t argument,
                       uint8_t response_type, uint32_t *response);
int sdhci_init_card(uint32_t base);
int sdhci_read_block(uint32_t base, uint32_t address, void *buffer);
int sdhci_write_block(uint32_t base, uint32_t address, const void *buffer);

#endif /* ARM_SDHCI_H */
