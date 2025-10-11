/* System 7X Nanokernel - POSIX Filesystem Syscalls Implementation
 *
 * POSIX-compatible syscalls bridging to VFS daemon infrastructure.
 */

#include "../../../include/Nanokernel/syscalls.h"
#include "../../../include/Nanokernel/fd_table.h"
#include "../../../include/Nanokernel/vfs_path.h"
#include "../../../include/Nanokernel/vfs.h"
#include "../../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

/* Global errno */
int errno = 0;

/* Get errno location */
int* __errno_location(void) {
    return &errno;
}

/* Initialize syscall subsystem */
void syscalls_init(void) {
    serial_printf("[SYSCALL] POSIX syscall subsystem initialized\n");
    fd_table_init();
    vfs_path_init();
}

/* sys_open - Open a file */
int sys_open(const char* path, int flags, mode_t mode) {
    (void)mode;  /* Unused for now */

    if (!path) {
        errno = EINVAL;
        return -1;
    }

    serial_printf("[SYSCALL] open('%s', 0x%x)\n", path, flags);

    /* Resolve path to volume + inode */
    ResolvedPath resolved;
    if (!vfs_resolve_path(path, &resolved)) {
        serial_printf("[SYSCALL] open: failed to resolve path '%s'\n", path);
        errno = ENOENT;
        return -1;
    }

    if (!resolved.volume) {
        errno = ENOENT;
        return -1;
    }

    /* Allocate file descriptor */
    int fd = fd_alloc(resolved.volume, resolved.inode, flags, path);
    if (fd < 0) {
        errno = EMFILE;
        return -1;
    }

    serial_printf("[SYSCALL] open: fd=%d, volume='%s', inode=%llu\n",
                  fd, resolved.volume->name, resolved.inode);

    errno = 0;
    return fd;
}

/* sys_read - Read from file descriptor */
ssize_t sys_read(int fd, void* buf, size_t count) {
    if (!buf) {
        errno = EINVAL;
        return -1;
    }

    FileDescriptor* entry = fd_get(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    /* Check if open for reading */
    if ((entry->flags & O_ACCMODE) == O_WRONLY) {
        errno = EBADF;
        return -1;
    }

    serial_printf("[SYSCALL] read(fd=%d, count=%zu) at pos=%llu\n",
                  fd, count, entry->position);

    /* Call VFS read */
    extern bool MVFS_ReadFile(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                              void* buffer, size_t length, size_t* bytes_read);

    size_t bytes_read = 0;
    bool success = MVFS_ReadFile(entry->volume, entry->inode, entry->position,
                                  buf, count, &bytes_read);

    if (!success) {
        serial_printf("[SYSCALL] read: failed\n");
        errno = EIO;
        return -1;
    }

    /* Update position */
    entry->position += bytes_read;

    serial_printf("[SYSCALL] read: %zd bytes OK\n", bytes_read);

    errno = 0;
    return (ssize_t)bytes_read;
}

/* sys_write - Write to file descriptor */
ssize_t sys_write(int fd, const void* buf, size_t count) {
    if (!buf) {
        errno = EINVAL;
        return -1;
    }

    FileDescriptor* entry = fd_get(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    /* Check if open for writing */
    if ((entry->flags & O_ACCMODE) == O_RDONLY) {
        errno = EBADF;
        return -1;
    }

    serial_printf("[SYSCALL] write(fd=%d, count=%zu) at pos=%llu\n",
                  fd, count, entry->position);

    /* Call VFS write */
    extern bool MVFS_WriteFile(VFSVolume* vol, uint64_t file_id, uint64_t offset,
                               const void* buffer, size_t length, size_t* bytes_written);

    size_t bytes_written = 0;
    bool success = MVFS_WriteFile(entry->volume, entry->inode, entry->position,
                                   buf, count, &bytes_written);

    if (!success) {
        serial_printf("[SYSCALL] write: failed\n");
        errno = EIO;
        return -1;
    }

    /* Update position */
    entry->position += bytes_written;

    serial_printf("[SYSCALL] write: %zd bytes OK\n", bytes_written);

    errno = 0;
    return (ssize_t)bytes_written;
}

/* sys_lseek - Reposition file offset */
off_t sys_lseek(int fd, off_t offset, int whence) {
    FileDescriptor* entry = fd_get(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    off_t new_pos;

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;

        case SEEK_CUR:
            new_pos = entry->position + offset;
            break;

        case SEEK_END:
            /* TODO: Get file size from VFS */
            serial_printf("[SYSCALL] lseek: SEEK_END not yet implemented\n");
            errno = EINVAL;
            return -1;

        default:
            errno = EINVAL;
            return -1;
    }

    if (new_pos < 0) {
        errno = EINVAL;
        return -1;
    }

    entry->position = (uint64_t)new_pos;

    serial_printf("[SYSCALL] lseek(fd=%d, offset=%ld, whence=%d) → %llu\n",
                  fd, offset, whence, entry->position);

    errno = 0;
    return (off_t)entry->position;
}

/* sys_close - Close file descriptor */
int sys_close(int fd) {
    serial_printf("[SYSCALL] close(fd=%d)\n", fd);

    if (!fd_free(fd)) {
        errno = EBADF;
        return -1;
    }

    errno = 0;
    return 0;
}

/* Directory handle */
struct DIR {
    VFSVolume* volume;
    uint64_t dir_inode;
    uint64_t position;
    bool valid;
};

/* sys_opendir - Open directory */
DIR* sys_opendir(const char* path) {
    if (!path) {
        errno = EINVAL;
        return NULL;
    }

    serial_printf("[SYSCALL] opendir('%s')\n", path);

    /* Resolve path */
    ResolvedPath resolved;
    if (!vfs_resolve_path(path, &resolved)) {
        errno = ENOENT;
        return NULL;
    }

    if (!resolved.is_directory) {
        errno = ENOTDIR;
        return NULL;
    }

    /* Allocate DIR structure */
    DIR* dir = (DIR*)malloc(sizeof(DIR));
    if (!dir) {
        errno = ENOMEM;
        return NULL;
    }

    dir->volume = resolved.volume;
    dir->dir_inode = resolved.inode;
    dir->position = 0;
    dir->valid = true;

    serial_printf("[SYSCALL] opendir: volume='%s', inode=%llu\n",
                  dir->volume->name, dir->dir_inode);

    errno = 0;
    return dir;
}

/* sys_readdir - Read directory entry */
struct dirent* sys_readdir(DIR* dir) {
    if (!dir || !dir->valid) {
        errno = EBADF;
        return NULL;
    }

    /* TODO: Implement directory enumeration via VFS */
    serial_printf("[SYSCALL] readdir: not yet fully implemented\n");

    errno = 0;
    return NULL;  /* End of directory */
}

/* sys_closedir - Close directory */
int sys_closedir(DIR* dir) {
    if (!dir) {
        errno = EINVAL;
        return -1;
    }

    serial_printf("[SYSCALL] closedir\n");

    dir->valid = false;
    free(dir);

    errno = 0;
    return 0;
}

/* sys_stat - Get file status */
int sys_stat(const char* path, struct stat* buf) {
    if (!path || !buf) {
        errno = EINVAL;
        return -1;
    }

    serial_printf("[SYSCALL] stat('%s')\n", path);

    /* Resolve path */
    ResolvedPath resolved;
    if (!vfs_resolve_path(path, &resolved)) {
        errno = ENOENT;
        return -1;
    }

    /* Fill stat structure */
    memset(buf, 0, sizeof(struct stat));
    buf->st_ino = resolved.inode;
    buf->st_mode = resolved.is_directory ? 0040755 : 0100644;  /* S_IFDIR or S_IFREG */
    buf->st_nlink = 1;
    buf->st_size = 0;  /* TODO: Get actual size from VFS */

    errno = 0;
    return 0;
}

/* sys_fstat - Get file status by descriptor */
int sys_fstat(int fd, struct stat* buf) {
    if (!buf) {
        errno = EINVAL;
        return -1;
    }

    FileDescriptor* entry = fd_get(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    serial_printf("[SYSCALL] fstat(fd=%d)\n", fd);

    /* Fill stat structure */
    memset(buf, 0, sizeof(struct stat));
    buf->st_ino = entry->inode;
    buf->st_mode = 0100644;  /* Regular file */
    buf->st_nlink = 1;
    buf->st_size = 0;  /* TODO: Get actual size from VFS */

    errno = 0;
    return 0;
}

/* sys_dup - Duplicate file descriptor */
int sys_dup(int oldfd) {
    int newfd = fd_dup(oldfd);
    if (newfd < 0) {
        errno = EBADF;
        return -1;
    }

    errno = 0;
    return newfd;
}

/* sys_dup2 - Duplicate to specific fd */
int sys_dup2(int oldfd, int newfd) {
    int result = fd_dup2(oldfd, newfd);
    if (result < 0) {
        errno = EBADF;
        return -1;
    }

    errno = 0;
    return result;
}
