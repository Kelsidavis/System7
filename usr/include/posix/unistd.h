/* System 7X - POSIX unistd.h Compatibility Header
 *
 * Maps POSIX functions to System 7X nanokernel syscalls.
 */

#ifndef _POSIX_UNISTD_H
#define _POSIX_UNISTD_H

#include "../../../include/Nanokernel/syscalls.h"

/* File descriptor constants */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Access modes for access() */
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

/* Map POSIX functions to syscalls */
#define read(fd, buf, count)  sys_read(fd, buf, count)
#define write(fd, buf, count) sys_write(fd, buf, count)
#define close(fd)             sys_close(fd)
#define lseek(fd, offset, whence) sys_lseek(fd, offset, whence)
#define dup(oldfd)            sys_dup(oldfd)
#define dup2(oldfd, newfd)    sys_dup2(oldfd, newfd)

/* Process control (stubs for now) */
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
gid_t getgid(void);

/* File system operations */
int chdir(const char* path);
char* getcwd(char* buf, size_t size);

/* Process execution (stubs) */
int execve(const char* filename, char* const argv[], char* const envp[]);
pid_t fork(void);

#endif /* _POSIX_UNISTD_H */
