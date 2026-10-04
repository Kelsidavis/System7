/*
 * M68KDecode.h - internal guest-memory and addressing helpers
 */

#ifndef M68K_DECODE_INTERNAL_H
#define M68K_DECODE_INTERNAL_H

#include "CPU/M68KInterp.h"
#include "CPU/M68KOpcodes.h"

UInt8 M68K_Read8(M68KAddressSpace* as, UInt32 addr);
UInt16 M68K_Read16(M68KAddressSpace* as, UInt32 addr);
UInt32 M68K_Read32(M68KAddressSpace* as, UInt32 addr);
void M68K_Write8(M68KAddressSpace* as, UInt32 addr, UInt8 value);
void M68K_Write16(M68KAddressSpace* as, UInt32 addr, UInt16 value);
void M68K_Write32(M68KAddressSpace* as, UInt32 addr, UInt32 value);
UInt16 M68K_Fetch16(M68KAddressSpace* as);
UInt32 M68K_Fetch32(M68KAddressSpace* as);
UInt32 M68K_EA_ComputeAddress(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size);
UInt32 M68K_EA_Read(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size);
void M68K_EA_Write(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size, UInt32 value);
UInt32 M68K_EA_ReadRMW(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size);
void M68K_EA_WriteRMW(M68KAddressSpace* as, UInt8 mode, UInt8 reg, M68KSize size, UInt32 value);

#endif
