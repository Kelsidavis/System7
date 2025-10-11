/* System 7X Nanokernel - VFS-Net Authentication & Credentials
 *
 * Secure credential storage for network filesystem access.
 */

#include "../../../include/Nanokernel/vfs_net_auth.h"
#include "../../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

#define VFSAUTH_MAX_CREDS 64

/* Credential storage */
static struct {
    bool initialized;
    VFSCred credentials[VFSAUTH_MAX_CREDS];
    int cred_count;
} g_vfsauth = { 0 };

/* Initialize authentication subsystem */
bool VFSAuth_Initialize(void) {
    if (g_vfsauth.initialized) {
        serial_printf("[VFS-AUTH] Already initialized\n");
        return true;
    }

    memset(&g_vfsauth, 0, sizeof(g_vfsauth));
    g_vfsauth.initialized = true;

    serial_printf("[VFS-AUTH] Credential management initialized\n");
    return true;
}

/* Find credential slot by host */
static VFSCred* VFSAuth_FindSlot(const char* host) {
    for (int i = 0; i < VFSAUTH_MAX_CREDS; i++) {
        if (g_vfsauth.credentials[i].host[0] != '\0' &&
            strcmp(g_vfsauth.credentials[i].host, host) == 0) {
            return &g_vfsauth.credentials[i];
        }
    }
    return NULL;
}

/* Find free credential slot */
static VFSCred* VFSAuth_AllocSlot(void) {
    for (int i = 0; i < VFSAUTH_MAX_CREDS; i++) {
        if (g_vfsauth.credentials[i].host[0] == '\0') {
            return &g_vfsauth.credentials[i];
        }
    }
    return NULL;
}

/* Register credentials for a host */
int VFSAuth_Register(const char* host, const char* username,
                     const char* token, bool is_oauth) {
    if (!g_vfsauth.initialized) {
        serial_printf("[VFS-AUTH] ERROR: Not initialized\n");
        return -1;
    }

    if (!host || !username || !token) {
        return -1;
    }

    serial_printf("[VFS-AUTH] Registering credentials for '%s'\n", host);

    /* Check if credentials already exist */
    VFSCred* cred = VFSAuth_FindSlot(host);
    if (!cred) {
        /* Allocate new slot */
        cred = VFSAuth_AllocSlot();
        if (!cred) {
            serial_printf("[VFS-AUTH] ERROR: No free credential slots\n");
            return -1;
        }
        g_vfsauth.cred_count++;
    } else {
        serial_printf("[VFS-AUTH] Updating existing credentials\n");
    }

    /* Store credentials */
    memset(cred, 0, sizeof(VFSCred));
    strncpy(cred->host, host, sizeof(cred->host) - 1);
    strncpy(cred->username, username, sizeof(cred->username) - 1);
    strncpy(cred->token, token, sizeof(cred->token) - 1);
    cred->is_oauth = is_oauth;
    cred->expiry = 0;  /* Never expires by default */

    serial_printf("[VFS-AUTH] Stored credentials for '%s' (user='%s', %s)\n",
                  host, username, is_oauth ? "OAuth token" : "password");

    return 0;
}

/* Lookup credentials by host */
const VFSCred* VFSAuth_Lookup(const char* host) {
    if (!g_vfsauth.initialized || !host) {
        return NULL;
    }

    VFSCred* cred = VFSAuth_FindSlot(host);
    if (!cred) {
        serial_printf("[VFS-AUTH] No credentials found for '%s'\n", host);
        return NULL;
    }

    /* Check expiry */
    if (cred->expiry != 0) {
        /* TODO: Get current time and check if expired */
        /* For now, assume never expired */
    }

    serial_printf("[VFS-AUTH] Found credentials for '%s' (user='%s')\n",
                  host, cred->username);

    return cred;
}

/* Remove credentials for a host */
void VFSAuth_Remove(const char* host) {
    if (!g_vfsauth.initialized || !host) {
        return;
    }

    VFSCred* cred = VFSAuth_FindSlot(host);
    if (cred) {
        serial_printf("[VFS-AUTH] Removing credentials for '%s'\n", host);
        memset(cred, 0, sizeof(VFSCred));
        g_vfsauth.cred_count--;
    }
}

/* Clear all credentials */
void VFSAuth_Clear(void) {
    if (!g_vfsauth.initialized) {
        return;
    }

    serial_printf("[VFS-AUTH] Clearing all credentials\n");
    memset(g_vfsauth.credentials, 0, sizeof(g_vfsauth.credentials));
    g_vfsauth.cred_count = 0;
}

/* List registered hosts */
void VFSAuth_ListHosts(void) {
    serial_printf("[VFS-AUTH] === Registered Credentials ===\n");

    bool found = false;
    for (int i = 0; i < VFSAUTH_MAX_CREDS; i++) {
        VFSCred* cred = &g_vfsauth.credentials[i];
        if (cred->host[0] != '\0') {
            serial_printf("[VFS-AUTH] %s → %s (%s)\n",
                          cred->host, cred->username,
                          cred->is_oauth ? "OAuth" : "password");
            found = true;
        }
    }

    if (!found) {
        serial_printf("[VFS-AUTH] (no credentials registered)\n");
    }

    serial_printf("[VFS-AUTH] Total: %d credential(s)\n", g_vfsauth.cred_count);
}
