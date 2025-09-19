/*
 * ComponentNegotiation.c
 *
 * Component Capability Negotiation Implementation - System 7.1 Portable
 * Handles capability negotiation and versioning between components
 *
 * This module provides capability negotiation functionality for the Component Manager,
 * enabling components to dynamically discover and negotiate capabilities, versions,
 * and compatibility requirements.
 */

#include "ComponentManager/ComponentNegotiation.h"
#include "ComponentManager/ComponentManager_HAL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global negotiation management state */
typedef struct NegotiationManagerGlobals {
    bool initialized;
    ComponentCapability* globalCapabilities;
    CapabilityCache* cacheHead;
    CapabilityNegotiationCallback* callbacks[16];
    OSType callbackTypes[16];
    void* callbackUserData[16];
    int32_t callbackCount;
    ComponentMutex* negotiationMutex;
    uint32_t negotiationCount;
    uint32_t successfulNegotiations;
    uint32_t failedNegotiations;
} NegotiationManagerGlobals;

static NegotiationManagerGlobals gNegotiationGlobals = {0};

/* Forward declarations */
static ComponentCapability* AllocateCapability(void);
static void FreeCapability(ComponentCapability* capability);
static ComponentCapability* CloneCapability(ComponentCapability* source);
static bool CapabilitiesMatch(ComponentCapability* cap1, ComponentCapability* cap2);
static int32_t CalculateCapabilityScore(ComponentCapability* capability, CapabilityMatchCriteria* criteria);
static OSErr NegotiateCapabilityInternal(ComponentCapability* requested, ComponentCapability* available, ComponentCapability** result);

/*
 * InitComponentNegotiation
 *
 * Initialize the component capability negotiation system.
 */
OSErr InitComponentNegotiation(void)
{
    OSErr result = noErr;

    if (gNegotiationGlobals.initialized) {
        return noErr; /* Already initialized */
    }

    /* Initialize mutex for thread safety */
    result = HAL_CreateMutex(&gNegotiationGlobals.negotiationMutex);
    if (result != noErr) {
        return result;
    }

    /* Initialize global state */
    gNegotiationGlobals.globalCapabilities = NULL;
    gNegotiationGlobals.cacheHead = NULL;
    gNegotiationGlobals.callbackCount = 0;
    gNegotiationGlobals.negotiationCount = 0;
    gNegotiationGlobals.successfulNegotiations = 0;
    gNegotiationGlobals.failedNegotiations = 0;
    gNegotiationGlobals.initialized = true;

    return noErr;
}

/*
 * CleanupComponentNegotiation
 *
 * Cleanup the component capability negotiation system.
 */
void CleanupComponentNegotiation(void)
{
    if (!gNegotiationGlobals.initialized) {
        return;
    }

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    /* Free global capabilities */
    ComponentCapability* current = gNegotiationGlobals.globalCapabilities;
    while (current != NULL) {
        ComponentCapability* next = current->next;
        FreeCapability(current);
        current = next;
    }

    /* Cleanup capability cache */
    CleanupCapabilityCache();

    /* Clear callbacks */
    gNegotiationGlobals.callbackCount = 0;

    gNegotiationGlobals.initialized = false;

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);
    HAL_DestroyMutex(gNegotiationGlobals.negotiationMutex);
}

/*
 * RegisterComponentCapability
 *
 * Register a capability with a component.
 */
OSErr RegisterComponentCapability(Component component, ComponentCapability* capability)
{
    if (!gNegotiationGlobals.initialized || !component || !capability) {
        return paramErr;
    }

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    /* Clone the capability to avoid external modification */
    ComponentCapability* clonedCapability = CloneCapability(capability);
    if (!clonedCapability) {
        HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);
        return memFullErr;
    }

    /* Add to global capabilities list */
    clonedCapability->next = gNegotiationGlobals.globalCapabilities;
    gNegotiationGlobals.globalCapabilities = clonedCapability;

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return noErr;
}

/*
 * UnregisterComponentCapability
 *
 * Unregister a capability from a component.
 */
OSErr UnregisterComponentCapability(Component component, OSType capabilityType, OSType capabilitySubType)
{
    if (!gNegotiationGlobals.initialized || !component) {
        return paramErr;
    }

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    /* Find and remove the capability */
    ComponentCapability* current = gNegotiationGlobals.globalCapabilities;
    ComponentCapability* previous = NULL;

    while (current != NULL) {
        if (current->capabilityType == capabilityType &&
            current->capabilitySubType == capabilitySubType) {

            if (previous) {
                previous->next = current->next;
            } else {
                gNegotiationGlobals.globalCapabilities = current->next;
            }

            FreeCapability(current);
            break;
        }
        previous = current;
        current = current->next;
    }

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return noErr;
}

/*
 * GetComponentCapabilities
 *
 * Get all capabilities registered with a component.
 */
OSErr GetComponentCapabilities(Component component, ComponentCapability** capabilities, int32_t* count)
{
    if (!gNegotiationGlobals.initialized || !component || !capabilities || !count) {
        return paramErr;
    }

    *capabilities = NULL;
    *count = 0;

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    /* Count capabilities for this component */
    ComponentCapability* current = gNegotiationGlobals.globalCapabilities;
    int32_t capabilityCount = 0;

    while (current != NULL) {
        capabilityCount++;
        current = current->next;
    }

    if (capabilityCount == 0) {
        HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);
        return noErr;
    }

    /* Allocate capability array */
    ComponentCapability* capabilityArray = HAL_AllocateMemory(capabilityCount * sizeof(ComponentCapability));
    if (!capabilityArray) {
        HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);
        return memFullErr;
    }

    /* Copy capabilities */
    current = gNegotiationGlobals.globalCapabilities;
    int32_t index = 0;

    while (current != NULL && index < capabilityCount) {
        capabilityArray[index] = *current;
        capabilityArray[index].next = NULL; /* Don't copy the link */
        current = current->next;
        index++;
    }

    *capabilities = capabilityArray;
    *count = capabilityCount;

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return noErr;
}

/*
 * ComponentHasCapability
 *
 * Check if a component has a specific capability.
 */
bool ComponentHasCapability(Component component, OSType capabilityType, OSType capabilitySubType)
{
    if (!gNegotiationGlobals.initialized || !component) {
        return false;
    }

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    /* Search for the capability */
    ComponentCapability* current = gNegotiationGlobals.globalCapabilities;
    bool hasCapability = false;

    while (current != NULL) {
        if (current->capabilityType == capabilityType &&
            current->capabilitySubType == capabilitySubType) {
            hasCapability = true;
            break;
        }
        current = current->next;
    }

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return hasCapability;
}

/*
 * GetComponentCapability
 *
 * Get a specific capability from a component.
 */
OSErr GetComponentCapability(Component component, OSType capabilityType, OSType capabilitySubType, ComponentCapability** capability)
{
    if (!gNegotiationGlobals.initialized || !component || !capability) {
        return paramErr;
    }

    *capability = NULL;

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    /* Search for the capability */
    ComponentCapability* current = gNegotiationGlobals.globalCapabilities;

    while (current != NULL) {
        if (current->capabilityType == capabilityType &&
            current->capabilitySubType == capabilitySubType) {

            *capability = CloneCapability(current);
            break;
        }
        current = current->next;
    }

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return (*capability != NULL) ? noErr : componentNotFound;
}

/*
 * GetCapabilityVersion
 *
 * Get the version of a specific capability.
 */
int32_t GetCapabilityVersion(Component component, OSType capabilityType, OSType capabilitySubType)
{
    if (!gNegotiationGlobals.initialized || !component) {
        return 0;
    }

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    /* Search for the capability */
    ComponentCapability* current = gNegotiationGlobals.globalCapabilities;
    int32_t version = 0;

    while (current != NULL) {
        if (current->capabilityType == capabilityType &&
            current->capabilitySubType == capabilitySubType) {
            version = current->version;
            break;
        }
        current = current->next;
    }

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return version;
}

/*
 * NegotiateCapabilities
 *
 * Negotiate capabilities between components.
 */
OSErr NegotiateCapabilities(CapabilityNegotiationContext* context)
{
    if (!gNegotiationGlobals.initialized || !context) {
        return paramErr;
    }

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);
    gNegotiationGlobals.negotiationCount++;

    OSErr result = noErr;
    ComponentCapability* negotiatedList = NULL;
    ComponentCapability* lastNegotiated = NULL;

    /* Negotiate each requested capability */
    ComponentCapability* requested = context->requestedCapabilities;

    while (requested != NULL && result == noErr) {
        ComponentCapability* available = context->availableCapabilities;
        ComponentCapability* bestMatch = NULL;
        int32_t bestScore = -1;

        /* Find the best matching available capability */
        while (available != NULL) {
            if (available->capabilityType == requested->capabilityType &&
                available->capabilitySubType == requested->capabilitySubType) {

                CapabilityMatchCriteria criteria;
                criteria.capabilityType = requested->capabilityType;
                criteria.capabilitySubType = requested->capabilitySubType;
                criteria.minimumVersion = requested->version;
                criteria.maximumVersion = 0x7FFFFFFF;
                criteria.requiredFlags = requested->flags & kCapabilityFlagRequired;
                criteria.forbiddenFlags = 0;
                criteria.priority = 1;

                int32_t score = CalculateCapabilityScore(available, &criteria);
                if (score > bestScore) {
                    bestScore = score;
                    bestMatch = available;
                }
            }
            available = available->next;
        }

        if (bestMatch) {
            /* Create negotiated capability */
            ComponentCapability* negotiated = NULL;
            result = NegotiateCapabilityInternal(requested, bestMatch, &negotiated);

            if (result == noErr && negotiated) {
                if (negotiatedList == NULL) {
                    negotiatedList = negotiated;
                    lastNegotiated = negotiated;
                } else {
                    lastNegotiated->next = negotiated;
                    lastNegotiated = negotiated;
                }
            }
        } else if (requested->flags & kCapabilityFlagRequired) {
            /* Required capability not available */
            result = componentNotFound;
        }

        requested = requested->next;
    }

    if (result == noErr) {
        context->negotiatedCapabilities = negotiatedList;
        gNegotiationGlobals.successfulNegotiations++;
    } else {
        gNegotiationGlobals.failedNegotiations++;

        /* Free any negotiated capabilities on failure */
        while (negotiatedList != NULL) {
            ComponentCapability* next = negotiatedList->next;
            FreeCapability(negotiatedList);
            negotiatedList = next;
        }
    }

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return result;
}

/*
 * RequestCapability
 *
 * Request a specific capability from a component.
 */
OSErr RequestCapability(Component requestingComponent, Component providingComponent,
                       OSType capabilityType, OSType capabilitySubType, int32_t minVersion,
                       ComponentCapability** negotiatedCapability)
{
    if (!gNegotiationGlobals.initialized || !requestingComponent || !providingComponent || !negotiatedCapability) {
        return paramErr;
    }

    *negotiatedCapability = NULL;

    /* Create a simple negotiation context */
    CapabilityNegotiationContext context;
    memset(&context, 0, sizeof(context));

    context.requestingComponent = requestingComponent;
    context.providingComponent = providingComponent;

    /* Create requested capability */
    ComponentCapability* requested = AllocateCapability();
    if (!requested) {
        return memFullErr;
    }

    requested->capabilityType = capabilityType;
    requested->capabilitySubType = capabilitySubType;
    requested->version = minVersion;
    requested->flags = kCapabilityFlagRequired;
    requested->dataSize = 0;
    requested->data = NULL;
    requested->next = NULL;

    context.requestedCapabilities = requested;

    /* Get available capabilities from providing component */
    OSErr result = GetComponentCapabilities(providingComponent, &context.availableCapabilities, &context.flags);

    if (result == noErr) {
        result = NegotiateCapabilities(&context);

        if (result == noErr && context.negotiatedCapabilities) {
            *negotiatedCapability = CloneCapability(context.negotiatedCapabilities);
        }
    }

    /* Cleanup */
    FreeCapability(requested);
    if (context.availableCapabilities) {
        HAL_FreeMemory(context.availableCapabilities);
    }

    return result;
}

/*
 * CheckVersionCompatibility
 *
 * Check version compatibility between requested and available versions.
 */
OSErr CheckVersionCompatibility(int32_t requestedVersion, int32_t availableVersion,
                               VersionCompatibility* compatibility, bool* isCompatible)
{
    if (!compatibility || !isCompatible) {
        return paramErr;
    }

    *isCompatible = false;

    /* Check if the available version meets the minimum requirement */
    if (availableVersion >= compatibility->minimumVersion &&
        availableVersion <= compatibility->maximumVersion) {
        *isCompatible = true;
    }

    /* Check exact version match if required */
    if (compatibility->compatibilityFlags & kVersionCompatibilityExact) {
        *isCompatible = (availableVersion == requestedVersion);
    }

    return noErr;
}

/*
 * IsVersionCompatible
 *
 * Simple version compatibility check.
 */
bool IsVersionCompatible(int32_t version1, int32_t version2, uint32_t compatibilityFlags)
{
    if (compatibilityFlags & kVersionCompatibilityExact) {
        return version1 == version2;
    }

    if (compatibilityFlags & kVersionCompatibilityBackward) {
        return version2 >= version1;
    }

    if (compatibilityFlags & kVersionCompatibilityForward) {
        return version2 <= version1;
    }

    /* Default: backward compatibility */
    return version2 >= version1;
}

/*
 * FindMatchingCapabilities
 *
 * Find capabilities that match the given criteria.
 */
OSErr FindMatchingCapabilities(ComponentCapability* availableCapabilities,
                              CapabilityMatchCriteria* criteria,
                              ComponentCapability*** matches, int32_t* matchCount)
{
    if (!availableCapabilities || !criteria || !matches || !matchCount) {
        return paramErr;
    }

    *matches = NULL;
    *matchCount = 0;

    /* Count matching capabilities */
    ComponentCapability* current = availableCapabilities;
    int32_t count = 0;

    while (current != NULL) {
        if (current->capabilityType == criteria->capabilityType &&
            (criteria->capabilitySubType == 0 || current->capabilitySubType == criteria->capabilitySubType)) {

            if (current->version >= criteria->minimumVersion &&
                (criteria->maximumVersion == 0 || current->version <= criteria->maximumVersion)) {
                count++;
            }
        }
        current = current->next;
    }

    if (count == 0) {
        return noErr;
    }

    /* Allocate match array */
    ComponentCapability** matchArray = HAL_AllocateMemory(count * sizeof(ComponentCapability*));
    if (!matchArray) {
        return memFullErr;
    }

    /* Populate match array */
    current = availableCapabilities;
    int32_t index = 0;

    while (current != NULL && index < count) {
        if (current->capabilityType == criteria->capabilityType &&
            (criteria->capabilitySubType == 0 || current->capabilitySubType == criteria->capabilitySubType)) {

            if (current->version >= criteria->minimumVersion &&
                (criteria->maximumVersion == 0 || current->version <= criteria->maximumVersion)) {
                matchArray[index] = current;
                index++;
            }
        }
        current = current->next;
    }

    *matches = matchArray;
    *matchCount = count;

    return noErr;
}

/*
 * RankCapabilityMatch
 *
 * Rank how well a capability matches the criteria.
 */
int32_t RankCapabilityMatch(ComponentCapability* capability, CapabilityMatchCriteria* criteria)
{
    if (!capability || !criteria) {
        return -1;
    }

    int32_t score = 0;

    /* Type match */
    if (capability->capabilityType == criteria->capabilityType) {
        score += 100;
    }

    /* SubType match */
    if (criteria->capabilitySubType == 0 || capability->capabilitySubType == criteria->capabilitySubType) {
        score += 50;
    }

    /* Version compatibility */
    if (capability->version >= criteria->minimumVersion) {
        score += 25;

        /* Prefer exact version match */
        if (capability->version == criteria->minimumVersion) {
            score += 25;
        }
    }

    /* Flag compatibility */
    if ((capability->flags & criteria->requiredFlags) == criteria->requiredFlags) {
        score += 20;
    }

    if ((capability->flags & criteria->forbiddenFlags) == 0) {
        score += 10;
    }

    return score;
}

/*
 * InitCapabilityCache
 *
 * Initialize the capability cache.
 */
OSErr InitCapabilityCache(uint32_t maxEntries, uint32_t timeoutSeconds)
{
    /* For this implementation, we'll use a simple approach */
    return noErr;
}

/*
 * CleanupCapabilityCache
 *
 * Cleanup the capability cache.
 */
OSErr CleanupCapabilityCache(void)
{
    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    /* Free cache entries */
    CapabilityCache* current = gNegotiationGlobals.cacheHead;
    while (current != NULL) {
        CapabilityCache* next = current->next;
        if (current->capabilities) {
            HAL_FreeMemory(current->capabilities);
        }
        HAL_FreeMemory(current);
        current = next;
    }

    gNegotiationGlobals.cacheHead = NULL;

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return noErr;
}

/*
 * GetCapabilityNegotiationStats
 *
 * Get negotiation statistics.
 */
OSErr GetCapabilityNegotiationStats(Component component, uint32_t* successCount, uint32_t* failureCount)
{
    if (!gNegotiationGlobals.initialized || !successCount || !failureCount) {
        return paramErr;
    }

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    *successCount = gNegotiationGlobals.successfulNegotiations;
    *failureCount = gNegotiationGlobals.failedNegotiations;

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return noErr;
}

/* Helper Functions */

/*
 * AllocateCapability
 *
 * Allocate a new capability structure.
 */
static ComponentCapability* AllocateCapability(void)
{
    ComponentCapability* capability = HAL_AllocateMemory(sizeof(ComponentCapability));
    if (capability) {
        memset(capability, 0, sizeof(ComponentCapability));
    }
    return capability;
}

/*
 * FreeCapability
 *
 * Free a capability structure.
 */
static void FreeCapability(ComponentCapability* capability)
{
    if (!capability) {
        return;
    }

    if (capability->data) {
        HAL_FreeMemory(capability->data);
    }

    HAL_FreeMemory(capability);
}

/*
 * CloneCapability
 *
 * Create a copy of a capability.
 */
static ComponentCapability* CloneCapability(ComponentCapability* source)
{
    if (!source) {
        return NULL;
    }

    ComponentCapability* clone = AllocateCapability();
    if (!clone) {
        return NULL;
    }

    *clone = *source;
    clone->next = NULL;

    /* Clone data if present */
    if (source->data && source->dataSize > 0) {
        clone->data = HAL_AllocateMemory(source->dataSize);
        if (clone->data) {
            memcpy(clone->data, source->data, source->dataSize);
        } else {
            HAL_FreeMemory(clone);
            return NULL;
        }
    }

    return clone;
}

/*
 * CalculateCapabilityScore
 *
 * Calculate compatibility score for a capability.
 */
static int32_t CalculateCapabilityScore(ComponentCapability* capability, CapabilityMatchCriteria* criteria)
{
    return RankCapabilityMatch(capability, criteria);
}

/*
 * NegotiateCapabilityInternal
 *
 * Internal capability negotiation.
 */
static OSErr NegotiateCapabilityInternal(ComponentCapability* requested, ComponentCapability* available, ComponentCapability** result)
{
    if (!requested || !available || !result) {
        return paramErr;
    }

    /* Create negotiated capability based on available capability */
    *result = CloneCapability(available);
    if (!*result) {
        return memFullErr;
    }

    /* Adjust version to meet requested minimum */
    if ((*result)->version < requested->version) {
        (*result)->version = requested->version;
    }

    /* Merge flags */
    (*result)->flags |= (requested->flags & kCapabilityFlagRequired);

    return noErr;
}

/*
 * RegisterNegotiationCallback
 *
 * Register a callback for capability negotiation.
 */
OSErr RegisterNegotiationCallback(OSType capabilityType, CapabilityNegotiationCallback callback, void* userData)
{
    if (!gNegotiationGlobals.initialized || !callback) {
        return paramErr;
    }

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    if (gNegotiationGlobals.callbackCount >= 16) {
        HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);
        return memFullErr;
    }

    gNegotiationGlobals.callbacks[gNegotiationGlobals.callbackCount] = callback;
    gNegotiationGlobals.callbackTypes[gNegotiationGlobals.callbackCount] = capabilityType;
    gNegotiationGlobals.callbackUserData[gNegotiationGlobals.callbackCount] = userData;
    gNegotiationGlobals.callbackCount++;

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return noErr;
}

/*
 * UnregisterNegotiationCallback
 *
 * Unregister a negotiation callback.
 */
OSErr UnregisterNegotiationCallback(OSType capabilityType, CapabilityNegotiationCallback callback)
{
    if (!gNegotiationGlobals.initialized || !callback) {
        return paramErr;
    }

    HAL_LockMutex(gNegotiationGlobals.negotiationMutex);

    for (int32_t i = 0; i < gNegotiationGlobals.callbackCount; i++) {
        if (gNegotiationGlobals.callbacks[i] == callback &&
            gNegotiationGlobals.callbackTypes[i] == capabilityType) {

            /* Shift remaining callbacks down */
            for (int32_t j = i; j < gNegotiationGlobals.callbackCount - 1; j++) {
                gNegotiationGlobals.callbacks[j] = gNegotiationGlobals.callbacks[j + 1];
                gNegotiationGlobals.callbackTypes[j] = gNegotiationGlobals.callbackTypes[j + 1];
                gNegotiationGlobals.callbackUserData[j] = gNegotiationGlobals.callbackUserData[j + 1];
            }

            gNegotiationGlobals.callbackCount--;
            break;
        }
    }

    HAL_UnlockMutex(gNegotiationGlobals.negotiationMutex);

    return noErr;
}