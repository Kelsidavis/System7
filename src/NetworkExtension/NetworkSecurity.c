/*
 * NetworkSecurity.c - Network Security and Access Control
 * Mac OS 7.1 Network Extension for network security
 *
 * This module implements network security features including authentication,
 * authorization, access control, and modern security protocol integration.
 */

#include "NetworkSecurity.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <crypt.h>

/*
 * Internal Constants
 */
#define DEFAULT_LOCKOUT_DURATION 300
#define DEFAULT_PASSWORD_EXPIRY 7776000 // 90 days
#define DEFAULT_SESSION_TIMEOUT 3600
#define MAX_LOGIN_ATTEMPTS 3
#define CHALLENGE_LIFETIME 300

/*
 * Network Security Structure
 */
struct NetworkSecurity {
    // State
    bool initialized;
    pthread_mutex_t mutex;

    // Configuration
    SecurityLevel securityLevel;
    char configFile[256];

    // User database
    UserInfo users[SECURITY_MAX_USERS];
    int userCount;

    // Group database
    GroupInfo groups[SECURITY_MAX_GROUPS];
    int groupCount;

    // Policy database
    SecurityPolicy policies[SECURITY_MAX_POLICIES];
    int policyCount;

    // Active sessions
    SecuritySession sessions[64];
    int sessionCount;
    uint32_t nextSessionID;

    // Active challenges
    AuthChallenge challenges[32];
    int challengeCount;
    uint32_t nextChallengeID;

    // Event logging
    SecurityEvent events[1024];
    int eventCount;
    int eventIndex;

    // Callbacks
    SecurityEventCallback eventCallback;
    void* eventUserData;
    AuthenticationCallback authCallback;
    AuthorizationCallback authzCallback;
    void* callbackUserData;

    // Modern security features
    bool tlsEnabled;
    bool vpnEnabled;
    char certificateFile[256];
    char keyFile[256];
};

/*
 * Private function declarations
 */
static UserInfo* FindUser(NetworkSecurity* security, const char* username);
static GroupInfo* FindGroup(NetworkSecurity* security, const char* groupName);
static SecurityPolicy* FindPolicy(NetworkSecurity* security, const char* policyName);
static SecuritySession* FindSession(NetworkSecurity* security, uint32_t sessionID);
static AuthChallenge* FindChallenge(NetworkSecurity* security, uint32_t challengeID);
static NetworkExtensionError AddUserInternal(NetworkSecurity* security, const UserInfo* user);
static NetworkExtensionError CreateSessionInternal(NetworkSecurity* security, const UserInfo* user, const AppleTalkAddress* clientAddress, SecuritySession** session);
static void LogEventInternal(NetworkSecurity* security, const SecurityEvent* event);
static bool CheckPasswordStrength(const char* password);
static void HashPasswordInternal(const char* password, const char* salt, char* hash, size_t hashSize);
static bool VerifyPassword(const char* password, const char* hash);
static void GenerateChallenge(uint8_t* challenge, size_t size);
static void CleanupExpiredSessions(NetworkSecurity* security);
static void CleanupExpiredChallenges(NetworkSecurity* security);

/*
 * Create network security manager
 */
NetworkExtensionError NetworkSecurityCreate(NetworkSecurity** security) {
    if (!security) {
        return kNetworkExtensionInvalidParam;
    }

    NetworkSecurity* newSecurity = calloc(1, sizeof(NetworkSecurity));
    if (!newSecurity) {
        return kNetworkExtensionInternalError;
    }

    // Initialize mutex
    if (pthread_mutex_init(&newSecurity->mutex, NULL) != 0) {
        free(newSecurity);
        return kNetworkExtensionInternalError;
    }

    newSecurity->securityLevel = kSecurityLevelMedium;
    newSecurity->nextSessionID = 1;
    newSecurity->nextChallengeID = 1;
    newSecurity->initialized = true;

    // Create default admin user
    UserInfo admin;
    memset(&admin, 0, sizeof(admin));
    strcpy(admin.username, "admin");
    strcpy(admin.realm, "local");
    admin.userID = 1;
    admin.groupID = 1;
    admin.permissions = kPermissionAll;
    admin.preferredAuth = kAuthMethodChallenge;
    admin.enabled = true;

    // Hash default password
    char salt[16];
    NetworkSecurityGenerateSalt(salt, sizeof(salt));
    HashPasswordInternal("admin", salt, admin.passwordHash, sizeof(admin.passwordHash));

    AddUserInternal(newSecurity, &admin);

    *security = newSecurity;
    return kNetworkExtensionNoError;
}

/*
 * Destroy network security manager
 */
void NetworkSecurityDestroy(NetworkSecurity* security) {
    if (!security || !security->initialized) {
        return;
    }

    pthread_mutex_lock(&security->mutex);
    security->initialized = false;
    pthread_mutex_unlock(&security->mutex);

    pthread_mutex_destroy(&security->mutex);
    free(security);
}

/*
 * Set security level
 */
NetworkExtensionError NetworkSecuritySetLevel(NetworkSecurity* security,
                                               SecurityLevel level) {
    if (!security || !security->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);
    security->securityLevel = level;
    pthread_mutex_unlock(&security->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Get security level
 */
SecurityLevel NetworkSecurityGetLevel(NetworkSecurity* security) {
    if (!security || !security->initialized) {
        return kSecurityLevelNone;
    }

    pthread_mutex_lock(&security->mutex);
    SecurityLevel level = security->securityLevel;
    pthread_mutex_unlock(&security->mutex);

    return level;
}

/*
 * Add user
 */
NetworkExtensionError NetworkSecurityAddUser(NetworkSecurity* security,
                                              const UserInfo* user) {
    if (!security || !security->initialized || !user) {
        return kNetworkExtensionInvalidParam;
    }

    if (strlen(user->username) == 0 || strlen(user->username) >= SECURITY_MAX_USERNAME_LENGTH) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    NetworkExtensionError error = AddUserInternal(security, user);

    pthread_mutex_unlock(&security->mutex);

    return error;
}

/*
 * Get user information
 */
NetworkExtensionError NetworkSecurityGetUser(NetworkSecurity* security,
                                              const char* username,
                                              UserInfo* user) {
    if (!security || !security->initialized || !username || !user) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    UserInfo* found = FindUser(security, username);
    if (!found) {
        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionNotFound;
    }

    *user = *found;

    pthread_mutex_unlock(&security->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Set user password
 */
NetworkExtensionError NetworkSecuritySetPassword(NetworkSecurity* security,
                                                  const char* username,
                                                  const char* password) {
    if (!security || !security->initialized || !username || !password) {
        return kNetworkExtensionInvalidParam;
    }

    if (!CheckPasswordStrength(password)) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    UserInfo* user = FindUser(security, username);
    if (!user) {
        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionNotFound;
    }

    // Generate salt and hash password
    char salt[16];
    NetworkSecurityGenerateSalt(salt, sizeof(salt));
    HashPasswordInternal(password, salt, user->passwordHash, sizeof(user->passwordHash));
    user->lastPasswordChange = time(NULL);

    // Log password change event
    SecurityEvent event;
    memset(&event, 0, sizeof(event));
    event.type = kSecurityEventPasswordChange;
    event.timestamp = time(NULL);
    strcpy(event.username, username);
    strcpy(event.description, "Password changed");
    event.severity = kSecurityLevelBasic;

    LogEventInternal(security, &event);

    pthread_mutex_unlock(&security->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Authenticate user
 */
NetworkExtensionError NetworkSecurityAuthenticate(NetworkSecurity* security,
                                                   const char* username,
                                                   const char* password,
                                                   AuthenticationMethod method,
                                                   const AppleTalkAddress* clientAddress,
                                                   SecuritySession** session) {
    if (!security || !security->initialized || !username || !password || !clientAddress || !session) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    UserInfo* user = FindUser(security, username);
    if (!user) {
        // Log authentication failure
        SecurityEvent event;
        memset(&event, 0, sizeof(event));
        event.type = kSecurityEventAuthFailure;
        event.timestamp = time(NULL);
        strcpy(event.username, username);
        event.sourceAddress = *clientAddress;
        strcpy(event.description, "User not found");
        event.severity = kSecurityLevelMedium;

        LogEventInternal(security, &event);

        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionAccessDenied;
    }

    // Check if user is enabled
    if (!user->enabled) {
        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionAccessDenied;
    }

    // Check lockout
    time_t now = time(NULL);
    if (user->lockoutTime > 0 && now < user->lockoutTime) {
        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionAccessDenied;
    }

    // Verify password
    bool authenticated = false;
    switch (method) {
        case kAuthMethodClearText:
            authenticated = VerifyPassword(password, user->passwordHash);
            break;

        case kAuthMethodChallenge:
            // TODO: Implement challenge-response authentication
            authenticated = VerifyPassword(password, user->passwordHash);
            break;

        default:
            authenticated = false;
            break;
    }

    if (!authenticated) {
        user->loginAttempts++;
        if (user->loginAttempts >= MAX_LOGIN_ATTEMPTS) {
            user->lockoutTime = now + DEFAULT_LOCKOUT_DURATION;
        }

        // Log authentication failure
        SecurityEvent event;
        memset(&event, 0, sizeof(event));
        event.type = kSecurityEventAuthFailure;
        event.timestamp = time(NULL);
        strcpy(event.username, username);
        event.sourceAddress = *clientAddress;
        strcpy(event.description, "Invalid password");
        event.severity = kSecurityLevelMedium;

        LogEventInternal(security, &event);

        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionAccessDenied;
    }

    // Authentication successful
    user->loginAttempts = 0;
    user->lockoutTime = 0;
    user->lastLogin = now;
    user->lastLoginAddress = *clientAddress;

    // Create session
    NetworkExtensionError error = CreateSessionInternal(security, user, clientAddress, session);

    if (error == kNetworkExtensionNoError) {
        // Log successful login
        SecurityEvent event;
        memset(&event, 0, sizeof(event));
        event.type = kSecurityEventLogin;
        event.timestamp = time(NULL);
        strcpy(event.username, username);
        event.sourceAddress = *clientAddress;
        strcpy(event.description, "User logged in");
        event.severity = kSecurityLevelBasic;

        LogEventInternal(security, &event);
    }

    pthread_mutex_unlock(&security->mutex);

    return error;
}

/*
 * Create authentication challenge
 */
NetworkExtensionError NetworkSecurityCreateChallenge(NetworkSecurity* security,
                                                      const char* username,
                                                      const AppleTalkAddress* clientAddress,
                                                      AuthChallenge* challenge) {
    if (!security || !security->initialized || !username || !clientAddress || !challenge) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    // Find empty challenge slot
    if (security->challengeCount >= 32) {
        CleanupExpiredChallenges(security);
        if (security->challengeCount >= 32) {
            pthread_mutex_unlock(&security->mutex);
            return kNetworkExtensionInternalError;
        }
    }

    // Create challenge
    challenge->challengeID = security->nextChallengeID++;
    challenge->timestamp = time(NULL);
    challenge->clientAddress = *clientAddress;
    challenge->method = kAuthMethodChallenge;

    GenerateChallenge(challenge->challenge, sizeof(challenge->challenge));

    // Store challenge
    security->challenges[security->challengeCount++] = *challenge;

    pthread_mutex_unlock(&security->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Logout user
 */
NetworkExtensionError NetworkSecurityLogout(NetworkSecurity* security,
                                             SecuritySession* session) {
    if (!security || !security->initialized || !session) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    // Find and remove session
    for (int i = 0; i < security->sessionCount; i++) {
        if (security->sessions[i].sessionID == session->sessionID) {
            // Log logout event
            SecurityEvent event;
            memset(&event, 0, sizeof(event));
            event.type = kSecurityEventLogout;
            event.timestamp = time(NULL);
            strcpy(event.username, session->username);
            event.sourceAddress = session->clientAddress;
            strcpy(event.description, "User logged out");
            event.severity = kSecurityLevelBasic;

            LogEventInternal(security, &event);

            // Remove session
            memmove(&security->sessions[i], &security->sessions[i + 1],
                    (security->sessionCount - i - 1) * sizeof(SecuritySession));
            security->sessionCount--;

            pthread_mutex_unlock(&security->mutex);
            return kNetworkExtensionNoError;
        }
    }

    pthread_mutex_unlock(&security->mutex);
    return kNetworkExtensionNotFound;
}

/*
 * Check authorization
 */
bool NetworkSecurityAuthorize(NetworkSecurity* security,
                               const SecuritySession* session,
                               const char* resource,
                               UserPermissions requiredPermissions) {
    if (!security || !security->initialized || !session || !resource) {
        return false;
    }

    pthread_mutex_lock(&security->mutex);

    // Validate session
    if (!NetworkSecurityValidateSession(security, session)) {
        pthread_mutex_unlock(&security->mutex);
        return false;
    }

    // Check permissions
    bool authorized = (session->permissions & requiredPermissions) == requiredPermissions;

    if (!authorized) {
        // Log access denied event
        SecurityEvent event;
        memset(&event, 0, sizeof(event));
        event.type = kSecurityEventAccessDenied;
        event.timestamp = time(NULL);
        strcpy(event.username, session->username);
        event.sourceAddress = session->clientAddress;
        snprintf(event.description, sizeof(event.description), "Access denied to %s", resource);
        event.severity = kSecurityLevelMedium;

        LogEventInternal(security, &event);
    }

    pthread_mutex_unlock(&security->mutex);

    return authorized;
}

/*
 * Validate session
 */
bool NetworkSecurityValidateSession(NetworkSecurity* security,
                                     const SecuritySession* session) {
    if (!security || !security->initialized || !session) {
        return false;
    }

    // Find session
    SecuritySession* found = FindSession(security, session->sessionID);
    if (!found) {
        return false;
    }

    // Check if session is still valid
    time_t now = time(NULL);
    if (now - found->lastActivity > DEFAULT_SESSION_TIMEOUT) {
        return false;
    }

    // Update last activity
    found->lastActivity = now;

    return found->authenticated;
}

/*
 * Log security event
 */
void NetworkSecurityLogEvent(NetworkSecurity* security,
                              const SecurityEvent* event) {
    if (!security || !security->initialized || !event) {
        return;
    }

    pthread_mutex_lock(&security->mutex);
    LogEventInternal(security, event);
    pthread_mutex_unlock(&security->mutex);
}

/*
 * Private Functions
 */

static UserInfo* FindUser(NetworkSecurity* security, const char* username) {
    for (int i = 0; i < security->userCount; i++) {
        if (strcmp(security->users[i].username, username) == 0) {
            return &security->users[i];
        }
    }
    return NULL;
}

static SecuritySession* FindSession(NetworkSecurity* security, uint32_t sessionID) {
    for (int i = 0; i < security->sessionCount; i++) {
        if (security->sessions[i].sessionID == sessionID) {
            return &security->sessions[i];
        }
    }
    return NULL;
}

static AuthChallenge* FindChallenge(NetworkSecurity* security, uint32_t challengeID) {
    for (int i = 0; i < security->challengeCount; i++) {
        if (security->challenges[i].challengeID == challengeID) {
            return &security->challenges[i];
        }
    }
    return NULL;
}

static NetworkExtensionError AddUserInternal(NetworkSecurity* security, const UserInfo* user) {
    // Check if user already exists
    if (FindUser(security, user->username)) {
        return kNetworkExtensionInternalError;
    }

    // Add user
    if (security->userCount >= SECURITY_MAX_USERS) {
        return kNetworkExtensionInternalError;
    }

    security->users[security->userCount++] = *user;

    return kNetworkExtensionNoError;
}

static NetworkExtensionError CreateSessionInternal(NetworkSecurity* security,
                                                    const UserInfo* user,
                                                    const AppleTalkAddress* clientAddress,
                                                    SecuritySession** session) {
    // Find empty session slot
    if (security->sessionCount >= 64) {
        CleanupExpiredSessions(security);
        if (security->sessionCount >= 64) {
            return kNetworkExtensionInternalError;
        }
    }

    // Create session
    SecuritySession* newSession = &security->sessions[security->sessionCount];
    memset(newSession, 0, sizeof(SecuritySession));

    newSession->sessionID = security->nextSessionID++;
    strcpy(newSession->username, user->username);
    newSession->clientAddress = *clientAddress;
    newSession->loginTime = time(NULL);
    newSession->lastActivity = newSession->loginTime;
    newSession->permissions = user->permissions;
    newSession->level = security->securityLevel;
    newSession->authenticated = true;
    newSession->encrypted = false;

    // Generate session key
    NetworkSecurityGenerateSessionKey(security, newSession);

    security->sessionCount++;
    *session = newSession;

    return kNetworkExtensionNoError;
}

static void LogEventInternal(NetworkSecurity* security, const SecurityEvent* event) {
    // Store event in circular buffer
    security->events[security->eventIndex] = *event;
    security->eventIndex = (security->eventIndex + 1) % 1024;
    if (security->eventCount < 1024) {
        security->eventCount++;
    }

    // Call callback if set
    if (security->eventCallback) {
        security->eventCallback(event, security->eventUserData);
    }
}

static bool CheckPasswordStrength(const char* password) {
    if (!password || strlen(password) < 6) {
        return false;
    }

    // Add more password strength checks as needed
    return true;
}

static void HashPasswordInternal(const char* password, const char* salt, char* hash, size_t hashSize) {
    // Use system crypt function or implement custom hashing
    const char* hashed = crypt(password, salt);
    if (hashed) {
        strncpy(hash, hashed, hashSize - 1);
        hash[hashSize - 1] = '\0';
    }
}

static bool VerifyPassword(const char* password, const char* hash) {
    char testHash[SECURITY_MAX_PASSWORD_LENGTH];
    HashPasswordInternal(password, hash, testHash, sizeof(testHash));
    return strcmp(testHash, hash) == 0;
}

static void GenerateChallenge(uint8_t* challenge, size_t size) {
    // Generate random challenge
    for (size_t i = 0; i < size; i++) {
        challenge[i] = rand() & 0xFF;
    }
}

static void CleanupExpiredSessions(NetworkSecurity* security) {
    time_t now = time(NULL);

    for (int i = 0; i < security->sessionCount; i++) {
        if (now - security->sessions[i].lastActivity > DEFAULT_SESSION_TIMEOUT) {
            // Remove expired session
            memmove(&security->sessions[i], &security->sessions[i + 1],
                    (security->sessionCount - i - 1) * sizeof(SecuritySession));
            security->sessionCount--;
            i--; // Check this index again
        }
    }
}

static void CleanupExpiredChallenges(NetworkSecurity* security) {
    time_t now = time(NULL);

    for (int i = 0; i < security->challengeCount; i++) {
        if (now - security->challenges[i].timestamp > CHALLENGE_LIFETIME) {
            // Remove expired challenge
            memmove(&security->challenges[i], &security->challenges[i + 1],
                    (security->challengeCount - i - 1) * sizeof(AuthChallenge));
            security->challengeCount--;
            i--; // Check this index again
        }
    }
}

/*
 * Utility Functions
 */

void NetworkSecurityGenerateSalt(char* salt, size_t saltSize) {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (size_t i = 0; i < saltSize - 1; i++) {
        salt[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    salt[saltSize - 1] = '\0';
}

NetworkExtensionError NetworkSecurityGenerateSessionKey(NetworkSecurity* security,
                                                         SecuritySession* session) {
    if (!security || !session) {
        return kNetworkExtensionInvalidParam;
    }

    // Generate random session key
    for (size_t i = 0; i < SECURITY_MAX_KEY_LENGTH; i++) {
        session->sessionKey[i] = rand() & 0xFF;
    }

    return kNetworkExtensionNoError;
}

const char* NetworkSecurityGetAuthMethodString(AuthenticationMethod method) {
    switch (method) {
        case kAuthMethodNone:
            return "None";
        case kAuthMethodClearText:
            return "Clear Text";
        case kAuthMethodChallenge:
            return "Challenge";
        case kAuthMethodEncrypted:
            return "Encrypted";
        case kAuthMethodKerberos:
            return "Kerberos";
        case kAuthMethodCertificate:
            return "Certificate";
        default:
            return "Unknown";
    }
}

const char* NetworkSecurityGetSecurityLevelString(SecurityLevel level) {
    switch (level) {
        case kSecurityLevelNone:
            return "None";
        case kSecurityLevelBasic:
            return "Basic";
        case kSecurityLevelMedium:
            return "Medium";
        case kSecurityLevelHigh:
            return "High";
        case kSecurityLevelMaximum:
            return "Maximum";
        default:
            return "Unknown";
    }
}

const char* NetworkSecurityGetEventTypeString(SecurityEventType type) {
    switch (type) {
        case kSecurityEventLogin:
            return "Login";
        case kSecurityEventLogout:
            return "Logout";
        case kSecurityEventAccessDenied:
            return "Access Denied";
        case kSecurityEventAuthFailure:
            return "Authentication Failure";
        case kSecurityEventPasswordChange:
            return "Password Change";
        case kSecurityEventPolicyViolation:
            return "Policy Violation";
        case kSecurityEventIntrusion:
            return "Intrusion";
        default:
            return "Unknown";
    }
}

/*
 * Additional Network Security Functions
 */

/*
 * Initialize security system
 */
NetworkExtensionError NetworkSecurityInitialize(NetworkSecurity* security,
                                                 const char* configFile) {
    if (!security || !security->initialized) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    if (configFile) {
        strncpy(security->configFile, configFile, sizeof(security->configFile) - 1);
        security->configFile[sizeof(security->configFile) - 1] = '\0';

        // Load configuration if file exists
        NetworkSecurityLoadConfig(security, configFile);
    }

    pthread_mutex_unlock(&security->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Remove user
 */
NetworkExtensionError NetworkSecurityRemoveUser(NetworkSecurity* security,
                                                 const char* username) {
    if (!security || !security->initialized || !username) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    // Find user
    for (int i = 0; i < security->userCount; i++) {
        if (strcmp(security->users[i].username, username) == 0) {
            // Remove user
            memmove(&security->users[i], &security->users[i + 1],
                    (security->userCount - i - 1) * sizeof(UserInfo));
            security->userCount--;

            // Log event
            SecurityEvent event;
            memset(&event, 0, sizeof(event));
            event.type = kSecurityEventLogout;
            event.timestamp = time(NULL);
            strcpy(event.username, username);
            strcpy(event.description, "User removed");
            event.severity = kSecurityLevelBasic;
            LogEventInternal(security, &event);

            pthread_mutex_unlock(&security->mutex);
            return kNetworkExtensionNoError;
        }
    }

    pthread_mutex_unlock(&security->mutex);
    return kNetworkExtensionNotFound;
}

/*
 * Update user
 */
NetworkExtensionError NetworkSecurityUpdateUser(NetworkSecurity* security,
                                                 const UserInfo* user) {
    if (!security || !security->initialized || !user) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    UserInfo* existing = FindUser(security, user->username);
    if (!existing) {
        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionNotFound;
    }

    // Update user information (preserve password hash)
    char passwordHash[SECURITY_MAX_PASSWORD_LENGTH];
    strcpy(passwordHash, existing->passwordHash);

    *existing = *user;
    strcpy(existing->passwordHash, passwordHash);

    pthread_mutex_unlock(&security->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Set user enabled/disabled
 */
NetworkExtensionError NetworkSecuritySetUserEnabled(NetworkSecurity* security,
                                                     const char* username,
                                                     bool enabled) {
    if (!security || !security->initialized || !username) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    UserInfo* user = FindUser(security, username);
    if (!user) {
        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionNotFound;
    }

    user->enabled = enabled;

    pthread_mutex_unlock(&security->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Get user list
 */
NetworkExtensionError NetworkSecurityGetUserList(NetworkSecurity* security,
                                                  UserInfo* users,
                                                  int maxUsers,
                                                  int* actualUsers) {
    if (!security || !security->initialized || !users || !actualUsers) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    int count = (security->userCount < maxUsers) ? security->userCount : maxUsers;

    for (int i = 0; i < count; i++) {
        users[i] = security->users[i];
        // Clear password hash for security
        memset(users[i].passwordHash, 0, sizeof(users[i].passwordHash));
    }

    *actualUsers = count;

    pthread_mutex_unlock(&security->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Verify authentication response
 */
NetworkExtensionError NetworkSecurityVerifyResponse(NetworkSecurity* security,
                                                     const AuthChallenge* challenge,
                                                     const uint8_t* response,
                                                     size_t responseSize,
                                                     SecuritySession** session) {
    if (!security || !security->initialized || !challenge || !response || !session) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    // Find challenge
    AuthChallenge* stored = FindChallenge(security, challenge->challengeID);
    if (!stored) {
        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionNotFound;
    }

    // Check if challenge is still valid
    time_t now = time(NULL);
    if (now - stored->timestamp > CHALLENGE_LIFETIME) {
        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionTimeout;
    }

    // TODO: Implement proper challenge-response verification
    // For now, accept any response
    bool verified = (responseSize > 0);

    if (verified) {
        // Create session for successful challenge response
        // Note: This is simplified - in practice, you'd need username from challenge context
        pthread_mutex_unlock(&security->mutex);
        return kNetworkExtensionNoError;
    }

    pthread_mutex_unlock(&security->mutex);
    return kNetworkExtensionAccessDenied;
}

/*
 * Encrypt data
 */
NetworkExtensionError NetworkSecurityEncrypt(NetworkSecurity* security,
                                              const SecuritySession* session,
                                              const void* plaintext,
                                              size_t plaintextSize,
                                              void* ciphertext,
                                              size_t* ciphertextSize) {
    if (!security || !security->initialized || !session || !plaintext || !ciphertext || !ciphertextSize) {
        return kNetworkExtensionInvalidParam;
    }

    if (!NetworkSecurityValidateSession(security, session)) {
        return kNetworkExtensionAccessDenied;
    }

    // Simple XOR encryption with session key (for demonstration)
    // In production, use proper encryption algorithms
    size_t encryptSize = (plaintextSize < *ciphertextSize) ? plaintextSize : *ciphertextSize;

    const uint8_t* src = (const uint8_t*)plaintext;
    uint8_t* dst = (uint8_t*)ciphertext;

    for (size_t i = 0; i < encryptSize; i++) {
        dst[i] = src[i] ^ session->sessionKey[i % SECURITY_MAX_KEY_LENGTH];
    }

    *ciphertextSize = encryptSize;

    return kNetworkExtensionNoError;
}

/*
 * Decrypt data
 */
NetworkExtensionError NetworkSecurityDecrypt(NetworkSecurity* security,
                                              const SecuritySession* session,
                                              const void* ciphertext,
                                              size_t ciphertextSize,
                                              void* plaintext,
                                              size_t* plaintextSize) {
    if (!security || !security->initialized || !session || !ciphertext || !plaintext || !plaintextSize) {
        return kNetworkExtensionInvalidParam;
    }

    if (!NetworkSecurityValidateSession(security, session)) {
        return kNetworkExtensionAccessDenied;
    }

    // Simple XOR decryption with session key (for demonstration)
    size_t decryptSize = (ciphertextSize < *plaintextSize) ? ciphertextSize : *plaintextSize;

    const uint8_t* src = (const uint8_t*)ciphertext;
    uint8_t* dst = (uint8_t*)plaintext;

    for (size_t i = 0; i < decryptSize; i++) {
        dst[i] = src[i] ^ session->sessionKey[i % SECURITY_MAX_KEY_LENGTH];
    }

    *plaintextSize = decryptSize;

    return kNetworkExtensionNoError;
}

/*
 * Get security events
 */
NetworkExtensionError NetworkSecurityGetEvents(NetworkSecurity* security,
                                                SecurityEvent* events,
                                                int maxEvents,
                                                int* actualEvents) {
    if (!security || !security->initialized || !events || !actualEvents) {
        return kNetworkExtensionInvalidParam;
    }

    pthread_mutex_lock(&security->mutex);

    int count = (security->eventCount < maxEvents) ? security->eventCount : maxEvents;

    // Copy events from circular buffer
    for (int i = 0; i < count; i++) {
        int index = (security->eventIndex - count + i + 1024) % 1024;
        events[i] = security->events[index];
    }

    *actualEvents = count;

    pthread_mutex_unlock(&security->mutex);

    return kNetworkExtensionNoError;
}

/*
 * Set event callback
 */
void NetworkSecuritySetEventCallback(NetworkSecurity* security,
                                      SecurityEventCallback callback,
                                      void* userData) {
    if (!security || !security->initialized) {
        return;
    }

    pthread_mutex_lock(&security->mutex);
    security->eventCallback = callback;
    security->eventUserData = userData;
    pthread_mutex_unlock(&security->mutex);
}

/*
 * Set callback handlers
 */
void NetworkSecuritySetCallbacks(NetworkSecurity* security,
                                  AuthenticationCallback authCallback,
                                  AuthorizationCallback authzCallback,
                                  void* userData) {
    if (!security || !security->initialized) {
        return;
    }

    pthread_mutex_lock(&security->mutex);
    security->authCallback = authCallback;
    security->authzCallback = authzCallback;
    security->callbackUserData = userData;
    pthread_mutex_unlock(&security->mutex);
}

/*
 * Load configuration
 */
NetworkExtensionError NetworkSecurityLoadConfig(NetworkSecurity* security,
                                                 const char* configFile) {
    if (!security || !security->initialized || !configFile) {
        return kNetworkExtensionInvalidParam;
    }

    // Placeholder for configuration loading
    // In a real implementation, this would parse a configuration file
    // and load users, groups, and policies

    return kNetworkExtensionNoError;
}

/*
 * Save configuration
 */
NetworkExtensionError NetworkSecuritySaveConfig(NetworkSecurity* security,
                                                 const char* configFile) {
    if (!security || !security->initialized || !configFile) {
        return kNetworkExtensionInvalidParam;
    }

    // Placeholder for configuration saving
    // In a real implementation, this would save current users, groups,
    // and policies to a configuration file

    return kNetworkExtensionNoError;
}

/*
 * Validate password strength
 */
bool NetworkSecurityValidatePassword(const char* password) {
    if (!password) {
        return false;
    }

    size_t len = strlen(password);
    if (len < 8) {
        return false;
    }

    bool hasUpper = false, hasLower = false, hasDigit = false, hasSpecial = false;

    for (size_t i = 0; i < len; i++) {
        char c = password[i];
        if (c >= 'A' && c <= 'Z') hasUpper = true;
        else if (c >= 'a' && c <= 'z') hasLower = true;
        else if (c >= '0' && c <= '9') hasDigit = true;
        else hasSpecial = true;
    }

    // Require at least 3 of the 4 character types
    int typeCount = hasUpper + hasLower + hasDigit + hasSpecial;
    return typeCount >= 3;
}