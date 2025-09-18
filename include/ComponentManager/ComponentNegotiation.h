/*
 * ComponentNegotiation.h
 *
 * Component Capability Negotiation API - System 7.1 Portable Implementation
 * Handles capability negotiation and versioning between components
 */

#ifndef COMPONENTNEGOTIATION_H
#define COMPONENTNEGOTIATION_H

#include "ComponentManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Capability descriptor */
typedef struct ComponentCapability {
    OSType capabilityType;          /* Type of capability */
    OSType capabilitySubType;       /* Sub-type of capability */
    int32_t version;                /* Capability version */
    uint32_t flags;                 /* Capability flags */
    uint32_t dataSize;              /* Size of capability data */
    void* data;                     /* Capability-specific data */
    struct ComponentCapability* next;
} ComponentCapability;

/* Capability types */
#define kCapabilityTypeCodec        'cdec'  /* Codec capability */
#define kCapabilityTypeEffect       'efct'  /* Effect capability */
#define kCapabilityTypeImporter     'impt'  /* Importer capability */
#define kCapabilityTypeExporter     'expt'  /* Exporter capability */
#define kCapabilityTypeRenderer     'rndr'  /* Renderer capability */
#define kCapabilityTypeCompressor   'comp'  /* Compressor capability */
#define kCapabilityTypeDecompressor 'dcmp'  /* Decompressor capability */
#define kCapabilityTypeGeneric      'genc'  /* Generic capability */

/* Capability flags */
#define kCapabilityFlagRequired     (1<<0)  /* Required capability */
#define kCapabilityFlagOptional     (1<<1)  /* Optional capability */
#define kCapabilityFlagPreferred    (1<<2)  /* Preferred capability */
#define kCapabilityFlagDeprecated   (1<<3)  /* Deprecated capability */
#define kCapabilityFlagExperimental (1<<4)  /* Experimental capability */
#define kCapabilityFlagPlatformSpecific (1<<5) /* Platform-specific capability */

/* Negotiation context */
typedef struct CapabilityNegotiationContext {
    Component requestingComponent;
    Component providingComponent;
    ComponentCapability* requestedCapabilities;
    ComponentCapability* availableCapabilities;
    ComponentCapability* negotiatedCapabilities;
    uint32_t flags;
    int32_t priority;
    void* userData;
} CapabilityNegotiationContext;

/* Negotiation flags */
#define kNegotiationFlagStrict      (1<<0)  /* Strict negotiation - all required capabilities must match */
#define kNegotiationFlagBestEffort  (1<<1)  /* Best effort - use best available capabilities */
#define kNegotiationFlagFallback    (1<<2)  /* Allow fallback to lower versions */
#define kNegotiationFlagCache       (1<<3)  /* Cache negotiation results */

/* Version compatibility */
typedef struct VersionCompatibility {
    int32_t minimumVersion;         /* Minimum supported version */
    int32_t maximumVersion;         /* Maximum supported version */
    int32_t preferredVersion;       /* Preferred version */
    int32_t currentVersion;         /* Current version */
    uint32_t compatibilityFlags;    /* Compatibility flags */
} VersionCompatibility;

/* Compatibility flags */
#define kVersionCompatibilityBackward   (1<<0)  /* Backward compatible */
#define kVersionCompatibilityForward    (1<<1)  /* Forward compatible */
#define kVersionCompatibilityExact      (1<<2)  /* Exact version match required */
#define kVersionCompatibilityRange      (1<<3)  /* Version range supported */

/* Negotiation initialization */
OSErr InitComponentNegotiation(void);
void CleanupComponentNegotiation(void);

/* Capability management */
OSErr RegisterComponentCapability(Component component, ComponentCapability* capability);
OSErr UnregisterComponentCapability(Component component, OSType capabilityType, OSType capabilitySubType);
OSErr GetComponentCapabilities(Component component, ComponentCapability** capabilities, int32_t* count);

/* Capability queries */
bool ComponentHasCapability(Component component, OSType capabilityType, OSType capabilitySubType);
OSErr GetComponentCapability(Component component, OSType capabilityType, OSType capabilitySubType, ComponentCapability** capability);
int32_t GetCapabilityVersion(Component component, OSType capabilityType, OSType capabilitySubType);

/* Capability negotiation */
OSErr NegotiateCapabilities(CapabilityNegotiationContext* context);
OSErr RequestCapability(Component requestingComponent, Component providingComponent,
                       OSType capabilityType, OSType capabilitySubType, int32_t minVersion,
                       ComponentCapability** negotiatedCapability);

/* Version compatibility checking */
OSErr CheckVersionCompatibility(int32_t requestedVersion, int32_t availableVersion,
                               VersionCompatibility* compatibility, bool* isCompatible);
OSErr GetVersionCompatibilityInfo(Component component, VersionCompatibility* compatibility);
bool IsVersionCompatible(int32_t version1, int32_t version2, uint32_t compatibilityFlags);

/* Capability matching */
typedef struct CapabilityMatchCriteria {
    OSType capabilityType;
    OSType capabilitySubType;
    int32_t minimumVersion;
    int32_t maximumVersion;
    uint32_t requiredFlags;
    uint32_t forbiddenFlags;
    int32_t priority;
} CapabilityMatchCriteria;

OSErr FindMatchingCapabilities(ComponentCapability* availableCapabilities,
                              CapabilityMatchCriteria* criteria,
                              ComponentCapability*** matches, int32_t* matchCount);
int32_t RankCapabilityMatch(ComponentCapability* capability, CapabilityMatchCriteria* criteria);

/* Capability enumeration */
typedef bool (*CapabilityEnumeratorFunc)(ComponentCapability* capability, void* userData);
OSErr EnumerateComponentCapabilities(Component component, CapabilityEnumeratorFunc enumerator, void* userData);
OSErr EnumerateAllCapabilities(OSType capabilityType, CapabilityEnumeratorFunc enumerator, void* userData);

/* Capability dependencies */
typedef struct CapabilityDependency {
    OSType dependentCapabilityType;
    OSType dependentCapabilitySubType;
    OSType requiredCapabilityType;
    OSType requiredCapabilitySubType;
    int32_t minimumVersion;
    bool isRequired;
    struct CapabilityDependency* next;
} CapabilityDependency;

OSErr AddCapabilityDependency(Component component, CapabilityDependency* dependency);
OSErr RemoveCapabilityDependency(Component component, OSType dependentType, OSType dependentSubType);
OSErr ResolveCapabilityDependencies(Component component);
OSErr CheckCapabilityDependencies(Component component, bool* allSatisfied);

/* Codec-specific negotiation */
typedef struct CodecCapability {
    ComponentCapability base;
    OSType codecType;              /* 'vide', 'soun', etc. */
    OSType codecSubType;           /* Specific codec fourCC */
    uint32_t maxWidth;             /* Maximum width supported */
    uint32_t maxHeight;            /* Maximum height supported */
    uint32_t maxFrameRate;         /* Maximum frame rate */
    uint32_t maxBitRate;           /* Maximum bit rate */
    uint32_t supportedFormats;     /* Supported pixel/audio formats */
    uint32_t qualityLevels;        /* Supported quality levels */
} CodecCapability;

OSErr NegotiateCodecCapability(Component codec, CodecCapability* requested, CodecCapability** negotiated);
bool IsCodecCompatible(CodecCapability* capability1, CodecCapability* capability2);

/* Effect-specific negotiation */
typedef struct EffectCapability {
    ComponentCapability base;
    OSType effectType;             /* Type of effect */
    uint32_t inputFormats;         /* Supported input formats */
    uint32_t outputFormats;        /* Supported output formats */
    uint32_t processingFlags;      /* Processing capability flags */
    float minParameter;            /* Minimum parameter value */
    float maxParameter;            /* Maximum parameter value */
    uint32_t parameterCount;       /* Number of parameters */
} EffectCapability;

OSErr NegotiateEffectCapability(Component effect, EffectCapability* requested, EffectCapability** negotiated);

/* Capability caching */
typedef struct CapabilityCache {
    Component component;
    ComponentCapability* capabilities;
    int32_t capabilityCount;
    uint64_t cacheTime;
    uint32_t cacheTimeout;
    bool isValid;
    struct CapabilityCache* next;
} CapabilityCache;

OSErr InitCapabilityCache(uint32_t maxEntries, uint32_t timeoutSeconds);
OSErr CleanupCapabilityCache(void);
OSErr CacheComponentCapabilities(Component component, ComponentCapability* capabilities, int32_t count);
OSErr GetCachedCapabilities(Component component, ComponentCapability** capabilities, int32_t* count);
OSErr InvalidateCapabilityCache(Component component);

/* Dynamic capability negotiation */
typedef OSErr (*CapabilityNegotiationCallback)(CapabilityNegotiationContext* context, void* userData);
OSErr RegisterNegotiationCallback(OSType capabilityType, CapabilityNegotiationCallback callback, void* userData);
OSErr UnregisterNegotiationCallback(OSType capabilityType, CapabilityNegotiationCallback callback);

/* Capability profiles */
typedef struct CapabilityProfile {
    char* profileName;
    ComponentCapability* capabilities;
    int32_t capabilityCount;
    VersionCompatibility versionInfo;
    uint32_t profileFlags;
} CapabilityProfile;

OSErr CreateCapabilityProfile(const char* profileName, CapabilityProfile** profile);
OSErr DestroyCapabilityProfile(CapabilityProfile* profile);
OSErr AddCapabilityToProfile(CapabilityProfile* profile, ComponentCapability* capability);
OSErr NegotiateWithProfile(Component component, CapabilityProfile* profile, ComponentCapability** result);

/* Capability serialization */
OSErr SerializeCapabilities(ComponentCapability* capabilities, int32_t count, void** data, uint32_t* size);
OSErr DeserializeCapabilities(void* data, uint32_t size, ComponentCapability** capabilities, int32_t* count);
OSErr SaveCapabilitiesToFile(ComponentCapability* capabilities, int32_t count, const char* filePath);
OSErr LoadCapabilitiesFromFile(const char* filePath, ComponentCapability** capabilities, int32_t* count);

/* Capability debugging and introspection */
OSErr DumpComponentCapabilities(Component component, char** capabilitiesString);
OSErr ValidateCapabilityConsistency(Component component);
OSErr GetCapabilityNegotiationStats(Component component, uint32_t* successCount, uint32_t* failureCount);

/* Platform-specific capability handling */
OSErr GetPlatformSpecificCapabilities(Component component, int16_t platformType,
                                     ComponentCapability** capabilities, int32_t* count);
OSErr FilterCapabilitiesByPlatform(ComponentCapability* capabilities, int32_t count,
                                  int16_t platformType, ComponentCapability** filtered, int32_t* filteredCount);

/* Capability upgrade and migration */
OSErr UpgradeCapability(ComponentCapability* oldCapability, int32_t newVersion, ComponentCapability** newCapability);
OSErr MigrateCapabilities(Component oldComponent, Component newComponent);
OSErr GetCapabilityUpgradePath(ComponentCapability* capability, int32_t targetVersion,
                              ComponentCapability*** upgradePath, int32_t* pathLength);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTNEGOTIATION_H */