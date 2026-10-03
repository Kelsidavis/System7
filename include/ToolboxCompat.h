/* Toolbox Compatibility Layer - Mac Classic API emulation */

#ifndef TOOLBOX_COMPAT_H
#define TOOLBOX_COMPAT_H

#include <stddef.h>

/* BlockMove - Mac OS Classic memory move routine */
void BlockMove(const void* srcPtr, void* destPtr, size_t byteCount);

#endif /* TOOLBOX_COMPAT_H */
