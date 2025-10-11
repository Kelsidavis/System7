/* System 7X Nanokernel - VFS-Net: Network & Virtual Filesystem Layer
 *
 * Modern network filesystem abstraction supporting WebDAV, SFTP, SMB, etc.
 * Provides transparent access to local, remote, and synthetic filesystems.
 */

#ifndef NANOKERNEL_VFS_NET_H
#define NANOKERNEL_VFS_NET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "syscalls.h"  /* For POSIX types: mode_t, ssize_t, off_t, struct dirent, struct stat */

/* Forward declarations */
typedef struct VFSNetDriver VFSNetDriver;
typedef struct VFSNetMount VFSNetMount;
typedef struct VFSNetConnection VFSNetConnection;

/* Network filesystem driver interface */
struct VFSNetDriver {
    /* Driver identification */
    const char* scheme;           /* "smb", "sftp", "webdav", "grpcfs" */
    bool        secure;           /* Default security (HTTPS, SSH, TLS) */
    const char* default_port;     /* Default port string (e.g., "443", "22") */

    /* Driver operations */

    /* Mount a remote filesystem
     * url: Full URL (e.g., "webdav://server/docs")
     * mount_point: Local mount path (e.g., "/Volumes/RemoteDocs")
     * Returns: 0 on success, negative errno on failure
     */
    int (*mount)(const char* url, const char* mount_point, void* options);

    /* Unmount a remote filesystem */
    int (*unmount)(const char* mount_point);

    /* File operations */
    int (*open)(const char* path, int flags, mode_t mode);
    ssize_t (*read)(int fd, void* buf, size_t n);
    ssize_t (*write)(int fd, const void* buf, size_t n);
    int (*close)(int fd);
    off_t (*lseek)(int fd, off_t offset, int whence);

    /* Directory operations */
    int (*opendir)(const char* path);
    int (*readdir)(int dirfd, struct dirent* entry);
    int (*closedir)(int dirfd);

    /* Metadata operations */
    int (*stat)(const char* path, struct stat* st);
    int (*mkdir)(const char* path, mode_t mode);
    int (*unlink)(const char* path);
    int (*rename)(const char* oldpath, const char* newpath);

    /* Driver-specific data */
    void* private_data;
};

/* Network mount entry */
struct VFSNetMount {
    char mount_point[256];        /* Local mount path */
    char scheme[16];              /* Protocol scheme */
    char host[256];               /* Remote host */
    uint16_t port;                /* Remote port */
    char path[256];               /* Remote path */
    bool secure;                  /* Use encryption */
    bool mounted;                 /* Mount status */
    VFSNetDriver* driver;         /* Associated driver */
    void* driver_data;            /* Driver-specific mount data */
    uint32_t mount_flags;         /* Mount options */
};

/* VFS-Net Core API */

/* Initialize VFS-Net subsystem */
bool VFSNet_Initialize(void);

/* Shutdown VFS-Net subsystem */
void VFSNet_Shutdown(void);

/* Register a network filesystem driver */
bool VFSNet_RegisterDriver(VFSNetDriver* driver);

/* Unregister a network filesystem driver */
void VFSNet_UnregisterDriver(const char* scheme);

/* Find driver by scheme */
VFSNetDriver* VFSNet_FindDriver(const char* scheme);

/* Mount a network filesystem
 * url: Full URL (e.g., "webdav://server:443/docs")
 * mount_point: Local mount path
 * options: Driver-specific options (can be NULL)
 * Returns: 0 on success, negative errno on failure
 */
int VFSNet_Mount(const char* url, const char* mount_point, void* options);

/* Unmount a network filesystem */
int VFSNet_Unmount(const char* mount_point);

/* Find mount by path (checks if path is under a network mount) */
VFSNetMount* VFSNet_FindMount(const char* path);

/* List all network mounts */
void VFSNet_ListMounts(void);

/* URL parsing helpers */
bool VFSNet_ParseURL(const char* url, char* scheme, size_t scheme_len,
                     char* host, size_t host_len,
                     uint16_t* port, char* path, size_t path_len);

#endif /* NANOKERNEL_VFS_NET_H */
