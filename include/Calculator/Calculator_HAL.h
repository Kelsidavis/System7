/*
 * Calculator HAL Header
 * Hardware Abstraction Layer for Calculator desk accessory
 */

#ifndef CALCULATOR_HAL_H
#define CALCULATOR_HAL_H

#include <Types.h>
#include <Quickdraw.h>
#include <Windows.h>
#include "calculator.h"

/* HAL Function prototypes */
void Calculator_HAL_Init(void);
void Calculator_HAL_Cleanup(void);

/* Window management */
WindowPtr Calculator_HAL_CreateWindow(const Rect *bounds, ConstStr255Param title);
void Calculator_HAL_DisposeWindow(WindowPtr window);
void Calculator_HAL_DragWindow(WindowPtr window, Point startPt);
Boolean Calculator_HAL_TrackGoAway(WindowPtr window, Point pt);

/* Drawing operations */
void Calculator_HAL_BeginDrawing(WindowPtr window);
void Calculator_HAL_EndDrawing(void);
void Calculator_HAL_ClearWindow(void);
void Calculator_HAL_DrawDisplayBackground(const Rect *displayRect);
void Calculator_HAL_DrawDisplayText(const char *text, const Rect *displayRect);
void Calculator_HAL_DrawButton(CalcButton *button);

/* Utility functions */
void Calculator_HAL_Delay(unsigned long milliseconds);
void Calculator_HAL_GlobalToLocal(Point *pt, WindowPtr window);
void Calculator_HAL_LocalToGlobal(Point *pt, WindowPtr window);
Boolean Calculator_HAL_PtInRect(Point pt, const Rect *r);

/* Platform-specific event processing */
void Calculator_HAL_ProcessEvents(void);

#endif /* CALCULATOR_HAL_H */