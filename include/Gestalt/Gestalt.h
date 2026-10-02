/*
 * Gestalt.h - Gestalt Manager public interface
 * Based on Inside Macintosh: Operating System Utilities
 * Clean-room implementation for freestanding System 7.1
 */

#ifndef GESTALT_H
#define GESTALT_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes (guard if already present in SystemTypes.h) */
#ifndef gestaltUnknownErr
#define gestaltUnknownErr   -5551
#endif

#ifndef gestaltDupSelectorErr
#define gestaltDupSelectorErr -5552
#endif

#ifndef gestaltTableFullErr
#define gestaltTableFullErr   -5553
#endif

#ifndef unimpErr
#define unimpErr            -4
#endif

#ifndef paramErr
#define paramErr            -50
#endif

#ifndef envBadVers
#define envBadVers          -5501
#endif

/* Types */
typedef UInt32 OSType;        /* 4-char selector */
typedef OSErr (*GestaltProc)(long *response); /* Callback returns noErr or error */

/* Lifecycle */
OSErr Gestalt_Init(void);
void  Gestalt_Shutdown(void);

/* Core calls */
OSErr Gestalt(OSType selector, long *response);

/* Registration */
OSErr NewGestalt(OSType selector, GestaltProc proc);
OSErr ReplaceGestalt(OSType selector, GestaltProc proc);

/* Convenience */
Boolean Gestalt_Has(OSType selector);  /* true if selector exists */

/* Model helpers */
void   Gestalt_SetMachineType(UInt16 machineType);  /* Override reported machine type */
UInt16 Gestalt_GetMachineType(void);                /* Returns 0 if none configured */

/* System Environment (minimal) */
typedef struct {
    UInt16 machineType;   /* e.g., 6 = Mac II (placeholder) */
    UInt32 systemVersion; /* BCD, e.g., 0x0710 for 7.1 */
    UInt8  hasFPU;        /* 0/1 */
    UInt8  hasMMU;        /* 0/1 */
} SysEnvRec;

OSErr GetSysEnv(short versionRequested, SysEnvRec *answer);

/* Endian detection for portable decisions (do NOT alter resource on-disk endianness) */
#ifndef SYS71_LITTLE_ENDIAN
  #if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #define SYS71_LITTLE_ENDIAN 1
  #else
    #define SYS71_LITTLE_ENDIAN 0
  #endif
#endif

/* Common selectors */
#define gestaltSystemVersion    FOURCC('s','y','s','v')
#define gestaltTimeMgrVersion   FOURCC('q','t','i','m')
#define gestaltResourceMgrVers  FOURCC('r','s','r','c')
#define gestaltMachineType      FOURCC('m','a','c','h')
#define gestaltProcessorType    FOURCC('p','r','o','c')
#define gestaltFPUType          FOURCC('f','p','u',' ')
#define gestaltInitBits         FOURCC('i','n','i','t')
#define gestaltMemoryMap        FOURCC('m','m','a','p')

#ifdef __cplusplus
}
#endif

#endif /* GESTALT_H */
