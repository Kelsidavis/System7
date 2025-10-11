/* System 7X - POSIX sys/stat.h Compatibility Header
 *
 * File status definitions compatible with POSIX.
 */

#ifndef _POSIX_SYS_STAT_H
#define _POSIX_SYS_STAT_H

#include "../../../../include/Nanokernel/syscalls.h"

/* File type macros */
#define S_IFMT   0170000  /* File type mask */
#define S_IFREG  0100000  /* Regular file */
#define S_IFDIR  0040000  /* Directory */
#define S_IFCHR  0020000  /* Character device */
#define S_IFBLK  0060000  /* Block device */
#define S_IFIFO  0010000  /* FIFO */
#define S_IFLNK  0120000  /* Symbolic link */
#define S_IFSOCK 0140000  /* Socket */

/* File type test macros */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* Permission bits */
#define S_ISUID 0004000  /* Set user ID on execution */
#define S_ISGID 0002000  /* Set group ID on execution */
#define S_ISVTX 0001000  /* Sticky bit */

#define S_IRWXU 0000700  /* User read, write, execute */
#define S_IRUSR 0000400  /* User read */
#define S_IWUSR 0000200  /* User write */
#define S_IXUSR 0000100  /* User execute */

#define S_IRWXG 0000070  /* Group read, write, execute */
#define S_IRGRP 0000040  /* Group read */
#define S_IWGRP 0000020  /* Group write */
#define S_IXGRP 0000010  /* Group execute */

#define S_IRWXO 0000007  /* Other read, write, execute */
#define S_IROTH 0000004  /* Other read */
#define S_IWOTH 0000002  /* Other write */
#define S_IXOTH 0000001  /* Other execute */

/* Map POSIX stat functions to syscalls */
#define stat(path, buf)  sys_stat(path, buf)
#define fstat(fd, buf)   sys_fstat(fd, buf)

/* File operations */
int chmod(const char* path, mode_t mode);
int fchmod(int fd, mode_t mode);
int mkdir(const char* path, mode_t mode);

#endif /* _POSIX_SYS_STAT_H */
