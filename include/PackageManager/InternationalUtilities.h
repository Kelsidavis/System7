/*
 * InternationalUtilities.h - International Utilities interface
 */

#ifndef PACKAGE_MANAGER_INTERNATIONAL_UTILITIES_H
#define PACKAGE_MANAGER_INTERNATIONAL_UTILITIES_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

Handle IUGetIntl(SInt16 theID);
void IUSetIntl(SInt16 refNum, SInt16 theID, const void* intlParam);
Boolean IUMetric(void);
void IUClearCache(void);

#ifdef __cplusplus
}
#endif

#endif
