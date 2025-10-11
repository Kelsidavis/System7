/* System 7X - VFSNetd Common Daemon Infrastructure
 *
 * Shared infrastructure for network filesystem daemons (WebDAVd, SFTPd).
 * Handles IPC communication with kernel VFS-Net layer.
 */

#include "../../../include/Nanokernel/ipc.h"
#include "../../../include/Nanokernel/fs_daemon.h"
#include "../../../include/System71StdLib.h"
#include <string.h>

/* VFSNetd daemon state */
typedef struct {
    const char* daemon_name;
    MessagePort request_port;
    MessagePort response_port;
    bool running;
} VFSNetDaemon;

/* Initialize network filesystem daemon */
bool VFSNetd_Init(VFSNetDaemon* daemon, const char* name) {
    if (!daemon || !name) {
        return false;
    }

    daemon->daemon_name = name;
    daemon->running = false;

    /* Create IPC ports (using message queue system) */
    extern MessagePort IPC_CreateQueue(const char* queue_name);
    char req_name[64], resp_name[64];
    snprintf(req_name, sizeof(req_name), "%s_req", name);
    snprintf(resp_name, sizeof(resp_name), "%s_resp", name);

    daemon->request_port = IPC_CreateQueue(req_name);
    daemon->response_port = IPC_CreateQueue(resp_name);

    if (!daemon->request_port || !daemon->response_port) {
        serial_printf("[%s] ERROR: Failed to create IPC ports\n", name);
        return false;
    }

    serial_printf("[%s] Daemon initialized (req_port=%p, resp_port=%p)\n",
                  name, daemon->request_port, daemon->response_port);

    return true;
}

/* Register with kernel FSD subsystem */
bool VFSNetd_Register(VFSNetDaemon* daemon) {
    if (!daemon) {
        return false;
    }

    extern bool FSD_Register(const char* name, uint32_t pid,
                             MessagePort req_port, MessagePort resp_port);

    uint32_t pid = 1000;  /* TODO: Get actual process ID */

    if (!FSD_Register(daemon->daemon_name, pid,
                     daemon->request_port, daemon->response_port)) {
        serial_printf("[%s] ERROR: Failed to register with FSD\n",
                      daemon->daemon_name);
        return false;
    }

    serial_printf("[%s] Registered with kernel FSD subsystem\n",
                  daemon->daemon_name);

    daemon->running = true;
    return true;
}

/* Process incoming requests (stub - would be expanded with actual handlers) */
void VFSNetd_ProcessRequests(VFSNetDaemon* daemon) {
    if (!daemon || !daemon->running) {
        return;
    }

    /* In real implementation:
     * - Poll request_port for incoming messages
     * - Dispatch to appropriate handler based on message type
     * - Send responses back through response_port
     * - Yield to other daemons when idle
     */

    serial_printf("[%s] Processing requests (stub)\n", daemon->daemon_name);
}

/* Shutdown daemon */
void VFSNetd_Shutdown(VFSNetDaemon* daemon) {
    if (!daemon) {
        return;
    }

    serial_printf("[%s] Shutting down\n", daemon->daemon_name);

    extern void FSD_Unregister(const char* name);
    FSD_Unregister(daemon->daemon_name);

    daemon->running = false;
}
