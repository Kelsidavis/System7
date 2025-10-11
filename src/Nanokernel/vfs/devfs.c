/* System 7X Nanokernel - /dev Virtual Filesystem
 *
 * Device node filesystem for character and block devices.
 * Provides /dev/null, /dev/zero, /dev/random, /dev/console, etc.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/Nanokernel/vfs_mount.h"
#include "../../../include/System71StdLib.h"
#include <string.h>

/* /dev device types */
typedef enum {
    DEV_NULL = 1,
    DEV_ZERO,
    DEV_RANDOM,
    DEV_CONSOLE,
    DEV_TTY,
    DEV_UNKNOWN
} DevEntry;

/* Identify /dev entry by name */
static DevEntry devfs_identify(const char* name) {
    if (strcmp(name, "null") == 0) return DEV_NULL;
    if (strcmp(name, "zero") == 0) return DEV_ZERO;
    if (strcmp(name, "random") == 0) return DEV_RANDOM;
    if (strcmp(name, "console") == 0) return DEV_CONSOLE;
    if (strcmp(name, "tty") == 0) return DEV_TTY;
    return DEV_UNKNOWN;
}

/* /dev lookup operation */
static bool devfs_lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                         uint64_t* entry_id, bool* is_dir) {
    (void)vol; (void)dir_id;

    DevEntry entry = devfs_identify(name);
    if (entry == DEV_UNKNOWN) {
        return false;
    }

    if (entry_id) *entry_id = (uint64_t)entry;
    if (is_dir) *is_dir = false;

    return true;
}

/* /dev read operation */
static bool devfs_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                       void* buffer, size_t length, size_t* bytes_read) {
    (void)vol; (void)offset;

    DevEntry dev = (DevEntry)file_id;

    switch (dev) {
        case DEV_NULL:
            /* Read from /dev/null returns EOF */
            if (bytes_read) *bytes_read = 0;
            return true;

        case DEV_ZERO:
            /* Read from /dev/zero returns zeros */
            memset(buffer, 0, length);
            if (bytes_read) *bytes_read = length;
            return true;

        case DEV_RANDOM:
            /* Read from /dev/random returns pseudo-random data */
            /* Simple PRNG for demonstration */
            {
                unsigned char* buf = (unsigned char*)buffer;
                static uint32_t seed = 0x12345678;
                for (size_t i = 0; i < length; i++) {
                    seed = seed * 1103515245 + 12345;
                    buf[i] = (unsigned char)(seed >> 16);
                }
            }
            if (bytes_read) *bytes_read = length;
            return true;

        case DEV_CONSOLE:
        case DEV_TTY:
            /* Read from console - would read from keyboard/serial */
            if (bytes_read) *bytes_read = 0;  /* No input for now */
            return true;

        default:
            return false;
    }
}

/* /dev write operation */
static bool devfs_write(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                        const void* buffer, size_t length, size_t* bytes_written) {
    (void)vol; (void)offset;

    DevEntry dev = (DevEntry)file_id;

    switch (dev) {
        case DEV_NULL:
            /* Write to /dev/null discards data */
            if (bytes_written) *bytes_written = length;
            return true;

        case DEV_CONSOLE:
        case DEV_TTY:
            /* Write to console - output to serial */
            for (size_t i = 0; i < length; i++) {
                extern void serial_putchar(char c);
                serial_putchar(((const char*)buffer)[i]);
            }
            if (bytes_written) *bytes_written = length;
            return true;

        case DEV_ZERO:
        case DEV_RANDOM:
            /* Cannot write to /dev/zero or /dev/random */
            return false;

        default:
            return false;
    }
}

/* /dev filesystem operations */
static FileSystemOps g_devfs_ops = {
    .fs_name = "devfs",
    .fs_version = 1,
    .probe = NULL,  /* Virtual filesystem, no probe */
    .mount = NULL,
    .unmount = NULL,
    .read = devfs_read,
    .write = devfs_write,
    .enumerate = NULL,  /* TODO: Enumerate devices */
    .lookup = devfs_lookup,
    .get_stats = NULL,
    .get_file_info = NULL,
    /* Optional operations */
    .format = NULL,
    .mkdir = NULL,
    .create_file = NULL,
    .delete = NULL,
    .rename = NULL
};

/* Mount /dev */
bool DevFS_Mount(void) {
    serial_printf("[DevFS] Mounting /dev virtual filesystem\n");

    /* Add to mount table */
    if (!VFSMount_Add("devfs", "/dev", "devfs", VFS_MOUNT_VIRTUAL,
                      &g_devfs_ops)) {
        serial_printf("[DevFS] ERROR: Failed to mount\n");
        return false;
    }

    serial_printf("[DevFS] Mounted successfully\n");
    return true;
}
