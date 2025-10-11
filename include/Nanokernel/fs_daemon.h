/* System 7X Nanokernel - Filesystem Daemon Interface
 *
 * Kernel-side abstraction for user-space filesystem daemons.
 * Routes VFS operations to appropriate daemons via IPC.
 */

#ifndef NANOKERNEL_FS_DAEMON_H
#define NANOKERNEL_FS_DAEMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ipc.h"

#define FSD_MAX_DAEMONS 8
#define FSD_MAX_PATH 256

/* Filesystem daemon message types */
typedef enum {
    FS_MSG_READ_FILE,
    FS_MSG_WRITE_FILE,
    FS_MSG_LIST_DIR,
    FS_MSG_LOOKUP,
    FS_MSG_CREATE_FILE,
    FS_MSG_DELETE_FILE,
    FS_MSG_GET_STATS,
    FS_MSG_GET_FILE_INFO,
    FS_MSG_MOUNT,
    FS_MSG_UNMOUNT,
    FS_MSG_SHUTDOWN,
} FSMessageType;

/* Filesystem request message */
typedef struct {
    FSMessageType type;
    uint32_t request_id;
    uint64_t file_id;       /* File or directory ID */
    uint64_t offset;        /* Read/write offset */
    uint32_t length;        /* Read/write length */
    uint32_t param1;        /* Generic parameter */
    uint32_t param2;        /* Generic parameter */
    char path[FSD_MAX_PATH]; /* File path */
} FSRequest;

/* Filesystem response message */
typedef struct {
    uint32_t request_id;
    int32_t  result;        /* Error code (0 = success) */
    uint32_t data_length;   /* Length of returned data */
    uint64_t param1;        /* Generic return parameter */
    uint64_t param2;        /* Generic return parameter */
    uint8_t  data[4096];    /* Inline data for small reads */
} FSResponse;

/* Filesystem daemon descriptor */
typedef struct {
    char         name[32];           /* Daemon name (e.g., "HFSd", "FATd") */
    uint32_t     pid;                /* Process ID */
    MessagePort  request_port;       /* Send requests here */
    MessagePort  response_port;      /* Receive responses here */
    bool         active;             /* Is daemon running? */
    uint32_t     next_request_id;    /* Next request ID */
} FSDaemon;

/* Filesystem Daemon Interface Functions */

/* Initialize FSD subsystem */
void FSD_Initialize(void);

/* Register a filesystem daemon */
bool FSD_Register(const char* name, uint32_t pid, MessagePort req_port, MessagePort resp_port);

/* Unregister a filesystem daemon */
void FSD_Unregister(const char* name);

/* Find daemon by name */
FSDaemon* FSD_Find(const char* name);

/* Send request to daemon and wait for response */
bool FSD_SendRequest(const char* fs_name, const FSRequest* req, FSResponse* resp);

/* Send request without waiting for response */
bool FSD_PostRequest(const char* fs_name, const FSRequest* req);

/* High-level VFS operations routed through daemons */
bool FSD_ReadFile(const char* fs_name, uint64_t file_id, uint64_t offset,
                  void* buffer, size_t length, size_t* bytes_read);

bool FSD_WriteFile(const char* fs_name, uint64_t file_id, uint64_t offset,
                   const void* buffer, size_t length, size_t* bytes_written);

bool FSD_ListDir(const char* fs_name, uint64_t dir_id,
                 bool (*callback)(void* user_data, const char* name,
                                  uint64_t id, bool is_dir),
                 void* user_data);

bool FSD_Lookup(const char* fs_name, uint64_t dir_id, const char* name,
                uint64_t* entry_id, bool* is_dir);

bool FSD_GetStats(const char* fs_name, uint64_t* total_bytes, uint64_t* free_bytes);

bool FSD_GetFileInfo(const char* fs_name, uint64_t entry_id,
                     uint64_t* size, bool* is_dir, uint64_t* mod_time);

bool FSD_Enumerate(const char* fs_name, uint64_t dir_id,
                   bool (*callback)(void* user_data, const char* name,
                                    uint64_t id, bool is_dir),
                   void* user_data);

/* Daemon management */
void FSD_ListDaemons(void);
int  FSD_GetDaemonCount(void);

#endif /* NANOKERNEL_FS_DAEMON_H */
