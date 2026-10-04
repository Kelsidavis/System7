/* Shared resource acquisition for Extension Manager loaders. */

#ifndef EXTENSION_MANAGER_RESOURCE_LOADER_H
#define EXTENSION_MANAGER_RESOURCE_LOADER_H

#include "ExtensionManager/ExtensionTypes.h"

#define EXTENSION_RESOURCE_NAME_SIZE 256

/* Writes the resource name as a C string. name must hold
 * EXTENSION_RESOURCE_NAME_SIZE bytes. */
void Extension_GetResourceInfo(Handle resourceHandle,
                               ResID *resourceID,
                               ResType *resourceType,
                               char *resourceName);

/* On success, returns a loaded resource and its C-string name. On failure,
 * handle is NULL and name is empty. */
OSErr Extension_LoadResource(ResType resourceType,
                             SInt16 resourceID,
                             Handle *resourceHandle,
                             char *resourceName);

#endif /* EXTENSION_MANAGER_RESOURCE_LOADER_H */
