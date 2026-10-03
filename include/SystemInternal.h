/* Internal system functions used across modules */
#ifndef SYSTEM_INTERNAL_H
#define SYSTEM_INTERNAL_H

#include "SystemTypes.h"
#include "OSUtils/OSUtils.h"
#include <math.h>

/* Cursor management */
void InvalidateCursor(void);
void Pointer_TakeOffScreen(void);                        /* erase the pointer now */
void Pointer_Shield(int left, int top, int right, int bottom);  /* erase it if it is in there */
void UpdateCursorDisplay(void);
int IsCursorVisible(void);
const Cursor* CursorManager_GetCurrentCursorImage(void);
Point CursorManager_GetCursorHotspot(void);
void CursorManager_HandleMouseMotion(Point globalPt);

/* List Manager */
void InitListManager(void);

/* Standard library functions */
void __assert_fail(const char* expr, const char* file, int line, const char* func);

#endif /* SYSTEM_INTERNAL_H */
