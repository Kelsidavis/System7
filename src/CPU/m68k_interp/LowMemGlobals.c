/*
 * LowMemGlobals.c - Low Memory Global Access Functions
 *
 * Provides host-side access to Mac OS low memory globals in the M68K
 * address space. These functions interface with the paged memory system
 * to read/write specific low memory locations.
 */

#include "CPU/LowMemGlobals.h"
#include "CPU/M68KInterp.h"
#include "CPU/CPUBackend.h"
#include "System71StdLib.h"
#include "CPU/CPULogging.h"

/* Forward declarations */
extern UInt8 M68K_Read8(M68KAddressSpace* as, UInt32 addr);
extern UInt16 M68K_Read16(M68KAddressSpace* as, UInt32 addr);
extern UInt32 M68K_Read32(M68KAddressSpace* as, UInt32 addr);
extern void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value);
extern void M68K_Write16(M68KAddressSpace* as, UInt32 addr, UInt16 value);
extern void M68K_Write32(M68KAddressSpace* as, UInt32 addr, UInt32 value);

/* Global pointer to current M68K address space */
static M68KAddressSpace* g_currentAS = NULL;

/*
 * LMInit - Initialize low memory globals system
 */
void LMInit(M68KAddressSpace* as)
{
    g_currentAS = as;
    M68K_LOG_INFO("Low memory globals system initialized (AS=%p)\n", as);
}

/*
 * LMGetCurrentAS - Get current address space (for internal use)
 */
static M68KAddressSpace* LMGetCurrentAS(void)
{
    if (!g_currentAS) {
        M68K_LOG_WARN("WARNING: No current address space set!\n");
    }
    return g_currentAS;
}

/*
 * Generic Read Functions
 */

UInt32 LMGetLong(UInt32 addr)
{
    M68KAddressSpace* as = LMGetCurrentAS();
    if (!as) return 0;

    if (addr >= M68K_LOW_MEM_SIZE) {
        M68K_LOG_WARN("WARNING: LMGetLong(0x%04X) beyond low memory\n", addr);
        return 0;
    }

    return M68K_Read32(as, addr);
}

void LMSetLong(UInt32 addr, UInt32 value)
{
    M68KAddressSpace* as = LMGetCurrentAS();
    if (!as) return;

    if (addr >= M68K_LOW_MEM_SIZE) {
        M68K_LOG_WARN("WARNING: LMSetLong(0x%04X) beyond low memory\n", addr);
        return;
    }

    M68K_Write32(as, addr, value);
}

UInt16 LMGetWord(UInt32 addr)
{
    M68KAddressSpace* as = LMGetCurrentAS();
    if (!as) return 0;

    if (addr >= M68K_LOW_MEM_SIZE) {
        M68K_LOG_WARN("WARNING: LMGetWord(0x%04X) beyond low memory\n", addr);
        return 0;
    }

    return M68K_Read16(as, addr);
}

void LMSetWord(UInt32 addr, UInt16 value)
{
    M68KAddressSpace* as = LMGetCurrentAS();
    if (!as) return;

    if (addr >= M68K_LOW_MEM_SIZE) {
        M68K_LOG_WARN("WARNING: LMSetWord(0x%04X) beyond low memory\n", addr);
        return;
    }

    M68K_Write16(as, addr, value);
}

UInt8 LMGetByte(UInt32 addr)
{
    M68KAddressSpace* as = LMGetCurrentAS();
    if (!as) return 0;

    if (addr >= M68K_LOW_MEM_SIZE) {
        M68K_LOG_WARN("WARNING: LMGetByte(0x%04X) beyond low memory\n", addr);
        return 0;
    }

    return M68K_Read8(as, addr);
}

void LMSetByte(UInt32 addr, UInt8 value)
{
    M68KAddressSpace* as = LMGetCurrentAS();
    if (!as) return;

    if (addr >= M68K_LOW_MEM_SIZE) {
        M68K_LOG_WARN("WARNING: LMSetByte(0x%04X) beyond low memory\n", addr);
        return;
    }

    M68K_Write8(as, addr, value);
}

/*
 * Specific Accessors for Common Globals
 */

UInt32 LMGetCurrentA5(void)
{
    return LMGetLong(LMG_CurrentA5);
}

void LMSetCurrentA5(UInt32 a5)
{
    LMSetLong(LMG_CurrentA5, a5);
    M68K_LOG_DEBUG("CurrentA5 set to 0x%08X\n", a5);
}

UInt32 LMGetExpandMem(void)
{
    return LMGetLong(LMG_ExpandMem);
}

void LMSetExpandMem(UInt32 expandMem)
{
    LMSetLong(LMG_ExpandMem, expandMem);
    M68K_LOG_DEBUG("ExpandMem set to 0x%08X\n", expandMem);
}

UInt32 LMGetTicks(void)
{
    return LMGetLong(LMG_Ticks);
}

void LMSetTicks(UInt32 ticks)
{
    LMSetLong(LMG_Ticks, ticks);
}

UInt32 LMGetMemTop(void)
{
    return LMGetLong(LMG_MemTop);
}

void LMSetMemTop(UInt32 value)
{
    LMSetLong(LMG_MemTop, value);
}

UInt32 LMGetSysZone(void)
{
    return LMGetLong(LMG_SysZone);
}

void LMSetSysZone(UInt32 value)
{
    LMSetLong(LMG_SysZone, value);
}

UInt32 LMGetApplZone(void)
{
    return LMGetLong(LMG_ApplZone);
}

void LMSetApplZone(UInt32 value)
{
    LMSetLong(LMG_ApplZone, value);
}

void* LMGetThePort(void)
{
    return (void*)(uintptr_t)LMGetLong(LMG_ThePort);
}

void LMSetThePort(void* port)
{
    LMSetLong(LMG_ThePort, (UInt32)(uintptr_t)port);
}
