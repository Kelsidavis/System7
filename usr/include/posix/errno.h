/* System 7X - POSIX errno.h Compatibility Header
 *
 * Error number definitions compatible with POSIX.
 */

#ifndef _POSIX_ERRNO_H
#define _POSIX_ERRNO_H

#include "../../../include/Nanokernel/syscalls.h"

/* errno is defined in syscalls.h */

/* Error descriptions */
const char* strerror(int errnum);

#endif /* _POSIX_ERRNO_H */
