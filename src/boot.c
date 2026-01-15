#include "Platform/include/boot.h"
#include "SystemInternal.h"
#include "System71StdLib.h"
#include <stdint.h>

/* Forward declaration of kernel main */
extern void kernel_main(uint32_t magic, uint32_t* mb2_info);

void boot_main(uint32_t magic, uint32_t* mb2_info) {
    /* Use serial_puts directly to bypass logging system during early boot */
    extern void serial_puts(const char* str);
#if defined(__aarch64__)
    extern void uart_puts(const char *s);
#endif

    /* Ensure PowerPC serial hardware is configured even if OF console is absent. */
    serial_init();

    serial_puts("BOOT:M\n");  /* Entry marker */

    hal_boot_init(mb2_info);

    serial_puts("BOOT:H\n");  /* After hal_boot_init */

    serial_puts("BOOT\n");
    serial_puts("BOOT:K\n");  /* Before kernel_main */

    /* Call the kernel main function */
    kernel_main(magic, mb2_info);

    serial_puts("BOOT:EXIT\n");  /* If kernel_main returns */
}
