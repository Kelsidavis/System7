/* Shared QuickDraw state used by the core and platform input backends. */
#ifndef QUICKDRAW_GLOBALS_H
#define QUICKDRAW_GLOBALS_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

extern QDGlobals qd;
extern GrafPtr g_currentPort;

#ifdef __cplusplus
}
#endif

#endif /* QUICKDRAW_GLOBALS_H */
