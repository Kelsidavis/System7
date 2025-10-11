/* System 7X Nanokernel - POSIX Syscalls Interface
 *
 * POSIX-compatible system call interface for file operations.
 */

#ifndef NANOKERNEL_SYSCALLS_H
#define NANOKERNEL_SYSCALLS_H

#include <stdint.h>
#include <stddef.h>
#include "fd_table.h"

/* POSIX types */
typedef long ssize_t;
typedef long off_t;
typedef unsigned int mode_t;

/* errno values */
#define EPERM   1   /* Operation not permitted */
#define ENOENT  2   /* No such file or directory */
#define ESRCH   3   /* No such process */
#define EINTR   4   /* Interrupted system call */
#define EIO     5   /* I/O error */
#define ENXIO   6   /* No such device or address */
#define E2BIG   7   /* Argument list too long */
#define ENOEXEC 8   /* Exec format error */
#define EBADF   9   /* Bad file number */
#define ECHILD  10  /* No child processes */
#define EAGAIN  11  /* Try again */
#define ENOMEM  12  /* Out of memory */
#define EACCES  13  /* Permission denied */
#define EFAULT  14  /* Bad address */
#define ENOTBLK 15  /* Block device required */
#define EBUSY   16  /* Device or resource busy */
#define EEXIST  17  /* File exists */
#define EXDEV   18  /* Cross-device link */
#define ENODEV  19  /* No such device */
#define ENOTDIR 20  /* Not a directory */
#define EISDIR  21  /* Is a directory */
#define EINVAL  22  /* Invalid argument */
#define ENFILE  23  /* File table overflow */
#define EMFILE  24  /* Too many open files */
#define ENOTTY  25  /* Not a typewriter */
#define ETXTBSY 26  /* Text file busy */
#define EFBIG   27  /* File too large */
#define ENOSPC  28  /* No space left on device */
#define ESPIPE  29  /* Illegal seek */
#define EROFS   30  /* Read-only file system */
#define EMLINK  31  /* Too many links */
#define EPIPE   32  /* Broken pipe */

/* Global errno variable */
extern int errno;

/* Get/set errno */
int* __errno_location(void);

/* Initialize syscall subsystem */
void syscalls_init(void);

/* File operations */
int sys_open(const char* path, int flags, mode_t mode);
ssize_t sys_read(int fd, void* buf, size_t count);
ssize_t sys_write(int fd, const void* buf, size_t count);
off_t sys_lseek(int fd, off_t offset, int whence);
int sys_close(int fd);

/* Directory operations */
struct DIR;
typedef struct DIR DIR;

struct dirent {
    uint64_t d_ino;           /* Inode number */
    uint64_t d_off;           /* Offset to next dirent */
    unsigned short d_reclen;   /* Length of this record */
    unsigned char d_type;      /* Type of file */
    char d_name[256];          /* Filename */
};

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14

DIR* sys_opendir(const char* path);
struct dirent* sys_readdir(DIR* dir);
int sys_closedir(DIR* dir);

/* File metadata */
struct stat {
    uint64_t st_dev;     /* Device */
    uint64_t st_ino;     /* Inode */
    uint32_t st_mode;    /* Protection and file type */
    uint32_t st_nlink;   /* Number of hard links */
    uint32_t st_uid;     /* User ID */
    uint32_t st_gid;     /* Group ID */
    uint64_t st_rdev;    /* Device ID (if special file) */
    int64_t  st_size;    /* Total size, in bytes */
    int64_t  st_blksize; /* Block size for filesystem I/O */
    int64_t  st_blocks;  /* Number of 512B blocks allocated */
    int64_t  st_atime;   /* Time of last access */
    int64_t  st_mtime;   /* Time of last modification */
    int64_t  st_ctime;   /* Time of last status change */
};

int sys_stat(const char* path, struct stat* buf);
int sys_fstat(int fd, struct stat* buf);

/* File control */
int sys_fcntl(int fd, int cmd, ... /* arg */);
int sys_ioctl(int fd, unsigned long request, ...);

/* unistd operations */
int sys_dup(int oldfd);
int sys_dup2(int oldfd, int newfd);

#endif /* NANOKERNEL_SYSCALLS_H */
