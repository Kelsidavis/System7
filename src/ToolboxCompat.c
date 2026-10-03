/* Toolbox Compatibility Layer - Implementation */

#include "ToolboxCompat.h"
#include <string.h>

/* BlockMove - wrapper around memmove for Mac OS Classic compatibility */
void BlockMove(const void* srcPtr, void* destPtr, size_t byteCount) {
    memmove(destPtr, srcPtr, byteCount);
}
