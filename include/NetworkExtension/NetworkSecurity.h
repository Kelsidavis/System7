/*
 * NetworkSecurity.h - Network Security and Access Control
 * Mac OS 7.1 Network Extension for network security
 *
 * Provides:
 * - AppleTalk network access control
 * - Authentication and authorization
 * - Security policy enforcement
 * - Encryption and secure communication
 * - Modern security protocol integration (TLS, VPN)
 */

#ifndef NETWORKSECURITY_H
#define NETWORKSECURITY_H

#include "NetworkExtension.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Security Constants
 */
#define SECURITY_MAX_USERS 256
#define SECURITY_MAX_GROUPS 64
#define SECURITY_MAX_POLICIES 128
#define SECURITY_MAX_USERNAME_LENGTH 32
#define SECURITY_MAX_PASSWORD_LENGTH 64
#define SECURITY_MAX_REALM_LENGTH 64
#define SECURITY_MAX_KEY_LENGTH 32

/*
 * Authentication Methods
 */
typedef enum {
    kAuthMethodNone = 0,
    kAuthMethodClearText = 1,
    kAuthMethodChallenge = 2,
    kAuthMethodEncrypted = 3,
    kAuthMethodKerberos = 4,
    kAuthMethodCertificate = 5
} AuthenticationMethod;

/*
 * Security Levels
 */
typedef enum {
    kSecurityLevelNone = 0,
    kSecurityLevelBasic = 1,
    kSecurityLevelMedium = 2,
    kSecurityLevelHigh = 3,
    kSecurityLevelMaximum = 4
} SecurityLevel;

/*
 * User Permissions
 */
typedef enum {
    kPermissionNone = 0x0000,
    kPermissionRead = 0x0001,
    kPermissionWrite = 0x0002,
    kPermissionExecute = 0x0004,
    kPermissionDelete = 0x0008,
    kPermissionAdmin = 0x0010,
    kPermissionNetwork = 0x0020,
    kPermissionService = 0x0040,
    kPermissionAll = 0xFFFF
} UserPermissions;

/*
 * Security Events
 */
typedef enum {
    kSecurityEventLogin = 1,
    kSecurityEventLogout = 2,
    kSecurityEventAccessDenied = 3,
    kSecurityEventAuthFailure = 4,
    kSecurityEventPasswordChange = 5,
    kSecurityEventPolicyViolation = 6,
    kSecurityEventIntrusion = 7
} SecurityEventType;

/*
 * User Information Structure
 */
typedef struct {
    char username[SECURITY_MAX_USERNAME_LENGTH];
    char passwordHash[SECURITY_MAX_PASSWORD_LENGTH];
    char realm[SECURITY_MAX_REALM_LENGTH];
    uint16_t userID;
    uint16_t groupID;
    UserPermissions permissions;
    AuthenticationMethod preferredAuth;
    bool enabled;
    time_t lastLogin;
    time_t lastPasswordChange;
    int loginAttempts;
    time_t lockoutTime;
    AppleTalkAddress lastLoginAddress;
} UserInfo;

/*
 * Group Information Structure
 */
typedef struct {
    char groupName[SECURITY_MAX_USERNAME_LENGTH];
    uint16_t groupID;
    UserPermissions permissions;
    bool enabled;
    uint16_t memberIDs[SECURITY_MAX_USERS];
    int memberCount;
} GroupInfo;

/*
 * Security Policy Structure
 */
typedef struct {
    char policyName[SECURITY_MAX_USERNAME_LENGTH];
    uint16_t policyID;
    SecurityLevel level;
    bool enabled;

    // Authentication policy
    AuthenticationMethod requiredAuth;
    int maxLoginAttempts;
    uint32_t lockoutDuration;
    uint32_t passwordExpiry;

    // Network access policy
    bool allowGuestAccess;
    bool requireEncryption;
    AppleTalkAddress allowedNetworks[16];
    int allowedNetworkCount;
    AppleTalkAddress deniedNetworks[16];
    int deniedNetworkCount;

    // Service access policy
    char allowedServices[16][32];
    int allowedServiceCount;
    char deniedServices[16][32];
    int deniedServiceCount;

    // Time-based policy
    bool timeRestricted;
    uint8_t allowedHours[24]; // Bitmap for each hour
    uint8_t allowedDays[7];   // Bitmap for each day
} SecurityPolicy;

/*
 * Security Event Structure
 */
typedef struct {
    SecurityEventType type;
    time_t timestamp;
    char username[SECURITY_MAX_USERNAME_LENGTH];
    AppleTalkAddress sourceAddress;
    char description[256];
    SecurityLevel severity;
} SecurityEvent;

/*
 * Authentication Challenge Structure
 */
typedef struct {
    uint32_t challengeID;
    uint8_t challenge[16];
    time_t timestamp;
    AppleTalkAddress clientAddress;
    AuthenticationMethod method;
} AuthChallenge;

/*
 * Security Session Structure
 */
typedef struct {
    uint32_t sessionID;
    char username[SECURITY_MAX_USERNAME_LENGTH];
    AppleTalkAddress clientAddress;
    time_t loginTime;
    time_t lastActivity;
    UserPermissions permissions;
    SecurityLevel level;
    bool authenticated;
    bool encrypted;
    uint8_t sessionKey[SECURITY_MAX_KEY_LENGTH];
} SecuritySession;

/*
 * Forward Declarations
 */
typedef struct NetworkSecurity NetworkSecurity;

/*
 * Callback Types
 */
typedef void (*SecurityEventCallback)(const SecurityEvent* event, void* userData);
typedef bool (*AuthenticationCallback)(const char* username, const char* password, AuthenticationMethod method, void* userData);
typedef bool (*AuthorizationCallback)(const char* username, const char* resource, UserPermissions requiredPermissions, void* userData);

/*
 * Network Security Functions
 */

/**
 * Create network security manager
 */
NetworkExtensionError NetworkSecurityCreate(NetworkSecurity** security);

/**
 * Destroy network security manager
 */
void NetworkSecurityDestroy(NetworkSecurity* security);

/**
 * Initialize security system
 */
NetworkExtensionError NetworkSecurityInitialize(NetworkSecurity* security,
                                                 const char* configFile);

/**
 * Set security level
 */
NetworkExtensionError NetworkSecuritySetLevel(NetworkSecurity* security,
                                               SecurityLevel level);

/**
 * Get security level
 */
SecurityLevel NetworkSecurityGetLevel(NetworkSecurity* security);

/*
 * User Management Functions
 */

/**
 * Add user
 */
NetworkExtensionError NetworkSecurityAddUser(NetworkSecurity* security,
                                              const UserInfo* user);

/**
 * Remove user
 */
NetworkExtensionError NetworkSecurityRemoveUser(NetworkSecurity* security,
                                                 const char* username);

/**
 * Update user
 */
NetworkExtensionError NetworkSecurityUpdateUser(NetworkSecurity* security,
                                                 const UserInfo* user);

/**
 * Get user information
 */
NetworkExtensionError NetworkSecurityGetUser(NetworkSecurity* security,
                                              const char* username,
                                              UserInfo* user);

/**
 * Set user password
 */
NetworkExtensionError NetworkSecuritySetPassword(NetworkSecurity* security,
                                                  const char* username,
                                                  const char* password);

/**
 * Enable/disable user
 */
NetworkExtensionError NetworkSecuritySetUserEnabled(NetworkSecurity* security,
                                                     const char* username,
                                                     bool enabled);

/**
 * Get user list
 */
NetworkExtensionError NetworkSecurityGetUserList(NetworkSecurity* security,
                                                  UserInfo* users,
                                                  int maxUsers,
                                                  int* actualUsers);

/*
 * Group Management Functions
 */

/**
 * Add group
 */
NetworkExtensionError NetworkSecurityAddGroup(NetworkSecurity* security,
                                               const GroupInfo* group);

/**
 * Remove group
 */
NetworkExtensionError NetworkSecurityRemoveGroup(NetworkSecurity* security,
                                                  const char* groupName);

/**
 * Add user to group
 */
NetworkExtensionError NetworkSecurityAddUserToGroup(NetworkSecurity* security,
                                                     const char* username,
                                                     const char* groupName);

/**
 * Remove user from group
 */
NetworkExtensionError NetworkSecurityRemoveUserFromGroup(NetworkSecurity* security,
                                                          const char* username,
                                                          const char* groupName);

/*
 * Authentication Functions
 */

/**
 * Authenticate user
 */
NetworkExtensionError NetworkSecurityAuthenticate(NetworkSecurity* security,
                                                   const char* username,
                                                   const char* password,
                                                   AuthenticationMethod method,
                                                   const AppleTalkAddress* clientAddress,
                                                   SecuritySession** session);

/**
 * Create authentication challenge
 */
NetworkExtensionError NetworkSecurityCreateChallenge(NetworkSecurity* security,
                                                      const char* username,
                                                      const AppleTalkAddress* clientAddress,
                                                      AuthChallenge* challenge);

/**
 * Verify authentication response
 */
NetworkExtensionError NetworkSecurityVerifyResponse(NetworkSecurity* security,
                                                     const AuthChallenge* challenge,
                                                     const uint8_t* response,
                                                     size_t responseSize,
                                                     SecuritySession** session);

/**
 * Logout user
 */
NetworkExtensionError NetworkSecurityLogout(NetworkSecurity* security,
                                             SecuritySession* session);

/**
 * Validate session
 */
bool NetworkSecurityValidateSession(NetworkSecurity* security,
                                     const SecuritySession* session);

/*
 * Authorization Functions
 */

/**
 * Check authorization
 */
bool NetworkSecurityAuthorize(NetworkSecurity* security,
                               const SecuritySession* session,
                               const char* resource,
                               UserPermissions requiredPermissions);

/**
 * Check network access
 */
bool NetworkSecurityCheckNetworkAccess(NetworkSecurity* security,
                                        const SecuritySession* session,
                                        const AppleTalkAddress* address);

/**
 * Check service access
 */
bool NetworkSecurityCheckServiceAccess(NetworkSecurity* security,
                                        const SecuritySession* session,
                                        const char* serviceName);

/**
 * Check time-based access
 */
bool NetworkSecurityCheckTimeAccess(NetworkSecurity* security,
                                     const SecuritySession* session);

/*
 * Policy Management Functions
 */

/**
 * Add security policy
 */
NetworkExtensionError NetworkSecurityAddPolicy(NetworkSecurity* security,
                                                const SecurityPolicy* policy);

/**
 * Remove security policy
 */
NetworkExtensionError NetworkSecurityRemovePolicy(NetworkSecurity* security,
                                                   const char* policyName);

/**
 * Update security policy
 */
NetworkExtensionError NetworkSecurityUpdatePolicy(NetworkSecurity* security,
                                                   const SecurityPolicy* policy);

/**
 * Apply policy to user
 */
NetworkExtensionError NetworkSecurityApplyPolicy(NetworkSecurity* security,
                                                  const char* username,
                                                  const char* policyName);

/*
 * Event Logging Functions
 */

/**
 * Log security event
 */
void NetworkSecurityLogEvent(NetworkSecurity* security,
                              const SecurityEvent* event);

/**
 * Get security events
 */
NetworkExtensionError NetworkSecurityGetEvents(NetworkSecurity* security,
                                                SecurityEvent* events,
                                                int maxEvents,
                                                int* actualEvents);

/**
 * Set event callback
 */
void NetworkSecuritySetEventCallback(NetworkSecurity* security,
                                      SecurityEventCallback callback,
                                      void* userData);

/*
 * Encryption Functions
 */

/**
 * Encrypt data
 */
NetworkExtensionError NetworkSecurityEncrypt(NetworkSecurity* security,
                                              const SecuritySession* session,
                                              const void* plaintext,
                                              size_t plaintextSize,
                                              void* ciphertext,
                                              size_t* ciphertextSize);

/**
 * Decrypt data
 */
NetworkExtensionError NetworkSecurityDecrypt(NetworkSecurity* security,
                                              const SecuritySession* session,
                                              const void* ciphertext,
                                              size_t ciphertextSize,
                                              void* plaintext,
                                              size_t* plaintextSize);

/**
 * Generate session key
 */
NetworkExtensionError NetworkSecurityGenerateSessionKey(NetworkSecurity* security,
                                                         SecuritySession* session);

/*
 * Modern Security Integration
 */

/**
 * Enable TLS support
 */
NetworkExtensionError NetworkSecurityEnableTLS(NetworkSecurity* security);

/**
 * Configure certificate
 */
NetworkExtensionError NetworkSecuritySetCertificate(NetworkSecurity* security,
                                                     const char* certFile,
                                                     const char* keyFile);

/**
 * Enable VPN support
 */
NetworkExtensionError NetworkSecurityEnableVPN(NetworkSecurity* security,
                                                const char* vpnType);

/**
 * Set firewall rules
 */
NetworkExtensionError NetworkSecuritySetFirewallRules(NetworkSecurity* security,
                                                       const char* rulesFile);

/*
 * Configuration Functions
 */

/**
 * Load configuration
 */
NetworkExtensionError NetworkSecurityLoadConfig(NetworkSecurity* security,
                                                 const char* configFile);

/**
 * Save configuration
 */
NetworkExtensionError NetworkSecuritySaveConfig(NetworkSecurity* security,
                                                 const char* configFile);

/**
 * Set callback handlers
 */
void NetworkSecuritySetCallbacks(NetworkSecurity* security,
                                  AuthenticationCallback authCallback,
                                  AuthorizationCallback authzCallback,
                                  void* userData);

/*
 * Utility Functions
 */

/**
 * Hash password
 */
void NetworkSecurityHashPassword(const char* password,
                                 const char* salt,
                                 char* hash,
                                 size_t hashSize);

/**
 * Generate salt
 */
void NetworkSecurityGenerateSalt(char* salt, size_t saltSize);

/**
 * Validate password strength
 */
bool NetworkSecurityValidatePassword(const char* password);

/**
 * Get authentication method string
 */
const char* NetworkSecurityGetAuthMethodString(AuthenticationMethod method);

/**
 * Get security level string
 */
const char* NetworkSecurityGetSecurityLevelString(SecurityLevel level);

/**
 * Get event type string
 */
const char* NetworkSecurityGetEventTypeString(SecurityEventType type);

#ifdef __cplusplus
}
#endif

#endif /* NETWORKSECURITY_H */