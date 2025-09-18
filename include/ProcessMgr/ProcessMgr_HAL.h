/*
 * ProcessMgr_HAL.h - Hardware Abstraction Layer for Process Manager
 *
 * Modern platform support for x86_64 and ARM64 architectures
 */

#ifndef __PROCESSMGR_HAL_H__
#define __PROCESSMGR_HAL_H__

#include "ProcessMgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CPU Architecture Types */
typedef enum {
    kCPUArch_Unknown = 0,
    kCPUArch_x86_64 = 1,
    kCPUArch_ARM64 = 2,
    kCPUArch_RISCV64 = 3
} CPUArchitecture;

/* CPU Features Structure */
typedef struct CPUFeatures {
    CPUArchitecture architecture;
    UInt32 cpuCount;
    Boolean hasSSE;
    Boolean hasSSE2;
    Boolean hasAVX;
    Boolean hasAVX2;
    Boolean hasNEON;
    Boolean hasSVE;
    Boolean isAppleSilicon;
    char platformName[32];
} CPUFeatures;

/* HAL Functions */
OSErr ProcessMgr_HAL_Initialize(void);
OSErr ProcessMgr_HAL_CreateContext(ProcessControlBlock* pcb);
OSErr ProcessMgr_HAL_LaunchProcess(ProcessControlBlock* pcb);
OSErr ProcessMgr_HAL_SwitchContext(ProcessControlBlock* fromProcess,
                                   ProcessControlBlock* toProcess);
OSErr ProcessMgr_HAL_TerminateProcess(ProcessControlBlock* pcb);
OSErr ProcessMgr_HAL_Yield(void);
OSErr ProcessMgr_HAL_Shutdown(void);

ProcessControlBlock* ProcessMgr_HAL_GetCurrentProcess(void);
void ProcessMgr_HAL_GetCPUFeatures(CPUFeatures* features);

#ifdef __cplusplus
}
#endif

#endif /* __PROCESSMGR_HAL_H__ */