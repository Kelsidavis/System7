/*
 * DialogTypes.h - Dialog Manager Type Definitions
 *
 * This header declares dialog-specific types and constants not provided by
 * SystemTypes.h. Compatibility coverage varies by type and operation.
 */

#ifndef DIALOG_TYPES_H
#define DIALOG_TYPES_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    struct {
        unsigned sound1:3;      /* Sound for stage 1 */
        unsigned boxDrwn1:1;    /* Draw box for stage 1 */
        unsigned boldItm1:1;    /* Bold item for stage 1 */
        unsigned sound2:3;      /* Sound for stage 2 */
        unsigned boxDrwn2:1;    /* Draw box for stage 2 */
        unsigned boldItm2:1;    /* Bold item for stage 2 */
        unsigned sound3:3;      /* Sound for stage 3 */
        unsigned boxDrwn3:1;    /* Draw box for stage 3 */
        unsigned boldItm3:1;    /* Bold item for stage 3 */
        unsigned sound4:3;      /* Sound for stage 4 */
        unsigned boxDrwn4:1;    /* Draw box for stage 4 */
        unsigned boldItm4:1;    /* Bold item for stage 4 */
    } stages;
} StageListUnion;

#ifndef itemTypeMask
enum {
    itemTypeMask = 0x7F       /* Mask for item type (127) */
};
#endif

/* DITL append methods */
enum {
    overlayDITL = 0,          /* Overlay items */
    appendDITLRight = 1,      /* Append to right */
    appendDITLBottom = 2      /* Append to bottom */
};

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_TYPES_H */
