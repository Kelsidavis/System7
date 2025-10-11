/* System 7X Nanokernel - File Descriptor Table
 *
 * POSIX-style file descriptor management for VFS syscalls.
 * Maps integer file descriptors to VFS volumes and file positions.
 */

#ifndef NANOKERNEL_FD_TABLE_H
#define NANOKERNEL_FD_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FD_TABLE_MAX 256  /* Max open files per process */
#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

/* File descriptor flags */
#define FD_FLAG_CLOEXEC  (1 << 0)
#define FD_FLAG_NONBLOCK (1 << 1)

/* File open modes */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_ACCMODE   0x0003  /* Mask for access mode */
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_NONBLOCK  0x0800
#define O_DIRECTORY 0x10000
#define O_CLOEXEC   0x80000

/* Seek modes */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Forward declarations */
typedef struct VFSVolume VFSVolume;

/* File descriptor entry */
typedef struct {
    bool        in_use;
    int         flags;           /* O_RDONLY, O_WRONLY, O_RDWR, etc. */
    int         fd_flags;        /* FD_FLAG_CLOEXEC, etc. */
    VFSVolume*  volume;          /* Mounted volume */
    uint64_t    inode;           /* File inode/catalog node ID */
    uint64_t    position;        /* Current file position */
    char        path[256];       /* Original path (for debugging) */
    uint32_t    pid;             /* Owner process ID */
} FileDescriptor;

/* File descriptor table (per-process or global) */
typedef struct {
    FileDescriptor entries[FD_TABLE_MAX];
    bool initialized;
} FDTable;

/* Initialize file descriptor table */
void fd_table_init(void);

/* Allocate a new file descriptor */
int fd_alloc(VFSVolume* vol, uint64_t inode, int flags, const char* path);

/* Get file descriptor entry */
FileDescriptor* fd_get(int fd);

/* Free a file descriptor */
bool fd_free(int fd);

/* Duplicate a file descriptor */
int fd_dup(int oldfd);
int fd_dup2(int oldfd, int newfd);

/* Check if fd is valid */
bool fd_is_valid(int fd);

/* Get current position */
uint64_t fd_tell(int fd);

/* Set file descriptor flags */
bool fd_set_flags(int fd, int flags);

/* Get file descriptor flags */
int fd_get_flags(int fd);

/* List all open file descriptors (for debugging) */
void fd_list(void);

#endif /* NANOKERNEL_FD_TABLE_H */
