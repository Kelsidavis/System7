#include "ExtensionManager/ResourceLoader.h"
#include "ResourceManager.h"
#include <string.h>

void Extension_GetResourceInfo(Handle resourceHandle,
                               ResID *resourceID,
                               ResType *resourceType,
                               char *resourceName)
{
    Str255 pascalName = {0};
    GetResInfo(resourceHandle, resourceID, resourceType,
               (char *)pascalName);

    if (resourceName) {
        UInt8 nameLength = pascalName[0];
        memcpy(resourceName, &pascalName[1], nameLength);
        resourceName[nameLength] = '\0';
    }
}

OSErr Extension_LoadResource(ResType resourceType,
                             SInt16 resourceID,
                             Handle *resourceHandle,
                             char *resourceName)
{
    if (!resourceHandle) {
        return extBadResource;
    }

    *resourceHandle = NULL;
    if (!resourceName) {
        return extBadResource;
    }

    resourceName[0] = '\0';
    *resourceHandle = GetResource(resourceType, resourceID);
    if (!*resourceHandle) {
        return extBadResource;
    }

    LoadResource(*resourceHandle);
    if (!**resourceHandle) {
        ReleaseResource(*resourceHandle);
        *resourceHandle = NULL;
        return extBadResource;
    }

    Extension_GetResourceInfo(*resourceHandle, NULL, NULL, resourceName);
    return extNoErr;
}
