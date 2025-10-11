/* System 7X Nanokernel - VFS-Net Authentication & Credentials
 *
 * Credential management for network filesystem access.
 * Stores username/password and OAuth tokens securely.
 */

#ifndef NANOKERNEL_VFS_NET_AUTH_H
#define NANOKERNEL_VFS_NET_AUTH_H

#include <stdint.h>
#include <stdbool.h>

#define VFSAUTH_MAX_USERNAME 64
#define VFSAUTH_MAX_TOKEN    256
#define VFSAUTH_MAX_HOST     256

/* Authentication credential structure */
typedef struct {
    char host[VFSAUTH_MAX_HOST];        /* Host or realm */
    char username[VFSAUTH_MAX_USERNAME]; /* Username */
    char token[VFSAUTH_MAX_TOKEN];       /* Password or OAuth token */
    bool is_oauth;                       /* Token type */
    uint64_t expiry;                     /* Token expiry (Unix timestamp, 0 = never) */
} VFSCred;

/* Authentication subsystem API */

/* Initialize authentication subsystem */
bool VFSAuth_Initialize(void);

/* Register credentials for a host */
int VFSAuth_Register(const char* host, const char* username,
                     const char* token, bool is_oauth);

/* Lookup credentials by host
 * Returns: Pointer to credential (internal storage), or NULL if not found
 * Note: Returned pointer is valid until next VFSAuth operation
 */
const VFSCred* VFSAuth_Lookup(const char* host);

/* Remove credentials for a host */
void VFSAuth_Remove(const char* host);

/* Clear all credentials */
void VFSAuth_Clear(void);

/* List registered hosts */
void VFSAuth_ListHosts(void);

#endif /* NANOKERNEL_VFS_NET_AUTH_H */
