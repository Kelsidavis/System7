/* System 7X Nanokernel - VFS-Net Core Implementation
 *
 * Network filesystem abstraction layer.
 */

#include "../../../include/Nanokernel/vfs_net.h"
#include "../../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

#define VFSNET_MAX_DRIVERS 16
#define VFSNET_MAX_MOUNTS  32

/* VFS-Net global state */
static struct {
    bool initialized;
    VFSNetDriver* drivers[VFSNET_MAX_DRIVERS];
    int driver_count;
    VFSNetMount mounts[VFSNET_MAX_MOUNTS];
    int mount_count;
} g_vfsnet = { 0 };

/* Initialize VFS-Net subsystem */
bool VFSNet_Initialize(void) {
    if (g_vfsnet.initialized) {
        serial_printf("[VFS-NET] Already initialized\n");
        return true;
    }

    memset(&g_vfsnet, 0, sizeof(g_vfsnet));
    g_vfsnet.initialized = true;

    serial_printf("[VFS-NET] Network filesystem layer initialized\n");
    return true;
}

/* Shutdown VFS-Net subsystem */
void VFSNet_Shutdown(void) {
    if (!g_vfsnet.initialized) {
        return;
    }

    /* Unmount all network filesystems */
    for (int i = 0; i < VFSNET_MAX_MOUNTS; i++) {
        if (g_vfsnet.mounts[i].mounted) {
            VFSNet_Unmount(g_vfsnet.mounts[i].mount_point);
        }
    }

    g_vfsnet.initialized = false;
    serial_printf("[VFS-NET] Shutdown complete\n");
}

/* Register a network filesystem driver */
bool VFSNet_RegisterDriver(VFSNetDriver* driver) {
    if (!g_vfsnet.initialized) {
        serial_printf("[VFS-NET] ERROR: Not initialized\n");
        return false;
    }

    if (!driver || !driver->scheme) {
        serial_printf("[VFS-NET] ERROR: Invalid driver\n");
        return false;
    }

    if (g_vfsnet.driver_count >= VFSNET_MAX_DRIVERS) {
        serial_printf("[VFS-NET] ERROR: Too many drivers\n");
        return false;
    }

    /* Check for duplicate */
    for (int i = 0; i < g_vfsnet.driver_count; i++) {
        if (strcmp(g_vfsnet.drivers[i]->scheme, driver->scheme) == 0) {
            serial_printf("[VFS-NET] WARNING: Driver '%s' already registered\n",
                          driver->scheme);
            return false;
        }
    }

    g_vfsnet.drivers[g_vfsnet.driver_count++] = driver;

    serial_printf("[VFS-NET] Registered driver: %s%s (default port: %s)\n",
                  driver->scheme,
                  driver->secure ? " (secure)" : "",
                  driver->default_port ? driver->default_port : "N/A");

    return true;
}

/* Unregister a network filesystem driver */
void VFSNet_UnregisterDriver(const char* scheme) {
    if (!scheme) return;

    for (int i = 0; i < g_vfsnet.driver_count; i++) {
        if (strcmp(g_vfsnet.drivers[i]->scheme, scheme) == 0) {
            serial_printf("[VFS-NET] Unregistering driver: %s\n", scheme);

            /* Shift remaining drivers */
            for (int j = i; j < g_vfsnet.driver_count - 1; j++) {
                g_vfsnet.drivers[j] = g_vfsnet.drivers[j + 1];
            }
            g_vfsnet.driver_count--;
            return;
        }
    }
}

/* Find driver by scheme */
VFSNetDriver* VFSNet_FindDriver(const char* scheme) {
    if (!scheme) return NULL;

    for (int i = 0; i < g_vfsnet.driver_count; i++) {
        if (strcmp(g_vfsnet.drivers[i]->scheme, scheme) == 0) {
            return g_vfsnet.drivers[i];
        }
    }

    return NULL;
}

/* Parse URL into components
 * Format: scheme://[user@]host[:port]/path
 */
bool VFSNet_ParseURL(const char* url, char* scheme, size_t scheme_len,
                     char* host, size_t host_len,
                     uint16_t* port, char* path, size_t path_len) {
    if (!url || !scheme || !host) {
        return false;
    }

    scheme[0] = '\0';
    host[0] = '\0';
    if (port) *port = 0;
    if (path) path[0] = '\0';

    /* Find scheme separator */
    const char* scheme_end = strstr(url, "://");
    if (!scheme_end) {
        return false;
    }

    /* Extract scheme */
    size_t scheme_size = scheme_end - url;
    if (scheme_size >= scheme_len) {
        return false;
    }
    strncpy(scheme, url, scheme_size);
    scheme[scheme_size] = '\0';

    /* Convert scheme to lowercase */
    for (char* p = scheme; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = *p + ('a' - 'A');
        }
    }

    /* Skip "://" */
    const char* rest = scheme_end + 3;

    /* Skip optional user@ */
    const char* at = strchr(rest, '@');
    const char* slash = strchr(rest, '/');
    if (at && (!slash || at < slash)) {
        rest = at + 1;
    }

    /* Find host end (either ':', '/', or end of string) */
    const char* colon = strchr(rest, ':');
    slash = strchr(rest, '/');

    const char* host_end = rest;
    while (*host_end && *host_end != ':' && *host_end != '/') {
        host_end++;
    }

    /* Extract host */
    size_t host_size = host_end - rest;
    if (host_size >= host_len) {
        return false;
    }
    strncpy(host, rest, host_size);
    host[host_size] = '\0';

    /* Extract port if present */
    if (*host_end == ':') {
        if (port) {
            *port = (uint16_t)atoi(host_end + 1);
        }
        /* Skip to slash or end */
        while (*host_end && *host_end != '/') {
            host_end++;
        }
    }

    /* Extract path */
    if (*host_end == '/' && path) {
        strncpy(path, host_end, path_len - 1);
        path[path_len - 1] = '\0';
    } else if (path) {
        strcpy(path, "/");
    }

    return true;
}

/* Find free mount slot */
static VFSNetMount* VFSNet_AllocMount(void) {
    for (int i = 0; i < VFSNET_MAX_MOUNTS; i++) {
        if (!g_vfsnet.mounts[i].mounted) {
            return &g_vfsnet.mounts[i];
        }
    }
    return NULL;
}

/* Mount a network filesystem */
int VFSNet_Mount(const char* url, const char* mount_point, void* options) {
    if (!g_vfsnet.initialized || !url || !mount_point) {
        return -1;
    }

    serial_printf("[VFS-NET] Mounting '%s' → '%s'\n", url, mount_point);

    /* Parse URL */
    char scheme[32];
    char host[256];
    uint16_t port = 0;
    char remote_path[256];

    if (!VFSNet_ParseURL(url, scheme, sizeof(scheme),
                         host, sizeof(host),
                         &port, remote_path, sizeof(remote_path))) {
        serial_printf("[VFS-NET] ERROR: Failed to parse URL '%s'\n", url);
        return -1;
    }

    /* Find driver */
    VFSNetDriver* driver = VFSNet_FindDriver(scheme);
    if (!driver) {
        serial_printf("[VFS-NET] ERROR: No driver for scheme '%s'\n", scheme);
        return -1;
    }

    /* Use default port if not specified */
    if (port == 0 && driver->default_port) {
        port = (uint16_t)atoi(driver->default_port);
    }

    /* Allocate mount entry */
    VFSNetMount* mount = VFSNet_AllocMount();
    if (!mount) {
        serial_printf("[VFS-NET] ERROR: No free mount slots\n");
        return -1;
    }

    /* Initialize mount entry */
    memset(mount, 0, sizeof(VFSNetMount));
    strncpy(mount->mount_point, mount_point, sizeof(mount->mount_point) - 1);
    strncpy(mount->scheme, scheme, sizeof(mount->scheme) - 1);
    strncpy(mount->host, host, sizeof(mount->host) - 1);
    strncpy(mount->path, remote_path, sizeof(mount->path) - 1);
    mount->port = port;
    mount->secure = driver->secure;
    mount->driver = driver;

    /* Call driver mount operation */
    if (driver->mount) {
        int result = driver->mount(url, mount_point, options);
        if (result != 0) {
            serial_printf("[VFS-NET] ERROR: Driver mount failed: %d\n", result);
            memset(mount, 0, sizeof(VFSNetMount));
            return result;
        }
    }

    mount->mounted = true;
    g_vfsnet.mount_count++;

    serial_printf("[VFS-NET] Mounted %s://%s:%u%s → %s\n",
                  mount->scheme, mount->host, mount->port,
                  mount->path, mount->mount_point);

    return 0;
}

/* Unmount a network filesystem */
int VFSNet_Unmount(const char* mount_point) {
    if (!g_vfsnet.initialized || !mount_point) {
        return -1;
    }

    for (int i = 0; i < VFSNET_MAX_MOUNTS; i++) {
        VFSNetMount* mount = &g_vfsnet.mounts[i];
        if (mount->mounted && strcmp(mount->mount_point, mount_point) == 0) {
            serial_printf("[VFS-NET] Unmounting '%s'\n", mount_point);

            /* Call driver unmount */
            if (mount->driver && mount->driver->unmount) {
                mount->driver->unmount(mount_point);
            }

            memset(mount, 0, sizeof(VFSNetMount));
            g_vfsnet.mount_count--;

            serial_printf("[VFS-NET] Unmount complete\n");
            return 0;
        }
    }

    serial_printf("[VFS-NET] ERROR: Mount point '%s' not found\n", mount_point);
    return -1;
}

/* Find mount by path (longest prefix match) */
VFSNetMount* VFSNet_FindMount(const char* path) {
    if (!path) return NULL;

    VFSNetMount* best_match = NULL;
    size_t best_len = 0;

    for (int i = 0; i < VFSNET_MAX_MOUNTS; i++) {
        VFSNetMount* mount = &g_vfsnet.mounts[i];
        if (!mount->mounted) continue;

        size_t mount_len = strlen(mount->mount_point);

        /* Check if path starts with mount point */
        if (strncmp(path, mount->mount_point, mount_len) == 0) {
            /* Ensure it's a proper prefix (followed by '/' or end) */
            if (path[mount_len] == '\0' || path[mount_len] == '/') {
                if (mount_len > best_len) {
                    best_match = mount;
                    best_len = mount_len;
                }
            }
        }
    }

    return best_match;
}

/* List all network mounts */
void VFSNet_ListMounts(void) {
    serial_printf("[VFS-NET] === Network Mounts ===\n");

    bool found = false;
    for (int i = 0; i < VFSNET_MAX_MOUNTS; i++) {
        VFSNetMount* mount = &g_vfsnet.mounts[i];
        if (mount->mounted) {
            serial_printf("[VFS-NET] %s://%s:%u%s → %s%s\n",
                          mount->scheme, mount->host, mount->port, mount->path,
                          mount->mount_point,
                          mount->secure ? " [SECURE]" : "");
            found = true;
        }
    }

    if (!found) {
        serial_printf("[VFS-NET] (no network mounts)\n");
    }

    serial_printf("[VFS-NET] Total: %d mount(s)\n", g_vfsnet.mount_count);
}
