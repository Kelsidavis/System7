/* System 7X Nanokernel - File Descriptor Table Implementation
 *
 * POSIX-style file descriptor management.
 */

#include "../../include/Nanokernel/fd_table.h"
#include "../../include/Nanokernel/vfs.h"
#include "../../include/System71StdLib.h"
#include <string.h>

/* Global file descriptor table */
static FDTable g_fd_table = { 0 };

/* Initialize file descriptor table */
void fd_table_init(void) {
    if (g_fd_table.initialized) {
        serial_printf("[FD] File descriptor table already initialized\n");
        return;
    }

    memset(&g_fd_table, 0, sizeof(FDTable));

    /* Reserve stdin, stdout, stderr */
    g_fd_table.entries[FD_STDIN].in_use = true;
    g_fd_table.entries[FD_STDIN].flags = O_RDONLY;
    strncpy(g_fd_table.entries[FD_STDIN].path, "/dev/stdin", sizeof(g_fd_table.entries[FD_STDIN].path) - 1);

    g_fd_table.entries[FD_STDOUT].in_use = true;
    g_fd_table.entries[FD_STDOUT].flags = O_WRONLY;
    strncpy(g_fd_table.entries[FD_STDOUT].path, "/dev/stdout", sizeof(g_fd_table.entries[FD_STDOUT].path) - 1);

    g_fd_table.entries[FD_STDERR].in_use = true;
    g_fd_table.entries[FD_STDERR].flags = O_WRONLY;
    strncpy(g_fd_table.entries[FD_STDERR].path, "/dev/stderr", sizeof(g_fd_table.entries[FD_STDERR].path) - 1);

    g_fd_table.initialized = true;

    serial_printf("[FD] File descriptor table initialized (%d max FDs)\n", FD_TABLE_MAX);
}

/* Allocate a new file descriptor */
int fd_alloc(VFSVolume* vol, uint64_t inode, int flags, const char* path) {
    if (!g_fd_table.initialized) {
        serial_printf("[FD] ERROR: FD table not initialized\n");
        return -1;
    }

    /* Find free slot (start after stderr) */
    for (int fd = 3; fd < FD_TABLE_MAX; fd++) {
        if (!g_fd_table.entries[fd].in_use) {
            FileDescriptor* entry = &g_fd_table.entries[fd];

            /* Initialize entry */
            memset(entry, 0, sizeof(FileDescriptor));
            entry->in_use = true;
            entry->flags = flags;
            entry->volume = vol;
            entry->inode = inode;
            entry->position = 0;

            if (path) {
                strncpy(entry->path, path, sizeof(entry->path) - 1);
            }

            serial_printf("[FD] Allocated fd=%d for '%s' (inode=%llu)\n", fd, path ? path : "<none>", inode);
            return fd;
        }
    }

    serial_printf("[FD] ERROR: No free file descriptors\n");
    return -1;
}

/* Get file descriptor entry */
FileDescriptor* fd_get(int fd) {
    if (fd < 0 || fd >= FD_TABLE_MAX) {
        return NULL;
    }

    if (!g_fd_table.entries[fd].in_use) {
        return NULL;
    }

    return &g_fd_table.entries[fd];
}

/* Free a file descriptor */
bool fd_free(int fd) {
    if (fd < 0 || fd >= FD_TABLE_MAX) {
        return false;
    }

    if (!g_fd_table.entries[fd].in_use) {
        return false;
    }

    /* Don't allow closing stdin/stdout/stderr */
    if (fd <= FD_STDERR) {
        serial_printf("[FD] WARNING: Cannot close standard fd=%d\n", fd);
        return false;
    }

    serial_printf("[FD] Freeing fd=%d ('%s')\n", fd, g_fd_table.entries[fd].path);

    memset(&g_fd_table.entries[fd], 0, sizeof(FileDescriptor));
    return true;
}

/* Duplicate a file descriptor */
int fd_dup(int oldfd) {
    FileDescriptor* old = fd_get(oldfd);
    if (!old) {
        return -1;
    }

    /* Allocate new FD with same properties */
    int newfd = fd_alloc(old->volume, old->inode, old->flags, old->path);
    if (newfd < 0) {
        return -1;
    }

    /* Copy position */
    g_fd_table.entries[newfd].position = old->position;

    serial_printf("[FD] Duplicated fd=%d → fd=%d\n", oldfd, newfd);
    return newfd;
}

/* Duplicate to specific fd */
int fd_dup2(int oldfd, int newfd) {
    if (newfd < 0 || newfd >= FD_TABLE_MAX) {
        return -1;
    }

    FileDescriptor* old = fd_get(oldfd);
    if (!old) {
        return -1;
    }

    /* If newfd is open, close it first */
    if (g_fd_table.entries[newfd].in_use) {
        fd_free(newfd);
    }

    /* Copy descriptor */
    g_fd_table.entries[newfd] = *old;
    g_fd_table.entries[newfd].in_use = true;

    serial_printf("[FD] Duplicated fd=%d → fd=%d\n", oldfd, newfd);
    return newfd;
}

/* Check if fd is valid */
bool fd_is_valid(int fd) {
    if (fd < 0 || fd >= FD_TABLE_MAX) {
        return false;
    }
    return g_fd_table.entries[fd].in_use;
}

/* Get current position */
uint64_t fd_tell(int fd) {
    FileDescriptor* entry = fd_get(fd);
    if (!entry) {
        return (uint64_t)-1;
    }
    return entry->position;
}

/* Set file descriptor flags */
bool fd_set_flags(int fd, int flags) {
    FileDescriptor* entry = fd_get(fd);
    if (!entry) {
        return false;
    }

    entry->fd_flags = flags;
    return true;
}

/* Get file descriptor flags */
int fd_get_flags(int fd) {
    FileDescriptor* entry = fd_get(fd);
    if (!entry) {
        return -1;
    }
    return entry->fd_flags;
}

/* List all open file descriptors */
void fd_list(void) {
    serial_printf("[FD] === Open File Descriptors ===\n");

    int count = 0;
    for (int fd = 0; fd < FD_TABLE_MAX; fd++) {
        if (g_fd_table.entries[fd].in_use) {
            FileDescriptor* entry = &g_fd_table.entries[fd];
            const char* mode_str = "?";

            if (entry->flags == O_RDONLY) mode_str = "r";
            else if (entry->flags == O_WRONLY) mode_str = "w";
            else if (entry->flags == O_RDWR) mode_str = "rw";

            serial_printf("[FD] fd=%d: '%s' (inode=%llu, pos=%llu, mode=%s)\n",
                          fd, entry->path, entry->inode, entry->position, mode_str);
            count++;
        }
    }

    serial_printf("[FD] Total: %d open file descriptors\n", count);
}
