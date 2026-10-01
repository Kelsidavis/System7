/*
 * MacPaint.h - starting MacPaint from the Finder
 */

#ifndef MACPAINT_H
#define MACPAINT_H

#include "SystemTypes.h"
#include "FS/hfs_types.h"

/* Run MacPaint with a new picture, or with the painting named in dir */
void MacPaint_Launch(void);
void MacPaint_OpenDocument(VRefNum vref, DirID dir, const char* name);

#endif /* MACPAINT_H */
