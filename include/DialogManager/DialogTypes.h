/*
 * DialogTypes.h - Dialog Manager Type Definitions
 *
 * This header defines all the internal structures and types used by the
 * Dialog Manager, maintaining exact compatibility with Mac System 7.1.
 */

#ifndef DIALOG_TYPES_H
#define DIALOG_TYPES_H

#include "SystemTypes.h"

/* Forward declarations */


#ifdef __cplusplus
extern "C" {
#endif

/* Include base types */
#ifndef DIALOG_MANAGER_H
/* Rect, Point, Str255, Handle, and OSErr are defined in MacTypes.h */
/* Handle is defined in MacTypes.h */
/* Ptr is defined in MacTypes.h */
/* Handle is defined in MacTypes.h */
#endif

/* Dialog Manager globals structure */

/* Stage list structure for alert dialogs */
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

/* Dialog item internal structure */

/* Dialog item list header */

/* Extended dialog item structure for internal use */

/* Dialog resource structures */

/* DLOG resource structure */

/* DITL resource structure */

/* ALRT resource structure */

/* Dialog manager internal state */

/* Dialog event handling state */

/* Dialog drawing state */

/* Platform abstraction structure */

/* Error codes specific to Dialog Manager */

/* Dialog Manager feature flags */

/* Dialog item type masks and flags */
/* Note: Most dialog item constants are defined in SystemTypes.h */
/* Only define the ones that are missing */
#ifndef itemTypeMask
enum {
    itemTypeMask = 0x7F       /* Mask for item type (127) */
};
#endif

/* DITL append methods */
typedef short DITLMethod;
enum {
    overlayDITL = 0,          /* Overlay items */
    appendDITLRight = 1,      /* Append to right */
    appendDITLBottom = 2      /* Append to bottom */
};

/* Dialog window classes and styles */

/* Color theme structure for modern platforms */

/* Accessibility information */

#ifdef __cplusplus
}
#endif

#endif /* DIALOG_TYPES_H */
