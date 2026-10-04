#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "Finder/Icon/icon_types.h"

typedef struct { int16_t id; const char* name; const IconFamily* fam; } IconGenEntry;

#ifdef __cplusplus
extern "C" {
#endif

bool IconGen_FindByID(int16_t id, IconFamily* out);
extern const IconGenEntry gIconGenTable[];
extern const int gIconGenCount;

#ifdef __cplusplus
}
#endif
