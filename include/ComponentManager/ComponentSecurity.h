/*
 * ComponentSecurity.h
 *
 * Component Security and Validation API - System 7.1 Portable Implementation
 * Handles component security, validation, and sandboxing
 */

#ifndef COMPONENTSECURITY_H
#define COMPONENTSECURITY_H

#include "ComponentManager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Security levels */
typedef enum {
    kSecurityLevelNone = 0,         /* No security restrictions */
    kSecurityLevelMinimal = 1,      /* Basic validation only */
    kSecurityLevelStandard = 2,     /* Standard security checks */
    kSecurityLevelHigh = 3,         /* High security with sandboxing */
    kSecurityLevelMaximum = 4       /* Maximum security restrictions */
} ComponentSecurityLevel;

/* Security permission flags */
#define kSecurityPermissionFileRead     (1<<0)   /* Read file system */
#define kSecurityPermissionFileWrite    (1<<1)   /* Write file system */
#define kSecurityPermissionNetwork      (1<<2)   /* Network access */
#define kSecurityPermissionRegistry     (1<<3)   /* Registry/preferences access */
#define kSecurityPermissionSystem       (1<<4)   /* System API access */
#define kSecurityPermissionMemory       (1<<5)   /* Memory allocation */
#define kSecurityPermissionThreads      (1<<6)   /* Thread creation */
#define kSecurityPermissionIPC          (1<<7)   /* Inter-process communication */
#define kSecurityPermissionHardware     (1<<8)   /* Hardware access */
#define kSecurityPermissionCrypto       (1<<9)   /* Cryptographic operations */

/* Security context */
typedef struct ComponentSecurityContext {
    ComponentSecurityLevel level;
    uint32_t permissions;
    char* trustedPaths[16];         /* Array of trusted paths */
    int32_t trustedPathCount;
    void* platformSecurityContext;  /* Platform-specific security context */
    char* signatureData;            /* Component signature */
    uint32_t signatureSize;
    bool isVerified;                /* Signature verification status */
    uint64_t creationTime;          /* Security context creation time */
} ComponentSecurityContext;

/* Component signature information */
typedef struct ComponentSignature {
    uint32_t signatureType;         /* Signature algorithm type */
    uint32_t hashType;              /* Hash algorithm type */
    void* signatureData;            /* Raw signature data */
    uint32_t signatureSize;         /* Size of signature data */
    void* certificateData;          /* Certificate data */
    uint32_t certificateSize;       /* Certificate size */
    char* issuer;                   /* Certificate issuer */
    char* subject;                  /* Certificate subject */
    uint64_t validFrom;             /* Certificate valid from */
    uint64_t validTo;               /* Certificate valid to */
} ComponentSignature;

/* Signature types */
#define kSignatureTypeNone          0   /* No signature */
#define kSignatureTypeRSA           1   /* RSA signature */
#define kSignatureTypeDSA           2   /* DSA signature */
#define kSignatureTypeECDSA         3   /* ECDSA signature */
#define kSignatureTypeCustom        255 /* Custom signature scheme */

/* Hash types */
#define kHashTypeMD5                1   /* MD5 hash */
#define kHashTypeSHA1               2   /* SHA-1 hash */
#define kHashTypeSHA256             3   /* SHA-256 hash */
#define kHashTypeSHA512             4   /* SHA-512 hash */

/* Security initialization */
OSErr InitComponentSecurity(void);
void CleanupComponentSecurity(void);

/* Security context management */
OSErr CreateSecurityContext(ComponentSecurityLevel level, uint32_t permissions, ComponentSecurityContext** context);
OSErr DestroySecurityContext(ComponentSecurityContext* context);
OSErr CloneSecurityContext(ComponentSecurityContext* source, ComponentSecurityContext** destination);

/* Component validation */
OSErr ValidateComponent(Component component);
OSErr ValidateComponentFile(const char* filePath);
OSErr ValidateComponentSignature(Component component);
OSErr ValidateComponentPermissions(Component component, uint32_t requestedPermissions);

/* Component signing and verification */
OSErr SignComponent(const char* componentPath, const char* privateKeyPath, const char* certificatePath);
OSErr VerifyComponentSignature(const char* componentPath, const char* publicKeyPath);
OSErr ExtractComponentSignature(const char* componentPath, ComponentSignature** signature);
OSErr ValidateComponentCertificate(ComponentSignature* signature);

/* Security policy management */
typedef struct SecurityPolicy {
    ComponentSecurityLevel defaultLevel;
    uint32_t defaultPermissions;
    bool requireSignature;
    bool allowUnsigned;
    char* trustedIssuers[16];
    int32_t trustedIssuerCount;
    char* blockedComponents[32];
    int32_t blockedComponentCount;
} SecurityPolicy;

OSErr SetSecurityPolicy(SecurityPolicy* policy);
OSErr GetSecurityPolicy(SecurityPolicy** policy);
OSErr LoadSecurityPolicyFromFile(const char* filePath);
OSErr SaveSecurityPolicyToFile(const char* filePath);

/* Component sandboxing */
typedef struct ComponentSandbox {
    void* platformSandbox;          /* Platform-specific sandbox handle */
    char* allowedPaths[32];         /* Allowed file system paths */
    int32_t allowedPathCount;
    uint32_t memoryLimit;           /* Memory usage limit */
    uint32_t cpuTimeLimit;          /* CPU time limit */
    uint32_t networkTimeoutLimit;   /* Network timeout limit */
    bool networkAllowed;            /* Network access allowed */
    bool fileSystemWriteAllowed;    /* File system write allowed */
} ComponentSandbox;

OSErr CreateComponentSandbox(ComponentSecurityContext* securityContext, ComponentSandbox** sandbox);
OSErr DestroyComponentSandbox(ComponentSandbox* sandbox);
OSErr ApplySandboxToComponent(Component component, ComponentSandbox* sandbox);
OSErr ExecuteInSandbox(ComponentSandbox* sandbox, ComponentRoutine routine, ComponentParameters* params);

/* Security monitoring */
typedef enum {
    kSecurityEventValidationFailed,
    kSecurityEventSignatureInvalid,
    kSecurityEventPermissionDenied,
    kSecurityEventSandboxViolation,
    kSecurityEventMaliciousActivity,
    kSecurityEventUnauthorizedAccess
} ComponentSecurityEvent;

typedef void (*SecurityEventCallback)(Component component, ComponentSecurityEvent event, const char* details, void* userData);
OSErr RegisterSecurityEventCallback(SecurityEventCallback callback, void* userData);
OSErr UnregisterSecurityEventCallback(SecurityEventCallback callback);
OSErr ReportSecurityEvent(Component component, ComponentSecurityEvent event, const char* details);

/* Trust management */
typedef struct TrustDatabase {
    char* trustedComponents[256];
    int32_t trustedComponentCount;
    char* blockedComponents[256];
    int32_t blockedComponentCount;
    char* trustedIssuers[64];
    int32_t trustedIssuerCount;
    uint64_t lastUpdate;
} TrustDatabase;

OSErr InitTrustDatabase(TrustDatabase** database);
OSErr CleanupTrustDatabase(TrustDatabase* database);
OSErr AddTrustedComponent(TrustDatabase* database, const char* componentIdentifier);
OSErr RemoveTrustedComponent(TrustDatabase* database, const char* componentIdentifier);
OSErr IsComponentTrusted(TrustDatabase* database, const char* componentIdentifier);
OSErr BlockComponent(TrustDatabase* database, const char* componentIdentifier);
OSErr UnblockComponent(TrustDatabase* database, const char* componentIdentifier);
OSErr IsComponentBlocked(TrustDatabase* database, const char* componentIdentifier);

/* Cryptographic operations */
OSErr ComputeComponentHash(const char* componentPath, uint32_t hashType, void** hash, uint32_t* hashSize);
OSErr VerifyComponentIntegrity(const char* componentPath, void* expectedHash, uint32_t hashSize, uint32_t hashType);
OSErr EncryptComponentData(void* data, uint32_t dataSize, const char* keyPath, void** encryptedData, uint32_t* encryptedSize);
OSErr DecryptComponentData(void* encryptedData, uint32_t encryptedSize, const char* keyPath, void** data, uint32_t* dataSize);

/* Security auditing */
typedef struct SecurityAuditLog {
    ComponentSecurityEvent event;
    Component component;
    char* componentName;
    char* componentPath;
    uint64_t timestamp;
    char* details;
    ComponentSecurityLevel securityLevel;
    uint32_t permissions;
} SecurityAuditLog;

OSErr EnableSecurityAuditing(bool enable);
OSErr GetSecurityAuditLog(SecurityAuditLog** logs, int32_t* logCount);
OSErr ClearSecurityAuditLog(void);
OSErr SaveSecurityAuditLog(const char* filePath);

/* Quarantine management */
typedef struct QuarantineInfo {
    char* componentPath;
    char* reason;
    uint64_t quarantineTime;
    ComponentSecurityEvent triggerEvent;
    bool canRestore;
} QuarantineInfo;

OSErr QuarantineComponent(Component component, const char* reason);
OSErr RestoreFromQuarantine(const char* componentPath);
OSErr GetQuarantineInfo(const char* componentPath, QuarantineInfo** info);
OSErr ListQuarantinedComponents(QuarantineInfo*** components, int32_t* count);

/* Platform-specific security implementations */
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
typedef struct {
    HCRYPTPROV cryptProvider;
    HANDLE jobObject;
    SECURITY_ATTRIBUTES securityAttributes;
} Win32SecurityContext;
#elif defined(__APPLE__)
#include <Security/Security.h>
typedef struct {
    SecCodeRef codeRef;
    SecRequirementRef requirementRef;
    SecStaticCodeRef staticCodeRef;
} MacOSSecurityContext;
#elif defined(__linux__)
typedef struct {
    pid_t sandboxPid;
    int seccompFd;
    int namespaceFd;
} LinuxSecurityContext;
#endif

/* Security configuration */
typedef struct SecurityConfiguration {
    ComponentSecurityLevel minimumLevel;
    bool enforceSignatures;
    bool enableSandboxing;
    bool enableAuditing;
    bool enableQuarantine;
    uint32_t maxMemoryPerComponent;
    uint32_t maxCpuTimePerComponent;
    char* trustedCertificatesPath;
    char* securityPolicyPath;
} SecurityConfiguration;

OSErr LoadSecurityConfiguration(const char* configPath, SecurityConfiguration** config);
OSErr SaveSecurityConfiguration(const char* configPath, SecurityConfiguration* config);
OSErr ApplySecurityConfiguration(SecurityConfiguration* config);

/* Component privilege escalation prevention */
OSErr PreventPrivilegeEscalation(Component component);
OSErr CheckForPrivilegeEscalation(Component component, bool* detected);
OSErr MonitorComponentPrivileges(Component component, bool enable);

/* Secure component loading */
OSErr SecureLoadComponent(const char* componentPath, ComponentSecurityContext* securityContext, Component* component);
OSErr SecureUnloadComponent(Component component);
OSErr VerifyComponentBeforeExecution(Component component);

/* Security testing and fuzzing */
OSErr RunSecurityTests(Component component, bool* passed);
OSErr FuzzComponentAPI(Component component, uint32_t iterations);
OSErr ValidateComponentMemoryUsage(Component component);

#ifdef __cplusplus
}
#endif

#endif /* COMPONENTSECURITY_H */