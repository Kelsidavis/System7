/* System 7X - POSIX dirent.h Compatibility Header
 *
 * Directory entry definitions compatible with POSIX.
 */

#ifndef _POSIX_DIRENT_H
#define _POSIX_DIRENT_H

#include "../../../include/Nanokernel/syscalls.h"

/* Map POSIX directory functions to syscalls */
#define opendir(path)    sys_opendir(path)
#define readdir(dir)     sys_readdir(dir)
#define closedir(dir)    sys_closedir(dir)

/* Rewind directory stream */
void rewinddir(DIR* dirp);

/* Seek to position in directory */
void seekdir(DIR* dirp, long loc);

/* Get current position in directory */
long telldir(DIR* dirp);

#endif /* _POSIX_DIRENT_H */
