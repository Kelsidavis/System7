#ifndef DESKACCESSORY_H
#define DESKACCESSORY_H

#include "DeskManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The accessory owns its window; DA_DestroyWindow is safe to call twice. */
int DA_CreateWindow(DeskAccessory *da, const DAWindowAttr *attr);
void DA_DestroyWindow(DeskAccessory *da);

/* Registration copies the entry, but callback tables remain caller-owned. */
int DA_Register(const DARegistryEntry *entry);
void DA_Unregister(const char *name);
DARegistryEntry *DA_FindRegistryEntry(const char *name);
int DA_GetRegisteredDAs(DARegistryEntry **entries, int maxEntries);

/* Borrowed registry view; do not modify registrations while traversing next. */
const DARegistryEntry *DA_GetFirstRegisteredDA(void);

#ifdef __cplusplus
}
#endif

#endif /* DESKACCESSORY_H */
