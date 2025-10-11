/* System 7X Nanokernel - Filesystem Daemon Implementation
 *
 * Routes VFS operations to user-space filesystem daemons via IPC.
 */

#include "../../include/Nanokernel/fs_daemon.h"
#include "../../include/Nanokernel/ipc.h"
#include "../../include/System71StdLib.h"
#include <string.h>
#include <stdlib.h>

static struct {
    FSDaemon daemons[FSD_MAX_DAEMONS];
    int daemon_count;
    bool initialized;
} g_fsd_system = { 0 };

/* Initialize filesystem daemon subsystem */
void FSD_Initialize(void) {
    if (g_fsd_system.initialized) {
        serial_printf("[FSD] Already initialized\n");
        return;
    }

    memset(&g_fsd_system, 0, sizeof(g_fsd_system));
    g_fsd_system.initialized = true;

    serial_printf("[FSD] Filesystem daemon subsystem initialized\n");
}

/* Register a filesystem daemon */
bool FSD_Register(const char* name, uint32_t pid, MessagePort req_port, MessagePort resp_port) {
    if (!g_fsd_system.initialized) {
        serial_printf("[FSD] ERROR: System not initialized\n");
        return false;
    }

    if (g_fsd_system.daemon_count >= FSD_MAX_DAEMONS) {
        serial_printf("[FSD] ERROR: Too many daemons (max %d)\n", FSD_MAX_DAEMONS);
        return false;
    }

    /* Check for duplicate */
    for (int i = 0; i < FSD_MAX_DAEMONS; i++) {
        if (g_fsd_system.daemons[i].active &&
            strcmp(g_fsd_system.daemons[i].name, name) == 0) {
            serial_printf("[FSD] ERROR: Daemon '%s' already registered\n", name);
            return false;
        }
    }

    /* Find free slot */
    FSDaemon* daemon = NULL;
    for (int i = 0; i < FSD_MAX_DAEMONS; i++) {
        if (!g_fsd_system.daemons[i].active) {
            daemon = &g_fsd_system.daemons[i];
            break;
        }
    }

    if (!daemon) {
        serial_printf("[FSD] ERROR: No free daemon slots\n");
        return false;
    }

    /* Initialize daemon descriptor */
    memset(daemon, 0, sizeof(FSDaemon));
    strncpy(daemon->name, name, sizeof(daemon->name) - 1);
    daemon->pid = pid;
    daemon->request_port = req_port;
    daemon->response_port = resp_port;
    daemon->active = true;
    daemon->next_request_id = 1;

    g_fsd_system.daemon_count++;

    serial_printf("[FSD] Registered %s (pid %u)\n", name, pid);
    return true;
}

/* Unregister a filesystem daemon */
void FSD_Unregister(const char* name) {
    for (int i = 0; i < FSD_MAX_DAEMONS; i++) {
        if (g_fsd_system.daemons[i].active &&
            strcmp(g_fsd_system.daemons[i].name, name) == 0) {

            serial_printf("[FSD] Unregistering %s\n", name);
            g_fsd_system.daemons[i].active = false;
            g_fsd_system.daemon_count--;
            return;
        }
    }
}

/* Find daemon by name */
FSDaemon* FSD_Find(const char* name) {
    for (int i = 0; i < FSD_MAX_DAEMONS; i++) {
        if (g_fsd_system.daemons[i].active &&
            strcmp(g_fsd_system.daemons[i].name, name) == 0) {
            return &g_fsd_system.daemons[i];
        }
    }
    return NULL;
}

/* Send request to daemon and wait for response */
bool FSD_SendRequest(const char* fs_name, const FSRequest* req, FSResponse* resp) {
    FSDaemon* daemon = FSD_Find(fs_name);
    if (!daemon) {
        serial_printf("[FSD] ERROR: Daemon '%s' not found\n", fs_name);
        return false;
    }

    /* Send request */
    if (!IPC_Send(daemon->request_port, req, sizeof(FSRequest))) {
        serial_printf("[FSD] ERROR: Failed to send request to %s\n", fs_name);
        return false;
    }

    /* Wait for response */
    size_t resp_len = 0;
    if (!IPC_Receive(daemon->response_port, resp, sizeof(FSResponse), &resp_len)) {
        serial_printf("[FSD] ERROR: Failed to receive response from %s\n", fs_name);
        return false;
    }

    if (resp_len != sizeof(FSResponse)) {
        serial_printf("[FSD] ERROR: Invalid response size from %s (%zu != %zu)\n",
                      fs_name, resp_len, sizeof(FSResponse));
        return false;
    }

    return true;
}

/* Send request without waiting for response */
bool FSD_PostRequest(const char* fs_name, const FSRequest* req) {
    FSDaemon* daemon = FSD_Find(fs_name);
    if (!daemon) {
        serial_printf("[FSD] ERROR: Daemon '%s' not found\n", fs_name);
        return false;
    }

    /* Send request */
    if (!IPC_Send(daemon->request_port, req, sizeof(FSRequest))) {
        serial_printf("[FSD] ERROR: Failed to send request to %s\n", fs_name);
        return false;
    }

    return true;
}

/* Read file through daemon */
bool FSD_ReadFile(const char* fs_name, uint64_t file_id, uint64_t offset,
                  void* buffer, size_t length, size_t* bytes_read) {
    FSDaemon* daemon = FSD_Find(fs_name);
    if (!daemon) {
        return false;
    }

    /* Build request */
    FSRequest req = {
        .type = FS_MSG_READ_FILE,
        .request_id = daemon->next_request_id++,
        .file_id = file_id,
        .offset = offset,
        .length = (uint32_t)length,
    };

    /* Send request and get response */
    FSResponse resp = { 0 };
    if (!FSD_SendRequest(fs_name, &req, &resp)) {
        return false;
    }

    /* Check result */
    if (resp.result != 0) {
        serial_printf("[FSD] Read failed: error %d\n", resp.result);
        return false;
    }

    /* Copy data */
    size_t copy_len = (resp.data_length < length) ? resp.data_length : length;
    memcpy(buffer, resp.data, copy_len);

    if (bytes_read) {
        *bytes_read = copy_len;
    }

    return true;
}

/* Write file through daemon */
bool FSD_WriteFile(const char* fs_name, uint64_t file_id, uint64_t offset,
                   const void* buffer, size_t length, size_t* bytes_written) {
    FSDaemon* daemon = FSD_Find(fs_name);
    if (!daemon) {
        return false;
    }

    /* Build request */
    FSRequest req = {
        .type = FS_MSG_WRITE_FILE,
        .request_id = daemon->next_request_id++,
        .file_id = file_id,
        .offset = offset,
        .length = (uint32_t)length,
    };

    /* TODO: For now, we only support inline writes up to 4KB
     * In future, use shared memory for larger transfers */
    if (length > sizeof(((FSResponse*)0)->data)) {
        serial_printf("[FSD] ERROR: Write too large (%zu > 4096)\n", length);
        return false;
    }

    /* Send request and get response */
    FSResponse resp = { 0 };
    if (!FSD_SendRequest(fs_name, &req, &resp)) {
        return false;
    }

    /* Check result */
    if (resp.result != 0) {
        serial_printf("[FSD] Write failed: error %d\n", resp.result);
        return false;
    }

    if (bytes_written) {
        *bytes_written = (size_t)resp.param1;
    }

    return true;
}

/* List directory through daemon */
bool FSD_ListDir(const char* fs_name, uint64_t dir_id,
                 bool (*callback)(void* user_data, const char* name,
                                  uint64_t id, bool is_dir),
                 void* user_data) {
    FSDaemon* daemon = FSD_Find(fs_name);
    if (!daemon) {
        return false;
    }

    /* Build request */
    FSRequest req = {
        .type = FS_MSG_LIST_DIR,
        .request_id = daemon->next_request_id++,
        .file_id = dir_id,
    };

    /* Send request and get response */
    FSResponse resp = { 0 };
    if (!FSD_SendRequest(fs_name, &req, &resp)) {
        return false;
    }

    /* Check result */
    if (resp.result != 0) {
        serial_printf("[FSD] List dir failed: error %d\n", resp.result);
        return false;
    }

    /* TODO: For now, return success
     * In future, implement proper directory enumeration protocol */
    return true;
}

/* Lookup file/directory through daemon */
bool FSD_Lookup(const char* fs_name, uint64_t dir_id, const char* name,
                uint64_t* entry_id, bool* is_dir) {
    FSDaemon* daemon = FSD_Find(fs_name);
    if (!daemon) {
        return false;
    }

    /* Build request */
    FSRequest req = {
        .type = FS_MSG_LOOKUP,
        .request_id = daemon->next_request_id++,
        .file_id = dir_id,
    };
    strncpy(req.path, name, sizeof(req.path) - 1);

    /* Send request and get response */
    FSResponse resp = { 0 };
    if (!FSD_SendRequest(fs_name, &req, &resp)) {
        return false;
    }

    /* Check result */
    if (resp.result != 0) {
        return false;
    }

    if (entry_id) {
        *entry_id = resp.param1;
    }

    if (is_dir) {
        *is_dir = (bool)resp.param2;
    }

    return true;
}

/* Get filesystem statistics through daemon */
bool FSD_GetStats(const char* fs_name, uint64_t* total_bytes, uint64_t* free_bytes) {
    FSDaemon* daemon = FSD_Find(fs_name);
    if (!daemon) {
        return false;
    }

    /* Build request */
    FSRequest req = {
        .type = FS_MSG_GET_STATS,
        .request_id = daemon->next_request_id++,
    };

    /* Send request and get response */
    FSResponse resp = { 0 };
    if (!FSD_SendRequest(fs_name, &req, &resp)) {
        return false;
    }

    /* Check result */
    if (resp.result != 0) {
        return false;
    }

    if (total_bytes) {
        *total_bytes = resp.param1;
    }

    if (free_bytes) {
        *free_bytes = resp.param2;
    }

    return true;
}

/* List all registered daemons */
void FSD_ListDaemons(void) {
    serial_printf("[FSD] === Registered Daemons ===\n");

    int count = 0;
    for (int i = 0; i < FSD_MAX_DAEMONS; i++) {
        if (g_fsd_system.daemons[i].active) {
            FSDaemon* d = &g_fsd_system.daemons[i];
            serial_printf("[FSD] %s (pid %u, req_queue=%p, resp_queue=%p)\n",
                          d->name, d->pid, d->request_port, d->response_port);
            count++;
        }
    }

    serial_printf("[FSD] Total: %d daemon(s)\n", count);
}

/* Get daemon count */
int FSD_GetDaemonCount(void) {
    return g_fsd_system.daemon_count;
}
