/*
 * IntegrationTests.h - boot-time integration test interface
 */

#ifndef INTEGRATION_TESTS_H
#define INTEGRATION_TESTS_H

#include "SystemTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

OSErr IntegrationTests_Initialize(void);
void IntegrationTests_Run(void);
void IntegrationTests_Cleanup(void);

#ifdef __cplusplus
}
#endif

#endif
