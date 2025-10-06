/* Internal system functions used across modules */
#ifndef SYSTEM_INTERNAL_H
#define SYSTEM_INTERNAL_H

#include "SystemTypes.h"

/* Boot functions */
void boot_main(uint32_t magic, uint32_t* mb2_info);

/* Cursor management */
void InvalidateCursor(void);
void UpdateCursorDisplay(void);
int IsCursorVisible(void);

/* List Manager */
void InitListManager(void);

/* Delay function */
void Delay(UInt32 numTicks, UInt32* finalTicks);

/* Memory error tracking */
OSErr MemError(void);

/* Standard library functions */
void __assert_fail(const char* expr, const char* file, int line, const char* func);
double sqrt(double x);

#endif /* SYSTEM_INTERNAL_H */
