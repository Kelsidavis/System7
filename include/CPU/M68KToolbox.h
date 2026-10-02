/*
 * M68KToolbox.h - the Toolbox as a 68K application sees it
 *
 * A 68K application calls the system through A-line traps, with its
 * arguments on its own stack in its own (big-endian) memory. This module
 * answers those traps from the system's native managers.
 */

#ifndef M68K_TOOLBOX_H
#define M68K_TOOLBOX_H

#include "SystemTypes.h"
#include "SegmentLoader/SegmentLoader.h"

/* Make the application's address space ready to run it: the low-memory
 * globals a program reads, and the traps it may call. */
OSErr M68KToolbox_Prepare(SegmentLoaderContext* ctx, ConstStr255Param appName,
                          SInt16 resRefNum, CPUAddr stackTop);

#endif /* M68K_TOOLBOX_H */
