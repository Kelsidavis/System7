/* System 7X Nanokernel - /proc Virtual Filesystem
 *
 * Provides kernel information through filesystem interface.
 * Similar to Linux /proc but simplified for System 7X.
 */

#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/Nanokernel/filesystem.h"
#include "../../../include/Nanokernel/vfs_mount.h"
#include "../../../include/System71StdLib.h"
#include <string.h>

/* /proc entries */
typedef enum {
    PROC_VERSION,
    PROC_MOUNTS,
    PROC_MEMINFO,
    PROC_CPUINFO,
    PROC_UPTIME,
    PROC_UNKNOWN
} ProcEntry;

/* Identify /proc entry by name */
static ProcEntry procfs_identify(const char* name) {
    if (strcmp(name, "version") == 0) return PROC_VERSION;
    if (strcmp(name, "mounts") == 0) return PROC_MOUNTS;
    if (strcmp(name, "meminfo") == 0) return PROC_MEMINFO;
    if (strcmp(name, "cpuinfo") == 0) return PROC_CPUINFO;
    if (strcmp(name, "uptime") == 0) return PROC_UPTIME;
    return PROC_UNKNOWN;
}

/* Generate /proc file content */
static int procfs_read_entry(ProcEntry entry, char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return -1;
    }

    int len = 0;

    switch (entry) {
        case PROC_VERSION:
            len = snprintf(buffer, size,
                          "System 7X Nanokernel v1.0\n"
                          "Architecture: x86\n"
                          "Build: %s %s\n",
                          __DATE__, __TIME__);
            break;

        case PROC_MOUNTS:
            /* Use mount table listing */
            strncpy(buffer, "# Mount table\n", size);
            len = (int)strlen(buffer);
            /* TODO: Format mount table into buffer */
            break;

        case PROC_MEMINFO:
            len = snprintf(buffer, size,
                          "MemTotal: 32768 kB\n"
                          "MemFree: 16384 kB\n");
            break;

        case PROC_CPUINFO:
            len = snprintf(buffer, size,
                          "processor: 0\n"
                          "vendor_id: GenuineIntel\n"
                          "model name: System 7X CPU\n");
            break;

        case PROC_UPTIME:
            len = snprintf(buffer, size,
                          "0.00 0.00\n");
            break;

        default:
            return -1;
    }

    return len;
}

/* /proc lookup operation */
static bool procfs_lookup(VFSVolume* vol, uint64_t dir_id, const char* name,
                          uint64_t* entry_id, bool* is_dir) {
    (void)vol; (void)dir_id;

    ProcEntry entry = procfs_identify(name);
    if (entry == PROC_UNKNOWN) {
        return false;
    }

    if (entry_id) *entry_id = (uint64_t)entry;
    if (is_dir) *is_dir = false;

    return true;
}

/* /proc read operation */
static bool procfs_read(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                        void* buffer, size_t length, size_t* bytes_read) {
    (void)vol;

    /* Generate content */
    char temp_buffer[4096];
    int content_len = procfs_read_entry((ProcEntry)file_id, temp_buffer,
                                        sizeof(temp_buffer));

    if (content_len < 0) {
        return false;
    }

    /* Handle offset and length */
    if (offset >= (uint64_t)content_len) {
        if (bytes_read) *bytes_read = 0;
        return true;
    }

    size_t available = (size_t)(content_len - offset);
    size_t to_copy = (length < available) ? length : available;

    memcpy(buffer, temp_buffer + offset, to_copy);

    if (bytes_read) *bytes_read = to_copy;
    return true;
}

/* /proc filesystem operations */
static FileSystemOps g_procfs_ops = {
    .fs_name = "proc",
    .fs_version = 1,
    .probe = NULL,  /* Virtual filesystem, no probe */
    .mount = NULL,
    .unmount = NULL,
    .read = procfs_read,
    .write = NULL,  /* Read-only */
    .enumerate = NULL,  /* TODO: Enumerate proc entries */
    .lookup = procfs_lookup,
    .get_stats = NULL,
    .get_file_info = NULL,
    /* Optional operations */
    .format = NULL,
    .mkdir = NULL,
    .create_file = NULL,
    .delete = NULL,
    .rename = NULL
};

/* Mount /proc */
bool ProcFS_Mount(void) {
    serial_printf("[ProcFS] Mounting /proc virtual filesystem\n");

    /* Add to mount table */
    if (!VFSMount_Add("proc", "/proc", "proc", VFS_MOUNT_VIRTUAL | VFS_MOUNT_RDONLY,
                      &g_procfs_ops)) {
        serial_printf("[ProcFS] ERROR: Failed to mount\n");
        return false;
    }

    serial_printf("[ProcFS] Mounted successfully\n");
    return true;
}
