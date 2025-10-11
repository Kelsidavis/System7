/* System 7X - POSIX fcntl.h Compatibility Header
 *
 * File control definitions compatible with POSIX.
 */

#ifndef _POSIX_FCNTL_H
#define _POSIX_FCNTL_H

#include "../../../include/Nanokernel/syscalls.h"

/* Map POSIX open to syscall */
#define open(path, flags, ...) sys_open(path, flags, ##__VA_ARGS__)

/* fcntl commands */
#define F_DUPFD   0   /* Duplicate file descriptor */
#define F_GETFD   1   /* Get file descriptor flags */
#define F_SETFD   2   /* Set file descriptor flags */
#define F_GETFL   3   /* Get file status flags */
#define F_SETFL   4   /* Set file status flags */
#define F_GETLK   5   /* Get record locking information */
#define F_SETLK   6   /* Set record locking information */
#define F_SETLKW  7   /* Set record locking information (blocking) */

/* File control */
int fcntl(int fd, int cmd, ... /* arg */);

#endif /* _POSIX_FCNTL_H */
