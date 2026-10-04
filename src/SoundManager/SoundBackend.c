#include "SystemTypes.h"

#include "SoundManager/SoundBackend.h"
#include "SoundManager/SoundLogging.h"

/* Forward declarations of backend ops */
extern const SoundBackendOps kSoundBackendOps_HDA;
extern const SoundBackendOps kSoundBackendOps_SB16;

const SoundBackendOps* SoundBackend_GetOps(SoundBackendType type)
{
    switch (type) {
        case kSoundBackendHDA:
            return &kSoundBackendOps_HDA;
        case kSoundBackendSB16:
            return &kSoundBackendOps_SB16;
        default:
            return NULL;
    }
}

const char* SoundBackend_Name(SoundBackendType type)
{
    const SoundBackendOps* ops = SoundBackend_GetOps(type);
    return ops ? ops->name : "None";
}
